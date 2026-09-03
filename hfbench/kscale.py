#!/usr/bin/env python3
"""The scales are 85% of what a predicted K-quant still costs. Can they be predicted too?

RESULT 76: Q4_K_M coded as a residual on Q8_0 costs 0.5864 bits/weight, of which the residual codes
are only 0.0864 and the remaining 0.50 is the block's own scale machinery -- the fp16 super-block
scale and minimum, and the twelve bytes holding eight 6-bit sub-scales and eight 6-bit sub-minima --
which we still store verbatim.

Those numbers are not arbitrary. ggml picks each sub-block's scale and minimum by searching over the
same 32 weights, and we can already reconstruct those weights from Q8_0. So the natural estimate is
the one the quantiser starts from: a sub-block spanning [lo, hi] needs a step of (hi-lo)/15 and an
offset of -lo, which after the super-block normalisation gives predicted 6-bit integers. If the
prediction lands near, the scales cost a small residual instead of 96 bits per 256 weights.

Measures, per sub-block: the true 6-bit scale and minimum against the prediction, and what the
residual costs against storing them raw."""
import json, os, struct, subprocess, sys
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/qgq"
NSB = 40000

def sh(a, t=600): return subprocess.run(a, capture_output=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout.decode())
    except Exception: return None

sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, dec_q4k, get_scale_min_k4, h0

def hc(x, c):
    x = np.asarray(x).ravel().astype(np.int64); c = np.asarray(c).ravel().astype(np.int64)
    x = x - x.min(); c = c - c.min()
    nx = int(x.max()) + 1; nc = int(c.max()) + 1
    if nc * nx > 40_000_000: return h0(x)
    j = np.bincount(c * nx + x, minlength=nc * nx).astype(np.float64).reshape(nc, nx)
    tot = j.sum(); pc = j.sum(axis=1); out = 0.0
    for k in np.nonzero(pc)[0]:
        p = j[k][j[k] > 0] / pc[k]
        out += (pc[k] / tot) * float(-(p * np.log2(p)).sum())
    return out

def main():
    repo = __import__("sys").argv[1] if len(__import__("sys").argv)>1 else "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
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
    n144 = len(rk)//144
    a = rk[:n144*144].reshape(n144, 144)
    d_sb = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    dmin_sb = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
    sc, mn = get_scale_min_k4(a[:, 4:16])            # the true 6-bit integers, [n,8]
    n = min(len(x), n144); x = x[:n]; sc = sc[:n]; mn = mn[:n]
    d_sb = d_sb[:n]; dmin_sb = dmin_sb[:n]

    sub = x.reshape(n, 8, 32)
    lo = sub.min(axis=2); hi = sub.max(axis=2)
    # what the quantiser is aiming at, before the 6-bit grid
    step = (hi - lo) / 15.0
    with np.errstate(divide="ignore", invalid="ignore"):
        sc_pred = np.where(d_sb[:, None] > 0, np.rint(step / d_sb[:, None]), 0)
        mn_pred = np.where(dmin_sb[:, None] > 0, np.rint(-lo / dmin_sb[:, None]), 0)
    sc_pred = np.clip(sc_pred, 0, 63); mn_pred = np.clip(mn_pred, 0, 63)
    r_sc = (sc - sc_pred).astype(np.int64).ravel()
    r_mn = (mn - mn_pred).astype(np.int64).ravel()

    raw_bits = (h0(sc.astype(np.int64).ravel()) + h0(mn.astype(np.int64).ravel())) * 8 / 256 + 32.0/256
    res_bits = (h0(np.clip(r_sc, -32, 32)) + h0(np.clip(r_mn, -32, 32))) * 8 / 256 + 32.0/256
    print(f"\n  6-bit sub-scale : exact {100*float(np.mean(r_sc==0)):5.2f}%  "
          f"within +/-1 {100*float(np.mean(np.abs(r_sc)<=1)):5.2f}%  entropy {h0(sc.astype(np.int64).ravel()):.3f} -> {h0(np.clip(r_sc,-32,32)):.3f} bits")
    print(f"  6-bit sub-min   : exact {100*float(np.mean(r_mn==0)):5.2f}%  "
          f"within +/-1 {100*float(np.mean(np.abs(r_mn)<=1)):5.2f}%  entropy {h0(mn.astype(np.int64).ravel()):.3f} -> {h0(np.clip(r_mn,-32,32)):.3f} bits")
    print(f"\n  scale machinery: {raw_bits:.4f} bits/weight stored raw -> {res_bits:.4f} predicted")
    codes = 0.0864                     # the residual codes, from RESULT 76
    print(f"  Q4_K_M as a residual on Q8_0: {codes+raw_bits:.4f} -> {codes+res_bits:.4f} bits/weight")
    alone = 4.3621
    print(f"  => {100*(codes+raw_bits)/alone:.1f}% of standalone -> {100*(codes+res_bits)/alone:.1f}%  "
          f"({alone/(codes+raw_bits):.1f}x -> {alone/(codes+res_bits):.1f}x)")
    json.dump(dict(repo=repo, tensor=name, sb=int(n),
                   sc_exact=float(np.mean(r_sc==0)), mn_exact=float(np.mean(r_mn==0)),
                   raw_bits=raw_bits, res_bits=res_bits), open(f"{OUT}/kscale.json", "w"), indent=1)
    print("KSCALE_DONE")

if __name__ == "__main__":
    main()
