#!/usr/bin/env python3
"""The 23% per-chunk BF16 saving is the largest lever on the page and rests on a handful of tensors.

Thirty repositories from the byte-weighted sample, one 1 MiB slice of a large 2D BF16 tensor each, the
Fano-equivalent cost (high plane order-0 + raw low byte) against 16 bits and against Xet's ~14.0 --
gives the lever a distribution rather than a point. Also records the exponent-plane entropy alone,
which is what decides the ratio."""
import json, sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("safetensors:BF16", 0) or 0) > 500e6]
    # stride through the byte-ranked list so the sample is not just the largest repos
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["safetensors:BF16"]))
    step = max(1, len(cand) // 40)
    pick = cand[::step][:40]
    res = []
    print(f"BF16 repos >500MB in sample: {len(cand)}; probing {len(pick)} spread across the size range")
    for r in pick:
        rid = r["id"]
        try:
            d = api(f"{H}/api/models/{rid}")
            sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sts: continue
            wmap, cache = shard_map(rid, sts)
            ks = [k for k in wmap if k.endswith(".weight") and "embed" not in k and "lm_head" not in k and "norm" not in k]
            if not ks: continue
            k = sorted(ks)[len(ks) // 2]
            fn = wmap[k]
            if fn not in cache: cache[fn] = st_header(rid, fn)
            hb, off = cache[fn]
            if not hb or k not in hb or hb[k]["dtype"] != "BF16" or len(hb[k]["shape"]) != 2: continue
            m = hb[k]; nb = min(1 << 20, m["data_offsets"][1] - m["data_offsets"][0])
            raw = grab(rid, fn, off + m["data_offsets"][0], nb)
            if raw is None: continue
            u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
            hexp = h0((u >> 8).astype(np.uint16)); cost = hexp + 8.0
            res.append((rid, k, hexp, cost, 16 / cost))
        except Exception:
            continue
    if not res: print("nothing measured"); return
    c = np.array([x[3] for x in res]); ratio = 16 / c
    print(f"\nmeasured {len(res)} tensors from {len(res)} repos")
    print(f"  Fano cost bits/weight: min {c.min():.2f}  p25 {np.percentile(c,25):.2f}  median {np.median(c):.2f}  p75 {np.percentile(c,75):.2f}  max {c.max():.2f}")
    print(f"  ratio vs raw:          min {ratio.min():.2f}x  median {np.median(ratio):.2f}x  max {ratio.max():.2f}x   (page uses 1.49x)")
    print(f"  bytes saved vs Xet 14.0: median {100*(1-np.median(c)/14.0):.1f}%  p10 {100*(1-np.percentile(c,90)/14.0):.1f}%  p90 {100*(1-np.percentile(c,10)/14.0):.1f}%   (page uses 23%)")
    worst = sorted(res, key=lambda x: -x[3])[:3]; best = sorted(res, key=lambda x: x[3])[:3]
    print("  hardest:", "; ".join(f"{r[:26]} {cst:.2f}" for r, _k, _h, cst, _ in worst))
    print("  easiest:", "; ".join(f"{r[:26]} {cst:.2f}" for r, _k, _h, cst, _ in best))
    json.dump([dict(rid=a, k=b, hexp=c_, cost=d_, ratio=e) for a, b, c_, d_, e in res], open(f"{B}/qgq/fanospot.json", "w"), indent=1)
    print("FANOSPOT_DONE")

if __name__ == "__main__":
    main()
