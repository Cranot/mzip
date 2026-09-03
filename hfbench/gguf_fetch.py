#!/usr/bin/env python3
"""GGUF is ~29% of served weight bytes on the Hub and is entirely untested.

Fetch 8 MiB slices from the MIDDLE of popular GGUF files (guaranteed to be inside
tensor data, avoiding the variable-length KV header), for the dominant quant
types, then measure whole-file and 64K-chunk sizes for lz4 / bg4-lz4 / mzip.
"""
import json, os, subprocess, numpy as np, lz4.frame

W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
S = 8*1024*1024

def api(u):
    r = subprocess.run(["curl","-sS","--max-time","40",u], capture_output=True, text=True)
    try: return json.loads(r.stdout)
    except Exception: return None

# a popular small GGUF repo with the standard quant ladder
REPOS = ["Qwen/Qwen2.5-0.5B-Instruct-GGUF", "bartowski/Llama-3.2-1B-Instruct-GGUF",
         "unsloth/Qwen3-0.6B-GGUF"]
WANT = ["q4_k_m", "q8_0", "q6_k", "bf16", "f16"]
picked = {}
for repo in REPOS:
    tree = api(f"https://huggingface.co/api/models/{repo}/tree/main?recursive=true&expand=true")
    if not isinstance(tree, list): continue
    for x in tree:
        if x.get("type")!="file" or not x["path"].lower().endswith(".gguf"): continue
        s = (x.get("lfs") or {}).get("size") or x.get("size",0)
        low = x["path"].lower()
        for q in WANT:
            if q in low and q not in picked and s > 3*S:
                picked[q] = (repo, x["path"], s)
    if len(picked) >= 4: break
print("picked:", {k:(v[0].split('/')[-1], v[2]//1_000_000) for k,v in picked.items()}, flush=True)

def rng(url, a, b, out):
    r = subprocess.run(["curl","-sSL","--max-time","300","-r",f"{a}-{b}","-o",out,url])
    return r.returncode==0 and os.path.exists(out) and os.path.getsize(out)==S

POP = np.array([bin(i).count("1") for i in range(256)], dtype=np.uint8)
def xet_rec(data):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//4)*4
    pc=POP[a[:n].reshape(-1,4)]
    H=np.stack([np.bincount(pc[:,k],minlength=9) for k in range(4)]).astype(np.float64)
    base=H.sum(axis=0)+1.0; q=base/base.sum(); mx=0.0
    for i in range(4):
        t=H[i].sum()
        if t==0: continue
        p=H[i]/t; nz=p>0; mx=max(mx,float(np.sum(p[nz]*np.log(p[nz]/q[nz]))))
    return mx>0.02
def bg4(data):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
lz=lambda b: len(lz4.frame.compress(b,compression_level=0))
bgl=lambda b: len(lz4.frame.compress(bg4(b),compression_level=0))
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
for q,(repo,path,size) in picked.items():
    sp=f"{W}/gguf-{q}.slice"
    if not (os.path.exists(sp) and os.path.getsize(sp)==S):
        url=f"https://huggingface.co/{repo}/resolve/main/{path}"
        mid=size//2
        if not rng(url, mid, mid+S-1, sp): print(f"{q}: fetch failed",flush=True); continue
    raw=open(sp,"rb").read(); N=len(raw)
    for csz in (64*1024, 0):
        chunks=[raw] if csz==0 else [raw[i:i+csz] for i in range(0,N,csz)]
        L=B=A=M=0; bad=0; nb=0
        for i,ch in enumerate(chunks):
            L+=lz(ch); b=bgl(ch); B+=b; r=xet_rec(ch); nb+=int(r); A+=b if r else lz(ch)
            s,ok=mz(ch,f"{W}/g.{q}.{csz}.{i}"); M+=max(s,0); bad+=int(not ok or s<0)
        rows.append(dict(quant=q,repo=repo,n=N,chunk=csz,lz4=L,bg4lz4=B,auto=A,bg4_frac=nb/len(chunks),mzip=M,bad=bad))
        cs="whole" if csz==0 else "64K"
        print(f"  gguf {q:7s} {cs:>5s}  lz4 {L:>9,} ({N/L:.3f}x)  bg4 {B:>9,} ({N/B:.3f}x)  auto {A:>9,} (bg4 {100*nb/len(chunks):.0f}%)  "
              f"mzip {M:>9,} ({N/M:.3f}x)  mzip vs auto {100*(A-M)/A:+6.2f}%  {'RT_FAIL' if bad else ''}",flush=True)
json.dump(rows,open(f"/root/mzip-hfbench/chunk/gguf.json","w"),indent=1)
print("ALLDONE_GGUF")
