#!/usr/bin/env python3
"""Measure the resolution rate directly -- it is the only thing the headline still hinges on.

The saving ranges from 17.0% to 22.9% depending on how many declared parents actually resolve to a
public base whose tensors can be matched. My floor of 30% came from runs whose dominant failure was
'tensor absent in base', and that is a limit of a name mapper written for llama-style names only, not
a fact about the Hub. Measuring the rate needs no prediction and no weight bytes at all -- only
headers: does the base exist, does it ship safetensors, and do the GGUF's tensors map onto it.

The mapper is widened to the naming schemes actually in use, and every failure is counted by reason so
the residual stays visible instead of collapsing into a number."""
import json, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, api
from fp8_given_base import header as st_header
from crossrepo4 import shard_map
from hfget import get_json

SUF = {"attn_q": ["self_attn.q_proj", "attention.wq", "attn.q_proj"],
       "attn_k": ["self_attn.k_proj", "attention.wk", "attn.k_proj"],
       "attn_v": ["self_attn.v_proj", "attention.wv", "attn.v_proj"],
       "attn_output": ["self_attn.o_proj", "attention.wo", "attn.o_proj", "self_attn.dense"],
       "ffn_gate": ["mlp.gate_proj", "feed_forward.w1", "mlp.w1"],
       "ffn_up": ["mlp.up_proj", "feed_forward.w3", "mlp.w3", "mlp.fc1"],
       "ffn_down": ["mlp.down_proj", "feed_forward.w2", "mlp.w2", "mlp.fc2"]}
PRE = ["model.layers.{n}.", "layers.{n}.", "transformer.h.{n}.", "model.decoder.layers.{n}.",
       "gpt_neox.layers.{n}.", "transformer.blocks.{n}."]

def candidates(g):
    if g == "token_embd.weight":
        return ["model.embed_tokens.weight", "embed_tokens.weight", "transformer.wte.weight",
                "model.embedding.weight", "tok_embeddings.weight"]
    if g == "output.weight":
        return ["lm_head.weight", "output.weight", "model.lm_head.weight"]
    m = re.match(r"blk\.(\d+)\.([a-z_0-9]+)\.weight$", g)
    if not m: return []
    n, part = m.group(1), m.group(2)
    if part not in SUF: return []
    return [p.format(n=n) + s + ".weight" for p in PRE for s in SUF[part]]

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter()
    resolved = 0; attempted = 0; rb_ok = 0.0; rb_all = 0.0
    cover = []
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    for rid, base in order:
        if attempted >= LIMIT: break
        attempted += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: why["no gguf file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base gone or gated"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base has no safetensors"] += 1; continue
            tf = min(gg, key=lambda f: len(f))
            hg = read_header(rid, tf)
            if not hg or not hg.get("tensors"): why["gguf header unreadable"] += 1; continue
            wmap, cache = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        tot = hit = 0.0
        for gname, gm in hg["tensors"].items():
            sz = int(np.prod(gm["dims"]))
            if sz < 4096: continue
            tot += sz
            for c in candidates(gname):
                if c in wmap: hit += sz; break
        if tot <= 0: why["no sizeable tensors"] += 1; continue
        frac = hit / tot
        cover.append(frac)
        rb_all += tot
        if frac >= 0.8:
            resolved += 1; rb_ok += tot
        else:
            why[f"tensor names unmatched ({int(100*frac)//25*25}-{int(100*frac)//25*25+25}% matched)"] += 1
    print(f"declared quantized-of pairs attempted: {attempted}")
    print(f"  RESOLVED (>=80% of GGUF tensor elements map onto the base): {resolved}"
          f"  = {100*resolved/max(attempted,1):.1f}% by repo")
    if rb_all: print(f"  by tensor elements: {100*rb_ok/rb_all:.1f}%")
    if cover:
        c = sorted(cover)
        q = lambda f: c[min(len(c)-1, int(f*len(c)))]
        print(f"  coverage distribution over pairs that got as far as a header: "
              f"p10 {q(.1):.2f}  median {q(.5):.2f}  p90 {q(.9):.2f}")
    print("\nwhy the rest did not resolve:")
    for r, n in why.most_common(12): print(f"  {n:5d}  {r}")
    json.dump({"attempted": attempted, "resolved": resolved, "cover": cover, "why": dict(why)},
              open(f"{B}/qgq/resolverate.json", "w"), indent=1)
    print("RESOLVERATE_DONE")

if __name__ == "__main__":
    main()
