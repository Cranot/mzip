#!/usr/bin/env python3
"""De-confound: my widen/narrow arms changed CONTAINER and FILE SIZE together
(element count preserved -> byte count doubled/halved). Close it by filling the
missing cell of a 2x2: {4 MiB, 8 MiB} x {F16 container, F32box container}.

Already measured:
  8 MiB F16     -> lpaq1 LOSES  (bloom -0.86, sdxl -0.96, whisper -6.04)
  8 MiB F32box  -> lpaq1 WINS   (wav2vec2 +0.34, clip +0.30, bloom-widen +0.33, sdxl-widen +0.28)
  4 MiB F16     -> lpaq1 LOSES  (bloom-nat16 -1.02, sdxl-nat16 -1.11, wav2vec2-narrow16 -1.17, clip-narrow16 -1.02)
  4 MiB F32box  -> MISSING  <-- this script

Prediction written before running: lpaq1 WINS on both 4 MiB F32box files.
If so, file size is ruled out and the container is the driver in both directions.
SIZES ONLY, every size cmp-verified.
"""
import json, os, subprocess, shutil, numpy as np
from concurrent.futures import ThreadPoolExecutor

B="/root/mzip-hfbench/build"; WB="/root/mzip-hfbench/weightsB"
D="/root/mzip-hfbench/advprec2"; SCR=D+"/scr"; OUT=D+"/results.jsonl"
shutil.rmtree(D, ignore_errors=True); os.makedirs(SCR, exist_ok=True)
HALF=4*1024*1024

files=[]
for tag in ("wav2vec2-base","clip-vitL14"):
    raw=np.fromfile(f"{WB}/{tag}.bin",dtype=np.uint8)[:HALF]
    u=raw.view(np.uint32)
    dead13=float(100.0*np.mean((u&0x1fff)==0))
    p=f"{D}/{tag}-half32.bin"; raw.tofile(p)
    files.append((f"{tag}-half32",p,f"4 MiB F32box, low13=0 in {dead13:.2f}% of words"))
    print(f"{tag}: 4MiB F32box slice, dead_low13={dead13:.2f}%", flush=True)

def run(c,**k): return subprocess.run(c,capture_output=True,text=True,**k)

def job(a):
    name,path,desc,coder=a
    d=f"{SCR}/{name}__{coder}"; os.makedirs(d,exist_ok=True)
    src=f"{d}/f"; shutil.copyfile(path,src); raw=os.path.getsize(src)
    size,ok=None,False
    if coder=="mzip":
        run([f"{B}/mzip_fixed","c",src,f"{d}/c","19"])
        if os.path.exists(f"{d}/c"):
            size=os.path.getsize(f"{d}/c"); run([f"{B}/mzip_fixed","d",f"{d}/c",f"{d}/r"])
            ok=os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode==0
    else:
        run([f"{B}/lpaq_x","9",src,f"{d}/c"])
        if os.path.exists(f"{d}/c"):
            size=os.path.getsize(f"{d}/c"); run([f"{B}/lpaq_x","d",f"{d}/c",f"{d}/r"])
            ok=os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode==0
    shutil.rmtree(d,ignore_errors=True)
    with open(OUT,"a") as fh: fh.write(json.dumps(dict(name=name,desc=desc,coder=coder,raw=raw,size=size,roundtrip=ok))+"\n"); fh.flush()
    print(f"{name:24s} {coder:6s} {str(size):>10s} rt={ok}", flush=True)

open(OUT,"w").close()
jobs=[(n,p,d,c) for (n,p,d) in files for c in ("lpaq1","mzip")]
with ThreadPoolExecutor(max_workers=4) as ex: list(ex.map(job,jobs))
open(D+"/DONE","w").write("done\n")
print("ADVPREC2_ALLDONE")
