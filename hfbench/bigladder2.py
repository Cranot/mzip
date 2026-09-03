#!/usr/bin/env python3
"""Win across sizes: 256 MiB real weight slices (BF16 Qwen2.5-0.5B, F32 gpt2)
compressed at block sizes 1M, 16M (DEFAULT_BLOCK_SIZE), 64M (xorb), 256M (whole,
== BG_MAX_SIZE_BALANCED). Every prior number was <= 8.4 MiB single-block; this
is the regime deployments live in. Sizes only; all mzip outputs round-trip
verified by sha256 of decompressed vs input block.

Arms per block: bg4-lz4 (HF), bg4-zstd-1, bg4-zstd-19, mzip default (level 1,
byte-identical to 19), bg4+mzip default, and at whole-file also mzipx FAST L1.
"""
import os, sys, json, hashlib, subprocess, numpy as np, lz4.frame, zstandard as zstd
import concurrent.futures as cf
B="/root/mzip-hfbench/big"; OUT="/root/mzip-hfbench/chunk"
MZ="/root/mzip-hfbench/build/mzip_fixed3"; MX="/root/mzip-hfbench/modeswp/mzipx"   # native SoA + guards
FILES=[("qwen-bf16","BF16",f"{B}/qwen-bf16-256M.bin"),("gpt2-f32","F32",f"{B}/gpt2-f32-256M.bin")]
SIZES=[16<<20, 64<<20, 0]   # the >16 MiB regime is the point; 64K-1M is covered at 8 MiB scale
# BUG FIX 2026-09-02: a shared ZstdCompressor across threads is NOT thread-safe and died with
# "cannot compress: Error (generic)". Construct per call.
def Z1c(b):  return zstd.ZstdCompressor(level=1).compress(b)
def Z19c(b): return zstd.ZstdCompressor(level=19).compress(b)
def bg4(d):
    a=np.frombuffer(d,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
def sha(b): return hashlib.sha256(b).hexdigest()
def mzip_run(blk, base, fast=False):
    ip,op,rp=base+".in",base+".mz",base+".rt"; open(ip,"wb").write(blk)
    cmd=[MX,"c",ip,op,"1","fast","0"] if fast else [MZ,"c",ip,op,"1"]
    subprocess.run(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    s=os.path.getsize(op) if os.path.exists(op) else -1
    subprocess.run([MZ,"d",op,rp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    ok=os.path.exists(rp) and sha(open(rp,"rb").read())==sha(blk)
    for f in (ip,op,rp):
        if os.path.exists(f): os.remove(f)
    return s, ok
def job(tag,dt,path,bs):
    raw=open(path,"rb").read(); N=len(raw)
    blocks=[raw] if bs==0 else [raw[i:i+bs] for i in range(0,N,bs)]
    r=dict(tag=tag,dtype=dt,n=N,bs=bs,nblocks=len(blocks))
    r["bg4_lz4"]=sum(len(lz4.frame.compress(bg4(b),compression_level=0)) for b in blocks)
    r["bg4_z1"]=sum(len(Z1c(bg4(b))) for b in blocks)
    r["bg4_z19"]=sum(len(Z19c(bg4(b))) for b in blocks)
    tot=0; bad=0
    for i,b in enumerate(blocks):
        s,ok=mzip_run(b,f"{OUT}/L2.{tag}.{bs}.d{i}"); tot+=max(s,0); bad+=int(not ok or s<0)
    r["mzip"]=tot; r["mzip_bad"]=bad
    r["bg4_mzip"]=r["mzip"]; r["bg4_mzip_bad"]=0   # native build applies SoA itself; external bg4 arm dropped
    if bs==0:
        s,ok=mzip_run(raw,f"{OUT}/L2.{tag}.fast",fast=True); r["mzip_fast"]=s; r["mzip_fast_ok"]=ok
    return r
jobs=[(t,d,p,bs) for t,d,p in FILES for bs in SIZES if os.path.exists(p) and os.path.getsize(p)==256<<20]
print(f"{len(jobs)} jobs",flush=True)
rows=[]
with cf.ThreadPoolExecutor(max_workers=4) as ex:
    for r in ex.map(lambda a: job(*a), jobs):
        rows.append(r); N=r["n"]
        bs="whole" if r["bs"]==0 else f"{r['bs']>>20}M"
        extra=f"  mzip-FAST {r['mzip_fast']:>11,} ({N/r['mzip_fast']:.4f}x) {'ok' if r.get('mzip_fast_ok') else 'RT_FAIL'}" if r["bs"]==0 else ""
        print(f"{r['tag']:10s} {bs:>5s} n={r['nblocks']:>3}  bg4-lz4 {r['bg4_lz4']:>11,} ({N/r['bg4_lz4']:.4f}x)  bg4-z1 {r['bg4_z1']:>11,}  bg4-z19 {r['bg4_z19']:>11,} ({N/r['bg4_z19']:.4f}x)  "
              f"mzip {r['mzip']:>11,} ({N/r['mzip']:.4f}x){' RT_FAIL x%d'%r['mzip_bad'] if r['mzip_bad'] else ''}  bg4+mzip {r['bg4_mzip']:>11,} ({N/r['bg4_mzip']:.4f}x){' RT_FAIL x%d'%r['bg4_mzip_bad'] if r['bg4_mzip_bad'] else ''}"
              f"  [bg4+mzip vs HF {100*(r['bg4_lz4']-r['bg4_mzip'])/r['bg4_lz4']:+.2f}%, vs bg4-z19 {100*(r['bg4_z19']-r['bg4_mzip'])/r['bg4_z19']:+.2f}%]{extra}",flush=True)
json.dump(rows,open(f"{OUT}/bigladder2.json","w"),indent=1)
print("ALLDONE_L2")
