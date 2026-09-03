#!/usr/bin/env python3
"""Attack axis: is the win an artifact of the untrained-fp16 token rows?
Ablate them out and re-measure. Also independently reproduce the fp32-enc slice.
"""
import os, subprocess, numpy as np, json, hashlib
W = "/root/mzip-hfbench/vfy-semk10"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
PY = "/root/mzip-hfbench/venv/bin/python"
X3 = "/root/mzip-hfbench/wz_xform3.py"
os.makedirs(W, exist_ok=True)

def mz(path, tag):
    o = f"{W}/{tag}.mz"
    if os.path.exists(o): os.remove(o)
    subprocess.run([MZIP, "c", path, o, "19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    s = os.path.getsize(o) if os.path.exists(o) else 0
    return s

def trial(src, tag, ks=(10,)):
    n = os.path.getsize(src)
    base = mz(src, tag + ".base")
    out = {"tag": tag, "n": n, "plain": base}
    w = np.fromfile(src, dtype="<u4")
    out["fp16_frac"] = round(float(((w & 0x1FFF) == 0).mean()), 5)
    for k in ks:
        tp = f"{W}/{tag}.k{k}.t"
        subprocess.run([PY, X3, "f", f"semk{k}", src, tp], check=True)
        assert os.path.getsize(tp) == n
        out[f"k{k}"] = mz(tp, f"{tag}.k{k}")
        out[f"d{k}_pct"] = round(100.0*(out[f"k{k}"]-base)/base, 4)
        os.remove(tp)
    print(json.dumps(out), flush=True)
    return out

res = []
# 1. independent reproduction of the fp32-enc slice claim
enc = "/root/mzip-hfbench/wz/fp32-enc.bin"
if os.path.exists(enc):
    res.append(trial(enc, "fp32enc", ks=(6, 8, 10, 12, 14)))

# 2. ablation: MiniLM fp32 slice with the fp16-signature region removed.
src = "/root/mzip-hfbench/weights/weights-fp32.bin"
w = np.fromfile(src, dtype="<u4")
mask = (w & 0x1FFF) == 0
print(f"total elements {w.size}, fp16-signature {int(mask.sum())} "
      f"({100*mask.mean():.2f}%)", flush=True)
# where are they? first / last index
idx = np.nonzero(mask)[0]
print(f"fp16 elems index range {idx.min()}..{idx.max()}; "
      f"contiguous-prefix count {int(np.argmax(~mask)) if (~mask).any() else w.size}", flush=True)
# BERT hidden=384 for MiniLM: rows of 384 floats. rows 0..1007 -> elems 0..386,  
row = 384
rowmask = mask.reshape(-1, row).all(axis=1)
print(f"rows fully fp16: {int(rowmask.sum())} of {rowmask.size}", flush=True)
first_bad = int(np.argmax(~rowmask)) if (~rowmask).any() else rowmask.size
print(f"first non-fp16 row: {first_bad}", flush=True)

# tail = everything after the fp16-row block, truncated to a 4-byte multiple
cut = first_bad * row
tail = w[cut:]
tp = f"{W}/minilm.nofp16.bin"
tail.tofile(tp)
print(f"tail bytes {os.path.getsize(tp)}", flush=True)
res.append(trial(tp, "minilm_nofp16", ks=(6, 8, 10, 12, 14)))

# 3. control: the same-length tail region compared honestly (already above),
#    plus the fp16 block alone
head = w[:cut]
hp = f"{W}/minilm.fp16only.bin"
head.tofile(hp)
res.append(trial(hp, "minilm_fp16only", ks=(10,)))

json.dump(res, open(f"{W}/abl_results.json", "w"), indent=1)
print("ABL_DONE", flush=True)
