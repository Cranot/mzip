#!/usr/bin/env python3
"""Remap int16 deltas to a <=255-symbol alphabet for FSE: frequency-ranked values, one escape symbol.

Byte planes destroy a sparse delta's structure (h0_lo + h0_hi = 1.17 against a joint bound of 0.855).
FSE's alphabet is at most 255 symbols; a delta uses a few hundred distinct values with a steep
frequency ranking, so the 254 most frequent get a symbol each and everything else escapes to a raw
int16 side stream. Emits the symbol plane and reports the escape rate and the raw bits it costs."""
import sys, json
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0
OUT = "/root/mzip-hfbench/delta"
meta = json.load(open(f"{OUT}/meta.json"))
for tag in ("dense", "sparse"):
    lo = np.fromfile(f"{OUT}/{tag}_lo.bin", dtype=np.uint8); hi = np.fromfile(f"{OUT}/{tag}_hi.bin", dtype=np.uint8)
    d = (hi.astype(np.int32) << 8 | lo.astype(np.int32)).astype(np.int16).astype(np.int32)
    vals, counts = np.unique(d, return_counts=True)
    order = np.argsort(-counts); keep = vals[order[:254]]
    lut = {int(v): i for i, v in enumerate(keep)}
    sym = np.array([lut.get(int(v), 254) for v in d], dtype=np.uint8)
    esc = float(np.mean(sym == 254))
    sym.tofile(f"{OUT}/{tag}_sym.bin")
    meta[tag]["distinct"] = int(len(vals)); meta[tag]["escape_rate"] = esc
    meta[tag]["escape_bits_per_weight"] = 16 * esc
    print(f"{tag}: distinct values {len(vals)}, escape rate {100*esc:.3f}% (+{16*esc:.3f} b/w raw), h0(sym) {h0(sym):.3f}, joint bound {meta[tag]['h0_int16']:.3f}")
json.dump(meta, open(f"{OUT}/meta.json", "w"), indent=1)
print("DUMP2_DONE")
