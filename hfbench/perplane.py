#!/usr/bin/env python3
"""The same-speed, better-ratio candidate: bg4 split, then entropy-code EACH plane
as its own zstd frame (own Huffman table), storing a plane raw if coding does not
help. Compare per 64K chunk against HF bg4-lz4, concatenated bg4-zstd-1 (Xet-style
single block), the order-0 entropy bound, and mzip. CPU-time throughput included
(rusage of this process; all arms are in-process here except mzip)."""
import os, json, time, resource, math, numpy as np, lz4.frame, zstandard as zstd
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; CS=65536
INPUTS=[("MiniLM F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2 F32",f"{W}/gpt2.slice"),("bert F32",f"{W}/bert-base.slice"),
 ("bge F32",f"{W}/bge-small.slice"),("wfp32 F32",f"{W}/weights-fp32.bin"),("wbf16 BF16",f"{W}/weights-bf16.bin"),
 ("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice"),("gguf-q8_0",f"{W}/gguf-q8_0.slice")]
mz={r["tag"].split(" ")[0]:r for r in json.load(open(f"{OUT}/soa_measure.json"))}   # native mzip @64K per slice
def planes(d,k):
    a=np.frombuffer(d,dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k)
    return [m[:,j].tobytes() for j in range(k)], a[n:].tobytes()
def H0(b):
    h=np.bincount(np.frombuffer(b,dtype=np.uint8),minlength=256); p=h[h>0]/h.sum(); return float(-(p*np.log2(p)).sum())
Z1=zstd.ZstdCompressor(level=1); Z3=zstd.ZstdCompressor(level=3); ZD=zstd.ZstdDecompressor()
def cpu(): r=resource.getrusage(resource.RUSAGE_SELF); return r.ru_utime+r.ru_stime
def perplane_enc(chunk,k,Z):
    """1 byte per plane flag (0=raw,1=zstd) + payloads. Returns encoded bytes (list) for decode."""
    ps,tail=planes(chunk,k); out=[]
    for p in ps:
        c=Z.compress(p)
        out.append((1,c) if len(c)<len(p) else (0,p))
    return out,tail,len(ps)
def perplane_dec(out,tail,k,n):
    ps=[ZD.decompress(c) if f==1 else c for f,c in out]
    m=np.empty((len(ps[0]),k),dtype=np.uint8)
    for j in range(k): m[:,j]=np.frombuffer(ps[j],dtype=np.uint8)
    return m.tobytes()+tail
def size_of(out,tail): return sum(1+len(c) for _,c in out)+len(tail)+2   # +2: flags/len header approx

print(f"{'input':11s} {'HF bg4-lz4':>10s} {'bg4-z1 cat':>10s} {'PP4-z1':>10s} {'PP4-z3':>10s} {'PP2-z1':>10s} {'H0 bound':>10s} {'mzip':>10s} | PP4-z1 vs HF  vs bound  vs mzip")
rows=[]; T={k:0 for k in ("n","hf","cat","pp4","pp4z3","pp2","h0","mz")}; tc=td=0.0
for tag,p in INPUTS:
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); N=len(raw); ch=[raw[i:i+CS] for i in range(0,N,CS)]
    hf=cat=pp4=pp4z3=pp2=0; h0=0.0
    c0=cpu()
    for c in ch:
        ps4,t4=planes(c,4); hf+=len(lz4.frame.compress(b"".join(ps4)+t4,compression_level=0))
    for c in ch:
        ps4,t4=planes(c,4); cat+=len(Z1.compress(b"".join(ps4)+t4))
    c1=cpu(); encs=[]
    for c in ch:
        o,t,k=perplane_enc(c,4,Z1); pp4+=size_of(o,t); encs.append((o,t,k))
    c2=cpu(); ok=True
    for (o,t,k),c in zip(encs,ch): ok=ok and perplane_dec(o,t,k,len(c))==c
    c3=cpu()
    for c in ch:
        o,t,k=perplane_enc(c,4,Z3); pp4z3+=size_of(o,t)
        o,t,k=perplane_enc(c,2,Z1); pp2+=size_of(o,t)
        ps4,t4=planes(c,4); h0+=sum(H0(x)*len(x)/8 for x in ps4)+len(t4)
    m=mz.get(tag.split(" ")[0],{}).get("soa",0)
    enc_mbps=N/1048576/max(c2-c1,1e-9); dec_mbps=N/1048576/max(c3-c2,1e-9)
    rows.append(dict(tag=tag,n=N,hf=hf,cat=cat,pp4=pp4,pp4z3=pp4z3,pp2=pp2,h0=h0,mzip=m,pp4_enc_mbps=enc_mbps,pp4_dec_mbps=dec_mbps,rt=ok))
    for k,v in (("n",N),("hf",hf),("cat",cat),("pp4",pp4),("pp4z3",pp4z3),("pp2",pp2),("h0",h0),("mz",m)): T[k]+=v
    tc+=c2-c1; td+=c3-c2
    print(f"{tag:11s} {hf:>10,} {cat:>10,} {pp4:>10,} {pp4z3:>10,} {pp2:>10,} {int(h0):>10,} {m:>10,} | {100*(hf-pp4)/hf:+7.2f}%  {100*(h0-pp4)/h0:+6.2f}%  {100*(m-pp4)/m if m else 0:+6.2f}%   enc {enc_mbps:6.0f} dec {dec_mbps:6.0f} MB/s {'ok' if ok else 'RT_FAIL'}",flush=True)
n=T["n"]
print(f"\nTOTAL {n:,} B (9 inputs): HF {T['hf']:,} ({n/T['hf']:.4f}x) | bg4-z1 cat {T['cat']:,} ({n/T['cat']:.4f}x, {100*(T['hf']-T['cat'])/T['hf']:+.2f}%) | PP4-z1 {T['pp4']:,} ({n/T['pp4']:.4f}x, {100*(T['hf']-T['pp4'])/T['hf']:+.2f}%) | PP4-z3 {T['pp4z3']:,} ({100*(T['hf']-T['pp4z3'])/T['hf']:+.2f}%) | PP2-z1 {T['pp2']:,} ({100*(T['hf']-T['pp2'])/T['hf']:+.2f}%) | H0 bound {int(T['h0']):,} ({n/T['h0']:.4f}x) | mzip {T['mz']:,} ({n/T['mz']:.4f}x)")
print(f"PP4-z1 throughput (CPU, in-process, incl. split/regroup): enc {n/1048576/tc:.0f} MB/s  dec {n/1048576/td:.0f} MB/s")
json.dump(rows,open(f"{OUT}/perplane.json","w"),indent=1); print("ALLDONE_PP")
