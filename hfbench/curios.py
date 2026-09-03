#!/usr/bin/env python3
"""Four cheap curiosities.

  1. Does RESULT 115's joint-alphabet gain hold on F32? (exponent byte + top mantissa byte as one symbol)
  2. What is the 2.00x outlier in RESULT 116 -- sparsity? -- and how common is sparsity in the 24 tensors.
  3. Embedding tables: Fano cost and progressive split, which every layering result skipped.
  4. Embedding share of model bytes, from safetensors headers, so (3) carries a weight.
"""
import json, sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def get(rid, want=None, nb=4 << 20):
    d = api(f"{H}/api/models/{rid}")
    sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    wmap, cache = shard_map(rid, sts)
    for fn in sts:
        if fn not in cache: cache[fn] = st_header(rid, fn)
    k = want(wmap) if want else None
    if not k: return None, None, None, wmap, cache
    fn = wmap[k]; hb, off = cache[fn]; m = hb[k]
    take = min(nb, m["data_offsets"][1] - m["data_offsets"][0])
    raw = grab(rid, fn, off + m["data_offsets"][0], take)
    return k, m, (np.frombuffer(raw.tobytes(), dtype=np.uint8) if raw is not None else None), wmap, cache

def main():
    print("=== 1. joint alphabet on F32 ===")
    for rid in ("openai-community/gpt2", "sentence-transformers/all-MiniLM-L6-v2"):
        try:
            k, m, b, _, _ = get(rid, lambda w: next(k for k in sorted(w) if k.endswith(".weight") and len(k.split(".")) > 3 and "embed" not in k and "norm" not in k and "bias" not in k))
        except Exception as e:
            print(f"  {rid}: {e}"); continue
        if b is None or m["dtype"] != "F32": print(f"  {rid}: not F32 ({m and m['dtype']})"); continue
        u = b.view(np.uint32)
        top8 = (u >> 24).astype(np.uint16); top16 = (u >> 16).astype(np.int64); m2 = ((u >> 16) & 0xFF).astype(np.uint16)
        planes = h0(top8) + h0(m2) + 16                       # Fano-style: two coded planes + two raw bytes
        joint = h0(top16) + 16
        print(f"  {rid[:34]:34s} {k.split('.')[-2][:10]:10s} planes {planes:6.2f}  joint16 {joint:6.2f}  gain {100*(1-joint/planes):4.1f}%  (raw 32)")
    print("\n=== 2. sparsity across the RESULT 116 tensors ===")
    rows = json.load(open(f"{B}/qgq/fanospot.json"))
    zs = []
    for r in rows:
        rid, k = r["rid"], r["k"]
        try:
            _, m, b, _, _ = get(rid, lambda w, k=k: k if k in w else None, nb=1 << 20)
        except Exception:
            continue
        if b is None: continue
        u = b.view(np.uint16); z = float(np.mean((u & 0x7FFF) == 0))
        zs.append((z, r["cost"], rid))
    zs.sort(reverse=True)
    print(f"  tensors {len(zs)}; zero fraction: max {zs[0][0]*100:.1f}%  median {np.median([z for z,_,_ in zs])*100:.3f}%  >1%: {sum(1 for z,_,_ in zs if z > 0.01)}")
    for z, c, rid in zs[:3]: print(f"    {100*z:5.1f}% zeros  cost {c:5.2f}  {rid[:40]}")
    print("\n=== 3./4. embeddings ===")
    print(f"  {'model':28s} {'embed cost':>10s} {'proj cost':>9s} {'12/4 L1':>8s} {'joint SUM':>9s} {'embed share of model bytes':>27s}")
    for rid in ("Qwen/Qwen2.5-0.5B", "HuggingFaceTB/SmolLM2-360M", "Qwen/Qwen2.5-7B"):
        try:
            k, m, b, wmap, cache = get(rid, lambda w: next(k for k in sorted(w) if "embed_tokens" in k))
        except Exception as e:
            print(f"  {rid}: {e}"); continue
        if b is None: continue
        u = b.view(np.uint16)
        e_cost = h0((u >> 8).astype(np.uint16)) + 8.0
        l1 = h0((u >> 4).astype(np.int64)); joint = l1 + 4
        kp, mp, bp, _, _ = get(rid, lambda w: next(k for k in sorted(w) if "layers.0.mlp.down_proj" in k))
        p_cost = h0((bp.view(np.uint16) >> 8).astype(np.uint16)) + 8.0 if bp is not None else float("nan")
        tot = 0; emb = 0
        for fn, (hb, _o) in cache.items():
            if not hb: continue
            for kk, v in hb.items():
                if not isinstance(v, dict) or "data_offsets" not in v: continue
                sz = v["data_offsets"][1] - v["data_offsets"][0]; tot += sz
                if "embed_tokens" in kk or "lm_head" in kk: emb += sz
        print(f"  {rid[:28]:28s} {e_cost:10.2f} {p_cost:9.2f} {l1:8.2f} {joint:9.2f} {100*emb/max(tot,1):26.1f}%")
    print("CURIOS_DONE")

if __name__ == "__main__":
    main()
