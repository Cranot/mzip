#!/usr/bin/env python3
"""Recompute the 'auto' column with Xet's EXACT BG4Predictor (bg4_prediction.rs
lines 211-250): KL of each byte-position popcount histogram against the POOLED
histogram (+1 base count per bin), max over the 4 positions, recommend bg4 if
> 0.02. Zero-count bins contribute 0 (Rust's 0*ln(0) is NaN and f64::max drops it).
lz4/bg4 only — fast, no mzip, so contention is irrelevant."""
import json, os, numpy as np, lz4.frame

W="/root/mzip-hfbench/weights"; C="/root/mzip-hfbench/corpora"; OUT="/root/mzip-hfbench/chunk"
INPUTS=[("MiniLM-L6-v2 F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2 F32",f"{W}/gpt2.slice"),
 ("bert-base F32",f"{W}/bert-base.slice"),("bge-small F32",f"{W}/bge-small.slice"),
 ("weights-fp32 F32",f"{W}/weights-fp32.bin"),("weights-bf16",f"{W}/weights-bf16.bin"),
 ("Qwen2.5-0.5B BF16",f"{W}/Qwen2.5-0.5B.slice"),("prose-wikitext",f"{C}/prose-wikitext.bin"),
 ("code-python",f"{C}/code-python.bin"),("tabular-numeric",f"{C}/tabular-numeric.bin")]
INPUTS=[(t,p) for t,p in INPUTS if os.path.exists(p)]
POP=np.array([bin(i).count("1") for i in range(256)],dtype=np.uint8)

def xet_bg4_recommended(data):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//4)*4
    if n<4: return False
    pc=POP[a[:n].reshape(-1,4)]
    H=np.stack([np.bincount(pc[:,k],minlength=9) for k in range(4)]).astype(np.float64)  # [4][9]
    base=H.sum(axis=0)+1.0; base_total=base.sum()
    q=base/base_total
    mx=0.0
    for i in range(4):
        tot=H[i].sum()
        if tot==0: continue
        p=H[i]/tot
        nz=p>0
        kl=float(np.sum(p[nz]*np.log(p[nz]/q[nz])))
        mx=max(mx,kl)
    return mx>0.02

def bg4_split(data):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
lz=lambda b: len(lz4.frame.compress(b,compression_level=0))
bg=lambda b: len(lz4.frame.compress(bg4_split(b),compression_level=0))

rows=[]
for tag,p in INPUTS:
    raw=open(p,"rb").read(); N=len(raw)
    for csz in (32*1024,64*1024,128*1024,1024*1024,0):
        chunks=[raw] if csz==0 else [raw[i:i+csz] for i in range(0,N,csz)]
        au=0; nb=0
        for ch in chunks:
            r=xet_bg4_recommended(ch); nb+=int(r); au+=bg(ch) if r else lz(ch)
        rows.append(dict(tag=tag,chunk=csz,auto_exact=au,bg4_frac=nb/len(chunks)))
        cs="whole" if csz==0 else f"{csz//1024}K"
        print(f"  {tag:20s} {cs:>6s}  auto(exact) {au:>9,}  bg4 chosen on {100*nb/len(chunks):5.1f}% of chunks",flush=True)
json.dump(rows,open(f"{OUT}/auto_exact.json","w"),indent=1)
print("ALLDONE_AUTO")
