#!/usr/bin/env python3
"""Progressive weights by bit planes: is the layering free against Fano's flat encoding?

RESULT 114 layered by quantiser (Q4_0 -> Q8_0 -> bf16) at 12.7 bits/weight: below Xet's 14.0, above
Fano's 10.7. Fano gets 10.7 by coding the high byte (sign + exponent, 2.7 bits) and passing the mantissa
byte raw. Split the mantissa instead of quantising:

    L1  sign | exponent | mantissa[6:3]   12 bits, entropy-coded   -> a bf16 truncated to 4 mantissa bits
    L2  mantissa[2:0]                       3 bits, raw              -> exactness
    (or 12/4 with an 8-bit exponent byte + 4-bit nibble)

The top 12 bits' entropy should be ~2.7 (exponent) + ~4 (near-uniform mantissa top) = ~6.7; the rest is
raw. If L1 + L2 = 10.7, progressive delivery costs NOTHING against Fano's flat file. Quality of the L1
model: relative error <= 2^-5 (3.1%), between Q4 and Q8. Also the 8/8 split for reference (L1 = exponent
plane only -- powers of two, not a usable model, but the number tells where the bits are)."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
H = "https://huggingface.co"

def main():
    repos = ["Qwen/Qwen2.5-0.5B", "HuggingFaceTB/SmolLM2-360M"]
    print(f"{'model':28s} {'tensor':11s} {'Fano flat':>9s} {'12/4 L1':>8s} {'L2':>4s} {'SUM':>6s} {'13/3 L1':>8s} {'L2':>4s} {'SUM':>6s} {'8/8 L1':>7s} {'SUM':>6s}")
    for rid in repos:
        d = api(f"{H}/api/models/{rid}")
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        wmap, cache = shard_map(rid, sts)
        k = next(k for k in sorted(wmap) if k.endswith(".weight") and "mlp" in k and "layers.0." in k)
        fn = wmap[k]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]; m = hb[k]
        rows, cols = m["shape"]; nrow = min(rows, max(64, (4 << 20) // (cols * 2)))
        raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
        u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        hi = (u >> 8).astype(np.uint16)
        fano = h0(hi) + 8.0
        out = [fano]
        for keep in (12, 13, 8):                       # bits kept in layer 1
            top = (u >> (16 - keep)).astype(np.int64)  # symbol space 2^keep
            L1 = h0(top)
            L2 = float(16 - keep)                      # raw remainder
            out += [L1, L2, L1 + L2]
        print(f"{rid[:28]:28s} {k.split('.')[-2][:11]:11s} {out[0]:9.2f} {out[1]:8.2f} {out[2]:4.0f} {out[3]:6.2f} "
              f"{out[4]:8.2f} {out[5]:4.0f} {out[6]:6.2f} {out[7]:7.2f} {out[9]:6.2f}")
    print("\n  bits per weight. 12/4: L1 is bf16 truncated to 4 mantissa bits (rel err <= 3.1%%, Q4-Q8 class), usable;")
    print("  13/3: 5 mantissa bits (<= 1.6%%). SUM vs 'Fano flat' is the layering cost; zero means progressive is free.")
    print("PROGRESSIVE2_DONE")

if __name__ == "__main__":
    main()
