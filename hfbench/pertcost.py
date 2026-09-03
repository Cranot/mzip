#!/usr/bin/env python3
"""What does a PERTURBED twin cost to store against its sibling? The 7.7% of weight bytes in RESULT 103
cannot enter the headline until this is known.

A fine-tune coded cheaply (RESULT 90) because 90% of its weights were untouched. A perturbed twin has
~0% untouched: every weight moved by a hair (r 0.996-0.9988, identical 0.0%). Whether that hair is one
ulp (a bf16 -> f32 -> bf16 round trip, a noise injection, a re-cast) or a genuine training step decides
whether the delta costs 1 bit or 8. Measured on pairs from the dominant cluster, one full 2D tensor
each: identical share, the integer bf16 delta's entropy, the distribution of |delta| in ulps, and the
cost against storing the tensor alone (Xet at 1.14x for BF16)."""
import json, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map

PAIRS = [("cryptoDev23/albedo-qwen3.6-35b", "isomsom/Affine-5cdqb2dugt-g2"),
         ("cryptoDev23/albedo-qwen3.6-35b", "aljosadro/albedo-qwen3.6-35b"),
         ("vect0r18/mirror-dora7-champi", "HarperJane/20260828-072405")]

def resolve(prefix):
    """the sample stores truncated ids in lineage2.out; resolve a prefix to a full repo id via search"""
    d = api(f"{H}/api/models?search={prefix.split('/')[-1][:24]}&author={prefix.split('/')[0]}&limit=3")
    if isinstance(d, list) and d: return d[0]["id"]
    return prefix

def tensor_slice(rid, want_key=None, nel_cap=6_000_000):
    d = api(f"{H}/api/models/{rid}")
    if not d: return None
    sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    if not sts: return None
    wmap, cache = shard_map(rid, sts)
    keys = [k for k in wmap if k.endswith(".weight") and "layers." in k and "norm" not in k and "embed" not in k]
    keys.sort()
    k = want_key if want_key in wmap else (keys[0] if keys else None)
    if not k: return None
    fn = wmap[k]
    if fn not in cache: cache[fn] = st_header(rid, fn)
    hb, off = cache[fn]
    if not hb or k not in hb or hb[k]["dtype"] != "BF16": return None
    m = hb[k]; nel = int(np.prod(m["shape"])); take = min(nel, nel_cap)
    raw = grab(rid, fn, off + m["data_offsets"][0], take * 2)
    if raw is None: return None
    return k, np.frombuffer(raw.tobytes(), dtype=np.uint16).astype(np.int32)

def main():
    print(f"{'pair':64s} {'tensor':18s} {'ident':>6s} {'|d|=1':>6s} {'|d|<=2':>7s} {'h(delta)':>9s} {'vs raw':>7s} {'vs Xet':>7s}")
    for a, b in PAIRS:
        ra, rb = resolve(a), resolve(b)
        A = tensor_slice(ra)
        if not A: print(f"{ra[:30]} unreadable"); continue
        k, wa = A
        Bt = tensor_slice(rb, want_key=k)
        if not Bt: print(f"{rb[:30]} unreadable / no {k}"); continue
        _, wb = Bt
        n = min(wa.size, wb.size); wa, wb = wa[:n], wb[:n]
        d = wa - wb
        ident = float(np.mean(d == 0))
        ad = np.abs(d)
        one = float(np.mean(ad == 1)); two = float(np.mean(ad <= 2))
        hd = h0(d)
        raw_bits = 16.0
        print(f"{(ra[:30]+' ~ '+rb[:30])[:64]:64s} {k.split('.')[-2][:18]:18s} {100*ident:5.1f}% {100*one:5.1f}% {100*two:6.1f}% "
              f"{hd:9.3f} {100*hd/raw_bits:6.1f}% {100*hd/(raw_bits/1.14):6.1f}%")
    print("\n  |d|=1 = share of weights exactly one bf16 ulp apart. h(delta) in bits/weight; raw bf16 is 16;")
    print("  Xet stores BF16 at ~14.0 bits/weight (1.14x). A twin costs h(delta) against its sibling.")
    print("PERTCOST_DONE")

if __name__ == "__main__":
    main()
