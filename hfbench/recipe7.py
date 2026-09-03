#!/usr/bin/env python3
"""Compare a llama.cpp-made GGUF with one made by another tool that kept HuggingFace tensor names.

spelunk-cloud's Q8_0 has tensors named model.layers.N.mlp.up_proj.weight -- it was not written by
convert_hf_to_gguf. Q8_0 is a closed form (d = amax/127 per 32, q = round(x/d)), so if the other tool
implements the same block format the bytes should still agree tensor for tensor once names are
mapped. That tests whether the recipe survives a change of TOOL, not just of publisher.

    python3 recipe7.py <ours.gguf> <theirs.gguf>"""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from recipe import read_header_file
from namemap import candidates
from recipe3 import BS

def main():
    a, b = sys.argv[1], sys.argv[2]
    HA, HB = read_header_file(a), read_header_file(b)
    ta, tb = HA["tensors"], HB["tensors"]
    matched = same = tot_b = same_b = 0; bt = bsame = 0; shape_mis = type_mis = 0; unmapped = 0
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for g, m in ta.items():
            hf = next((c for c in candidates(g) if c in tb), None)
            if not hf: unmapped += 1; continue
            t = tb[hf]
            if m["dims"] != t["dims"] and sorted(m["dims"]) != sorted(t["dims"]): shape_mis += 1; continue
            if m["ttype"] != t["ttype"]: type_mis += 1; continue
            nb = m["nbytes"]
            if not nb: continue
            fa.seek(HA["data_start"] + m["offset"]); fb.seek(HB["data_start"] + t["offset"])
            xa = np.frombuffer(fa.read(nb), dtype=np.uint8); xb = np.frombuffer(fb.read(nb), dtype=np.uint8)
            if xa.size != xb.size: shape_mis += 1; continue
            matched += 1; eq = xa == xb; tot_b += nb; same_b += int(eq.sum()); same += int(eq.all())
            bs = BS.get(m["ttype"])
            if bs and nb % bs == 0:
                blk = ~(~eq).reshape(-1, bs).any(axis=1); bt += blk.size; bsame += int(blk.sum())
    print(f"ours {len(ta)} tensors, theirs {len(tb)}; name-mapped {matched}, unmapped {unmapped}, "
          f"type mismatch {type_mis}, shape mismatch {shape_mis}")
    print(f"byte-identical tensors {same}/{matched}; bytes identical {100*same_b/max(tot_b,1):.3f}%")
    if bt: print(f"Q8_0 blocks identical {100*bsame/bt:.2f}% of {bt:,}")
    print("RECIPE7_DONE")

if __name__ == "__main__":
    main()
