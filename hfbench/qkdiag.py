#!/usr/bin/env python3
"""OLMoE's Q/K residual: are the differing superblocks contiguous rows or scattered?

blk.14.attn_q.weight differs in 3.3% of bytes, same type, both sides Q4_K; experts are exact. If the
differing blocks form whole rows, the converter treated some rows differently (a norm fold, a head
permutation on a subset); if they are scattered one block at a time, it is the scale search landing
differently on marginal blocks -- version drift in the quantiser. The pattern names the cause."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from recipe import read_header_file

def main():
    a, b = sys.argv[1], sys.argv[2]
    HA, HB = read_header_file(a), read_header_file(b)
    for name in ("blk.14.attn_q.weight", "blk.14.attn_k.weight", "blk.11.attn_q.weight", "token_embd.weight"):
        ta, tb = HA["tensors"].get(name), HB["tensors"].get(name)
        if not ta or not tb or ta["ttype"] != tb["ttype"]: print(name, "n/a"); continue
        bs = 144; nb = ta["nbytes"]; cols = ta["dims"][0]; per_row = cols // 256
        with open(a, "rb") as fa, open(b, "rb") as fb:
            fa.seek(HA["data_start"] + ta["offset"]); fb.seek(HB["data_start"] + tb["offset"])
            xa = np.frombuffer(fa.read(nb), dtype=np.uint8).reshape(-1, bs)
            xb = np.frombuffer(fb.read(nb), dtype=np.uint8).reshape(-1, bs)
        diff = (xa != xb).any(axis=1)
        rows = diff.reshape(-1, per_row)
        row_any = rows.any(axis=1); row_all = rows.all(axis=1)
        # within a differing block, which bytes? 0-3 d/dmin, 4-15 scales, 16+ codes
        d = (xa != xb)[diff]
        off = d.any(axis=0)
        print(f"{name}: blocks {diff.size:,}, differing {int(diff.sum()):,} ({100*diff.mean():.2f}%)")
        print(f"   rows {rows.shape[0]:,}: rows with any diff {int(row_any.sum()):,}, rows entirely differing {int(row_all.sum()):,}")
        runs = np.diff(np.flatnonzero(diff))
        print(f"   median gap between differing blocks {int(np.median(runs)) if runs.size else 0} blocks (1 = contiguous)")
        print(f"   byte regions touched: d/dmin {bool(off[:4].any())}, scales {bool(off[4:16].any())}, codes {bool(off[16:].any())}")
        if d.size:
            sc_only = (d[:, :16].any(axis=1) & ~d[:, 16:].any(axis=1)).mean()
            print(f"   blocks where ONLY the scale region differs: {100*sc_only:.1f}%")
    print("QKDIAG_DONE")

if __name__ == "__main__":
    main()
