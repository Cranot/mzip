#!/usr/bin/env python3
"""Measure 5 coders on every model-weight slice. SIZES ONLY, every size
round-trip verified. Writes JSONL incrementally so partials are readable."""
import json, os, subprocess, shutil, sys, hashlib, re
from concurrent.futures import ThreadPoolExecutor

B   = "/root/mzip-hfbench/build"
W2  = "/root/mzip-hfbench/weights2"
W1  = "/root/mzip-hfbench/weights"
SCR = "/root/mzip-hfbench/cmscratch"
OUT = "/root/mzip-hfbench/cmresults.jsonl"
shutil.rmtree(SCR, ignore_errors=True); os.makedirs(SCR, exist_ok=True)

meta = json.load(open(f"{W2}/slices.json"))
SLICES = [(m["tag"], f"{W2}/{m['tag']}.bin", m["dtype"], m["arch"], "mid-network") for m in meta]
# legacy slices already on disk (their own offsets, kept for continuity with prior baselines)
SLICES += [
  ("minilm-legacy",   f"{W1}/weights-fp32.bin",      "F32",  "encoder", "legacy-head"),
  ("smollm2-legacy",  f"{W1}/weights-bf16.bin",      "BF16", "llm-dec", "legacy-head"),
  ("qwen0.5b-legacy", f"{W1}/Qwen2.5-0.5B.slice",    "BF16", "llm-dec", "legacy+4MB"),
]

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)

def job(args):
    tag, path, dtype, arch, offkind, coder = args
    d = f"{SCR}/{tag}__{coder}"
    os.makedirs(d, exist_ok=True)
    src = f"{d}/f"
    shutil.copyfile(path, src)
    raw = os.path.getsize(src)
    size, ok, note = None, False, ""
    try:
        if coder == "mzip":
            run([f"{B}/mzip_fixed","c",src,f"{d}/c","19"])
            if os.path.exists(f"{d}/c"):
                size = os.path.getsize(f"{d}/c")
                run([f"{B}/mzip_fixed","d",f"{d}/c",f"{d}/r"])
                ok = os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode == 0
        elif coder == "lpaq1":
            run([f"{B}/lpaq_x","9",src,f"{d}/c"])
            if os.path.exists(f"{d}/c"):
                size = os.path.getsize(f"{d}/c")
                run([f"{B}/lpaq_x","d",f"{d}/c",f"{d}/r"])
                ok = os.path.exists(f"{d}/r") and run(["cmp","-s",src,f"{d}/r"]).returncode == 0
        elif coder == "zpaq-m5":
            run(["zpaq","a",f"{d}/a.zpaq","f","-m5"], cwd=d)
            if os.path.exists(f"{d}/a.zpaq"):
                size = os.path.getsize(f"{d}/a.zpaq")
                os.makedirs(f"{d}/x", exist_ok=True)
                run(["zpaq","x",f"{d}/a.zpaq","f","-to",f"{d}/x/f"], cwd=d)
                ok = os.path.exists(f"{d}/x/f") and run(["cmp","-s",src,f"{d}/x/f"]).returncode == 0
        elif coder == "bwtcm":
            r = run([f"{B}/bwtcm",src,"0"])
            m = re.search(r"comp=(\d+) B", r.stdout)
            f_ok = re.search(r"full_ok=(\d)", r.stdout)
            b_ok = re.search(r"bwt_ok=(\d)", r.stdout)
            if m:
                size = int(m.group(1))
                ok = bool(f_ok and f_ok.group(1) == "1" and b_ok and b_ok.group(1) == "1")
                note = "in-process roundtrip (comp incl +8 hdr)"
        elif coder == "xz-9e":
            with open(f"{d}/c","wb") as fo:
                subprocess.run(["xz","-9e","-T1","-c",src], stdout=fo, stderr=subprocess.DEVNULL)
            size = os.path.getsize(f"{d}/c")
            with open(f"{d}/r","wb") as fo:
                subprocess.run(["xz","-d","-c",f"{d}/c"], stdout=fo, stderr=subprocess.DEVNULL)
            ok = run(["cmp","-s",src,f"{d}/r"]).returncode == 0
    except Exception as e:
        note = f"EXC {e}"
    rec = dict(tag=tag, dtype=dtype, arch=arch, offkind=offkind, coder=coder,
               raw=raw, size=size, roundtrip=ok, note=note)
    shutil.rmtree(d, ignore_errors=True)
    with open(OUT,"a") as f:
        f.write(json.dumps(rec)+"\n"); f.flush()
    print(f"{tag:16s} {coder:8s} {str(size):>10s} rt={ok}", flush=True)
    return rec

CODERS = ["xz-9e","bwtcm","lpaq1","zpaq-m5","mzip"]
jobs = [(t,p,d,a,o,c) for (t,p,d,a,o) in SLICES for c in CODERS]
print(f"{len(SLICES)} slices x {len(CODERS)} coders = {len(jobs)} jobs", flush=True)
open(OUT,"w").close()
with ThreadPoolExecutor(max_workers=4) as ex:
    list(ex.map(job, jobs))
open("/root/mzip-hfbench/CMDONE","w").write("done\n")
print("ALLDONE_CM")
