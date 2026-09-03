#!/usr/bin/env python3
"""ADVERSARIAL CONTROL: hold EFFECTIVE PRECISION constant, change only the CONTAINER.

Claim under test: "ordering (lpaq1 vs mzip) is set by effective precision".
Their own bitcheck.py prints 'effective ~10 mantissa bits' for SIX slices:
  wav2vec2-base(F32box), clip-vitL14(F32box)  -> lpaq1 WINS
  bloom-560m, whisper-lv3, sdxl-unet (F16)    -> lpaq1 LOSES
Same effective precision, opposite outcome. So test it by construction:

  widen32: F16 loser -> float32 (13 dead low bits). Effective precision UNCHANGED
           at 10 live mantissa bits; container bit-density 0.625 -> 0.3125.
  narrow16: F32box winner -> float16, ONLY if bit-exact. Effective precision
           UNCHANGED at 10 live bits; density 0.3125 -> 0.625.

If ordering is set by effective precision  -> outcome must NOT flip.
If ordering is set by container bit-density -> outcome flips in both directions.
SIZES ONLY. Every size round-trip verified with cmp.
"""
import json, os, subprocess, shutil, numpy as np
from concurrent.futures import ThreadPoolExecutor

B   = "/root/mzip-hfbench/build"
WB  = "/root/mzip-hfbench/weightsB"
D   = "/root/mzip-hfbench/advprec"
SCR = D + "/scr"
OUT = D + "/results.jsonl"
shutil.rmtree(D, ignore_errors=True)
os.makedirs(SCR, exist_ok=True)

HALF = 4 * 1024 * 1024   # 4 MiB of F16 -> 2Mi elements -> 8 MiB as F32

files = []
notes = []

# ---- widen: F16 losers -> F32 container, same values, same element count ----
for tag in ("bloom-560m", "whisper-lv3", "sdxl-unet"):
    raw = np.fromfile(f"{WB}/{tag}.bin", dtype=np.uint8)[:HALF]
    h = raw.view(np.float16)
    f = h.astype(np.float32)
    # losslessness of the widening (must be exact except NaN payloads)
    back = f.astype(np.float16)
    exact = bool(np.array_equal(back.view(np.uint16), h.view(np.uint16)))
    u = f.view(np.uint32)
    dead13 = float(100.0 * np.mean((u & 0x1fff) == 0))
    p16 = f"{D}/{tag}-nat16.bin";   h.tofile(p16)
    p32 = f"{D}/{tag}-widen32.bin"; f.tofile(p32)
    files.append((f"{tag}-nat16",   p16, "F16 native (control, same elements)"))
    files.append((f"{tag}-widen32", p32, f"F32 container, low13=0 in {dead13:.2f}% of words"))
    notes.append(f"{tag}: widen exact={exact} dead_low13={dead13:.2f}%")

# ---- narrow: F32box winners -> F16 container, only if bit-exact ----
for tag in ("wav2vec2-base", "clip-vitL14"):
    raw = np.fromfile(f"{WB}/{tag}.bin", dtype=np.uint8)
    f = raw.view(np.float32)
    n = f.astype(np.float16)
    back = n.astype(np.float32)
    exact = bool(np.array_equal(back.view(np.uint32), f.view(np.uint32)))
    notes.append(f"{tag}: narrow bit-exact={exact} (nan={int(np.isnan(f).sum())} "
                 f"maxabs={float(np.nanmax(np.abs(f))):.6g} "
                 f"minabs_nonzero={float(np.min(np.abs(f[f!=0]))) if np.any(f!=0) else 0:.6g})")
    if exact:
        p = f"{D}/{tag}-narrow16.bin"; n.tofile(p)
        files.append((f"{tag}-narrow16", p, "F16 container, bit-exact narrowing"))

with open(D + "/notes.txt", "w") as fh:
    fh.write("\n".join(notes) + "\n")
print("\n".join(notes), flush=True)

def run(cmd, **kw): return subprocess.run(cmd, capture_output=True, text=True, **kw)

def job(a):
    name, path, desc, coder = a
    d = f"{SCR}/{name}__{coder}"; os.makedirs(d, exist_ok=True)
    src = f"{d}/f"; shutil.copyfile(path, src); raw = os.path.getsize(src)
    size, ok = None, False
    try:
        if coder == "mzip":
            run([f"{B}/mzip_fixed", "c", src, f"{d}/c", "19"])
            if os.path.exists(f"{d}/c"):
                size = os.path.getsize(f"{d}/c")
                run([f"{B}/mzip_fixed", "d", f"{d}/c", f"{d}/r"])
                ok = os.path.exists(f"{d}/r") and run(["cmp", "-s", src, f"{d}/r"]).returncode == 0
        elif coder == "lpaq1":
            run([f"{B}/lpaq_x", "9", src, f"{d}/c"])
            if os.path.exists(f"{d}/c"):
                size = os.path.getsize(f"{d}/c")
                run([f"{B}/lpaq_x", "d", f"{d}/c", f"{d}/r"])
                ok = os.path.exists(f"{d}/r") and run(["cmp", "-s", src, f"{d}/r"]).returncode == 0
        elif coder == "zpaq-m5":
            run(["zpaq", "a", f"{d}/a.zpaq", "f", "-m5"], cwd=d)
            if os.path.exists(f"{d}/a.zpaq"):
                size = os.path.getsize(f"{d}/a.zpaq")
                os.makedirs(f"{d}/x", exist_ok=True)
                run(["zpaq", "x", f"{d}/a.zpaq", "f", "-to", f"{d}/x/f"], cwd=d)
                ok = os.path.exists(f"{d}/x/f") and run(["cmp", "-s", src, f"{d}/x/f"]).returncode == 0
    except Exception as e:
        print("EXC", name, coder, e, flush=True)
    shutil.rmtree(d, ignore_errors=True)
    rec = dict(name=name, desc=desc, coder=coder, raw=raw, size=size, roundtrip=ok)
    with open(OUT, "a") as fh:
        fh.write(json.dumps(rec) + "\n"); fh.flush()
    print(f"{name:26s} {coder:8s} {str(size):>10s} rt={ok}", flush=True)

open(OUT, "w").close()
jobs = [(n, p, d, c) for (n, p, d) in files for c in ("lpaq1", "mzip", "zpaq-m5")]
print(f"{len(files)} files x 3 coders = {len(jobs)} jobs", flush=True)
with ThreadPoolExecutor(max_workers=4) as ex:
    list(ex.map(job, jobs))
open(D + "/DONE", "w").write("done\n")
print("ADVPREC_ALLDONE")
