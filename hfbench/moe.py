#!/usr/bin/env python3
"""Does the prediction hold for fused mixture-of-experts tensors -- where the bytes actually are?

MoE was 21% of the unmapped elements and by far the largest single tensors on the Hub: one
blk.N.ffn_down_exps.weight in GLM-5.3 holds 3,221 M elements. GGUF fuses every expert into that one
tensor with dims [n_embd, n_ff, n_expert]; HF keeps mlp.experts.E.down_proj.weight separately. The
layout is expert-major and contiguous, so expert 0 occupies the first n_embd*n_ff elements of the fused
tensor and its element order matches HF's row-major [n_ff, n_embd] exactly.

If that is right, expert 0 should predict from HF's expert 0 like any other matrix, and the only thing
that was ever missing is the concatenation. Measured on both Q8_0 and K-quant targets."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import q8_blocks
from crossrepo4 import shard_map
from alltypes import dec_target, TT, SZ
from namemap import expert_names
CODE_BITS = {12: 4, 13: 5, 14: 6}

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter(); out = []; done = tried = 0
    print(f"{'repo':26s} {'tensor':22s} {'type':6s} {'blocks':>7s} {'exact':>8s} {'alone':>7s} {'cost':>7s} {'ratio':>7s}")
    for rid, base in order:
        if done >= 6 or tried >= 220: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: continue
            db = api(f"{H}/api/models/{base}")
            if not db: continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: continue
            gf = min(gg, key=len)
            hg = read_header(rid, gf)
            if not hg or not hg.get("tensors"): continue
            exps = [(n, m) for n, m in hg["tensors"].items()
                    if expert_names(n) and m["ttype"] in (8, 12, 13, 14)]
            if not exps: why["no fused expert tensor"] += 1; continue
            wmap, cache = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        got = 0
        for gname, gm in sorted(exps, key=lambda c: -int(np.prod(c[1]["dims"])))[:3]:
            sname = next((t.format(e=0) for t in expert_names(gname) if t.format(e=0) in wmap), None)
            if not sname: why["expert 0 not found in base"] += 1; continue
            shard = wmap[sname]
            if shard not in cache: cache[shard] = st_header(base, shard)
            hb, offb = cache[shard]
            if not hb or sname not in hb: why["base header unreadable"] += 1; continue
            meta = hb[sname]
            if meta.get("dtype") not in ("BF16", "F16"): why[f"base dtype {meta.get('dtype')}"] += 1; continue
            dims = list(gm["dims"])
            per_expert = int(np.prod(dims[:2])) if len(dims) >= 3 else int(np.prod(dims))
            hf_n = int(np.prod(meta["shape"]))
            if per_expert != hf_n:
                why[f"expert slice {per_expert} != hf {hf_n}"] += 1; continue
            tt = gm["ttype"]
            unit = 256 if tt in SZ else 32
            nunits = min(per_expert // unit, (3 << 20) // unit)
            if nunits < 60: why["expert too small"] += 1; continue
            nby = nunits * (SZ[tt] if tt in SZ else 34)
            rg = fetch(rid, gf, hg["data_start"] + gm["offset"], nby)
            rw = grab(base, shard, offb + meta["data_offsets"][0], nunits * unit * 2)
            if rg is None or rw is None: why["fetch failed"] += 1; continue
            r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
            w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
            if tt == 8:
                dq, q = q8_blocks(rg)
                n = min(len(q), len(w) // 32)
                if n < 60: why["short overlap"] += 1; continue
                q = q[:n]; x = w[:n*32].reshape(n, 32)
                dpred = (np.abs(x).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
                with np.errstate(divide="ignore", invalid="ignore"):
                    pred = np.where(dpred[:, None] != 0, np.rint(x / dpred[:, None]), 0)
                pred = np.clip(pred, -127, 127).astype(np.int32)
                resid = (q - pred).ravel()
                alone = h0(q.ravel()) + 0.5; cost = h0(resid) + 0.5; label = "Q8_0"
            else:
                try: q, D, M, off, nmax = dec_target(rg, tt)
                except Exception: why[f"no decoder {TT.get(tt,tt)}"] += 1; continue
                n = min(len(q), len(w) // 256)
                if n < 60: why["short overlap"] += 1; continue
                q = q[:n]; D = D[:n]; M = M[:n]; x = w[:n*256].reshape(n, 256)
                with np.errstate(divide="ignore", invalid="ignore"):
                    pred = np.where(D != 0, np.rint((x + M) / D) + off, 0)
                pred = np.clip(pred, 0, nmax).astype(np.int32)
                resid = (q - pred).ravel()
                sbb = (SZ[tt]*8 - CODE_BITS[tt]*256) / 256.0
                alone = h0(q.ravel()) + sbb; cost = h0(resid) + sbb; label = TT[tt]
            ex = float(np.mean(resid == 0))
            got += 1
            out.append(dict(rid=rid, t=label, exact=ex, alone=alone, cost=cost))
            print(f"{rid[:26]:26s} {gname.split('.')[-2][:22]:22s} {label:6s} {n:7d} "
                  f"{100*ex:7.2f}% {alone:7.3f} {cost:7.3f} {100*cost/alone:6.1f}%")
        if got: done += 1
    if out:
        a = sum(o["alone"] for o in out); c = sum(o["cost"] for o in out)
        print(f"\n  {len(out)} fused expert tensors across {done} repos: "
              f"{100*c/a:.1f}% of standalone ({a/c:.1f}x), mean exact {100*np.mean([o['exact'] for o in out]):.2f}%")
    print(f"\npairs attempted {tried}")
    for r, n in why.most_common(8): print(f"  {n:5d}  {r}")
    json.dump(out, open(f"{B}/qgq/moe.json", "w"), indent=1, default=float)
    print("MOE_DONE")

if __name__ == "__main__":
    main()
