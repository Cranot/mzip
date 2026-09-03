#!/usr/bin/env python3
"""Push the per-plane order-0 scheme further.
 (A) plane entropy map + conditional entropies (cross-plane: each plane | exponent
     plane; order-1: plane | previous element's same plane) -> ceilings.
 (B) CONDITIONED per-plane FSE: split each non-exponent plane into sub-streams by
     exponent value, code each sub-stream with its own FSE table (raw fallback),
     at 64K and 1M chunks. Real coded sizes vs plain PP-FSE vs bounds.
 (C) plain PP-FSE gap-to-bound vs chunk size (64K / 128K / 1M) on all inputs."""
import os, json, subprocess, numpy as np
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; FSE="/root/mzip-hfbench/fse/fse"
S=[("MiniLM F32",f"{W}/MiniLM-L6-v2.slice",4,3),("gpt2 F32",f"{W}/gpt2.slice",4,3),("bge F32",f"{W}/bge-small.slice",4,3),
   ("wfp32 F32",f"{W}/weights-fp32.bin",4,3),("wbf16 BF16",f"{W}/weights-bf16.bin",2,1),("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice",2,1),
   ("gguf-f16",f"{W}/gguf-f16.slice",2,1)]
def H(x):
    if len(x)==0: return 0.0
    h=np.bincount(x,minlength=256).astype(np.float64); p=h[h>0]/h.sum(); return float(-(p*np.log2(p)).sum())
def Hc(x,y):
    j=np.bincount((y.astype(np.int32)<<8)|x.astype(np.int32),minlength=65536).astype(np.float64).reshape(256,256)
    tot=j.sum(); py=j.sum(axis=1); h=0.0
    for yi in np.nonzero(py)[0]:
        p=j[yi][j[yi]>0]/py[yi]; h+=(py[yi]/tot)*float(-(p*np.log2(p)).sum())
    return h
def fse(b):
    if len(b)<64: return len(b)+1
    open("/tmp/pf.in","wb").write(b); subprocess.run([FSE,"-f","/tmp/pf.in","/tmp/pf.out"],capture_output=True)
    s=os.path.getsize("/tmp/pf.out") if os.path.exists("/tmp/pf.out") else len(b); return min(s,len(b))+1

print("=== (A) plane entropy map: H0 per plane | H(plane | exponent plane) | H(plane | prev same plane) ===")
for tag,p,k,ei in S:
    a=np.frombuffer(open(p,"rb").read(),dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k); e=m[:,ei]
    parts=[]
    for j in range(k):
        x=m[:,j]; h0=H(x); hx=Hc(x,e) if j!=ei else h0; h1=Hc(x[1:],x[:-1])
        parts.append(f"p{j}: {h0:.2f}/{hx:.2f}/{h1:.2f}")
    bound0=sum(H(m[:,j]) for j in range(k))/k/8; boundc=(sum(Hc(m[:,j],e) for j in range(k) if j!=ei)+H(e))/k/8
    print(f"  {tag:11s} {'  '.join(parts)}   => order-0 bound {1/bound0:.4f}x ; conditioned-on-exponent bound {1/boundc:.4f}x ({100*(bound0-boundc)/bound0:+.2f}%)",flush=True)

print("\n=== (B) CONDITIONED per-plane FSE (sub-streams by exponent value) vs plain PP-FSE, real coded sizes ===")
for tag,p,k,ei in S[:1]+S[4:6]:
    raw=open(p,"rb").read(); N=len(raw)
    for CS in (65536, 1<<20):
        plain=cond=0
        for i in range(0,N,CS):
            c=np.frombuffer(raw[i:i+CS],dtype=np.uint8); n=(len(c)//k)*k; m=c[:n].reshape(-1,k); e=m[:,ei]; tail=len(c)-n
            plain+=sum(fse(m[:,j].tobytes()) for j in range(k))+tail
            s=fse(e.tobytes())
            for j in range(k):
                if j==ei: continue
                x=m[:,j]
                for v in np.unique(e): s+=fse(x[e==v].tobytes())
            cond+=s+tail
        print(f"  {tag:11s} chunk {CS>>10:>5}K: plain PP-FSE {plain:>9,} ({N/plain:.4f}x)   conditioned {cond:>9,} ({N/cond:.4f}x)   delta {100*(plain-cond)/plain:+.2f}%",flush=True)

print("\n=== (C) plain PP-FSE gap-to-bound vs chunk size ===")
for tag,p,k,ei in S:
    raw=open(p,"rb").read(); N=len(raw); a=np.frombuffer(raw,dtype=np.uint8)
    n=(len(a)//k)*k; m=a[:n].reshape(-1,k); bound=sum(H(m[:,j])*len(m)/8 for j in range(k))+(len(a)-n)
    row=[]
    for CS in (65536,131072,1<<20):
        t=0
        for i in range(0,N,CS):
            c=np.frombuffer(raw[i:i+CS],dtype=np.uint8); nn=(len(c)//k)*k; mm=c[:nn].reshape(-1,k)
            t+=sum(fse(mm[:,j].tobytes()) for j in range(k))+(len(c)-nn)
        row.append(f"{CS>>10:>4}K {t:>9,} ({100*(t-bound)/bound:+.2f}%)")
    print(f"  {tag:11s} bound {int(bound):>9,} | "+"  ".join(row),flush=True)
for x in ("/tmp/pf.in","/tmp/pf.out"):
    if os.path.exists(x): os.remove(x)
print("ALLDONE_PUSH")
