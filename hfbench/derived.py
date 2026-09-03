#!/usr/bin/env python3
"""How much of the week's model bytes is DERIVED from something the Hub already holds?

Every lever measured so far needs the reference in the SAME repo -- a sibling Q8_0. But the Hub
declares cross-repo lineage in its own tags: base_model:finetune:X, :merge:, :adapter:, :quantized:.
Those files are exactly the ones Xet cannot dedup: a fine-tuned weight differs from its base in the
low mantissa bits, so every chunk hash differs and dedup sees pure novelty.

This is a census, not a measurement: how many bytes carry a declared parent, by relation.
"""
import json, collections
B = "/root/mzip-hfbench"
pop = json.load(open(f"{B}/upload-mix/population_models.json"))
tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))

# pop may be a list of dicts or a dict keyed by id; normalise to id -> bytes
def rows(p):
    if isinstance(p, dict):
        for k, v in p.items():
            yield k, (v if isinstance(v, (int, float)) else (v.get("bytes") or v.get("size") or 0))
    else:
        for x in p:
            yield x.get("id") or x.get("repo"), (x.get("bytes") or x.get("size") or x.get("used_storage") or 0)

tot = 0.0
byrel = collections.Counter(); nrel = collections.Counter()
parents = collections.Counter()
tagged_bytes = 0.0; ntagged = 0; n = 0
for rid, nb in rows(pop):
    if not rid: continue
    nb = float(nb or 0); tot += nb; n += 1
    t = (tags.get(rid) or {}).get("tags") or []
    rels = set()
    for x in t:
        if not x.startswith("base_model:"): continue
        p = x.split(":")
        if len(p) >= 3:
            rels.add(p[1]); parents[":".join(p[2:])] += nb
        elif len(p) == 2:
            rels.add("declared"); parents[p[1]] += nb
    if rels:
        ntagged += 1; tagged_bytes += nb
        for r in rels: byrel[r] += nb; nrel[r] += 1

print(f"model repos in the population: {n}   bytes {tot/1e12:.1f} TB")
print(f"declaring a base_model parent: {ntagged} ({100*ntagged/max(n,1):.1f}% of repos)"
      f"   {tagged_bytes/1e12:.1f} TB ({100*tagged_bytes/max(tot,1):.1f}% of bytes)\n")
print(f"{'relation':14s} {'repos':>7s} {'TB':>9s} {'% of model bytes':>18s}")
for r, b in byrel.most_common():
    print(f"{r:14s} {nrel[r]:7d} {b/1e12:9.2f} {100*b/max(tot,1):17.1f}%")
print("\ntop declared parents by dependent bytes:")
for p, b in parents.most_common(12):
    print(f"  {b/1e12:8.2f} TB  {p}")
json.dump({"total": tot, "tagged": tagged_bytes, "byrel": dict(byrel), "nrel": dict(nrel),
           "n": n, "ntagged": ntagged, "parents": dict(parents.most_common(200))},
          open(f"{B}/upload-mix/derived.json", "w"), indent=1)
print("DERIVED_DONE")
