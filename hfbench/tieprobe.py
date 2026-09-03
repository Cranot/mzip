#!/usr/bin/env python3
"""The 1-5% of K-quant codes that do NOT match their parent: is the mismatch itself predictable?

Every residual so far is coded at order-0 -- h0(resid) -- which treats a miss as equally likely at
every position. But a miss is a rounding disagreement, and rounding disagrees where the pre-rounding
value sits near a tie: frac((x+M)/D) close to 0.5. If misses cluster there, the residual is
CONDITIONALLY predictable and its cost is h(resid | tie-distance), which can only be lower.

Measured on one Q4_K tensor given a sibling Q8_0 (the case with the most misses) and one Q8_0 given
its base. Reports h0(resid) against h(resid | bucket) for 16 buckets of distance-to-tie, and the
fraction of misses that fall in the nearest-to-tie bucket. If the conditional gain is under 0.01
bits/weight this lever is closed."""
import json, sys
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, h0, api
from alltypes import dec_target, TT, SZ

def cond_h(resid, key, nb):
    """H(resid | bucket(key)) in bits per symbol"""
    b = np.clip((key * nb).astype(int), 0, nb - 1)
    tot = resid.size; acc = 0.0
    for i in range(nb):
        m = b == i
        if m.any(): acc += m.sum() * h0(resid[m])
    return acc / tot

def main():
    repo = "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d["siblings"] if s["rfilename"].lower().endswith(".gguf")]
    q8f = next(f for f in files if "q8_0" in f.lower())
    h8 = read_header(repo, q8f)
    print(f"{'target':8s} {'tensor':22s} {'miss%':>6s} {'h0(resid)':>10s} {'h(resid|tie)':>13s} {'gain':>7s} {'misses in tie-bucket':>21s}")
    for key, tt_want in (("q4_k_m", 12), ("q5_k_m", 13), ("q6_k", 14)):
        f = next((x for x in files if key in x.lower()), None)
        if not f: continue
        hk = read_header(repo, f)
        cands = [(n, m) for n, m in hk["tensors"].items() if m["ttype"] == tt_want
                 and n in h8["tensors"] and h8["tensors"][n]["ttype"] == 8 and "ffn" in n]
        if not cands: continue
        name, gm = max(cands, key=lambda c: int(np.prod(c[1]["dims"])))
        nsb = min(15000, int(np.prod(gm["dims"])) // 256)
        r8 = fetch(repo, q8f, h8["data_start"] + h8["tensors"][name]["offset"], nsb * 8 * 34)
        rk = fetch(repo, f, hk["data_start"] + gm["offset"], nsb * SZ[tt_want])
        if r8 is None or rk is None: continue
        x = dec_q8(r8)[:nsb * 256].reshape(nsb, 256)
        q, D, M, off, nmax = dec_target(rk, tt_want)
        n = min(len(q), len(x)); q, D, M, x = q[:n], D[:n], M[:n], x[:n]
        with np.errstate(divide="ignore", invalid="ignore"):
            v = np.where(D != 0, (x + M) / D, 0.0)
        pred = np.clip(np.rint(v) + off, 0, nmax).astype(np.int32)
        resid = (q - pred).ravel()
        frac = np.abs(v - np.rint(v)).ravel()          # 0 = exactly on integer, 0.5 = exactly on tie
        tie_dist = 0.5 - frac                           # 0 = on the tie
        h_uncond = h0(resid)
        h_cond = cond_h(resid, 1.0 - 2 * tie_dist, 16)  # key in [0,1], 1 = on the tie
        miss = resid != 0
        near = tie_dist < (0.5 / 16)
        share = float(miss[near].sum() / max(miss.sum(), 1))
        print(f"{TT[tt_want]:8s} {name[:22]:22s} {100*miss.mean():5.2f}% {h_uncond:10.4f} {h_cond:13.4f} "
              f"{h_uncond - h_cond:7.4f} {100*share:20.1f}%")
    print("\n  gain = bits/weight recoverable by conditioning the residual on distance to the rounding tie.")
    print("  'misses in tie-bucket' = share of all mismatches that sit in the 1/16 of values nearest a tie;")
    print("  6.25% would be no clustering at all.")
    print("TIEPROBE_DONE")

if __name__ == "__main__":
    main()
