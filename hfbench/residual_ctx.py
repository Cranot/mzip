#!/usr/bin/env python3
"""The low-rank residual is predictable from what the DECODER already knows.

After a rank-r reconstruction the store must code the residual r = published - round_bf16(base + UV),
and that residual is now the dominant cost. But the decoder holds base + UV in full float precision, so
it knows exactly how close each element was to a BF16 rounding boundary. An element whose value sits far
from a tie almost certainly rounds the same way the publisher's arithmetic did; one sitting near a tie is
where the disagreements are. That distance is free context: it costs nothing to transmit.

Measured here: H(residual) as the coder models it today, against H(residual | distance-to-tie bucket),
and against H(residual | distance, magnitude). If the conditional entropy collapses, the low-rank scheme
improves by the same factor on its dominant term."""
import glob, json, os
import numpy as np

P = "/root/mzip-hfbench/pairs"
K = 1536

def bf16_to_f32(u16): return (u16.astype(np.uint32) << 16).view(np.float32)
def f32_to_bf16_rne(x):
    u = x.astype(np.float32).view(np.uint32)
    return ((u + 0x7FFF + ((u >> 16) & 1)) >> 16).astype(np.uint16)

def H(x, nsym=None):
    x = x.ravel()
    if x.size == 0: return 0.0
    h = np.bincount(x, minlength=(nsym or int(x.max()) + 1)).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def Hc(x, ctx):
    x = x.ravel().astype(np.int64); ctx = ctx.ravel().astype(np.int64)
    nx = int(x.max()) + 1; nc = int(ctx.max()) + 1
    j = np.bincount(ctx * nx + x, minlength=nc * nx).astype(np.float64).reshape(nc, nx)
    tot = j.sum(); pc = j.sum(axis=1); out = 0.0
    for c in np.nonzero(pc)[0]:
        p = j[c][j[c] > 0] / pc[c]
        out += (pc[c] / tot) * float(-(p * np.log2(p)).sum())
    return out

def run(tag, base_u16, ft_u16, rank):
    n = min(base_u16.size, ft_u16.size) // K * K
    b = bf16_to_f32(base_u16[:n]).reshape(-1, K)
    f = bf16_to_f32(ft_u16[:n]).reshape(-1, K)
    D = f - b
    U, S, Vt = np.linalg.svd(D, full_matrices=False)
    Ur = U[:, :rank] * S[:rank]; Vr = Vt[:rank, :]
    R = (b + Ur @ Vr).astype(np.float32)                 # what the decoder holds, in full precision
    approx = f32_to_bf16_rne(R)
    target = ft_u16[:n].reshape(-1, K)
    resid = (target.astype(np.int64) - approx.astype(np.int64))
    # zigzag the residual so it is a small non-negative symbol, and clamp the rare large ones
    zz = np.where(resid >= 0, 2 * resid, -2 * resid - 1)
    sym = np.clip(zz, 0, 254).astype(np.int64)
    esc = int((zz > 254).sum())
    # the decoder's free context: how far R was from a bf16 rounding tie
    u = R.view(np.uint32)
    frac = (u & 0xFFFF).astype(np.int64)                 # the 16 bits bf16 throws away
    dist = np.abs(frac - 0x8000)                         # 0 = exactly on the tie, 0x8000 = farthest
    dctx = (np.minimum(dist, 0x7FFF) >> 12).astype(np.int64)          # 8 buckets
    mag = np.clip((np.abs(R.view(np.uint32) >> 23) & 0xFF).astype(np.int64) // 8, 0, 31)   # exponent bucket
    out = dict(tag=tag, rank=rank, n=int(sym.size), exact=float(np.mean(resid == 0)), escapes=esc)
    out["H0"] = H(sym.ravel(), 255)
    out["H|dist"] = Hc(sym, dctx)
    out["H|dist,mag"] = Hc(sym, dctx * 32 + mag)
    out["bytes_H0"] = out["H0"] * sym.size / 8
    out["bytes_Hdist"] = out["H|dist"] * sym.size / 8
    out["bytes_Hdm"] = out["H|dist,mag"] * sym.size / 8
    # how the residual distributes over the distance buckets, to show the mechanism
    out["nonzero_by_dist"] = [float(np.mean(resid.ravel()[dctx.ravel() == c] != 0)) if (dctx == c).any() else 0.0 for c in range(8)]
    return out

def main():
    base = np.frombuffer(open(f"{P}/qwen25-1.5b.ft.bin", "rb").read(), dtype=np.uint16)
    cases = [("Sedibaai__SedibaLM", 32), ("SZLHOLDINGS__SZL-Khipu-1.5B", 64), ("SZLHOLDINGS__SZL-Forge-1.5B-ReceiptAgent", 64)]
    print(f"{'pair':44s} {'rank':>4s} {'exact%':>7s} {'H0':>7s} {'H|dist':>8s} {'H|d,mag':>8s} {'resid KB':>9s} -> {'KB':>8s} {'saved':>7s}")
    res = []
    for tag, r in cases:
        fp = f"{P}/community-{tag}.ft.bin"
        if not os.path.exists(fp): print(f"{tag}: missing"); continue
        ft = np.frombuffer(open(fp, "rb").read(), dtype=np.uint16)
        o = run(tag, base, ft, r); res.append(o)
        print(f"{tag[:44]:44s} {r:4d} {100*o['exact']:6.2f}% {o['H0']:7.4f} {o['H|dist']:8.4f} {o['H|dist,mag']:8.4f} "
              f"{o['bytes_H0']/1024:9.0f} -> {o['bytes_Hdm']/1024:8.0f} {100*(1-o['bytes_Hdm']/o['bytes_H0']):6.1f}%", flush=True)
        print("      residual-nonzero rate by distance-to-tie bucket (0 = on the tie): " +
              " ".join(f"{100*v:5.1f}%" for v in o["nonzero_by_dist"]))
    json.dump(res, open(f"{P}/residual_ctx.json", "w"), indent=1)
    print("RESIDUAL_CTX_DONE")

if __name__ == "__main__":
    main()
