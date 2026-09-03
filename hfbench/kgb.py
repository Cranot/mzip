#!/usr/bin/env python3
"""The last gap in the chain: a K-quant given the BASE, not given a sibling Q8_0.

RESULT 84 predicts Q4_K/Q5_K/Q6_K from a sibling Q8_0 in the same repo. RESULT 88 predicts a Q8_0 from
the base model in another repo. What has never been measured is the direct edge -- a K-quant from the
base's own bf16 weights -- and that is the one that decides whether a whole GGUF repo can be stored
against a public parent, or only its Q8_0.

It should be at least as good as the sibling route: bf16 is what llama.cpp actually quantised, so the
predictor sees the true input rather than a Q8_0 round-trip of it. Same arithmetic as RESULT 84 --
each type's codes are the inverse of its own dequantisation formula, evaluated on the base weights
using the target file's OWN stored scales -- plus the Q/K permutation from RESULT 88."""
import json, os, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import to_st
from crossrepo4 import shard_map, permute
from hfget import get_json
from alltypes import dec_target, TT, SZ

CODE_BITS = {12: 4, 13: 5, 14: 6, 11: 3, 10: 2}

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter(); bytype = collections.defaultdict(list); done = tried = 0
    print(f"{'repo':22s} {'file':10s} {'type':6s} {'kind':11s} {'exact':>8s} {'alone':>7s} {'cost':>7s} {'ratio':>7s}")
    for rid, base in order:
        if done >= 6 or tried >= 200: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            targets = [f for f in gg if any(t in f.lower() for t in ("q4_k", "q5_k", "q6_k"))]
            if not targets: why["no K-quant file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base not reachable"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base has no safetensors"] += 1; continue
            cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
            nh = int(cfg.get("num_attention_heads") or 0); nkv = int(cfg.get("num_key_value_heads") or nh)
            wmap, cache = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        got = 0
        for tf in sorted(targets)[:2]:
            hk = read_header(rid, tf)
            if not hk: why["gguf header unreadable"] += 1; continue
            best = {}
            for gname, gm in hk["tensors"].items():
                tt = gm["ttype"]
                if tt not in SZ or not to_st(gname): continue
                k = ("embedding" if gname == "token_embd.weight" else
                     "output" if gname == "output.weight" else gname.split(".")[2])
                sz = int(np.prod(gm["dims"]))
                key = (tt, k)
                pick_small = k in ("attn_q", "attn_k")
                if key not in best or (sz < best[key][2] if pick_small else sz > best[key][2]):
                    best[key] = (gname, gm, sz)
            for (tt, k) in sorted(best, key=lambda x: (x[0], x[1])):
                gname, gm, _ = best[(tt, k)]
                sname = to_st(gname); shard = wmap.get(sname)
                if not shard: why["tensor absent in base"] += 1; continue
                if shard not in cache: cache[shard] = st_header(base, shard)
                hb, offb = cache[shard]
                if not hb or sname not in hb: why["base header unreadable"] += 1; continue
                meta = hb[sname]
                if meta.get("dtype") not in ("BF16", "F16"): why[f"base dtype {meta.get('dtype')}"] += 1; continue
                if sorted(meta["shape"]) != sorted(list(gm["dims"])): why["shape mismatch"] += 1; continue
                rows, cols = meta["shape"][0], int(np.prod(meta["shape"][1:]))
                whole = k in ("attn_q", "attn_k")
                if whole and rows * cols * 2 > (48 << 20): why["q/k too large"] += 1; continue
                nrow = rows if whole else min(rows, max(256, (1 << 22) // max(cols * 2, 1)))
                nel = nrow * cols
                nsb = nel // 256
                if nsb < 60: why["too few superblocks"] += 1; continue
                rk = fetch(rid, tf, hk["data_start"] + gm["offset"], nsb * SZ[tt])
                rw = grab(base, shard, offb + meta["data_offsets"][0], nel * 2)
                if rk is None or rw is None: why["fetch failed"] += 1; continue
                r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
                w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
                if whole and nh:
                    heads = nh if k == "attn_q" else (nkv or nh)
                    wm = w[:rows*cols].reshape(rows, cols)
                    pw = permute(wm, heads)
                    if pw is not None: w = pw.ravel()
                try: q, D, M, off, nmax = dec_target(rk, tt)
                except Exception: why[f"no decoder for {TT.get(tt,tt)}"] += 1; continue
                n = min(len(q), len(w) // 256)
                if n < 60: why["short overlap"] += 1; continue
                q = q[:n]; D = D[:n]; M = M[:n]
                x = w[:n*256].reshape(n, 256)
                with np.errstate(divide="ignore", invalid="ignore"):
                    pred = np.where(D != 0, np.rint((x + M) / D) + off, 0)
                pred = np.clip(pred, 0, nmax).astype(np.int32)
                resid = (q - pred).ravel()
                sb_bits = (SZ[tt]*8 - CODE_BITS[tt]*256) / 256.0
                alone = h0(q.ravel()) + sb_bits
                cost = h0(resid) + sb_bits
                got += 1
                bytype[TT[tt]].append(dict(alone=alone, cost=cost, exact=float(np.mean(resid == 0))))
                print(f"{rid[:22]:22s} {tf.split('/')[-1][-10:]:10s} {TT[tt]:6s} {k[:11]:11s} "
                      f"{100*np.mean(resid == 0):7.2f}% {alone:7.3f} {cost:7.3f} {100*cost/alone:6.1f}%")
        if got: done += 1
    print(f"\n{'type':8s} {'n':>3s} {'codes exact':>12s} {'residual':>10s}")
    aw = ac = 0.0
    for t in sorted(bytype):
        v = bytype[t]; a = sum(x["alone"] for x in v); c = sum(x["cost"] for x in v); aw += a; ac += c
        print(f"{t:8s} {len(v):3d} {100*np.mean([x['exact'] for x in v]):11.2f}% {100*c/a:9.1f}%")
    if ac: print(f"\n  K-quant given the base: {100*ac/aw:.1f}% of standalone ({aw/ac:.1f}x)")
    print(f"\npairs attempted {tried}, repos measured {done}")
    for r, c in why.most_common(10): print(f"  {c:5d}  {r}")
    json.dump({k: v for k, v in bytype.items()}, open(f"{B}/qgq/kgb.json", "w"), indent=1, default=float)
    print("KGB_DONE")

if __name__ == "__main__":
    main()
