#!/usr/bin/env python3
"""GGUF quant blocks are [fp16 scale(s) | packed quants] at fixed strides.
HF gets ~1.000x on them; mzip/zstd get 0-4%. The scale bytes are structured
and the quants near-uniform, but no coder separates them because the stride is
18/34/144/176/210 -- not 2/4/8. Test a block-aware de-interleave.

Step 1: find (stride, phase) by minimising summed per-column byte entropy over
        candidate strides (slices are mid-file, alignment unknown).
Step 2: de-interleave into per-column streams (full SoA at that stride), and
        into a 2-stream split (low-entropy cols vs high-entropy cols); compress
        each arrangement with zstd-19 and mzip; compare to plain. Whole-file
        first (decisive), then 64K chunks for the winner.
Sizes only; mzip outputs round-trip verified.
"""
import os, json, subprocess, numpy as np, zstandard as zstd
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; MZ="/root/mzip-hfbench/build/mzip_fixed2"
Z19=zstd.ZstdCompressor(level=19)
STRIDES={"q4_k_m":[144,210,18],"q6_k":[210,144],"q8_0":[34,18]}

def col_entropy_sum(a, stride, phase):
    b=a[phase:]; n=(len(b)//stride)*stride
    if n<stride*256: return 1e18
    m=b[:n].reshape(-1,stride)
    tot=0.0
    for c in range(stride):
        h=np.bincount(m[:,c],minlength=256).astype(np.float64); p=h[h>0]/h.sum()
        tot+=-(p*np.log2(p)).sum()
    return tot/stride   # mean bits/byte over columns

def find_layout(a, strides):
    best=(1e18,None,None)
    for s in strides:
        for ph in range(s):
            e=col_entropy_sum(a,s,ph)
            if e<best[0]: best=(e,s,ph)
    return best

def mzip_sz(b, base):
    ip,op,rp=base+".in",base+".mz",base+".rt"; open(ip,"wb").write(b)
    subprocess.run([MZ,"c",ip,op,"1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); s=os.path.getsize(op)
    subprocess.run([MZ,"d",op,rp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); ok=open(rp,"rb").read()==b
    for f in (ip,op,rp): os.remove(f)
    return s, ok

def soa(a, s, ph, low_cols=None):
    """Full per-column SoA (low_cols=None) or 2-stream split. Returns bytes with
    head/tail passthrough so it is exactly invertible."""
    head=a[:ph].tobytes(); b=a[ph:]; n=(len(b)//s)*s; m=b[:n].reshape(-1,s); tail=b[n:].tobytes()
    if low_cols is None:
        body=b"".join(m[:,c].tobytes() for c in range(s))
    else:
        lo=[c for c in range(s) if c in low_cols]; hi=[c for c in range(s) if c not in low_cols]
        body=m[:,lo].tobytes()+m[:,hi].tobytes()
    return head+body+tail

rows=[]
for q,strides in STRIDES.items():
    p=f"{W}/gguf-{q}.slice"
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); a=np.frombuffer(raw,dtype=np.uint8); N=len(raw)
    e,s,ph=find_layout(a,strides)
    # column entropies at the found layout
    b=a[ph:]; n=(len(b)//s)*s; m=b[:n].reshape(-1,s)
    ce=[]
    for c in range(s):
        h=np.bincount(m[:,c],minlength=256).astype(np.float64); pp=h[h>0]/h.sum(); ce.append(float(-(pp*np.log2(pp)).sum()))
    ce=np.array(ce); low=set(np.where(ce<6.0)[0].tolist())
    print(f"\n=== gguf-{q}: best stride {s} phase {ph}, mean col entropy {e:.3f} b/B; {len(low)} low-entropy cols (<6 bits): {sorted(low)[:20]}{'...' if len(low)>20 else ''}",flush=True)
    print(f"    col entropies: min {ce.min():.2f} max {ce.max():.2f}; low-col mean {ce[list(low)].mean() if low else 0:.2f}, high-col mean {ce[[c for c in range(s) if c not in low]].mean():.2f}",flush=True)
    arms={"plain":raw,"soa_full":soa(a,s,ph),"soa_2stream":soa(a,s,ph,low)}
    r=dict(quant=q,stride=s,phase=ph,n=N,low_cols=len(low))
    for name,data in arms.items():
        assert len(data)==N
        z=len(Z19.compress(data)); mzs,ok=mzip_sz(data,f"{OUT}/gb.{q}.{name}")
        r[f"{name}_z19"]=z; r[f"{name}_mzip"]=mzs; r[f"{name}_rt"]=ok
        print(f"    {name:12s} zstd-19 {z:>9,} ({N/z:.4f}x)   mzip {mzs:>9,} ({N/mzs:.4f}x) {'rt ok' if ok else 'RT_FAIL'}",flush=True)
    pz=r["plain_z19"]; pm=r["plain_mzip"]
    for name in ("soa_full","soa_2stream"):
        print(f"    {name:12s} vs plain: zstd-19 {100*(pz-r[name+'_z19'])/pz:+.2f}%   mzip {100*(pm-r[name+'_mzip'])/pm:+.2f}%",flush=True)
    rows.append(r)
json.dump(rows,open(f"{OUT}/gguf_block.json","w"),indent=1)
print("\nALLDONE_GB")
