#!/usr/bin/env python3
"""Three compression levers the insights point at, each a header-and-slice measurement.

 2. ROW-CONDITIONED EXPONENTS. The exponent plane is memoryless along a row (R107) but rows -- output
    channels -- have different scales. Code each weight's exponent relative to its row's max exponent
    (side info: one byte per row). Gain = h0(exp) - mean_row h0(exp within row). Also the column version,
    and the 12-bit joint alphabet conditioned the same way.
 3. FANO ON DELTAS. R113: zstd on the bf16 delta of a twin pays 1.3x the h0 bound. Fano codes byte
    planes with order-0 Huffman; on the int16 delta that is h0(hi plane) + h0(lo plane). If that sits
    near the bound, the gap closes with code that already exists.
 4. SCALE PLANES. Quantised codes are maximum-entropy (R76), but every block also stores an fp16 scale
    (Q8_0: 16 bits per 32 weights = 0.5 b/w; Q4_K: d, dmin, and 12 bytes of 6-bit sub-scales per 256).
    Scales are log-normal-ish across a tensor. h0 of the scale planes vs their stored width."""
import sys, json
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api, read_header, fetch
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
from pertcost import tensor_slice, resolve
H = "https://huggingface.co"

def row_cond_h(sym, rows, cols):
    s = sym[:rows * cols].reshape(rows, cols)
    acc = 0.0
    for r in range(rows): acc += h0(s[r]) * cols
    return acc / (rows * cols)

def main():
    print("=== 2. row-conditioned exponent coding (bits/weight) ===")
    print(f"{'model':26s} {'tensor':11s} {'h0 exp':>7s} {'h|row':>6s} {'h|col':>6s} {'gain':>6s} {'joint12':>8s} {'joint12|row':>11s} {'gain':>6s}")
    for rid in ("Qwen/Qwen2.5-0.5B", "HuggingFaceTB/SmolLM2-360M", "Qwen/Qwen2.5-7B"):
        d = api(f"{H}/api/models/{rid}")
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        wmap, cache = shard_map(rid, sts)
        k = next(k for k in sorted(wmap) if "layers.0.mlp.down_proj" in k)
        fn = wmap[k]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]; m = hb[k]
        rows, cols = m["shape"]; nrow = min(rows, max(256, (8 << 20) // (cols * 2)))
        raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
        u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        e = ((u >> 7) & 0xFF).astype(np.int64)                   # 8 exponent bits (drop sign)
        sgn = (u >> 15).astype(np.int64)
        h_e = h0(e) + h0(sgn)
        h_row = row_cond_h(e, nrow, cols) + h0(sgn)
        h_col = row_cond_h(e[:nrow * cols].reshape(nrow, cols).T.ravel(), cols, nrow) + h0(sgn)
        top12 = (u >> 4).astype(np.int64)
        j12 = h0(top12); j12_row = row_cond_h(top12, nrow, cols)
        print(f"{rid[:26]:26s} {k.split('.')[-2][:11]:11s} {h_e:7.3f} {h_row:6.3f} {h_col:6.3f} {h_e-min(h_row,h_col):6.3f} {j12:8.3f} {j12_row:11.3f} {j12-j12_row:6.3f}")
    print("  gain in bits/weight; side info is one byte per row (negligible at cols >= 1024). 1%% of 16 = 0.16 b/w.")

    print("\n=== 3. Fano-style plane coding on twin deltas (bits/weight) ===")
    print(f"{'pair':50s} {'h0 bound':>9s} {'planes h0':>10s} {'zstd (R113)':>12s}")
    for a, b, zref in (("cryptoDev23/albedo-qwen3.6-35b", "isomsom/Affine-5cdqb2dugt-g2", 4.62),
                       ("fiveflow/rq_8b_32", "fiveflow/rq_8b_224", 1.20)):
        a2, b2 = resolve(a), resolve(b)
        A = tensor_slice(a2, nel_cap=4_000_000)
        if not A: continue
        k, wa = A; Bt = tensor_slice(b2, want_key=k, nel_cap=4_000_000)
        if not Bt: continue
        _, wb = Bt; n = min(wa.size, wb.size); dlt = (wa[:n] - wb[:n]).astype(np.int16)
        db = dlt.view(np.uint8).reshape(-1, 2)
        print(f"{(a[:22]+' ~ '+b[:22]):50s} {h0(dlt):9.3f} {h0(db[:,0])+h0(db[:,1]):10.3f} {zref:12.2f}")
    print("  planes h0 = what Fano's per-plane order-0 coder would pay on the delta with no new code.")

    print("\n=== 4. scale planes in quantised files (bits per weight of scale overhead) ===")
    repo = "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d["siblings"] if s["rfilename"].lower().endswith(".gguf")]
    for key, tt, bs, unit in (("q8_0", 8, 34, 32), ("q4_k_m", 12, 144, 256)):
        f = next((x for x in files if key in x.lower()), None)
        if not f: continue
        hk = read_header(repo, f)
        cands = [(n, m) for n, m in hk["tensors"].items() if m["ttype"] == tt and "ffn" in n]
        name, gm = max(cands, key=lambda c: int(np.prod(c[1]["dims"])))
        nb = min(20000, int(np.prod(gm["dims"])) // unit)
        rk = fetch(repo, f, hk["data_start"] + gm["offset"], nb * bs)
        a = np.asarray(rk)[:nb * bs].reshape(nb, bs)
        if tt == 8:
            dlo, dhi = a[:, 0], a[:, 1]
            stored = 16 / 32
            coded = (h0(dlo) + h0(dhi)) / 32
            print(f"  Q8_0  {name[:22]:22s} scale d: stored {stored:.3f} b/w, planes coded {coded:.3f} b/w  -> saves {stored-coded:.3f} b/w of {8.5:.1f} ({100*(stored-coded)/8.5:.1f}% of the file)")
        else:
            d_lo, d_hi, m_lo, m_hi = a[:, 0], a[:, 1], a[:, 2], a[:, 3]
            sc = a[:, 4:16]
            stored = (32 + 96) / 256
            coded = (h0(d_lo) + h0(d_hi) + h0(m_lo) + h0(m_hi) + sum(h0(sc[:, i]) for i in range(12))) / 256
            print(f"  Q4_K  {name[:22]:22s} d,dmin,scales: stored {stored:.3f} b/w, planes coded {coded:.3f} b/w -> saves {stored-coded:.3f} b/w of 4.5 ({100*(stored-coded)/4.5:.1f}% of the file)")
    print("LEVERS2_DONE")

if __name__ == "__main__":
    main()
