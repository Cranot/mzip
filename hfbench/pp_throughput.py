#!/usr/bin/env python3
"""Byte-weighted end-to-end throughput of per-plane huff0 on real slices: every
plane benchmarked with the coder's own `fse -h -b -i2`, raw pass-through where it
does not compress. Excludes only the split/regroup memcpy."""
import numpy as np, subprocess, re, os
F="/root/mzip-hfbench/fse/fse"; W="/root/mzip-hfbench/weights"
for tag,p,k in (("MiniLM F32",f"{W}/MiniLM-L6-v2.slice",4),("wbf16 BF16",f"{W}/weights-bf16.bin",2),("gguf-f16",f"{W}/gguf-f16.slice",2)):
    a=np.frombuffer(open(p,"rb").read(),dtype=np.uint8); m=a[:(len(a)//k)*k].reshape(-1,k); N=len(a)
    te=td=0.0; tot=0
    print(f"=== {tag}: {k} planes of {len(m):,} B (huff0) ===")
    for j in range(k):
        pl=m[:,j].tobytes(); open("/tmp/plane.bin","wb").write(pl)
        r=subprocess.run([F,"-h","-b","-i2","/tmp/plane.bin"],capture_output=True,text=True)
        line=[l for l in (r.stdout+r.stderr).splitlines() if "MB/s" in l]
        mm=re.search(r"->\s*(\d+)\s*\(([\d.]+)%\),\s*([\d.]+) MB/s\s*,\s*([\d.]+) MB/s",line[-1]) if line else None
        if not mm: print("   plane",j,"parse fail:",line); continue
        out,enc,dec=int(mm.group(1)),float(mm.group(3)),float(mm.group(4))
        raw= out>=len(pl); out=len(pl) if raw else out
        tot+=out; te+=len(pl)/1048576/enc; td+=len(pl)/1048576/dec
        print(f"   plane {j}: {len(pl):>9,} -> {out:>9,} ({100*out/len(pl):5.1f}%)  enc {enc:7.1f} MB/s  dec {dec:8.1f} MB/s {'(raw pass-through)' if raw else ''}")
    print(f"   FILE: {N:,} -> {tot:,} ({N/tot:.4f}x)  byte-weighted: enc {N/1048576/te:6.0f} MB/s  dec {N/1048576/td:6.0f} MB/s")
os.remove("/tmp/plane.bin")
