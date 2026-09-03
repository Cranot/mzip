#!/usr/bin/env python3
"""Print both sides' real tensor names for pairs that fail to resolve.

Three predictions about content have missed today, every one of them inferred from a proxy instead of
read from the data. The name map is the last thing standing between a measured 17% and a measured 21%,
so it gets written from evidence: for each failing pair, the GGUF names that did not map and the base's
own names at the same layer, printed side by side. Whatever pattern is there will be visible."""
import json, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, api
from crossrepo4 import shard_map
from resolverate import candidates

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    shown = 0; att = 0
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    for rid, base in order:
        if shown >= 7 or att >= LIMIT: break
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
        tot = hit = 0.0; unmapped = []
        for gname, gm in hg["tensors"].items():
            sz = int(np.prod(gm["dims"]))
            if sz < 4096: continue
            tot += sz
            if any(c in wmap for c in candidates(gname)): hit += sz
            else: unmapped.append((gname, sz))
        if tot <= 0 or hit / tot >= 0.8: continue
        shown += 1
        unmapped.sort(key=lambda x: -x[1])
        cfg = None
        print(f"\n=== {rid}  <-  {base}   ({100*hit/tot:.0f}% mapped)")
        print(f"  GGUF names that did not map (largest first):")
        for nm, sz in unmapped[:8]:
            print(f"    {nm:44s} {sz/1e6:9.2f} M elements")
        # what the base calls its layer-0 and layer-1 tensors
        lay = [k for k in wmap if re.search(r"(^|\.)(layers|h|blocks)\.[01]\.", k)]
        lay.sort()
        print(f"  base tensor names at layers 0-1 ({len(wmap)} tensors total):")
        for k in lay[:12]:
            print(f"    {k}")
        if not lay:
            other = sorted(wmap)[:10]
            print("    (no layer-indexed names; first few keys:)")
            for k in other: print(f"    {k}")
    print("\nNAMEDUMP_DONE")

if __name__ == "__main__":
    main()
