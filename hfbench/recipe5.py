#!/usr/bin/env python3
"""Which tensors differ between the regenerated and the published GGUF, and by how much?

recipe4 reported 315 of 334 tensors identical with every quantised superblock matching. The 19 that
differ are too small to move the byte total. This names them, with type, element count and the
fraction of differing bytes, so 'not exact' is explained rather than declared.

    python3 recipe5.py <ours.gguf> <published.gguf>"""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from recipe import read_header_file
TN = {0: "F32", 1: "F16", 2: "Q4_0", 3: "Q4_1", 6: "Q5_0", 7: "Q5_1", 8: "Q8_0", 9: "Q8_1", 10: "Q2_K",
      11: "Q3_K", 12: "Q4_K", 13: "Q5_K", 14: "Q6_K", 15: "Q8_K", 30: "BF16"}

def main():
    a, b = sys.argv[1], sys.argv[2]
    HA, HB = read_header_file(a), read_header_file(b)
    ta, tb = HA["tensors"], HB["tensors"]
    only_a = [k for k in ta if k not in tb]; only_b = [k for k in tb if k not in ta]
    if only_a or only_b: print(f"only in ours: {only_a[:5]}   only in published: {only_b[:5]}")
    rows = []
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for k in ta:
            if k not in tb: continue
            A, Bm = ta[k], tb[k]
            if A["ttype"] != Bm["ttype"] or A["dims"] != Bm["dims"]:
                rows.append((k, f"TYPE {TN.get(A['ttype'],A['ttype'])} vs {TN.get(Bm['ttype'],Bm['ttype'])}", A["dims"], 1.0, None)); continue
            nb = A["nbytes"]
            if nb == 0: continue
            fa.seek(HA["data_start"] + A["offset"]); fb.seek(HB["data_start"] + Bm["offset"])
            xa = np.frombuffer(fa.read(nb), dtype=np.uint8); xb = np.frombuffer(fb.read(nb), dtype=np.uint8)
            if xa.size != xb.size or (xa == xb).all(): continue
            frac = float(np.mean(xa != xb))
            detail = None
            if A["ttype"] == 0:                              # F32: how large are the value differences?
                va = xa.view(np.float32); vb = xb.view(np.float32)
                d = np.abs(va - vb); rel = d / np.maximum(np.abs(vb), 1e-30)
                detail = f"max|d|={d.max():.3e} max rel={rel.max():.3e} n_diff={int((va != vb).sum())}/{va.size}"
            rows.append((k, TN.get(A["ttype"], str(A["ttype"])), A["dims"], frac, detail))
    print(f"differing tensors: {len(rows)} of {len(ta)}")
    for k, tt, dims, frac, det in sorted(rows, key=lambda r: -r[3])[:30]:
        print(f"  {k[:44]:44s} {tt:6s} {str(dims):>16s} {100*frac:6.2f}% bytes differ  {det or ''}")
    print("RECIPE5_DONE")

if __name__ == "__main__":
    main()
