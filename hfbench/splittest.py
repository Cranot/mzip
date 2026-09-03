#!/usr/bin/env python3
"""Byte-plane split + CM. Separates the structured sign/exponent stream from the
near-random mantissa stream, then codes each. Round-trip verified end to end:
every plane is decompressed, re-interleaved, and sha256-compared to the original."""
import hashlib, json, os, re, shutil, subprocess, time
from concurrent.futures import ThreadPoolExecutor

B="/root/mzip-hfbench/build"; WB="/root/mzip-hfbench/weightsB"
SCR="/root/mzip-hfbench/spscratch"; OUT="/root/mzip-hfbench/splitresults.jsonl"

for _ in range(240):
    if os.path.exists("/root/mzip-hfbench/CMDONE_B"): break
    time.sleep(20)

shutil.rmtree(SCR,ignore_errors=True); os.makedirs(SCR,exist_ok=True)
meta=json.load(open(f"{WB}/slicesB.json"))

def run(c,**k): return subprocess.run(c,capture_output=True,text=True,**k)

def comp(coder,src,d,i):
    o=f"{d}/p{i}.c"; r=f"{d}/p{i}.r"
    if coder=="lpaq1":
        run([f"{B}/lpaq_x","9",src,o])
        if not os.path.exists(o): return None,None
        run([f"{B}/lpaq_x","d",o,r])
    elif coder=="mzip":
        run([f"{B}/mzip_fixed","c",src,o,"19"])
        if not os.path.exists(o): return None,None
        run([f"{B}/mzip_fixed","d",o,r])
    elif coder=="zpaq-m5":
        run(["zpaq","a",f"{d}/p{i}.zpaq",os.path.basename(src),"-m5"],cwd=d)
        o=f"{d}/p{i}.zpaq"
        if not os.path.exists(o): return None,None
        run(["zpaq","x",o,os.path.basename(src),"-to",r],cwd=d)
    if not os.path.exists(r): return os.path.getsize(o),None
    return os.path.getsize(o), open(r,"rb").read()

def job(a):
    tag,dtype,coder=a
    path=f"{WB}/{tag}.bin"
    d=f"{SCR}/{tag}__{coder}"; os.makedirs(d,exist_ok=True)
    raw=open(path,"rb").read()
    w=4 if dtype=="F32" else 2
    planes=[raw[k::w] for k in range(w)]
    total=0; recon=[None]*w; ok=True
    for i,pl in enumerate(planes):
        sp=f"{d}/p{i}"; open(sp,"wb").write(pl)
        sz,back=comp(coder,sp,d,i)
        if sz is None or back is None or back!=pl: ok=False; break
        total+=sz; recon[i]=back
    size=None
    if ok:
        out=bytearray(len(raw))
        for i in range(w): out[i::w]=recon[i]
        ok = hashlib.sha256(bytes(out)).hexdigest()==hashlib.sha256(raw).hexdigest()
        size=total
    rec=dict(tag=tag,dtype=dtype,coder=coder,mode="plane-split",planes=w,
             raw=len(raw),size=size,roundtrip=ok)
    shutil.rmtree(d,ignore_errors=True)
    with open(OUT,"a") as f: f.write(json.dumps(rec)+"\n"); f.flush()
    print(f"{tag:16s} {coder:8s} split {str(size):>10s} rt={ok}",flush=True)

jobs=[(m["tag"],m["dtype"],c) for m in meta for c in ["lpaq1","mzip","zpaq-m5"]]
print(f"{len(jobs)} split jobs",flush=True)
open(OUT,"w").close()
with ThreadPoolExecutor(max_workers=4) as ex: list(ex.map(job,jobs))
open("/root/mzip-hfbench/SPDONE","w").write("done\n")
print("ALLDONE_SPLIT")
