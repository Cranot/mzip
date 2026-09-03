#!/usr/bin/env python3
"""Per-plane ORDER-0 coding with the reference FSE / huff0 CLIs (no LZ at all),
adaptive stride (best of 2 or 4 per chunk), raw fallback per plane. Compare per
64K chunk to HF bg4-lz4, per-plane zstd-1 (PP4-z1), and the order-0 bound.
Speed via `fse -b` (its own benchmark) on a representative exponent plane and a
mantissa plane -- the coder's native throughput, not Python's."""
import os, json, subprocess, numpy as np
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; CS=65536; FSE="/root/mzip-hfbench/fse/fse"
INPUTS=[("MiniLM F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2 F32",f"{W}/gpt2.slice"),("bert F32",f"{W}/bert-base.slice"),
 ("bge F32",f"{W}/bge-small.slice"),("wfp32 F32",f"{W}/weights-fp32.bin"),("wbf16 BF16",f"{W}/weights-bf16.bin"),
 ("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice"),("gguf-q8_0",f"{W}/gguf-q8_0.slice")]
pp={r["tag"]:r for r in json.load(open(f"{OUT}/perplane.json"))}
def planes(d,k):
    a=np.frombuffer(d,dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k); return [m[:,j].tobytes() for j in range(k)], a[n:].tobytes()
def H0(b):
    h=np.bincount(np.frombuffer(b,dtype=np.uint8),minlength=256); p=h[h>0]/h.sum(); return float(-(p*np.log2(p)).sum())
def fse_size(b, mode):
    """mode '' = FSE, '-h' = huff0. Returns compressed size, or len(b) if it does not help (raw fallback)."""
    open("/tmp/pl.in","wb").write(b)
    r=subprocess.run([FSE]+([mode] if mode else [])+["-f","/tmp/pl.in","/tmp/pl.out"],capture_output=True)
    s=os.path.getsize("/tmp/pl.out") if os.path.exists("/tmp/pl.out") else len(b)
    return min(s,len(b))+1
def encode_chunk(c,mode):
    best=None
    for k in (2,4):
        ps,t=planes(c,k); s=sum(fse_size(p,mode) for p in ps)+len(t)+2
        if best is None or s<best[0]: best=(s,k)
    return best
rows=[]; T={k:0 for k in ("n","hf","pp4","fse","huf","h0")}
print(f"{'input':11s} {'HF bg4-lz4':>10s} {'PP4-z1':>10s} {'PP-FSE':>10s} {'PP-huff0':>10s} {'H0 bound':>10s} | FSE vs HF  FSE vs PP4-z1  FSE vs bound  stride picks")
for tag,p in INPUTS:
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); N=len(raw); ch=[raw[i:i+CS] for i in range(0,N,CS)]
    fse=huf=0; picks={2:0,4:0}; h0=0.0
    for c in ch:
        s,k=encode_chunk(c,""); fse+=s; picks[k]+=1
        huf+=encode_chunk(c,"-h")[0]
        ps,t=planes(c,4); h0+=sum(H0(x)*len(x)/8 for x in ps)+len(t)
    hf=pp[tag]["hf"]; pp4=pp[tag]["pp4"]
    rows.append(dict(tag=tag,n=N,hf=hf,pp4z1=pp4,fse=fse,huff0=huf,h0=h0,picks=picks))
    for k,v in (("n",N),("hf",hf),("pp4",pp4),("fse",fse),("huf",huf),("h0",h0)): T[k]+=v
    print(f"{tag:11s} {hf:>10,} {pp4:>10,} {fse:>10,} {huf:>10,} {int(h0):>10,} | {100*(hf-fse)/hf:+7.2f}%  {100*(pp4-fse)/pp4:+7.2f}%  {100*(h0-fse)/h0:+6.2f}%   s2:{picks[2]} s4:{picks[4]}",flush=True)
n=T["n"]
print(f"\nTOTAL {n:,} B: HF {T['hf']:,} ({n/T['hf']:.4f}x) | PP4-z1 {T['pp4']:,} ({n/T['pp4']:.4f}x, {100*(T['hf']-T['pp4'])/T['hf']:+.2f}%) | PP-FSE adaptive {T['fse']:,} ({n/T['fse']:.4f}x, {100*(T['hf']-T['fse'])/T['hf']:+.2f}%) | PP-huff0 {T['huf']:,} ({100*(T['hf']-T['huf'])/T['hf']:+.2f}%) | H0 bound {int(T['h0']):,} ({n/T['h0']:.4f}x)")
# native coder throughput on representative planes (the coder's own benchmark, not Python)
raw=open(f"{W}/weights-bf16.bin","rb").read()[:4*1024*1024]
ps,_=planes(raw,2); open("/tmp/expo.bin","wb").write(ps[1]); open("/tmp/mant.bin","wb").write(ps[0])
for name,f in (("BF16 exponent plane (2 MiB)","/tmp/expo.bin"),("BF16 mantissa plane (2 MiB)","/tmp/mant.bin")):
    for mode,lab in (("","FSE"),("-h","huff0")):
        r=subprocess.run([FSE]+([mode] if mode else [])+["-b","-i2",f],capture_output=True,text=True)
        line=[l for l in (r.stdout+r.stderr).splitlines() if "MB/s" in l]
        print(f"  {lab:5s} {name}: {line[-1].strip() if line else (r.stdout+r.stderr).strip()[-120:]}")
for x in ("/tmp/pl.in","/tmp/pl.out","/tmp/expo.bin","/tmp/mant.bin"):
    if os.path.exists(x): os.remove(x)
json.dump(rows,open(f"{OUT}/ppfse.json","w"),indent=1); print("ALLDONE_FSE")
