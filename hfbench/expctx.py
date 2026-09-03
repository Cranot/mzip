#!/usr/bin/env python3
"""Does the bf16 exponent plane have context structure that an order-0 coder leaves on the table?

Fano codes each byte plane with an order-0 Huffman table. On BF16 weights the high byte (sign + 7
exponent bits) carries the redundancy; the low byte is near-random and passes through. Order-0 on the
high byte gives ~1.49x. If exponents of neighbouring weights are correlated -- same row, same output
channel, similar magnitude -- a context model conditioned on the previous exponent (or the one directly
above in the matrix) would code the plane below h0. Measured on real BF16 tensors: h0 of the high byte,
H(high | previous high), H(high | high of the same column one row up), in bits per weight of the
original 16."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
H = "https://huggingface.co"

def cond_h(sym, ctx, alpha=256):
    idx = ctx.astype(np.int64) * alpha + sym.astype(np.int64)
    cnt = np.bincount(idx, minlength=alpha * alpha).reshape(alpha, alpha).astype(np.float64)
    rows = cnt.sum(axis=1, keepdims=True)
    with np.errstate(divide="ignore", invalid="ignore"):
        p = np.where(rows > 0, cnt / np.maximum(rows, 1), 0.0)
        lg = np.where(p > 0, np.log2(np.maximum(p, 1e-300)), 0.0)
    return float(-(cnt * lg).sum() / max(cnt.sum(), 1))

def main():
    repos = ["Qwen/Qwen2.5-0.5B", "meta-llama/Llama-3.2-1B", "HuggingFaceTB/SmolLM2-360M"]
    print(f"{'repo':30s} {'tensor':26s} {'h0 hi':>7s} {'h|prev':>7s} {'h|above':>8s} {'best gain':>10s} {'of 16 b/w':>10s}")
    for rid in repos:
        d = api(f"{H}/api/models/{rid}")
        if not d: print(rid, "unreadable"); continue
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        if not sts: continue
        wmap, cache = shard_map(rid, sts)
        keys = [k for k in wmap if k.endswith(".weight") and ("mlp" in k or "ffn" in k) and "layers.0." in k]
        if not keys: continue
        k = keys[0]; fn = wmap[k]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]
        m = hb[k]
        if m["dtype"] != "BF16": continue
        rows, cols = m["shape"]
        nrow = min(rows, max(64, (2 << 20) // (cols * 2)))
        raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
        if raw is None: continue
        u16 = np.frombuffer(raw.tobytes(), dtype=np.uint16).reshape(nrow, cols)
        hi = (u16 >> 8).astype(np.uint8)
        h_0 = h0(hi)
        prev = np.concatenate([[0], hi.ravel()[:-1]]).astype(np.uint8)
        h_prev = cond_h(hi.ravel(), prev)
        above = np.vstack([np.zeros((1, cols), dtype=np.uint8), hi[:-1]])
        h_above = cond_h(hi.ravel(), above.ravel())
        gain = h_0 - min(h_prev, h_above)
        print(f"{rid[:30]:30s} {k.split('.')[-2][:26]:26s} {h_0:7.3f} {h_prev:7.3f} {h_above:8.3f} {gain:10.3f} {100*gain/16:9.2f}%")
    print("\n  gain = bits/weight a context-1 model on the exponent plane would save over order-0 (Fano today).")
    print("  1% of 16 bits = ~3.5 TB/week on the Hub's BF16 uploads; below 0.5% it is not worth a coder change.")
    print("EXPCTX_DONE")

if __name__ == "__main__":
    main()
