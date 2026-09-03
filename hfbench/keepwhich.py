#!/usr/bin/env python3
"""Which file should the repo keep whole?

The population model keeps a repo's Q8_0 and codes every other quant as a residual on it, and the kept
file then dominates what the repo costs -- so the question is whether a SMALLER file predicts the others
just as well. Q6_K is about 24% smaller than Q8_0 and still carries six bits per weight, so if it
predicts Q4_K_M at the same rate, keeping it is strictly better.

Implements the Q6_K dequantiser (128 low nibbles, 64 bytes of high bit-pairs, 16 int8 sub-scales and an
fp16 super-block scale per 256 weights) and repeats the RESULT 76 measurement with Q6_K as the source
instead of Q8_0."""
import json, os, sys
import numpy as np
B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, dec_q4k, get_scale_min_k4, h0, api, H
NSB = 20000

def dec_q6k(raw):
    """block_q6_K: ql[128] qh[64] scales[16] d(fp16) = 210 bytes per 256 weights"""
    n = len(raw) // 210
    a = raw[:n*210].reshape(n, 210)
    ql = a[:, 0:128].astype(np.int32)
    qh = a[:, 128:192].astype(np.int32)
    sc = a[:, 192:208].view(np.int8).astype(np.float32)
    d = a[:, 208:210].copy().view(np.float16).astype(np.float32).ravel()
    out = np.empty((n, 256), dtype=np.float32)
    for half in range(2):                      # two 128-weight halves per super-block
        qlh = ql[:, half*64:(half+1)*64]
        qhh = qh[:, half*32:(half+1)*32]
        sch = sc[:, half*8:(half+1)*8]
        l = np.arange(32)
        is_ = l // 16                          # which of the 8 sub-scales
        q1 = (qlh[:, l]       & 0xF) | (((qhh[:, l] >> 0) & 3) << 4)
        q2 = (qlh[:, l + 32]  & 0xF) | (((qhh[:, l] >> 2) & 3) << 4)
        q3 = (qlh[:, l]       >> 4)  | (((qhh[:, l] >> 4) & 3) << 4)
        q4 = (qlh[:, l + 32]  >> 4)  | (((qhh[:, l] >> 6) & 3) << 4)
        base = half * 128
        out[:, base +  0:base + 32] = d[:, None] * sch[:, is_ + 0] * (q1 - 32)
        out[:, base + 32:base + 64] = d[:, None] * sch[:, is_ + 2] * (q2 - 32)
        out[:, base + 64:base + 96] = d[:, None] * sch[:, is_ + 4] * (q3 - 32)
        out[:, base + 96:base +128] = d[:, None] * sch[:, is_ + 6] * (q4 - 32)
    return out

def predict_q4k(x, rk):
    """codes of Q4_K_M predicted from reconstructed weights x, using Q4_K's own stored scales"""
    a = rk[:len(rk)//144*144].reshape(-1, 144)
    n = min(len(x), len(a)); x = x[:n]; a = a[:n]
    d_sb = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    dmin_sb = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
    sc, mn = get_scale_min_k4(a[:, 4:16])
    qs = a[:, 16:]
    lo = (qs & 0x0F).astype(np.int32); hi = (qs >> 4).astype(np.int32)
    q = np.empty((n, 256), dtype=np.int32)
    for p in range(4):
        q[:, p*64:p*64+32] = lo[:, p*32:(p+1)*32]
        q[:, p*64+32:p*64+64] = hi[:, p*32:(p+1)*32]
    dj = d_sb[:, None] * sc; mj = dmin_sb[:, None] * mn
    sub = np.repeat(np.arange(8), 32)[None, :]
    D = np.take_along_axis(dj, np.broadcast_to(sub, (n, 256)), axis=1)
    M = np.take_along_axis(mj, np.broadcast_to(sub, (n, 256)), axis=1)
    with np.errstate(divide="ignore", invalid="ignore"):
        pred = np.where(D != 0, np.rint((x[:n] + M) / D), 0).astype(np.int32)
    pred = np.clip(pred, 0, 15)
    return q, pred

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "Qwen/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    pick = lambda k: next((f for f in files if k in f.lower()), None)
    q8, q6, q4k = pick("q8_0"), pick("q6_k"), pick("q4_k_m")
    if not (q8 and q6 and q4k): print("need q8_0, q6_k and q4_k_m"); return
    print(f"{repo}\n  source candidates: {q8}, {q6}\n  target: {q4k}")
    h8, h6, hk = read_header(repo, q8), read_header(repo, q6), read_header(repo, q4k)
    common = [n for n in hk["tensors"]
              if n in h8["tensors"] and n in h6["tensors"]
              and h8["tensors"][n]["ttype"] == 8 and h6["tensors"][n]["ttype"] == 14
              and hk["tensors"][n]["ttype"] == 12]
    if not common: print("no tensor is Q8_0, Q6_K and Q4_K across the three"); return
    common.sort(key=lambda n: -int(np.prod(hk["tensors"][n]["dims"])))
    name = common[0]
    nsb = min(NSB, int(np.prod(hk["tensors"][name]["dims"])) // 256)
    print(f"  tensor {name}, {nsb:,} super-blocks\n")
    rk = fetch(repo, q4k, hk["data_start"] + hk["tensors"][name]["offset"], nsb*144)
    r8 = fetch(repo, q8,  h8["data_start"] + h8["tensors"][name]["offset"], nsb*8*34)
    r6 = fetch(repo, q6,  h6["data_start"] + h6["tensors"][name]["offset"], nsb*210)
    if rk is None or r8 is None or r6 is None: print("fetch failed"); return
    x8 = dec_q8(r8)[:nsb*256].reshape(nsb, 256)
    x6 = dec_q6k(r6)[:nsb]
    print(f"  {'source':10s} {'bits/weight':>12s} {'codes exact':>12s} {'residual bits':>14s} {'Q4_K_M cost':>12s}")
    res = {}
    for label, x, src_bits in (("Q8_0", x8, 8.5), ("Q6_K", x6, 6.5625)):
        q, pred = predict_q4k(x, rk)
        m = min(len(q), len(pred))
        resid = (q[:m] - pred[:m]).ravel()
        rb = h0(resid)
        exact = float(np.mean(resid == 0))
        total = rb + 0.2180                       # scales, from RESULT 79's real search
        res[label] = (exact, rb, total, src_bits)
        print(f"  {label:10s} {src_bits:12.4f} {100*exact:11.2f}% {rb:14.4f} {total:12.4f}")
    alone = 4.3621
    print()
    for label, (exact, rb, total, src_bits) in res.items():
        print(f"  keeping {label}: the kept file costs {src_bits:.4f} bits/weight, "
              f"each other quant {total:.4f} ({100*total/alone:.1f}% of standalone)")
    # a repo of N quants: one kept whole, the rest as residuals
    print(f"\n  a repo of 15 quants averaging {alone:.2f} bits/weight:")
    for label, (exact, rb, total, src_bits) in res.items():
        cost = src_bits + 14 * total
        base = 15 * alone
        print(f"    keep {label}: {cost:7.2f} vs {base:7.2f} bits/weight  -> {base/cost:.1f}x")
    json.dump({k: dict(exact=v[0], resid_bits=v[1], total=v[2], src_bits=v[3]) for k, v in res.items()},
              open(f"{B}/qgq/keepwhich.json", "w"), indent=1)
    print("KEEPWHICH_DONE")

if __name__ == "__main__":
    main()
