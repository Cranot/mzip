#!/usr/bin/env python3
"""Compare two GGUF files tensor by tensor. `cmp` is the wrong instrument for GGUF.

Two files with identical tensor data but different KV metadata have every tensor at a different
offset, and a byte-wise cmp reports the whole file as different -- 2.3 GB "differing" for a 1,216-byte
header delta. This walks both headers, pairs tensors by name, and compares each tensor's bytes where
they actually are. For quantised tensors it also reports the fraction of identical superblocks, which
is the number a recipe-plus-patch store would actually pay for.

    python3 recipe3.py A.gguf B.gguf [label]"""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from recipe import read_header_file

BS = {2: 18, 3: 20, 6: 22, 7: 24, 8: 34, 9: 36, 10: 84, 11: 110, 12: 144, 13: 176, 14: 210, 15: 292,
      16: 66, 17: 74, 18: 56, 19: 50, 20: 26, 21: 60, 22: 18, 23: 34}

def main():
    a, b = sys.argv[1], sys.argv[2]
    label = sys.argv[3] if len(sys.argv) > 3 else f"{a.split('/')[-1]} vs {b.split('/')[-1]}"
    HA, HB = read_header_file(a), read_header_file(b)
    ta, tb = HA["tensors"], HB["tensors"]
    common = [n for n in ta if n in tb]
    print(f"=== {label} ===")
    print(f"tensors: {len(ta)} / {len(tb)}, common {len(common)}, header delta {HB['data_start'] - HA['data_start']:+d} B")
    tot = same_b = 0; blk_tot = blk_same = 0; ident_tensors = 0; typemis = 0
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for n in common:
            xa_m, xb_m = ta[n], tb[n]
            if xa_m["ttype"] != xb_m["ttype"] or xa_m["dims"] != xb_m["dims"]:
                typemis += 1; continue
            nb = xa_m["nbytes"]
            if nb == 0: continue
            fa.seek(HA["data_start"] + xa_m["offset"]); fb.seek(HB["data_start"] + xb_m["offset"])
            xa = np.frombuffer(fa.read(nb), dtype=np.uint8); xb = np.frombuffer(fb.read(nb), dtype=np.uint8)
            if xa.size != xb.size: typemis += 1; continue
            tot += nb
            eq = xa == xb
            same_b += int(eq.sum())
            if eq.all(): ident_tensors += 1
            bs = BS.get(xa_m["ttype"])
            if bs and nb % bs == 0:
                blk = ~(~eq).reshape(-1, bs).any(axis=1)
                blk_tot += blk.size; blk_same += int(blk.sum())
    print(f"identical tensors: {ident_tensors}/{len(common) - typemis}   type/shape mismatches: {typemis}")
    print(f"bytes identical: {100*same_b/max(tot,1):.3f}%   ({tot - same_b:,} of {tot:,} differ)")
    if blk_tot:
        print(f"quantised superblocks identical: {100*blk_same/blk_tot:.2f}%   ({blk_tot - blk_same:,} of {blk_tot:,} differ)")
    print("RECIPE3_DONE")

if __name__ == "__main__":
    main()
