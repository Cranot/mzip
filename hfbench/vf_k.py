#!/usr/bin/env python3
"""Falsify the instrument, then test whether k is inert on an fp16-free slice.

Their headline argument: "k=10 is a UNIQUE optimum, which falsifies the
'any bit split works' explanation."  On gpt2 head, k=8/10/12 gave byte-identical
SIZES.  Either my probe is broken, or the mantissa split point is INERT there and
the whole gain is the sign/exponent separation (identical for every k).
"""
import os, subprocess, hashlib, json
W = "/root/mzip-hfbench/vfy-semk10"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
PY = "/root/mzip-hfbench/venv/bin/python"
X3 = "/root/mzip-hfbench/wz_xform3.py"
SRC = f"{W}/gpt2.head.bin"

def sha(p):
    h = hashlib.sha256()
    with open(p,"rb") as f:
        for c in iter(lambda: f.read(1<<20), b""): h.update(c)
    return h.hexdigest()

def mz(path, tag):
    o = f"{W}/kt.{tag}.mz"
    if os.path.exists(o): os.remove(o)
    subprocess.run([MZIP,"c",path,o,"19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return os.path.getsize(o) if os.path.exists(o) else 0

print("== INSTRUMENT FALSIFICATION ==", flush=True)
# (a) do the transformed inputs actually differ?
shas = {}
for k in (8,10,12):
    tp = f"{W}/kt.k{k}.t"
    subprocess.run([PY,X3,"f",f"semk{k}",SRC,tp], check=True)
    shas[k] = sha(tp)
    print(f"  input k={k}: size {os.path.getsize(tp)} sha {shas[k][:16]}", flush=True)
print(f"  inputs distinct: {len(set(shas.values()))==3}", flush=True)

# (b) is mzip deterministic AND sensitive?  compress k=10 twice, and a 1-byte mutant
a = mz(f"{W}/kt.k10.t", "det1"); b = mz(f"{W}/kt.k10.t", "det2")
print(f"  determinism: {a} vs {b} -> {'SAME' if a==b else 'DIFFERENT'}", flush=True)
d = open(f"{W}/kt.k10.t","rb").read()
mut = bytearray(d); mut[len(mut)//2] ^= 0xFF
open(f"{W}/kt.mut.t","wb").write(bytes(mut))
m = mz(f"{W}/kt.mut.t","mut")
print(f"  sensitivity: 1-byte flip {a} -> {m} (delta {m-a})", flush=True)
# stronger sensitivity control: zero out 1MB
mut2 = bytearray(d); mut2[0:1<<20] = b"\x00"*(1<<20)
open(f"{W}/kt.mut2.t","wb").write(bytes(mut2))
m2 = mz(f"{W}/kt.mut2.t","mut2")
print(f"  sensitivity: 1MB zeroed {a} -> {m2} (delta {m2-a})", flush=True)

print("\n== FULL k SWEEP on gpt2 head (fp16_frac 0.013%) ==", flush=True)
base = mz(SRC, "base")
print(f"  plain (no split)      {base}", flush=True)
rows = {"plain": base}
for k in (0,1,2,4,6,8,10,11,12,14,16,20,23):
    tp = f"{W}/kt.s{k}.t"
    subprocess.run([PY,X3,"f",f"semk{k}",SRC,tp], check=True)
    s = mz(tp, f"s{k}")
    rows[f"k{k}"] = s
    print(f"  k={k:<3d} {s:>10,}  {100*(s-base)/base:+7.4f}%", flush=True)
    os.remove(tp)
json.dump(rows, open(f"{W}/ksweep_gpt2.json","w"), indent=1)
print("KSWEEP_DONE", flush=True)
