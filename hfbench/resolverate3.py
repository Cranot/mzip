#!/usr/bin/env python3
"""Resolution rate, counting CORRECT parents only.

RESULT 92 counts a declared parent as resolved when its tensor names map (41.4% binary). RESULT 105
showed a parent can map perfectly and be WRONG: PocketWeights' declared base had every F32 norm
differing, because the GGUF is a base+instruct merge. Norms are exact upcasts of the source's bf16,
so one norm tensor decides in one request: identical -> the parent is the parent; differing -> it is
not, and the recipe would store the file whole.

Same 140 pairs as RESULT 92, one GGUF F32 norm tensor against the mapped base tensor upcast to f32."""
import json, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map
from namemap import candidates

def norm_candidates(g):
    import re
    m = re.match(r"blk\.(\d+)\.(attn_norm|ffn_norm)\.weight$", g)
    if not m: return []
    n, part = m.group(1), m.group(2)
    suf = {"attn_norm": ["input_layernorm", "attention_norm", "ln_1", "pre_attention_layernorm"],
           "ffn_norm": ["post_attention_layernorm", "ffn_norm", "ln_2", "pre_mlp_layernorm"]}[part]
    return [f"{p}{s}.weight" for p in ("model.layers.{n}.", "layers.{n}.", "transformer.h.{n}.", "model.language_model.layers.{n}.")
            for s in suf for p in [p.format(n=n)]]

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 140
    why = collections.Counter(); att = 0; resolved = 0; correct = 0; wrong = 0; unknown = 0
    print(f"{'repo':40s} {'norm tensor':22s} {'max rel err':>12s} {'verdict':>10s}")
    for rid, base in order[:LIMIT]:
        att += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: why["no gguf"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base gone"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base no safetensors"] += 1; continue
            hg = read_header(rid, min(gg, key=len))
            if not hg or not hg.get("tensors"): why["gguf header"] += 1; continue
            wmap, cache = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        # resolution as in RESULT 92
        tot = hit = 0.0
        for g, m in hg["tensors"].items():
            sz = int(np.prod(m["dims"]))
            if sz < 4096: continue
            tot += sz
            if any(c in wmap for c in candidates(g)): hit += sz
        if tot <= 0 or hit / tot < 0.8: continue
        resolved += 1
        # the norm check
        verdict = None
        for g, m in hg["tensors"].items():
            if m["ttype"] != 0: continue
            cs = norm_candidates(g)
            sn = next((c for c in cs if c in wmap), None)
            if not sn: continue
            shard = wmap[sn]
            if shard not in cache: cache[shard] = st_header(base, shard)
            hb, off = cache[shard]
            if not hb or sn not in hb: continue
            meta = hb[sn]
            n = int(np.prod(meta["shape"]))
            if n != int(np.prod(m["dims"])) or n > 65536: continue
            rg = fetch(rid, min(gg, key=len), hg["data_start"] + m["offset"], n * 4)
            rb = grab(base, shard, off + meta["data_offsets"][0], n * (2 if meta["dtype"] in ("BF16", "F16") else 4))
            if rg is None or rb is None: continue
            va = np.frombuffer(rg.tobytes(), dtype=np.float32).astype(np.float64)
            r = np.frombuffer(rb.tobytes(), dtype=np.uint16) if meta["dtype"] in ("BF16", "F16") else np.frombuffer(rb.tobytes(), dtype=np.float32)
            vb = (bf16_to_f32(r) if meta["dtype"] == "BF16" else r.view(np.float16).astype(np.float32) if meta["dtype"] == "F16" else r).astype(np.float64)
            k = min(va.size, vb.size)
            # llama.cpp stores Gemma-family norms as (w + 1); a raw compare flags every Gemma parent
            # WRONG with a relative error near 1e5 (base value ~0, GGUF value ~1). Test both readings.
            rel_raw = float(np.max(np.abs(va[:k] - vb[:k]) / np.maximum(np.abs(vb[:k]), 1e-6)))
            rel_p1 = float(np.max(np.abs(va[:k] - (vb[:k] + 1.0)) / np.maximum(np.abs(vb[:k] + 1.0), 1e-6)))
            rel = min(rel_raw, rel_p1)
            verdict = "CORRECT" if rel < 1e-5 else ("WRONG" if rel > 1e-2 else "near")
            print(f"{rid[:40]:40s} {g[:22]:22s} {rel:12.2e} {verdict:>10s}")
            break
        if verdict == "CORRECT": correct += 1
        elif verdict == "WRONG": wrong += 1
        else: unknown += 1
    print(f"\npairs {att}; resolved by names {resolved} ({100*resolved/max(att,1):.1f}%)")
    print(f"  of which parent CORRECT by norm check: {correct}  ({100*correct/max(att,1):.1f}% of pairs)")
    print(f"  parent WRONG (norms differ):           {wrong}")
    print(f"  no norm comparable / near:             {unknown}")
    json.dump(dict(att=att, resolved=resolved, correct=correct, wrong=wrong, unknown=unknown, why=dict(why)),
              open(f"{B}/qgq/resolverate3.json", "w"), indent=1)
    print("RESOLVERATE3_DONE")

if __name__ == "__main__":
    main()
