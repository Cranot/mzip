#!/usr/bin/env python3
"""The same falsification for RESULT 118's two remaining coder levers: do they survive per-chunk tables?

 (a) Wide-alphabet order-0 on twin deltas. The delta is an int16 symbol; a chunk uses a few hundred
     distinct values. Table costed as (symbol, length) pairs for used symbols: 16 + 4 bits each.
     Compared per 64 KiB chunk against zstd's measured 4.62 / 1.20 (R113) and the h0 bound.
 (b) Scale-plane coding in Q8_0: per chunk (~1,928 blocks), two 256-symbol plane tables against the
     0.211 bits/weight the planes save (R118)."""
import sys, heapq
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api, read_header, fetch
from pertcost import tensor_slice, resolve
from jointcost import huff_lengths
H = "https://huggingface.co"
CHUNK = 65536

def coded(sym_int):
    vals, inv = np.unique(sym_int, return_inverse=True)
    counts = np.bincount(inv, minlength=len(vals))
    L = huff_lengths(counts)
    body = sum(counts[s] * l for s, l in L.items())
    table = len(vals) * (16 + 4)
    return body, table

def main():
    print("=== (a) twin deltas, per 64 KiB chunk, tables included (bits/weight) ===")
    print(f"{'pair':50s} {'h0 bound':>9s} {'coded+table':>12s} {'zstd R113':>10s}")
    for a, b, z in (("cryptoDev23/albedo-qwen3.6-35b", "isomsom/Affine-5cdqb2dugt-g2", 4.62),
                    ("fiveflow/rq_8b_32", "fiveflow/rq_8b_224", 1.20)):
        A = tensor_slice(resolve(a), nel_cap=2_000_000)
        if not A: continue
        k, wa = A; Bt = tensor_slice(resolve(b), want_key=k, nel_cap=2_000_000)
        if not Bt: continue
        _, wb = Bt; n = min(wa.size, wb.size); d = (wa[:n] - wb[:n]).astype(np.int32)
        per = CHUNK // 2; nch = n // per; tot = 0
        for c in range(nch):
            body, table = coded(d[c*per:(c+1)*per]); tot += body + table
        print(f"{(a[:22]+' ~ '+b[:22]):50s} {h0(d):9.3f} {tot/(nch*per):12.3f} {z:10.2f}")
    print("\n=== (b) Q8_0 scale planes, per chunk, tables included ===")
    repo = "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    f = next(x["rfilename"] for x in d["siblings"] if "q8_0" in x["rfilename"].lower())
    hk = read_header(repo, f)
    name, gm = max(((n_, m) for n_, m in hk["tensors"].items() if m["ttype"] == 8 and "ffn" in n_), key=lambda c: int(np.prod(c[1]["dims"])))
    nb = min(30000, int(np.prod(gm["dims"])) // 32)
    a = np.asarray(fetch(repo, f, hk["data_start"] + gm["offset"], nb * 34))[:nb * 34].reshape(nb, 34)
    per = CHUNK // 34; nch = nb // per; stored = coded_t = 0
    for c in range(nch):
        blk = a[c*per:(c+1)*per]
        stored += 16 * len(blk)
        for col in (0, 1):
            body, _ = coded(blk[:, col].astype(np.int32)); coded_t += body + 256 + 4 * len(np.unique(blk[:, col]))
    w = nch * per * 32
    print(f"  {name[:24]:24s} scales stored {stored/w:.3f} b/w   planes coded + tables {coded_t/w:.3f} b/w   saves {(stored-coded_t)/w:.3f} b/w = {100*(stored-coded_t)/w/8.5:.1f}% of the file  (R118 bound: 0.211)")
    print("DELTACOST_DONE")

if __name__ == "__main__":
    main()
