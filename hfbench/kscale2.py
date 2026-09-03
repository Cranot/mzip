#!/usr/bin/env python3
"""Run ggml's actual scale search on the reconstructed weights, not just its starting point.

RESULT 78 predicted a K-quant's 6-bit sub-scales from the range of each sub-block -- which is where
ggml's search BEGINS -- and got 27-35% exact. The search does not stop there: make_qkx2_quants tries
21 candidate scales around the initial one, and for each solves a weighted least-squares fit for the
best (scale, min) pair, keeping whichever minimises the weighted error. Every input to that search is
recoverable from Q8_0 except the imatrix weights, so running the search itself should close most of
the remaining gap.

Implemented here vectorised over sub-blocks, following ggml's make_qkx2_quants for Q4_K
(nmax=15, rmin=-1, rdelta=0.1, nstep=20, squared error). The per-element weight differs between
llama.cpp's code paths, so three candidates are tried and the one that predicts best identifies which
path produced the file -- a fit used as a diagnostic, not a fudge."""
import json, os, sys
import numpy as np
B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, get_scale_min_k4, h0, api, H
NSB = 20000

def search(x, w):
    """ggml make_qkx2_quants over the last axis. x,w: [m,32] -> (scale[m], the_min[m])"""
    mn = np.minimum(x.min(axis=1), 0.0)
    mx = x.max(axis=1)
    rng = mx - mn
    ok = rng > 0
    sum_w = w.sum(axis=1)
    sum_x = (w * x).sum(axis=1)
    best_scale = np.where(ok, rng / 15.0, 0.0)
    best_min = mn.copy()
    # the initial candidate's error
    with np.errstate(divide="ignore", invalid="ignore"):
        iscale0 = np.where(ok, 15.0 / rng, 0.0)
    L0 = np.clip(np.rint(iscale0[:, None] * (x - mn[:, None])), 0, 15)
    diff0 = best_scale[:, None] * L0 + mn[:, None] - x
    best_err = (w * diff0 * diff0).sum(axis=1)
    for is_ in range(21):
        with np.errstate(divide="ignore", invalid="ignore"):
            iscale = np.where(ok, (-1.0 + 0.1 * is_ + 15.0) / rng, 0.0)
        L = np.clip(np.rint(iscale[:, None] * (x - mn[:, None])), 0, 15)
        sum_l = (w * L).sum(axis=1)
        sum_l2 = (w * L * L).sum(axis=1)
        sum_xl = (w * L * x).sum(axis=1)
        D = sum_w * sum_l2 - sum_l * sum_l
        good = D > 0
        with np.errstate(divide="ignore", invalid="ignore"):
            this_scale = np.where(good, (sum_w * sum_xl - sum_x * sum_l) / D, 0.0)
            this_min = np.where(good, (sum_l2 * sum_x - sum_l * sum_xl) / D, 0.0)
            pos = this_min > 0
            this_scale = np.where(pos & good, np.where(sum_l2 > 0, sum_xl / np.where(sum_l2 > 0, sum_l2, 1), 0.0), this_scale)
            this_min = np.where(pos, 0.0, this_min)
        d = this_scale[:, None] * L + this_min[:, None] - x
        err = (w * d * d).sum(axis=1)
        take = good & (err < best_err)
        best_err = np.where(take, err, best_err)
        best_scale = np.where(take, this_scale, best_scale)
        best_min = np.where(take, this_min, best_min)
    return best_scale, -best_min

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "Qwen/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8 = [f for f in files if "q8_0" in f.lower()][0]
    q4k = [f for f in files if "q4_k_m" in f.lower()][0]
    h8 = read_header(repo, q8); hk = read_header(repo, q4k)
    common = [n for n in h8["tensors"] if n in hk["tensors"]
              and h8["tensors"][n]["ttype"] == 8 and hk["tensors"][n]["ttype"] == 12]
    common.sort(key=lambda n: -int(np.prod(h8["tensors"][n]["dims"])))
    name = common[0]; t8, tk = h8["tensors"][name], hk["tensors"][name]
    nsb = min(NSB, int(np.prod(t8["dims"])) // 256)
    print(f"{repo}\n  {name}, {nsb:,} super-blocks")
    r8 = fetch(repo, q8, h8["data_start"] + t8["offset"], nsb*8*34)
    rk = fetch(repo, q4k, hk["data_start"] + tk["offset"], nsb*144)
    x = dec_q8(r8)[:nsb*256].reshape(nsb, 256)
    a = rk[:len(rk)//144*144].reshape(-1, 144)
    n = min(len(x), len(a)); x = x[:n]; a = a[:n]
    d_sb = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    dmin_sb = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
    sc_true, mn_true = get_scale_min_k4(a[:, 4:16])
    sub = x.reshape(n * 8, 32).astype(np.float64)

    print(f"\n  {'per-element weight':22s} {'sub-scale exact':>16s} {'sub-min exact':>14s} {'scale bits':>11s}")
    best = None
    for label, wf in (("uniform", lambda s: np.ones_like(s)),
                      ("|x|", lambda s: np.abs(s)),
                      ("sqrt(sigma2+x^2)", lambda s: np.sqrt((s*s).mean(axis=1, keepdims=True) + s*s))):
        w = wf(sub)
        scale, the_min = search(sub, w)
        scale = scale.reshape(n, 8); the_min = the_min.reshape(n, 8)
        with np.errstate(divide="ignore", invalid="ignore"):
            sc_p = np.clip(np.rint(np.where(d_sb[:, None] > 0, scale / d_sb[:, None], 0)), 0, 63)
            mn_p = np.clip(np.rint(np.where(dmin_sb[:, None] > 0, the_min / dmin_sb[:, None], 0)), 0, 63)
        r_sc = (sc_true - sc_p).astype(np.int64).ravel()
        r_mn = (mn_true - mn_p).astype(np.int64).ravel()
        bits = (h0(np.clip(r_sc, -32, 32)) + h0(np.clip(r_mn, -32, 32))) * 8 / 256 + 32.0/256
        print(f"  {label:22s} {100*float(np.mean(r_sc==0)):15.2f}% {100*float(np.mean(r_mn==0)):13.2f}% {bits:11.4f}")
        if best is None or bits < best[1]: best = (label, bits, float(np.mean(r_sc==0)), float(np.mean(r_mn==0)))
    codes, alone, raw = 0.0864, 4.3621, 0.4400
    lab, bits, se, me = best
    print(f"\n  best fit: {lab}  ({100*se:.2f}% / {100*me:.2f}% exact)")
    print(f"  scale machinery {raw:.4f} -> {bits:.4f} bits/weight")
    print(f"  Q4_K_M on Q8_0: {codes+raw:.4f} -> {codes+bits:.4f} bits/weight")
    print(f"  => {100*(codes+raw)/alone:.1f}% of standalone -> {100*(codes+bits)/alone:.1f}%  "
          f"({alone/(codes+raw):.1f}x -> {alone/(codes+bits):.1f}x)")
    json.dump(dict(repo=repo, tensor=name, best=lab, bits=bits, sc_exact=se, mn_exact=me),
              open(f"{B}/qgq/kscale2.json", "w"), indent=1)
    print("KSCALE2_DONE")

if __name__ == "__main__":
    main()
