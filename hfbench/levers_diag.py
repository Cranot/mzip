#!/usr/bin/env python3
"""Other levers, measured as bounds/cheap arms (no heavy compute):
 (a) shared entropy table across chunks: zstd dict trained on ONE model's exponent
     planes, applied to another model's planes (dict carries initial Huffman tables)
 (b) order-1 structure inside the exponent plane: H(X_i | X_{i-1}) vs H0
 (c) cross-plane conditioning: H(mantissa-high plane | exponent plane) vs its H0
 (d) sparsity: fraction of exact-zero elements (run-length lever?)"""
import os, numpy as np, zstandard as zstd
W="/root/mzip-hfbench/weights"; CS=65536
S=[("MiniLM F32",f"{W}/MiniLM-L6-v2.slice",4,3,2),("gpt2 F32",f"{W}/gpt2.slice",4,3,2),("bge F32",f"{W}/bge-small.slice",4,3,2),
   ("wfp32 F32",f"{W}/weights-fp32.bin",4,3,2),("wbf16 BF16",f"{W}/weights-bf16.bin",2,1,0),("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice",2,1,0),
   ("gguf-f16",f"{W}/gguf-f16.slice",2,1,0)]
def H(x):
    h=np.bincount(x,minlength=256).astype(np.float64); p=h[h>0]/h.sum(); return float(-(p*np.log2(p)).sum())
def Hcond(x,y):
    """H(x | y) via joint 256x256 counts."""
    j=np.bincount((y.astype(np.int32)<<8)|x.astype(np.int32),minlength=65536).astype(np.float64).reshape(256,256)
    tot=j.sum(); py=j.sum(axis=1); h=0.0
    for yi in range(256):
        if py[yi]==0: continue
        p=j[yi][j[yi]>0]/py[yi]; h+= (py[yi]/tot)*float(-(p*np.log2(p)).sum())
    return h
print(f"{'input':11s} {'H0 exp':>7s} {'H1 exp|prev':>11s} {'gain':>6s} | {'H0 manthi':>9s} {'H(manthi|exp)':>13s} {'gain':>6s} | {'zero elems':>10s}")
for tag,p,k,ei,mi in S:
    a=np.frombuffer(open(p,"rb").read(),dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k)
    e=m[:,ei]; mh=m[:,mi]
    h0=H(e); h1=Hcond(e[1:],e[:-1]); hm=H(mh); hmc=Hcond(mh,e)
    if k==4: z=float((m.view(np.uint32).ravel()==0).mean()) if False else float((np.frombuffer(m.tobytes(),dtype=np.uint32)==0).mean())
    else: z=float((np.frombuffer(m.tobytes(),dtype=np.uint16)==0).mean())
    print(f"{tag:11s} {h0:>7.3f} {h1:>11.3f} {100*(h0-h1)/h0:>5.1f}% | {hm:>9.3f} {hmc:>13.3f} {100*(hm-hmc)/hm:>5.1f}% | {100*z:>9.2f}%",flush=True)
print("\n(a) shared table via zstd dict: train on MiniLM exponent planes, apply per 16K exponent plane of OTHER F32 models; BF16 train on wbf16 -> apply Qwen")
def exp_planes(p,k,ei):
    raw=open(p,"rb").read(); out=[]
    for i in range(0,len(raw),CS):
        c=np.frombuffer(raw[i:i+CS],dtype=np.uint8); n=(len(c)//k)*k; out.append(c[:n].reshape(-1,k)[:,ei].tobytes())
    return out
for src,dst in ((("MiniLM F32",f"{W}/MiniLM-L6-v2.slice",4,3),("gpt2 F32",f"{W}/gpt2.slice",4,3)),
                (("MiniLM F32",f"{W}/MiniLM-L6-v2.slice",4,3),("bert F32",f"{W}/bert-base.slice",4,3)),
                (("wbf16 BF16",f"{W}/weights-bf16.bin",2,1),("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice",2,1))):
    tr=exp_planes(src[1],src[2],src[3]); te=exp_planes(dst[1],dst[2],dst[3])
    d=zstd.train_dictionary(4096,tr).as_bytes()
    z1=zstd.ZstdCompressor(level=1); zd=zstd.ZstdCompressor(level=1,dict_data=zstd.ZstdCompressionDict(d))
    a=sum(len(z1.compress(x)) for x in te); b=sum(len(zd.compress(x)) for x in te); bound=sum(H(np.frombuffer(x,dtype=np.uint8))*len(x)/8 for x in te)
    print(f"  {src[0]} -> {dst[0]:11s} exponent planes: zstd-1 {a:,}  +shared 4KB dict {b:,} ({100*(a-b)/a:+.2f}%)  H0 bound {int(bound):,}  (dict gap to bound {100*(b-bound)/bound:+.2f}%)")
print("ALLDONE_DIAG")
