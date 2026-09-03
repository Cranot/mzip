#!/usr/bin/env python3
"""Cross-repo lineage: how much of the week is DERIVED from something the Hub already holds?

The population file carries no sizes, so this joins the byte-carrying sample against the Hub's own
base_model tags. Those tags are the Hub telling us, in its own metadata, which uploads have a parent.
Files with a parent are precisely the ones Xet cannot dedup -- a fine-tuned weight differs from its
base in the low mantissa bits, so every chunk hash differs and dedup sees pure novelty."""
import json, collections
B = "/root/mzip-hfbench"
tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
WEIGHT = ("safetensors", "gguf", "torch-pickle", "other-weights")

tot = wtot = 0.0
byrel = collections.Counter(); wbyrel = collections.Counter(); nrel = collections.Counter()
n = ntag = 0
for r in rows:
    c = r.get("bytes_in_window") or {}
    nb = sum(float(v or 0) for v in c.values())
    wb = sum(float(v or 0) for k, v in c.items() if k.split(":")[0] in WEIGHT)
    tot += nb; wtot += wb; n += 1
    rels = set()
    for x in ((tags.get(r["id"]) or {}).get("tags") or []):
        if x.startswith("base_model:"):
            p = x.split(":")
            rels.add(p[1] if len(p) >= 3 else "declared")
    if rels:
        ntag += 1
        for rl in rels:
            byrel[rl] += nb; wbyrel[rl] += wb; nrel[rl] += 1

print(f"sampled model repos {n}, bytes {tot/1e12:.3f} TB, weight bytes {wtot/1e12:.3f} TB")
print(f"declaring a parent: {ntag} repos ({100*ntag/max(n,1):.1f}%)\n")
print(f"{'relation':12s} {'repos':>6s} {'% all bytes':>12s} {'% weight bytes':>15s}")
for rl, b in byrel.most_common():
    print(f"{rl:12s} {nrel[rl]:6d} {100*b/max(tot,1):11.1f}% {100*wbyrel[rl]/max(wtot,1):14.1f}%")
un = sum(wbyrel[k] for k in ("finetune", "merge", "adapter") if k in wbyrel)
print(f"\n  finetune+merge+adapter, unexploited by every lever so far: "
      f"{100*un/max(wtot,1):.1f}% of weight bytes")
json.dump({"tot": tot, "wtot": wtot, "byrel": dict(byrel), "wbyrel": dict(wbyrel),
           "nrel": dict(nrel), "n": n, "ntag": ntag},
          open(f"{B}/upload-mix/derived2.json", "w"), indent=1)
print("DERIVED2_DONE")
