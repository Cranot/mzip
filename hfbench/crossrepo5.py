#!/usr/bin/env python3
"""Q and K, done correctly: a permuted tensor cannot be compared on a prefix.

llama.cpp interleaves the two halves of every head's rows when it converts Q and K. That mapping is
defined over the WHOLE tensor, so the GGUF's first N rows correspond to a scattered set of base rows,
not to the base's first N. Comparing prefixes gave 61% in one sample and 3.9% in another -- both
artefacts of partial overlap, neither a measurement. Fetching the whole tensor makes the comparison
well-posed, and the head counts come from the base model's own config.json.

Both orders are computed and both reported. Selecting the better one silently would be choosing a
result rather than measuring one."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import to_st, q8_blocks
from crossrepo4 import shard_map, permute, cost_of
from hfget import get_json

MAXB = 48 << 20

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter(); bykind = collections.defaultdict(list); done = tried = 0
    print(f"{'repo':22s} {'kind':12s} {'rows':>7s} {'as-is':>8s} {'permuted':>9s} {'resid':>7s}")
    for rid, base in order:
        if done >= 8 or tried >= 260: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            q8 = next((f for f in gg if "q8_0" in f.lower()), None)
            if not q8: why["no Q8_0 file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base not reachable"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base has no safetensors"] += 1; continue
            hg = read_header(rid, q8)
            if not hg: why["gguf header unreadable"] += 1; continue
            cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
            nh = int(cfg.get("num_attention_heads") or 0)
            nkv = int(cfg.get("num_key_value_heads") or nh or 0)
            wmap, cache = shard_map(base, sb)
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
            # for q/k we must take the WHOLE tensor, so prefer the SMALLEST of that kind
            if k in ("attn_q", "attn_k"):
                if k not in best or sz < best[k][2]: best[k] = (gname, gm, sz)
            elif k not in best or sz > best[k][2]: best[k] = (gname, gm, sz)
        got = 0
        for k in sorted(best):
            gname, gm, _ = best[k]
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
            if whole:
                if rows * cols * 2 > MAXB: why["q/k tensor too large to take whole"] += 1; continue
                nrow = rows
            else:
                nrow = min(rows, max(64, (1 << 22) // max(cols * 2, 1)))
            nel = nrow * cols
            if nel < 6400: why["tensor too small"] += 1; continue
            rg = fetch(rid, q8, hg["data_start"] + gm["offset"], (nel // 32) * 34)
            rw = grab(base, shard, offb + meta["data_offsets"][0], nel * 2)
            if rg is None or rw is None: why["fetch failed"] += 1; continue
            dq, q = q8_blocks(rg)
            r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
            w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
            n = min(len(q), len(w) // 32)
            if n < 200: why["short overlap"] += 1; continue
            q = q[:n]; flat = w[:n*32]
            e_p, _nr, alone, cost_p, _d = cost_of(q, flat.reshape(n, 32))
            e_q = cost_q = None
            if whole and nh:
                heads = nh if k == "attn_q" else (nkv or nh)
                wm = w[:rows*cols].reshape(rows, cols)
                pw = permute(wm, heads)
                if pw is not None:
                    pf = pw.ravel()[:n*32].reshape(n, 32)
                    e_q, _n2, _a2, cost_q, _d2 = cost_of(q, pf)
            use = cost_q if (cost_q is not None and cost_q < cost_p) else cost_p
            got += 1
            bykind[k].append(dict(alone=alone, cost=use, exact=max(e_p, e_q or 0.0),
                                  permuted_won=bool(cost_q is not None and cost_q < cost_p)))
            print(f"{rid[:22]:22s} {k[:12]:12s} {nrow:7d} {100*e_p:7.2f}% "
                  f"{('%8.2f%%' % (100*e_q)) if e_q is not None else '        -':>9s} {100*use/alone:6.1f}%")
        if got: done += 1
    print(f"\n{'kind':12s} {'n':>3s} {'codes exact':>12s} {'residual':>10s} {'permuted won':>13s}")
    aw = ac = 0.0
    for k in sorted(bykind):
        v = bykind[k]; a = sum(x["alone"] for x in v); c = sum(x["cost"] for x in v); aw += a; ac += c
        pw = sum(1 for x in v if x["permuted_won"])
        print(f"{k[:12]:12s} {len(v):3d} {100*np.mean([x['exact'] for x in v]):11.2f}% "
              f"{100*c/a:9.1f}% {pw:>9d}/{len(v)}")
    if ac: print(f"\n  across every tensor kind: {100*ac/aw:.1f}% of standalone ({aw/ac:.1f}x)")
    print(f"\npairs attempted {tried}, repos measured {done}")
    for r, c in why.most_common(10): print(f"  {c:5d}  {r}")
    json.dump({"bykind": {k: v for k, v in bykind.items()}, "why": dict(why)},
              open(f"{B}/qgq/crossrepo5.json", "w"), indent=1, default=float)
    print("CROSSREPO5_DONE")

if __name__ == "__main__":
    main()
