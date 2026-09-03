#!/usr/bin/env python3
"""What are the pairs that do not resolve actually made of?

The resolution rate is 27.9%, and the coverage distribution is bimodal -- median 0.05, p90 1.00. Pairs
either map almost fully or almost not at all, which is not a mapper degrading gracefully but a whole
class of models it cannot express. The suspicion is mixture-of-experts: GGUF FUSES every expert into
one blk.N.ffn_gate_exps tensor while HF keeps them separate as mlp.experts.E.gate_proj, so no name map
can bridge it -- it needs a concatenation.

This reports, for the pairs that fail, which GGUF tensor names hold the elements. It decides whether
the 72% that does not resolve is fixable in principle or genuinely unavailable, which is the difference
between a floor and a ceiling in the headline."""
import json, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, api
from crossrepo4 import shard_map
from resolverate import candidates

def bucket(g):
    if "_exps" in g or "_exp." in g: return "MoE fused experts (needs concatenation)"
    if re.search(r"\.(ssm|conv1d|in_proj|out_proj|dt_|A_log|D)\b", g): return "state-space / hybrid block"
    if "norm" in g: return "norm vector"
    if g.startswith("blk.") and re.search(r"attn|ffn", g): return "attention/ffn, unrecognised naming"
    if "embd" in g or "output" in g: return "embedding or output"
    return "other: " + g.split(".")[-2] if g.count(".") >= 2 else "other"

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    miss = collections.Counter(); nfail = 0; att = 0
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 90
    for rid, base in order:
        if att >= LIMIT: break
        att += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: continue
            db = api(f"{H}/api/models/{base}")
            if not db: continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: continue
            hg = read_header(rid, min(gg, key=len))
            if not hg or not hg.get("tensors"): continue
            wmap, _c = shard_map(base, sb)
            if not wmap: continue
        except Exception:
            continue
        tot = hit = 0.0; local = collections.Counter()
        for gname, gm in hg["tensors"].items():
            sz = int(np.prod(gm["dims"]))
            if sz < 4096: continue
            tot += sz
            if any(c in wmap for c in candidates(gname)): hit += sz
            else: local[bucket(gname)] += sz
        if tot <= 0 or hit / tot >= 0.8: continue
        nfail += 1
        for k, v in local.items(): miss[k] += v / tot          # normalise so no single repo dominates
    print(f"pairs examined {att}, failing pairs analysed {nfail}\n")
    T = sum(miss.values()) or 1
    print(f"{'what the unmapped elements are':46s} {'share of unmapped':>18s}")
    for k, v in miss.most_common(10):
        print(f"{k[:46]:46s} {100*v/T:17.1f}%")
    print("\n  MoE fusion and state-space blocks are instrument limits -- the arithmetic works, the")
    print("  mapping is a concatenation this probe does not implement. Unrecognised attention/ffn")
    print("  naming is also fixable. Anything else is genuinely out of reach for now.")
    json.dump({k: v for k, v in miss.items()}, open(f"{B}/qgq/unmatched.json", "w"), indent=1, default=float)
    print("UNMATCHED_DONE")

if __name__ == "__main__":
    main()
