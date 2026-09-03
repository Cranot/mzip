#!/usr/bin/env python3
"""Family B coder sweep. Waits for family A's DONE ARTIFACT (not a process name)
before starting, so the two sweeps do not contend for cores."""
import json, os, subprocess, shutil, re, time
from concurrent.futures import ThreadPoolExecutor

B="/root/mzip-hfbench/build"; WB="/root/mzip-hfbench/weightsB"
SCR="/root/mzip-hfbench/cmscratchB"; OUT="/root/mzip-hfbench/cmresultsB.jsonl"

# exit condition written at entry: wait at most 180 x 20s (=60 min) for family A
for _ in range(180):
    if os.path.exists("/root/mzip-hfbench/CMDONE"): break
    time.sleep(20)

shutil.rmtree(SCR, ignore_errors=True); os.makedirs(SCR, exist_ok=True)
meta=json.load(open(f"{WB}/slicesB.json"))
SLICES=[(m["tag"], f"{WB}/{m['tag']}.bin", m["dtype"], m["arch"]) for m in meta]

def run(cmd,**kw): return subprocess.run(cmd,capture_output=True,text=True,**kw)

def job(a):
    tag,path,dtype,arch,coder=a
    d=f"{SCR}/{tag}__{coder}"; os.makedirs(d,exist_ok=True)
    src=f"{d}/f"; shutil.copyfile(path,src); raw=os.path.getsize(src)
    size,ok,note=None,False,""
    try:
        if coder=="mzip":
            run([f"{B}/mzip_fixed","c",src,f"{d}/c","19"])
            if os.path.exists(f"{d}/c"):
                size=os.path.getsize(f"{d}/c"); run([f"{B}/mzip_fixed","d",f"{d}/c",f"{d}/r"])
                ok=os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode==0
        elif coder=="lpaq1":
            run([f"{B}/lpaq_x","9",src,f"{d}/c"])
            if os.path.exists(f"{d}/c"):
                size=os.path.getsize(f"{d}/c"); run([f"{B}/lpaq_x","d",f"{d}/c",f"{d}/r"])
                ok=os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode==0
        elif coder=="zpaq-m5":
            run(["zpaq","a",f"{d}/a.zpaq","f","-m5"],cwd=d)
            if os.path.exists(f"{d}/a.zpaq"):
                size=os.path.getsize(f"{d}/a.zpaq"); os.makedirs(f"{d}/x",exist_ok=True)
                run(["zpaq","x",f"{d}/a.zpaq","f","-to",f"{d}/x/f"],cwd=d)
                ok=os.path.exists(f"{d}/x/f") and run(["cmp","-s",src,f"{d}/x/f"]).returncode==0
        elif coder=="bwtcm":
            r=run([f"{B}/bwtcm",src,"0"])
            m=re.search(r"comp=(\d+) B",r.stdout); f_ok=re.search(r"full_ok=(\d)",r.stdout)
            b_ok=re.search(r"bwt_ok=(\d)",r.stdout)
            if m:
                size=int(m.group(1))
                ok=bool(f_ok and f_ok.group(1)=="1" and b_ok and b_ok.group(1)=="1")
                note="in-process roundtrip (comp incl +8 hdr)"
        elif coder=="xz-9e":
            with open(f"{d}/c","wb") as fo:
                subprocess.run(["xz","-9e","-T1","-c",src],stdout=fo,stderr=subprocess.DEVNULL)
            size=os.path.getsize(f"{d}/c")
            with open(f"{d}/r","wb") as fo:
                subprocess.run(["xz","-d","-c",f"{d}/c"],stdout=fo,stderr=subprocess.DEVNULL)
            ok=run(["cmp","-s",src,f"{d}/r"]).returncode==0
    except Exception as e:
        note=f"EXC {e}"
    rec=dict(tag=tag,dtype=dtype,arch=arch,offkind="dense-matrix",coder=coder,
             raw=raw,size=size,roundtrip=ok,note=note)
    shutil.rmtree(d,ignore_errors=True)
    with open(OUT,"a") as f: f.write(json.dumps(rec)+"\n"); f.flush()
    print(f"{tag:16s} {coder:8s} {str(size):>10s} rt={ok}",flush=True)

CODERS=["xz-9e","bwtcm","lpaq1","zpaq-m5","mzip"]
jobs=[(t,p,d,a,c) for (t,p,d,a) in SLICES for c in CODERS]
print(f"{len(SLICES)} slices x {len(CODERS)} coders = {len(jobs)} jobs",flush=True)
open(OUT,"w").close()
with ThreadPoolExecutor(max_workers=4) as ex: list(ex.map(job,jobs))
open("/root/mzip-hfbench/CMDONE_B","w").write("done\n")
print("ALLDONE_CM_B")
