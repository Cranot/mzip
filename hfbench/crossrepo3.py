#!/usr/bin/env python3
"""Two checks the cross-repo result needs before it is a claim.

ONE: every row in that table measured output.weight, because the selector sorts tensors by size and
the output projection is the largest. That is the identical fault that made the first per-type quant
run print three identical rows this morning. This measures one tensor of EACH kind -- attention
projections, the three feed-forward matrices, the embedding -- so the rate is not a property of one
matrix.

TWO: 74 pairs were attempted and 6 measured. If the other 68 failed for reasons that would also apply
across the Hub, the population this lever reaches is much smaller than the 27.7% of weight bytes that
declare a quantized parent. Unexplained failures are not a benign default, so each one is counted by
reason."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import to_st, index_map, q8_blocks

def measure(rid, q8, hg, base, wmap, sts, gname, gm, hdr_cache):
    sname = to_st(gname)
    shard = wmap.get(sname) or (sts[0] if len(sts) == 1 else None)
    if not shard: return None, "no shard for tensor"
    if shard not in hdr_cache: hdr_cache[shard] = st_header(base, shard)
    hb, offb = hdr_cache[shard]
    if not hb: return None, "base header unreadable"
    if sname not in hb: return None, "tensor absent in base"
    meta = hb[sname]
    if meta.get("dtype") not in ("BF16", "F16"): return None, f"base dtype {meta.get('dtype')}"
    if sorted(meta["shape"]) != sorted(list(gm["dims"])): return None, "shape mismatch"
    nel = int(np.prod(meta["shape"])); nb = min(12000, nel // 32)
    if nb < 200: return None, "tensor too small"
    rg = fetch(rid, q8, hg["data_start"] + gm["offset"], nb * 34)
    rw = grab(base, shard, offb + meta["data_offsets"][0], nb * 32 * 2)
    if rg is None or rw is None: return None, "fetch failed"
    dq, q = q8_blocks(rg)
    r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
    w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
    n = min(len(q), len(w) // 32)
    if n < 200: return None, "short overlap"
    q = q[:n]; w = w[:n*32].reshape(n, 32)
    dpred = (np.abs(w).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
    with np.errstate(divide="ignore", invalid="ignore"):
        pred = np.where(dpred[:, None] != 0, np.rint(w / dpred[:, None]), 0)
    pred = np.clip(pred, -127, 127).astype(np.int32)
    resid = (q - pred).ravel()
    alone = h0(q.ravel()) + 0.5; cost = h0(resid) + 0.5
    return dict(scale=float(np.mean(dpred == dq[:n])), exact=float(np.mean(pred == q)),
                near=float(np.mean(np.abs(resid) <= 1)), alone=alone, cost=cost, n=n), None

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    gg = [p for p in pairs if "gguf" in p[0].lower()]
    order = gg + [p for p in pairs if p not in gg]
    why = collections.Counter(); bykind = collections.defaultdict(list)
    done = 0; tried = 0
    print(f"{'repo':26s} {'tensor kind':14s} {'blocks':>7s} {'scale':>7s} {'codes':>8s} {'|r|<=1':>8s} {'resid':>7s}")
    for rid, base in order:
        if done >= 5 or tried >= 220: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            ggufs = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not ggufs: why["no gguf in repo"] += 1; continue
            q8 = next((f for f in ggufs if "q8_0" in f.lower()), None)
            if not q8: why["no Q8_0 file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base not reachable"] += 1; continue
            sts = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sts: why["base has no safetensors"] += 1; continue
            hg = read_header(rid, q8)
            if not hg: why["gguf header unreadable"] += 1; continue
            wmap = index_map(base) or {}
            cands = [(n, m) for n, m in hg["tensors"].items() if m["ttype"] == 8 and to_st(n)]
            if not cands: why["no mappable Q8_0 tensor"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        # one tensor of each kind, largest of that kind
        best = {}
        for gname, gm in cands:
            k = ("embedding" if gname == "token_embd.weight" else
                 "output" if gname == "output.weight" else gname.split(".")[2])
            sz = int(np.prod(gm["dims"]))
            if k not in best or sz > best[k][2]: best[k] = (gname, gm, sz)
        hdr_cache = {}; got = 0
        for k in sorted(best):
            gname, gm, _ = best[k]
            try: r, err = measure(rid, q8, hg, base, wmap, sts, gname, gm, hdr_cache)
            except Exception: r, err = None, "exception"
            if not r:
                why[err or "unknown"] += 1; continue
            got += 1; bykind[k].append(r)
            print(f"{rid[:26]:26s} {k[:14]:14s} {r['n']:7d} {100*r['scale']:6.1f}% "
                  f"{100*r['exact']:7.2f}% {100*r['near']:7.2f}% {100*r['cost']/r['alone']:6.1f}%")
        if got: done += 1
    print(f"\n{'tensor kind':14s} {'n':>4s} {'scale':>8s} {'codes exact':>13s} {'|r|<=1':>9s} {'residual':>9s}")
    aw = ac = 0.0
    for k in sorted(bykind):
        v = bykind[k]
        s = 100*np.mean([x["scale"] for x in v]); e = 100*np.mean([x["exact"] for x in v])
        nr = 100*np.mean([x["near"] for x in v])
        a = sum(x["alone"] for x in v); c = sum(x["cost"] for x in v); aw += a; ac += c
        print(f"{k[:14]:14s} {len(v):4d} {s:7.1f}% {e:12.2f}% {nr:8.2f}% {100*c/a:8.1f}%")
    if ac: print(f"\n  across every tensor kind: {100*ac/aw:.1f}% of standalone ({aw/ac:.1f}x)")
    print(f"\npairs attempted {tried}, repos measured {done}")
    print("why the rest produced nothing:")
    for r, c in why.most_common():
        print(f"  {c:5d}  {r}")
    json.dump({"bykind": {k: v for k, v in bykind.items()}, "why": dict(why), "tried": tried},
              open(f"{B}/qgq/crossrepo3.json", "w"), indent=1, default=float)
    print("CROSSREPO3_DONE")

if __name__ == "__main__":
    main()
