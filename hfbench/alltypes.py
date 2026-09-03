#!/usr/bin/env python3
"""Does the prediction hold for every quant type, or only the two already measured?

The population claim -- 0.111 PB a week, a repo of 15 quants at 4-5x -- rests on Q4_0 and Q4_K_M. The
other types are most of a typical repo, so if they do not predict, the claim shrinks. For the
serving-path variant no search is needed: each type's codes are the inverse of its own dequantisation
formula, evaluated on weights reconstructed from the repo's Q8_0 and the target's OWN stored scales.

    Q4_K   x = d_j*q - m_j            q in [0,15]     q = (x + m_j)/d_j
    Q5_K   same, with a fifth bit     q in [0,31]     q = (x + m_j)/d_j
    Q6_K   x = d*sc_j*(q - 32)        q in [0,63]     q = x/(d*sc_j) + 32
    Q3_K   x = d*sc_j*(q - 4)         q in [0,7]      q = x/(d*sc_j) + 4
    Q2_K   x = d*sc_j*q - dmin*m_j    q in [0,3]      q = (x + dmin*m_j)/(d*sc_j)

Each is measured against the published file: how often the predicted code is exact, and what the
residual costs against storing the codes."""
import json, os, sys
import numpy as np
B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, get_scale_min_k4, h0, api, H
NSB = 15000
TT = {12: "Q4_K", 13: "Q5_K", 14: "Q6_K", 11: "Q3_K", 10: "Q2_K"}
SZ = {12: 144, 13: 176, 14: 210, 11: 110, 10: 84}

def unpack_k4(a, nib_off, nbytes=128):
    qs = a[:, nib_off:nib_off+nbytes].astype(np.int32)
    n = len(a)
    lo = qs & 0x0F; hi = qs >> 4
    q = np.empty((n, 256), dtype=np.int32)
    for p in range(4):
        q[:, p*64:p*64+32] = lo[:, p*32:(p+1)*32]
        q[:, p*64+32:p*64+64] = hi[:, p*32:(p+1)*32]
    return q

def dec_target(raw, tt):
    """returns (codes[n,256], D[n,256], M[n,256], nmax) so that q = round((x + M)/D) in every case"""
    s = SZ[tt]; n = len(raw)//s; a = raw[:n*s].reshape(n, s)
    if tt in (12, 13):                                     # Q4_K / Q5_K
        d = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
        dmin = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
        sc, mn = get_scale_min_k4(a[:, 4:16])
        if tt == 12:
            q = unpack_k4(a, 16)
            nmax = 15
        else:
            qh = a[:, 16:48].astype(np.int32)
            q = unpack_k4(a, 48)
            bit = np.empty((n, 256), dtype=np.int32)
            for p in range(8):
                bit[:, p*32:(p+1)*32] = (qh >> p) & 1
            q = q + (bit << 4)
            nmax = 31
        dj = d[:, None]*sc; mj = dmin[:, None]*mn
        sub = np.repeat(np.arange(8), 32)[None, :]
        D = np.take_along_axis(dj, np.broadcast_to(sub, (n, 256)), axis=1)
        M = np.take_along_axis(mj, np.broadcast_to(sub, (n, 256)), axis=1)
        return q, D, M, 0, nmax
    if tt == 14:                                           # Q6_K
        ql = a[:, 0:128].astype(np.int32); qh = a[:, 128:192].astype(np.int32)
        sc = a[:, 192:208].view(np.int8).astype(np.float32)
        d = a[:, 208:210].copy().view(np.float16).astype(np.float32).ravel()
        q = np.empty((n, 256), dtype=np.int32)
        for half in range(2):
            qlh = ql[:, half*64:(half+1)*64]; qhh = qh[:, half*32:(half+1)*32]
            l = np.arange(32); base = half*128
            q[:, base+  0:base+ 32] = (qlh[:, l]      & 0xF) | (((qhh[:, l] >> 0) & 3) << 4)
            q[:, base+ 32:base+ 64] = (qlh[:, l + 32] & 0xF) | (((qhh[:, l] >> 2) & 3) << 4)
            q[:, base+ 64:base+ 96] = (qlh[:, l]      >> 4)  | (((qhh[:, l] >> 4) & 3) << 4)
            q[:, base+ 96:base+128] = (qlh[:, l + 32] >> 4)  | (((qhh[:, l] >> 6) & 3) << 4)
        idx = np.empty(256, dtype=np.int64)
        for half in range(2):
            for grp in range(4):
                idx[half*128 + grp*32: half*128 + grp*32 + 32] = half*8 + grp*2 + np.arange(32)//16
        S = d[:, None] * sc[:, idx]
        return q, S, np.zeros_like(S), 32, 63
    raise ValueError(tt)

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8f = next((f for f in files if "q8_0" in f.lower()), None)
    if not q8f: print("no q8_0"); return
    h8 = read_header(repo, q8f)
    print(f"{repo}, source {q8f}\n")
    print(f"{'target':10s} {'type':6s} {'tensor':26s} {'codes exact':>12s} {'residual':>10s} {'alone':>8s} {'cost':>8s}")
    out = []
    for key in ("q5_k_m", "q6_k", "q4_k_m"):
        f = next((x for x in files if key in x.lower()), None)
        if not f: continue
        hk = read_header(repo, f)
        cand = [(n, hk["tensors"][n]["ttype"]) for n in hk["tensors"]
                if n in h8["tensors"] and h8["tensors"][n]["ttype"] == 8
                and hk["tensors"][n]["ttype"] in SZ]
        if not cand: print(f"{key:10s} no comparable tensor"); continue
        cand.sort(key=lambda c: -int(np.prod(hk["tensors"][c[0]]["dims"])))
        name, tt = cand[0]
        nsb = min(NSB, int(np.prod(h8["tensors"][name]["dims"]))//256)
        r8 = fetch(repo, q8f, h8["data_start"] + h8["tensors"][name]["offset"], nsb*8*34)
        rk = fetch(repo, f, hk["data_start"] + hk["tensors"][name]["offset"], nsb*SZ[tt])
        if r8 is None or rk is None: print(f"{key:10s} fetch failed"); continue
        x = dec_q8(r8)[:nsb*256].reshape(nsb, 256)
        try: q, D, M, off, nmax = dec_target(rk, tt)
        except Exception as e: print(f"{key:10s} {TT.get(tt,tt)} decode failed: {e}"); continue
        n = min(len(q), len(x)); q, D, M, x = q[:n], D[:n], M[:n], x[:n]
        with np.errstate(divide="ignore", invalid="ignore"):
            pred = np.where(D != 0, np.rint((x + M)/D) + off, 0)
        pred = np.clip(pred, 0, nmax).astype(np.int32)
        resid = (q - pred).ravel()
        exact = float(np.mean(resid == 0))
        alone = h0(q.ravel()) + (SZ[tt]*8 - (nmax.bit_length())*256)/256.0 if False else h0(q.ravel())
        # bits per weight: the codes' own entropy plus the type's fixed scale overhead
        scale_bits = (SZ[tt]*8 - {12:1024, 13:1280, 14:1536}[tt]) / 256.0
        alone = h0(q.ravel()) + scale_bits
        cost = h0(resid) + scale_bits
        print(f"{key:10s} {TT[tt]:6s} {name[:26]:26s} {100*exact:11.2f}% {h0(resid):10.4f} {alone:8.4f} "
              f"{cost:8.4f}  = {100*cost/alone:.1f}% ({alone/cost:.1f}x)")
        out.append(dict(target=key, type=TT[tt], tensor=name, exact=exact, cost=cost, alone=alone))
    json.dump(out, open(f"{B}/qgq/alltypes.json", "w"), indent=1)
    print("ALLTYPES_DONE")

if __name__ == "__main__":
    main()
