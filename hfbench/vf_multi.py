#!/usr/bin/env python3
"""Adversarial generalization test for the semk10 lever.

Claim under test: split fp32 into sign|exp|mant_hi10|mant_lo13 beats plain mzip.
Claimant said "no second fp32 model was available on the box". weights_multi.py
proves otherwise: it fetches 8MiB slices by HTTP range from 4 HF models.

This fetches the SAME slice convention as weights-fp32.bin (head of the tensor
region, i.e. offset = 8+headerlen, exactly what weights.py used with OFF=11416)
for every F32 model, plus a deep (+4MB) slice, and tests plain vs semk10.
"""
import json, os, struct, subprocess, sys, hashlib

W = "/root/mzip-hfbench/vfy-semk10"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
PY = "/root/mzip-hfbench/venv/bin/python"
X3 = "/root/mzip-hfbench/wz_xform3.py"
os.makedirs(W, exist_ok=True)
SLICE = 8 * 1024 * 1024

MODELS = [
    ("gpt2",       "openai-community/gpt2"),
    ("bert-base",  "google-bert/bert-base-uncased"),
    ("bge-small",  "BAAI/bge-small-en-v1.5"),
    ("distilbert", "distilbert/distilbert-base-uncased"),
    ("roberta",    "FacebookAI/roberta-base"),
]

def rng(url, a, b, out):
    r = subprocess.run(["curl", "-sSL", "--max-time", "600", "-r", f"{a}-{b}", "-o", out, url])
    return r.returncode == 0 and os.path.exists(out) and os.path.getsize(out) == (b - a + 1)

def mzip_size(path, tag):
    o = f"{W}/{tag}.mz"
    if os.path.exists(o): os.remove(o)
    subprocess.run([MZIP, "c", path, o, "19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(o): return 0
    return os.path.getsize(o)

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""): h.update(c)
    return h.hexdigest()

def fp16_frac(path):
    """fraction of fp32 elements whose low 13 mantissa bits are zero"""
    import numpy as np
    w = np.fromfile(path, dtype="<u4")
    return float(((w & 0x1FFF) == 0).mean()), int(w.size)

results = []
for tag, repo in MODELS:
    url = f"https://huggingface.co/{repo}/resolve/main/model.safetensors"
    hp = f"{W}/{tag}.hdr"
    if not rng(url, 0, 200000, hp):
        print(f"{tag}: header fetch FAILED", flush=True); continue
    raw = open(hp, "rb").read()
    n = struct.unpack("<Q", raw[:8])[0]
    if 8 + n > len(raw):
        print(f"{tag}: header {n} > probe", flush=True); os.remove(hp); continue
    hdr = json.loads(raw[8:8 + n])
    import collections
    dt = collections.Counter(v["dtype"] for k, v in hdr.items() if k != "__metadata__")
    dtype = dt.most_common(1)[0][0]
    start = 8 + n
    os.remove(hp)
    if dtype != "F32":
        print(f"{tag}: dtype {dtype}, skipping (fp32 lever)", flush=True); continue

    for lbl, off in (("head", start), ("deep", start + 4_000_000)):
        sp = f"{W}/{tag}.{lbl}.bin"
        if not os.path.exists(sp) or os.path.getsize(sp) != SLICE:
            if not rng(url, off, off + SLICE - 1, sp):
                print(f"{tag}.{lbl}: slice fetch FAILED", flush=True); continue
        frac, nel = fp16_frac(sp)
        base = mzip_size(sp, f"{tag}.{lbl}.base")
        row = {"tag": tag, "slice": lbl, "dtype": dtype, "n": SLICE,
               "fp16_frac": round(frac, 5), "plain": base}
        for k in (8, 10, 12):
            tp = f"{W}/{tag}.{lbl}.k{k}.t"
            subprocess.run([PY, X3, "f", f"semk{k}", sp, tp], check=True)
            assert os.path.getsize(tp) == SLICE, f"{tp} size changed!"
            sz = mzip_size(tp, f"{tag}.{lbl}.k{k}")
            row[f"k{k}"] = sz
            # round trip only for k=10 (the claimed lever)
            if k == 10:
                mz = f"{W}/{tag}.{lbl}.k10.mz"
                rt = f"{W}/{tag}.{lbl}.k10.rt"
                subprocess.run([MZIP, "d", mz, rt], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                orig = f"{W}/{tag}.{lbl}.k10.orig"
                subprocess.run([PY, X3, "i", "semk10", rt, orig, str(SLICE)], check=True)
                row["rt"] = "OK" if sha(orig) == sha(sp) else "FAIL"
                for f in (rt, orig): 
                    if os.path.exists(f): os.remove(f)
            os.remove(tp)
        row["delta_k10_pct"] = round(100.0 * (row["k10"] - base) / base, 4)
        results.append(row)
        print(json.dumps(row), flush=True)
        json.dump(results, open(f"{W}/multi_results.json", "w"), indent=1)

print("MULTI_DONE", flush=True)
