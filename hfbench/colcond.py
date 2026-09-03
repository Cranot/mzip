#!/usr/bin/env python3
"""Breadth for RESULT 118's column finding: on large models, which tensor kinds have per-column scale?

One tensor on one 7B showed 0.41 bits/weight from conditioning the exponent on the column. Before that
is priced it needs every projection kind on two large models, and the row version alongside, so the
lever's byte reach is a sum over kinds rather than one lucky matrix."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
H = "https://huggingface.co"
KINDS = ("self_attn.q_proj", "self_attn.k_proj", "self_attn.v_proj", "self_attn.o_proj", "mlp.gate_proj", "mlp.up_proj", "mlp.down_proj")

def cond(sym, rows, cols, axis):
    s = sym[:rows * cols].reshape(rows, cols)
    if axis == 1: s = s.T
    return sum(h0(s[i]) for i in range(s.shape[0])) / s.shape[0]

def main():
    print(f"{'model':22s} {'layer.kind':22s} {'rows x cols':>14s} {'h0':>6s} {'h|row':>6s} {'h|col':>6s} {'best gain':>10s}")
    for rid in ("Qwen/Qwen2.5-7B", "meta-llama/Llama-3.1-8B", "mistralai/Mistral-7B-v0.3"):
        d = api(f"{H}/api/models/{rid}")
        if not d: print(f"{rid}: unreadable (gated?)"); continue
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        if not sts: continue
        wmap, cache = shard_map(rid, sts)
        tot_g = 0.0; tot_n = 0
        for layer in (0, 14):
            for kind in KINDS:
                k = f"model.layers.{layer}.{kind}.weight"
                if k not in wmap: continue
                fn = wmap[k]
                if fn not in cache: cache[fn] = st_header(rid, fn)
                hb, off = cache[fn]
                if not hb or k not in hb or hb[k]["dtype"] != "BF16": continue
                m = hb[k]; rows, cols = m["shape"]
                nrow = min(rows, max(128, (6 << 20) // (cols * 2)))
                raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
                if raw is None: continue
                u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
                e = (u >> 7).astype(np.int64)                 # sign + exponent, 9 bits
                h = h0(e); hr = cond(e, nrow, cols, 0); hc = cond(e, nrow, cols, 1)
                g = h - min(hr, hc); tot_g += g * nrow * cols; tot_n += nrow * cols
                print(f"{rid[:22]:22s} {f'{layer}.{kind}'[:22]:22s} {f'{nrow}x{cols}':>14s} {h:6.3f} {hr:6.3f} {hc:6.3f} {g:10.3f}")
        if tot_n: print(f"  {rid[:22]} element-weighted gain {tot_g/tot_n:.3f} b/w = {100*(tot_g/tot_n)/16:.1f}% of the file\n")
    print("COLCOND_DONE")

if __name__ == "__main__":
    main()
