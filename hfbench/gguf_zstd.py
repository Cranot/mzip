#!/usr/bin/env python3
"""HF's bg4-lz4 gets ~1.000x on GGUF. Does stock zstd (which has an entropy
coder, unlike lz4) capture what mzip found? Arms per 64K chunk and whole:
zstd-1/3/19 plain, bg4+zstd-1/3/19, and bg2+zstd-3 (F16 is 2-byte)."""
import os, json, numpy as np, zstandard as zstd
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; CS=64*1024
def bg(data,k):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k)
    return b"".join(m[:,j].tobytes() for j in range(k))+a[n:].tobytes()
Z={L:zstd.ZstdCompressor(level=L) for L in (1,3,19)}
rows=[]
print(f"{'quant':8s} {'chunk':6s} {'z1':>9s} {'z3':>9s} {'z19':>9s} {'bg4z1':>9s} {'bg4z3':>9s} {'bg4z19':>9s} {'bg2z3':>9s}")
for q in ("q4_k_m","q6_k","q8_0","f16"):
    p=f"{W}/gguf-{q}.slice"
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); N=len(raw)
    for csz in (CS,0):
        ch=[raw] if csz==0 else [raw[i:i+csz] for i in range(0,N,csz)]
        r=dict(quant=q,n=N,chunk=csz)
        for L in (1,3,19):
            r[f"z{L}"]=sum(len(Z[L].compress(c)) for c in ch)
            r[f"bg4z{L}"]=sum(len(Z[L].compress(bg(c,4))) for c in ch)
        r["bg2z3"]=sum(len(Z[3].compress(bg(c,2))) for c in ch)
        rows.append(r)
        cs="whole" if csz==0 else "64K"
        print(f"{q:8s} {cs:6s} {r['z1']:>9,} {r['z3']:>9,} {r['z19']:>9,} {r['bg4z1']:>9,} {r['bg4z3']:>9,} {r['bg4z19']:>9,} {r['bg2z3']:>9,}",flush=True)
json.dump(rows,open(f"{OUT}/gguf_zstd.json","w"),indent=1)
print("ALLDONE_GZ")
