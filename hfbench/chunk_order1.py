#!/usr/bin/env python3
"""Per 64 KiB chunk: how much would an order-1 context model beat the order-0 one the coder uses?

The whole-slice probe said dense weight planes are memoryless but one slice was not. A chunk store
decides per chunk, so the question is what fraction of CHUNKS carry exploitable structure and how
much. For every chunk of every slice: H0 of each plane, H1 (conditioned on the previous byte of the
same plane, bucketed to 8 contexts so a real coder could carry the tables), and the resulting
whole-chunk gain. A context count of 8 is what the delta coder found affordable; 256 contexts are
also reported as the ceiling of the idea."""
import glob, json, os
import numpy as np

P = "/root/mzip-hfbench"
CS = 65536

def H(x):
    if x.size == 0: return 0.0
    h = np.bincount(x, minlength=256).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def H1(x, nb):
    """bits/symbol conditioned on the previous byte bucketed into nb contexts, plus the table cost"""
    if x.size < 2: return H(x)
    prev = np.concatenate(([x[0]], x[:-1])).astype(np.int64)
    ctx = (prev * nb) // 256
    j = np.bincount(ctx * 256 + x.astype(np.int64), minlength=nb * 256).astype(np.float64).reshape(nb, 256)
    tot = j.sum(); pc = j.sum(axis=1); out = 0.0; tables = 0
    for c in np.nonzero(pc)[0]:
        p = j[c][j[c] > 0] / pc[c]
        out += (pc[c] / tot) * float(-(p * np.log2(p)).sum())
        tables += 1
    # a huff0 table costs roughly 40 bytes per used context; charge it in bits/symbol
    return out + (tables * 40.0 * 8.0) / x.size

def main():
    rows = []
    for path in sorted(glob.glob(f"{P}/weights2/*.bin")) + sorted(glob.glob(f"{P}/weights2/*.slice")):
        raw = np.fromfile(path, dtype=np.uint8)
        name = os.path.basename(path)
        k = 2 if any(t in name for t in ("qwen", "tinyllama", "smolvlm", "llama2", "whisper", "sdxl", "bloom")) else 4
        nchunk = len(raw) // CS
        g8 = []; g256 = []
        for c in range(nchunk):
            ch = raw[c * CS:(c + 1) * CS].reshape(-1, k)
            t0 = t8 = t256 = 0.0
            for j in range(k):
                pl = np.ascontiguousarray(ch[:, j])
                h0 = H(pl); t0 += h0
                t8 += min(h0, H1(pl, 8)); t256 += min(h0, H1(pl, 256))
            g8.append(1 - t8 / t0 if t0 else 0); g256.append(1 - t256 / t0 if t0 else 0)
        g8 = np.array(g8); g256 = np.array(g256)
        rows.append((name, k, nchunk, float(g8.mean()), float(g8.max()), float((g8 > 0.02).mean()), float(g256.mean()), float(g256.max())))
        print(f"{name:24s} k={k} chunks={nchunk:4d} | order-1, 8 ctx: mean {100*g8.mean():5.2f}%  max {100*g8.max():5.2f}%  "
              f"chunks>2%: {100*(g8>0.02).mean():5.1f}% | 256 ctx: mean {100*g256.mean():5.2f}%  max {100*g256.max():5.2f}%", flush=True)
    json.dump(rows, open(f"{P}/chunk_order1.json", "w"), indent=1)
    print("CHUNK_ORDER1_DONE")

if __name__ == "__main__":
    main()
