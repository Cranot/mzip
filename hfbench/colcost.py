#!/usr/bin/env python3
"""Cost RESULT 119's column-conditioned exponent lever with real per-chunk tables, like R121/R122.

The claim: on large models' layer-0 tensors, coding the exponent conditioned on the column saves up to
0.5 bits/weight (0.41 on Qwen-7B down_proj, 1.21 on Mistral q_proj). h0 conditional entropy ignores
two costs a coder pays: one table per column-context (or a context-mixing scheme), and the fact that a
64 KiB chunk holds only ~32K weights spread over thousands of columns -- a per-column table has almost
no data per context. The honest per-chunk design is: K column-scale classes (side info: one byte per
column giving its class), one Huffman table per class per chunk. Measured for K in {1, 4, 16}."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
from jointcost import huff_lengths
H = "https://huggingface.co"; CHUNK = 65536

def coded(sym, alpha):
    counts = np.bincount(sym, minlength=alpha); L = huff_lengths(counts)
    return sum(counts[s] * l for s, l in L.items()), alpha + 4 * len(L)

def main():
    print(f"{'model':22s} {'tensor':18s} {'K=1 (Fano)':>11s} {'K=4':>8s} {'K=16':>8s} {'h0 bound gain':>14s} {'real gain':>10s}")
    for rid, kind in (("Qwen/Qwen2.5-7B", "model.layers.0.mlp.down_proj.weight"), ("Qwen/Qwen2.5-7B", "model.layers.0.mlp.gate_proj.weight"),
                      ("mistralai/Mistral-7B-v0.3", "model.layers.0.self_attn.q_proj.weight"), ("mistralai/Mistral-7B-v0.3", "model.layers.14.self_attn.q_proj.weight")):
        d = api(f"{H}/api/models/{rid}")
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        wmap, cache = shard_map(rid, sts)
        if kind not in wmap: print(f"  {rid} {kind}: absent"); continue
        fn = wmap[kind]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]; m = hb[kind]; rows, cols = m["shape"]
        nrow = min(rows, max(64, (4 << 20) // (cols * 2))); nrow -= nrow % max(1, (CHUNK // 2) // cols) or 0
        raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
        u = np.frombuffer(raw.tobytes(), dtype=np.uint16).reshape(nrow, cols)
        e = (u >> 7).astype(np.int64)                              # sign+exponent, 9 bits -> alphabet 512
        # column classes from the column's mean exponent (computed once per tensor; side info 1 byte/col)
        colmean = e.mean(axis=0)
        per = (CHUNK // 2) // cols                                   # rows per 64 KiB chunk
        res = {}
        for K in (1, 4, 16):
            cls = np.clip(((colmean - colmean.min()) / max(np.ptp(colmean), 1e-9) * K).astype(int), 0, K - 1)
            tot = 0
            for c0 in range(0, nrow - per + 1, per):
                blk = e[c0:c0 + per]
                for k in range(K):
                    sel = blk[:, cls == k].ravel()
                    if sel.size == 0: continue
                    body, table = coded(sel, 512); tot += body + table
            side = 8 * cols if K > 1 else 0                          # one byte per column, once per tensor
            res[K] = (tot + side) / ((nrow // per) * per * cols)
        # h0 bound gain for reference: h0(e) - mean over columns of h0(e[:,j])
        from kquant_given import h0
        hb0 = h0(e.ravel()); hcol = sum(h0(e[:, j]) for j in range(0, cols, max(1, cols // 512))) / len(range(0, cols, max(1, cols // 512)))
        print(f"{rid[:22]:22s} {kind.split('.')[-2][:18]:18s} {res[1]:11.3f} {res[4]:8.3f} {res[16]:8.3f} {hb0-hcol:14.3f} {res[1]-min(res[4],res[16]):10.3f}")
    print("\n  bits/weight for the exponent plane only (the mantissa byte is raw in every case). 'real gain' = best of K=4/16 vs K=1 with tables.")
    print("COLCOST_DONE")

if __name__ == "__main__":
    main()
