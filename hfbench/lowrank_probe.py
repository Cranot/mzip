#!/usr/bin/env python3
"""Is a fine-tune's weight delta LOW RANK, and does that beat coding its bytes?

A LoRA merge is exactly W' = W + B A with B (d x r), A (r x k), so D = W' - W has rank r before the
result is rounded to BF16. Every coder in this programme treats D as a byte stream. This probe asks
whether the generative structure is recoverable: SVD of D, look at the spectrum, and for each rank r
measure how many elements a rank-r reconstruction gets EXACTLY right in BF16.

The slice is a contiguous run of complete rows of the tensor, and any row subset of B A still has
rank <= r, so a slice is enough to see the rank.

Output per pair: shape, the singular-value spectrum's knee, and for r in {1,2,4,8,16,32,64,128}:
the fraction of BF16 elements reproduced exactly, and the byte cost of (factors + exact residual)
estimated by the order-0 entropy of the residual's zigzag planes plus the factors at int8/int16."""
import glob, json, os, struct, sys
import numpy as np

P = "/root/mzip-hfbench/pairs"
BASE_SLICE = f"{P}/qwen25-1.5b.ft.bin"          # the instruct model: base of every community fine-tune
K = 1536                                         # Qwen2.5-1.5B hidden size; the tensor is [8960, 1536]

def bf16_to_f32(u16):
    return (u16.astype(np.uint32) << 16).view(np.float32)

def f32_to_bf16_rne(x):
    """round-to-nearest-even from float32 to bf16 bit patterns, the rule torch and numpy agree on"""
    u = x.astype(np.float32).view(np.uint32)
    lsb = (u >> 16) & 1
    rounded = u + 0x7FFF + lsb
    return (rounded >> 16).astype(np.uint16)

def h0_bytes(a):
    """order-0 entropy of a byte array, in bytes"""
    if a.size == 0: return 0.0
    h = np.bincount(a.ravel(), minlength=256).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum()) * a.size / 8.0

def plane_cost(d16):
    """order-0 cost of the zigzag of a signed 16-bit residual, as two byte planes (the coder's model)"""
    d = d16.astype(np.int64)
    zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(np.uint32)
    lo = (zig & 0xFF).astype(np.uint8); hi = ((zig >> 8) & 0xFF).astype(np.uint8)
    return h0_bytes(lo) + h0_bytes(hi)

def probe(tag, base_u16, ft_u16, ranks):
    n = min(base_u16.size, ft_u16.size) // K * K
    b = bf16_to_f32(base_u16[:n]).reshape(-1, K)
    f = bf16_to_f32(ft_u16[:n]).reshape(-1, K)
    D = (f - b)                                   # exact in float32: bf16 differences are representable
    rows = D.shape[0]
    nz = float(np.mean(D != 0))
    U, S, Vt = np.linalg.svd(D, full_matrices=False)
    tot = float((S ** 2).sum()) or 1.0
    energy = np.cumsum(S ** 2) / tot
    knee = int(np.searchsorted(energy, 0.999) + 1)
    ft_bytes = ft_u16[:n].nbytes
    # what the byte coders achieve today on this delta (order-0 zigzag planes of the 16-bit difference)
    d16 = (ft_u16[:n].astype(np.int64) - base_u16[:n].astype(np.int64)).astype(np.int16)
    today = plane_cost(d16)
    out = dict(tag=tag, rows=rows, k=K, nonzero=nz, knee999=knee, s0=float(S[0]), s_last=float(S[-1]),
               energy_at=[float(energy[min(r, len(energy)) - 1]) for r in ranks], today_bytes=today,
               ft_bytes=ft_bytes, ranks={})
    target = ft_u16[:n].reshape(-1, K)
    for r in ranks:
        if r > len(S): break
        Ur = U[:, :r] * S[:r]; Vr = Vt[:r, :]
        # The factors must themselves be stored, so try them at three precisions. Quantisation is
        # per-column of U and per-row of V (one float32 scale each), and the product is formed in
        # float64 so a decoder reproduces it exactly from the stored integers.
        for prec, qmax, esz in (("f32", 0, 4), ("i16", 32767, 2), ("i8", 127, 1)):
            if prec == "f32":
                R = Ur @ Vr
                fbytes = (Ur.size + Vr.size) * 4
            else:
                su = np.abs(Ur).max(axis=0); su[su == 0] = 1.0
                sv = np.abs(Vr).max(axis=1); sv[sv == 0] = 1.0
                Uq = np.rint(Ur / su * qmax).astype(np.float64)
                Vq = np.rint(Vr / sv[:, None] * qmax).astype(np.float64)
                R = (Uq @ Vq) * (su / qmax)[None, :].T.reshape(1, -1)[0][0] if False else                     ((Uq * (su / qmax)) @ (Vq * (sv[:, None] / qmax)))
                fbytes = (Ur.size + Vr.size) * esz + (len(su) + len(sv)) * 4
            approx16 = f32_to_bf16_rne(b + R)
            exact = float(np.mean(approx16 == target))
            resid = (target.astype(np.int64) - approx16.astype(np.int64)).astype(np.int16)
            rbytes = plane_cost(resid)
            key = f"{r}:{prec}"
            out["ranks"][key] = dict(rank=r, prec=prec, exact=exact, resid_bytes=rbytes, factor_bytes=fbytes,
                                     total=rbytes + fbytes, ratio=ft_bytes / max(rbytes + fbytes, 1))
    return out

def main():
    base = np.frombuffer(open(BASE_SLICE, "rb").read(), dtype=np.uint16)
    ranks = [4, 8, 16, 32, 48, 64, 96, 128, 192, 256]
    files = sorted(glob.glob(f"{P}/community-*.ft.bin"))
    print(f"{'pair':46s} {'rows':>5s} {'nz%':>6s} {'r@99.9%':>8s} {'today':>9s} | best rank: exact%  bytes    ratio")
    results = []
    for fp in files:
        tag = os.path.basename(fp)[len("community-"):-len(".ft.bin")]
        ft = np.frombuffer(open(fp, "rb").read(), dtype=np.uint16)
        try: res = probe(tag, base, ft, ranks)
        except Exception as e: print(f"{tag:46s} FAILED {e}"); continue
        best = min(res["ranks"].items(), key=lambda kv: kv[1]["total"]) if res["ranks"] else None
        results.append(res)
        if best:
            key, v = best; td = res["today_bytes"]
            print(f"{tag[:46]:46s} {res['rows']:5d} {100*res['nonzero']:5.1f}% {res['knee999']:8d} "
                  f"{(res['ft_bytes']/td if td else 0):8.3f}x | {key:<8s} {100*v['exact']:6.2f}%  {v['total']:9.0f}  {v['ratio']:6.3f}x", flush=True)
        json.dump(res, open(f"{P}/lowrank-{tag}.json", "w"), indent=1)
    # the canonical (non-community) pairs too: are full fine-tunes low rank?
    for jf in sorted(glob.glob(f"{P}/*.json")):
        rec = json.load(open(jf))
        if not isinstance(rec, dict) or not rec.get("fetched") or rec.get("dtype") != "BF16": continue
        tag = rec["tag"]
        try:
            b = np.frombuffer(open(f"{P}/{tag}.base.bin", "rb").read(), dtype=np.uint16)
            f = np.frombuffer(open(f"{P}/{tag}.ft.bin", "rb").read(), dtype=np.uint16)
        except FileNotFoundError: continue
        shape = rec.get("shape") or []
        k = shape[-1] if shape else K
        if not k or (min(b.size, f.size) // k) < 64: continue
        globals()["K"] = k
        try: res = probe(tag, b, f, ranks)
        except Exception as e: print(f"{tag:46s} FAILED {e}"); continue
        best = min(res["ranks"].items(), key=lambda kv: kv[1]["total"]) if res["ranks"] else None
        if best:
            key, v = best; td = res["today_bytes"]
            print(f"{tag[:46]:46s} {res['rows']:5d} {100*res['nonzero']:5.1f}% {res['knee999']:8d} "
                  f"{(res['ft_bytes']/td if td else 0):8.3f}x | {key:<8s} {100*v['exact']:6.2f}%  {v['total']:9.0f}  {v['ratio']:6.3f}x", flush=True)
        json.dump(res, open(f"{P}/lowrank-{tag}.json", "w"), indent=1)
    print("LOWRANK_DONE")

if __name__ == "__main__":
    main()
