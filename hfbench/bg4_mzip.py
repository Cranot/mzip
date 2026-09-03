#!/usr/bin/env python3
"""bg4 + mzip at 64 KiB chunks.

At whole-file scale, byte-grouping before mzip gave ~0% because mzip's MS/SoA
family already captures it. At 64K the MS path never fires and mzip runs BWT on
INTERLEAVED floats, losing to bg4+zstd-1. So: apply Xet's own bg4 split, then
mzip, per chunk. Sizes only, every chunk round-trip verified.
"""
import os, json, subprocess, numpy as np
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; MZIP="/root/mzip-hfbench/build/mzip_fixed"
CS=64*1024
INPUTS=[("MiniLM-L6-v2","F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2","F32",f"{W}/gpt2.slice"),
 ("bert-base","F32",f"{W}/bert-base.slice"),("bge-small","F32",f"{W}/bge-small.slice"),
 ("weights-fp32","F32",f"{W}/weights-fp32.bin"),("weights-bf16","BF16",f"{W}/weights-bf16.bin"),
 ("Qwen2.5-0.5B","BF16",f"{W}/Qwen2.5-0.5B.slice")]
INPUTS=[x for x in INPUTS if os.path.exists(x[2])]
def bg4(data):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
def mz(b, base):
    ip,op,rp=base+".in",base+".mz",base+".rt"; open(ip,"wb").write(b)
    subprocess.run([MZIP,"c",ip,op,"1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    s=os.path.getsize(op) if os.path.exists(op) else -1
    subprocess.run([MZIP,"d",op,rp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    ok=os.path.exists(rp) and open(rp,"rb").read()==b
    for f in (ip,op,rp):
        if os.path.exists(f): os.remove(f)
    return s, ok
rows=[]
for tag,dt,p in INPUTS:
    raw=open(p,"rb").read(); N=len(raw)
    tot=0; bad=0
    for i in range(0,N,CS):
        s,ok=mz(bg4(raw[i:i+CS]), f"{OUT}/b4m.{tag}.{i}")
        tot+=max(s,0); bad+=int(not ok or s<0)
    rows.append(dict(tag=tag,dtype=dt,n=N,bg4_mzip=tot,bad=bad))
    print(f"  {tag:14s} {dt:4s}  bg4+mzip@64K {tot:>9,}  ({N/tot:.4f}x)  {'RT_FAIL x'+str(bad) if bad else 'rt ok'}",flush=True)
json.dump(rows,open(f"{OUT}/bg4_mzip.json","w"),indent=1)
print("ALLDONE_B4M")
