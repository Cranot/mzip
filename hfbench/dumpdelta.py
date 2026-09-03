#!/usr/bin/env python3
"""Dump twin / fine-tune deltas as byte planes for the FSE harness.

int16 delta -> low byte plane and high byte plane, written raw so a C harness can run FSE_compress per
64 KiB chunk on each. Also writes the entropy bound per plane so the harness output can be read
against it."""
import sys, json
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0
from pertcost import tensor_slice, resolve
OUT = "/root/mzip-hfbench/delta"
import os; os.makedirs(OUT, exist_ok=True)
pairs = {"dense": ("cryptoDev23/albedo-qwen3.6-35b", "isomsom/Affine-5cdqb2dugt-g2"),
         "sparse": ("fiveflow/rq_8b_32", "fiveflow/rq_8b_224")}
meta = {}
for tag, (a, b) in pairs.items():
    A = tensor_slice(resolve(a), nel_cap=2_000_000)
    k, wa = A; _, wb = tensor_slice(resolve(b), want_key=k, nel_cap=2_000_000)
    n = min(wa.size, wb.size); d = (wa[:n] - wb[:n]).astype(np.int16)
    db = d.view(np.uint8).reshape(-1, 2)
    db[:, 0].tofile(f"{OUT}/{tag}_lo.bin"); db[:, 1].tofile(f"{OUT}/{tag}_hi.bin")
    meta[tag] = dict(n=int(n), h0_lo=h0(db[:, 0]), h0_hi=h0(db[:, 1]), h0_int16=h0(d),
                     huff_r122=(4.048 if tag == "dense" else 1.405), zstd_r113=(4.62 if tag == "dense" else 1.20))
    print(f"{tag}: n={n} h0 lo {meta[tag]['h0_lo']:.3f} hi {meta[tag]['h0_hi']:.3f} int16 {meta[tag]['h0_int16']:.3f}")
json.dump(meta, open(f"{OUT}/meta.json", "w"), indent=1)
print("DUMP_DONE")
