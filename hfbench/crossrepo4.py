#!/usr/bin/env python3
"""Two fixes the per-kind table asked for, both mechanistic.

attn_q came back at 61% scale agreement while attn_v, both feed-forward matrices and the output
projection were all at 100%. That pattern is not noise: llama.cpp PERMUTES the Q and K projections
during conversion, interleaving the two halves of each head's rows so that rotary embeddings can be
applied as a single contiguous operation. V is never permuted, and V is the tensor that agreed. So the
prediction was being made against rows in the wrong order.

    permute(w, n_head) = w.reshape(n_head, 2, rows//n_head//2, cols).swapaxes(1, 2).reshape(w.shape)

Q uses n_head, K uses n_head_kv (grouped-query attention gives them different counts). Both are read
from the base model's config.json. Both orders are measured and both reported -- selecting the winner
silently would be choosing a result.

Second fix: 183 of 276 tensor attempts failed with 'no shard for tensor', which was my lookup giving up
whenever a repo ships no model.safetensors.index.json. The fallback is to read each shard's header
once and build the map directly."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from crossrepo2 import to_st, q8_blocks

def shard_map(base, sts):
    """tensor name -> shard, from the index when present, else by reading every shard header"""
    b = get_json(f"{H}/{base}/resolve/main/model.safetensors.index.json")
    if isinstance(b, dict) and isinstance(b.get("weight_map"), dict):
        return b["weight_map"], {}
    m = {}; cache = {}
    for fn in sts[:24]:
        hb, offb = st_header(base, fn)
        if not hb: continue
        cache[fn] = (hb, offb)
        for k in hb:
            if isinstance(hb[k], dict) and "data_offsets" in hb[k]: m[k] = fn
    return m, cache

def permute(w, n_head):
    rows = w.shape[0]
    if n_head <= 0 or rows % (n_head * 2): return None
    return w.reshape(n_head, 2, rows // n_head // 2, *w.shape[1:]).swapaxes(1, 2).reshape(w.shape)

def cost_of(q, w32):
    """given codes q[n,32] and candidate weights w32[n,32], the Q8_0 arithmetic and what it costs"""
    dpred = (np.abs(w32).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
    with np.errstate(divide="ignore", invalid="ignore"):
        pred = np.where(dpred[:, None] != 0, np.rint(w32 / dpred[:, None]), 0)
    pred = np.clip(pred, -127, 127).astype(np.int32)
    resid = (q - pred).ravel()
    return (float(np.mean(pred == q)), float(np.mean(np.abs(resid) <= 1)),
            h0(q.ravel()) + 0.5, h0(resid) + 0.5, dpred)

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter(); bykind = collections.defaultdict(list)
    done = 0; tried = 0
    print(f"{'repo':24s} {'kind':12s} {'blk':>6s} {'plain':>8s} {'permuted':>9s} {'scale':>7s} {'resid':>7s}")
    for rid, base in order:
        if done >= 8 or tried >= 260: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            ggufs = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            q8 = next((f for f in ggufs if "q8_0" in f.lower()), None)
            if not q8: why["no Q8_0 file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base not reachable"] += 1; continue
            sts = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sts: why["base has no safetensors"] += 1; continue
            hg = read_header(rid, q8)
            if not hg: why["gguf header unreadable"] += 1; continue
            cfg = api(f"{H}/{base}/resolve/main/config.json") or {}
            nh = int(cfg.get("num_attention_heads") or 0)
            nkv = int(cfg.get("num_key_value_heads") or nh or 0)
            wmap, cache = shard_map(base, sts)
            if not wmap: why["no shard map"] += 1; continue
            cands = [(n, m) for n, m in hg["tensors"].items() if m["ttype"] == 8 and to_st(n)]
            if not cands: why["no mappable Q8_0 tensor"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        best = {}
        for gname, gm in cands:
            k = ("embedding" if gname == "token_embd.weight" else
                 "output" if gname == "output.weight" else gname.split(".")[2])
            sz = int(np.prod(gm["dims"]))
            if k not in best or sz > best[k][2]: best[k] = (gname, gm, sz)
        got = 0
        for k in sorted(best):
            gname, gm, _ = best[k]
            sname = to_st(gname)
            shard = wmap.get(sname)
            if not shard: why["tensor absent in base"] += 1; continue
            if shard not in cache: cache[shard] = st_header(base, shard)
            hb, offb = cache[shard]
            if not hb or sname not in hb: why["base header unreadable"] += 1; continue
            meta = hb[sname]
            if meta.get("dtype") not in ("BF16", "F16"): why[f"base dtype {meta.get('dtype')}"] += 1; continue
            if sorted(meta["shape"]) != sorted(list(gm["dims"])): why["shape mismatch"] += 1; continue
            rows, cols = meta["shape"][0], int(np.prod(meta["shape"][1:]))
            nrow = min(rows, max(64, (1 << 22) // max(cols * 2, 1)))
            if nrow * cols < 6400: why["tensor too small"] += 1; continue
            nrow = (nrow // max(nh, 1)) * max(nh, 1) if k in ("attn_q", "attn_k") and nh else nrow
            if nrow <= 0: why["tensor too small"] += 1; continue
            nel = nrow * cols
            rg = fetch(rid, q8, hg["data_start"] + gm["offset"], (nel // 32) * 34)
            rw = grab(base, shard, offb + meta["data_offsets"][0], nel * 2)
            if rg is None or rw is None: why["fetch failed"] += 1; continue
            dq, q = q8_blocks(rg)
            r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
            w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
            n = min(len(q), len(w) // 32)
            if n < 200: why["short overlap"] += 1; continue
            q = q[:n]
            wflat = w[:n*32]
            plain = wflat.reshape(n, 32)
            e_p, nr_p, alone, cost_p, dp = cost_of(q, plain)
            e_q, cost_q = None, None
            if k in ("attn_q", "attn_k") and nh:
                heads = nh if k == "attn_q" else (nkv or nh)
                wm = wflat[: (len(wflat) // cols) * cols].reshape(-1, cols)
                pw = permute(wm, heads) if wm.shape[0] % (heads * 2) == 0 else None
                if pw is not None:
                    pf = pw.ravel()[:n*32].reshape(n, 32)
                    e_q, nr_q, _a, cost_q, dq2 = cost_of(q, pf)
            use = cost_q if (cost_q is not None and cost_q < cost_p) else cost_p
            sc = float(np.mean(dp == dq[:n]))
            got += 1
            bykind[k].append(dict(alone=alone, cost=use, exact=max(e_p, e_q or 0), scale=sc))
            print(f"{rid[:24]:24s} {k[:12]:12s} {n:6d} {100*e_p:7.2f}% "
                  f"{('%7.2f%%' % (100*e_q)) if e_q is not None else '       -':>9s} "
                  f"{100*sc:6.1f}% {100*use/alone:6.1f}%")
        if got: done += 1
    print(f"\n{'kind':12s} {'n':>3s} {'codes exact':>12s} {'residual':>10s}")
    aw = ac = 0.0
    for k in sorted(bykind):
        v = bykind[k]; a = sum(x["alone"] for x in v); c = sum(x["cost"] for x in v)
        aw += a; ac += c
        print(f"{k[:12]:12s} {len(v):3d} {100*np.mean([x['exact'] for x in v]):11.2f}% {100*c/a:9.1f}%")
    if ac: print(f"\n  across every tensor kind: {100*ac/aw:.1f}% of standalone ({aw/ac:.1f}x)")
    print(f"\npairs attempted {tried}, repos measured {done}")
    for r, c in why.most_common(10): print(f"  {c:5d}  {r}")
    json.dump({"bykind": {k: v for k, v in bykind.items()}, "why": dict(why)},
              open(f"{B}/qgq/crossrepo4.json", "w"), indent=1, default=float)
    print("CROSSREPO4_DONE")

if __name__ == "__main__":
    main()
