#!/usr/bin/env python3
"""What does it cost to rebuild a quant from another one?

The size of this lever is measured (a Q4_K_M costs 6.8% of standalone given a Q8_0) but its speed never
was, and a store cannot place a scheme without it. There are two variants and they differ sharply:

  codes only     the scales are stored verbatim, so reconstruction is one pass: dequantise the source,
                 predict each 4-bit code from the sub-block's own stored scale and minimum, add the
                 residual, repack.
  codes+scales   the scales are predicted too, which needs ggml's search re-run at READ time -- 21
                 candidate scales per sub-block, each a weighted least-squares fit. Smaller, but the
                 read pays for it.

Both are timed here in numpy, which is an upper bound on the cost: a C implementation of the same
arithmetic would be faster by an unmeasured factor, exactly as the delta coder's prototype was."""
import json, sys, time
import numpy as np
B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, get_scale_min_k4, api, H
from kscale2 import search
NSB = 20000

def main():
    repo = "Qwen/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8 = next(f for f in files if "q8_0" in f.lower())
    q4k = next(f for f in files if "q4_k_m" in f.lower())
    h8, hk = read_header(repo, q8), read_header(repo, q4k)
    common = [n for n in h8["tensors"] if n in hk["tensors"]
              and h8["tensors"][n]["ttype"] == 8 and hk["tensors"][n]["ttype"] == 12]
    common.sort(key=lambda n: -int(np.prod(h8["tensors"][n]["dims"])))
    name = common[0]
    nsb = min(NSB, int(np.prod(h8["tensors"][name]["dims"])) // 256)
    r8 = fetch(repo, q8, h8["data_start"] + h8["tensors"][name]["offset"], nsb*8*34)
    rk = fetch(repo, q4k, hk["data_start"] + hk["tensors"][name]["offset"], nsb*144)
    a = rk[:len(rk)//144*144].reshape(-1, 144)
    n = min(nsb, len(a)); a = a[:n]
    out_bytes = n * 144                      # the size of the file being rebuilt
    print(f"{repo}\n  {name}, {n:,} super-blocks, rebuilding {out_bytes/1e6:.1f} MB of Q4_K_M\n")

    def rebuild_codes_only(reps=5):
        best = 1e30
        for _ in range(reps):
            t0 = time.perf_counter()
            x = dec_q8(r8)[:n*256].reshape(n, 256)
            d_sb = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
            dmin_sb = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
            sc, mn = get_scale_min_k4(a[:, 4:16])
            dj = d_sb[:, None] * sc; mj = dmin_sb[:, None] * mn
            sub = np.repeat(np.arange(8), 32)[None, :]
            D = np.take_along_axis(dj, np.broadcast_to(sub, (n, 256)), axis=1)
            M = np.take_along_axis(mj, np.broadcast_to(sub, (n, 256)), axis=1)
            with np.errstate(divide="ignore", invalid="ignore"):
                pred = np.clip(np.where(D != 0, np.rint((x + M) / D), 0), 0, 15).astype(np.uint8)
            lo = pred[:, ::2] if False else None      # repack cost, measured below
            packed = np.empty((n, 128), dtype=np.uint8)
            for p in range(4):
                packed[:, p*32:(p+1)*32] = pred[:, p*64:p*64+32] | (pred[:, p*64+32:p*64+64] << 4)
            best = min(best, time.perf_counter() - t0)
        return best

    def rebuild_with_scales(reps=3):
        best = 1e30
        for _ in range(reps):
            t0 = time.perf_counter()
            x = dec_q8(r8)[:n*256].reshape(n, 256)
            sub32 = x.reshape(n*8, 32).astype(np.float64)
            w = np.sqrt((sub32*sub32).mean(axis=1, keepdims=True) + sub32*sub32)
            scale, the_min = search(sub32, w)          # ggml's search, 21 candidates
            d_sb = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
            dmin_sb = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
            scale = scale.reshape(n, 8); the_min = the_min.reshape(n, 8)
            with np.errstate(divide="ignore", invalid="ignore"):
                sc_p = np.clip(np.rint(np.where(d_sb[:, None] > 0, scale/d_sb[:, None], 0)), 0, 63)
                mn_p = np.clip(np.rint(np.where(dmin_sb[:, None] > 0, the_min/dmin_sb[:, None], 0)), 0, 63)
            dj = d_sb[:, None] * sc_p; mj = dmin_sb[:, None] * mn_p
            subx = np.repeat(np.arange(8), 32)[None, :]
            D = np.take_along_axis(dj, np.broadcast_to(subx, (n, 256)), axis=1)
            M = np.take_along_axis(mj, np.broadcast_to(subx, (n, 256)), axis=1)
            with np.errstate(divide="ignore", invalid="ignore"):
                pred = np.clip(np.where(D != 0, np.rint((x + M)/D), 0), 0, 15).astype(np.uint8)
            packed = np.empty((n, 128), dtype=np.uint8)
            for p in range(4):
                packed[:, p*32:(p+1)*32] = pred[:, p*64:p*64+32] | (pred[:, p*64+32:p*64+64] << 4)
            best = min(best, time.perf_counter() - t0)
        return best

    t1 = rebuild_codes_only()
    t2 = rebuild_with_scales()
    print(f"  {'variant':22s} {'time':>9s} {'throughput':>13s} {'cost of the file':>18s}")
    print(f"  {'codes only':22s} {t1:8.3f}s {out_bytes/t1/1e6:12.1f} MB/s {0.0864+0.4400:17.4f} b/wt")
    print(f"  {'codes + scale search':22s} {t2:8.3f}s {out_bytes/t2/1e6:12.1f} MB/s {0.0864+0.2180:17.4f} b/wt")
    print(f"\n  the search costs {t2/t1:.1f}x the read time to save "
          f"{100*(1-(0.0864+0.2180)/(0.0864+0.4400)):.0f}% of the residual")
    print(f"  a 4 GB quant would rebuild in {4000/(out_bytes/t1/1e6):.0f}s (codes only) or "
          f"{4000/(out_bytes/t2/1e6):.0f}s (with the search), in numpy")
    json.dump(dict(repo=repo, tensor=name, superblocks=int(n), bytes=int(out_bytes),
                   codes_only_s=t1, with_scales_s=t2,
                   codes_only_mbs=out_bytes/t1/1e6, with_scales_mbs=out_bytes/t2/1e6),
              open(f"{B}/qgq/speed.json", "w"), indent=1)
    print("QGQ_SPEED_DONE")

if __name__ == "__main__":
    main()
