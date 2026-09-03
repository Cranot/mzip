#!/usr/bin/env python3
"""Falsify RESULT 115: does the 12-bit joint alphabet's 1.5% survive its own table cost?

h0 is a bound that assumes the decoder already knows the symbol distribution. A real coder ships a
table per chunk. Fano's 8-bit plane needs at most 256 code lengths; a 12-bit joint alphabet needs up
to 4,096. If the table costs more than the 0.16 bits/weight the joint alphabet saves, the gain is an
artefact of the bound. Measured on real bf16 chunks: canonical-Huffman code lengths, actual coded size
(sum of lengths, not h0), table cost as (a) one length per used symbol with a used-symbol bitmap and
(b) a per-tensor table amortised over all chunks. Also the 12/4 progressive split under the same rules."""
import sys, heapq
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
H = "https://huggingface.co"
CHUNK = 65536  # bytes, Xet's chunk size

def huff_lengths(counts):
    """canonical Huffman code lengths for symbols with nonzero counts"""
    items = [(c, i) for i, c in enumerate(counts) if c > 0]
    if len(items) == 1: return {items[0][1]: 1}
    heap = [(c, [i]) for c, i in items]; heapq.heapify(heap)
    lengths = {i: 0 for _, i in items}
    while len(heap) > 1:
        c1, s1 = heapq.heappop(heap); c2, s2 = heapq.heappop(heap)
        for s in s1 + s2: lengths[s] += 1
        heapq.heappush(heap, (c1 + c2, s1 + s2))
    return lengths

def coded_bits(sym, alpha):
    counts = np.bincount(sym, minlength=alpha)
    L = huff_lengths(counts)
    body = sum(counts[s] * l for s, l in L.items())
    used = len(L)
    # table: a bitmap of used symbols (alpha bits) + 4 bits per used symbol's length
    table = alpha + 4 * used
    return body, table, used

def main():
    print(f"{'model':24s} {'tensor':11s} {'chunks':>6s} {'8-bit plane':>12s} {'12-bit joint':>13s} {'12-bit, per-tensor table':>24s} {'h0 gain':>8s} {'real gain':>10s}")
    for rid in ("Qwen/Qwen2.5-0.5B", "HuggingFaceTB/SmolLM2-360M", "Qwen/Qwen2.5-7B"):
        d = api(f"{H}/api/models/{rid}")
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        wmap, cache = shard_map(rid, sts)
        k = next(k for k in sorted(wmap) if "layers.0.mlp.down_proj" in k)
        fn = wmap[k]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]; m = hb[k]
        nb = min(4 << 20, m["data_offsets"][1] - m["data_offsets"][0]); nb -= nb % CHUNK
        raw = grab(rid, fn, off + m["data_offsets"][0], nb)
        u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        n = u.size; nchunks = nb // CHUNK; per = CHUNK // 2
        b8 = b12 = t8 = t12 = 0
        for c in range(nchunks):
            w = u[c*per:(c+1)*per]
            body, table, _ = coded_bits((w >> 8).astype(np.int64), 256); b8 += body + 8 * per; t8 += table
            body, table, _ = coded_bits((w >> 4).astype(np.int64), 4096); b12 += body + 4 * per; t12 += table
        # per-tensor table for the joint alphabet: one table for all chunks, body coded with it
        body_t, table_t, used_t = coded_bits((u >> 4).astype(np.int64), 4096); b12t = body_t + 4 * n + table_t
        bpw8 = (b8 + t8) / n; bpw12 = (b12 + t12) / n; bpw12t = b12t / n
        h0_8 = None
        from kquant_given import h0
        h0gain = (h0((u >> 8).astype(np.uint16)) + 8) - (h0((u >> 4).astype(np.int64)) + 4)
        print(f"{rid[:24]:24s} {k.split('.')[-2][:11]:11s} {nchunks:6d} {bpw8:12.3f} {bpw12:13.3f} {bpw12t:24.3f} {h0gain:8.3f} {bpw8-bpw12:10.3f}")
    print("\n  bits/weight including table cost. '8-bit plane' = Fano today (per-chunk 256-symbol table).")
    print("  'real gain' = what a per-chunk 4096-symbol table actually leaves of the h0 gain; the per-tensor")
    print("  column is the gain if one table is shared across all chunks of a tensor.")
    print("JOINTCOST_DONE")

if __name__ == "__main__":
    main()
