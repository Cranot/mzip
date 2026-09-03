#!/usr/bin/env python3
"""How much structure is left in the plane we code order-0?

The coder splits weights into byte planes and codes each with one static Huffman table, i.e. it
assumes the plane is memoryless. Real weight matrices are not: magnitudes carry a per-row (output
channel) and per-column (input channel) scale, and neighbouring weights correlate. This measures the
conditional entropy of the high byte (sign+exponent) under models a coder could actually implement,
against the order-0 entropy it uses today. Every number is bits per byte over the real slice; the
gap is the headroom on the LARGEST class in the table.

Models measured, all of which a decoder can evaluate from what it has already decoded:
  H0                 order-0, what the coder does now
  H | prev           the previous element's high byte (order-1 along the row)
  H | prev2          the two previous high bytes, bucketed
  H | row            which row (output channel) the element is in, bucketed by row-mean exponent
  H | col            which column (input channel), bucketed by column-mean exponent
  H | row+prev       both
  H | above          the element one row up in the same column (vertical neighbour)
Also: the same for the low (mantissa) byte conditioned on the high byte of the same element, which
is the one cross-plane dependency the format currently ignores."""
import glob, json, os, sys
import numpy as np

P = "/root/mzip-hfbench"

def H(x, nsym=256):
    h = np.bincount(x.ravel(), minlength=nsym).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def Hc(x, ctx):
    """H(x | ctx) in bits/symbol, both uint arrays of the same length"""
    nc = int(ctx.max()) + 1
    j = np.bincount(ctx.astype(np.int64) * 256 + x.astype(np.int64), minlength=nc * 256).astype(np.float64).reshape(nc, 256)
    tot = j.sum(); pc = j.sum(axis=1)
    out = 0.0
    for c in np.nonzero(pc)[0]:
        p = j[c][j[c] > 0] / pc[c]
        out += (pc[c] / tot) * float(-(p * np.log2(p)).sum())
    return out

def bucket(v, nb):
    """rank-bucket a float vector into nb buckets"""
    order = np.argsort(v, kind="stable")
    b = np.empty(len(v), dtype=np.int64)
    b[order] = (np.arange(len(v)) * nb) // len(v)
    return b

def probe(path, width, cols):
    raw = np.fromfile(path, dtype=np.uint8)
    n = (len(raw) // (width * cols)) * width * cols
    a = raw[:n].reshape(-1, width)
    hi = a[:, width - 1].copy()          # little-endian: last byte is sign+exponent
    lo = a[:, 0].copy()
    rows = len(hi) // cols
    hi2 = hi[: rows * cols].reshape(rows, cols)
    res = {}
    res["H0_hi"] = H(hi)
    res["H0_lo"] = H(lo)
    prev = np.concatenate(([hi[0]], hi[:-1]))
    res["hi|prev"] = Hc(hi, prev.astype(np.int64))
    prev2 = np.concatenate(([hi[0], hi[0]], hi[:-2]))
    res["hi|prev,prev2"] = Hc(hi, (prev.astype(np.int64) // 8) * 32 + prev2.astype(np.int64) // 8)
    # row / column scale models: bucket rows and columns by their mean exponent
    rmean = hi2.mean(axis=1); cmean = hi2.mean(axis=0)
    rb = bucket(rmean, min(16, rows)); cb = bucket(cmean, min(16, cols))
    res["hi|row"] = Hc(hi2.ravel(), np.repeat(rb, cols))
    res["hi|col"] = Hc(hi2.ravel(), np.tile(cb, rows))
    res["hi|row,col"] = Hc(hi2.ravel(), np.repeat(rb, cols) * 16 + np.tile(cb, rows))
    pv = np.concatenate(([hi2.ravel()[0]], hi2.ravel()[:-1]))
    res["hi|row,prev"] = Hc(hi2.ravel(), np.repeat(rb, cols) * 32 + (pv.astype(np.int64) // 8))
    above = np.vstack((hi2[0:1, :], hi2[:-1, :])).ravel()
    res["hi|above"] = Hc(hi2.ravel(), above.astype(np.int64))
    res["lo|hi"] = Hc(lo, hi.astype(np.int64))
    res["cols"] = cols; res["rows"] = rows; res["width"] = width
    return res

def main():
    # (path, element width, matrix columns) -- columns from the tensor shape recorded when fetched
    TARGETS = []
    for tag, w, c in [("weights2/qwen2.5-1.5b.bin", 2, 1536), ("weights2/tinyllama-1.1b.bin", 2, 2048),
                      ("weights2/smolvlm.bin", 2, 1152), ("weights2/gpt2.bin", 4, 768),
                      ("weights2/sd15-unet.bin", 4, 320), ("weights2/vit-base.bin", 4, 768),
                      ("weights2/llama2-7b.bin", 2, 4096), ("weights2/whisper-lv3.bin", 2, 1280),
                      ("weights2/sdxl-unet.bin", 2, 640), ("weights2/bloom-560m.bin", 2, 1024)]:
        p = f"{P}/{tag}"
        if os.path.exists(p): TARGETS.append((p, w, c))
    keys = ["H0_hi", "hi|prev", "hi|prev,prev2", "hi|row", "hi|col", "hi|row,col", "hi|row,prev", "hi|above", "H0_lo", "lo|hi"]
    print(f"{'slice':22s} {'w':>2s} " + " ".join(f"{k:>13s}" for k in keys))
    allres = {}
    for path, w, c in TARGETS:
        r = probe(path, w, c)
        allres[os.path.basename(path)] = r
        print(f"{os.path.basename(path):22s} {w:2d} " + " ".join(f"{r[k]:13.4f}" for k in keys), flush=True)
    # the headline: best implementable model vs what we code today, per slice
    print()
    print(f"{'slice':22s} {'H0_hi':>8s} {'best':>8s} {'model':>14s} {'hi gain':>8s} {'whole-plane-pair gain':>22s}")
    for name, r in allres.items():
        cands = {k: r[k] for k in ("hi|prev", "hi|prev,prev2", "hi|row", "hi|col", "hi|row,col", "hi|row,prev", "hi|above")}
        bk = min(cands, key=cands.get)
        gain_hi = 1 - cands[bk] / r["H0_hi"] if r["H0_hi"] else 0
        # a 16-bit element costs H0_hi + H0_lo today; with the best hi model and lo|hi it costs less
        today = r["H0_hi"] + r["H0_lo"]; best = cands[bk] + min(r["H0_lo"], r["lo|hi"])
        print(f"{name:22s} {r['H0_hi']:8.4f} {cands[bk]:8.4f} {bk:>14s} {100*gain_hi:7.1f}% {100*(1-best/today):21.1f}%")
    json.dump(allres, open(f"{P}/structure_probe.json", "w"), indent=1)
    print("STRUCTURE_DONE")

if __name__ == "__main__":
    main()
