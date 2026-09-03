#!/usr/bin/env python3
"""The resolution rate again, with the name map written from the names that actually appear.

The first measurement was 27.9% by repo and 17.8% by tensor elements, and its failures were 49%
unfamiliar naming and 21% MoE fusion. Both were limits of a mapper written for llama-style names, so
that number was an instrument floor rather than a fact about the Hub. namemap.py now covers MoE fused
experts (by expert 0's presence, since the fused tensor is reachable by concatenation), DeepSeek MLA,
shared experts, vision towers and multimodal projectors.

The comparison is the point: same 140 pairs, same criterion, only the map changed. Whatever it moves is
what the map was costing."""
import json, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, api
from crossrepo4 import shard_map
from namemap import candidates
import resolverate as old

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
    why = collections.Counter()
    res_new = res_old = 0; att = 0; el_ok = el_all = 0.0; cov = []
    for rid, base in order:
        if att >= LIMIT: break
        att += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: why["no gguf file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base gone or gated"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base has no safetensors"] += 1; continue
            hg = read_header(rid, min(gg, key=len))
            if not hg or not hg.get("tensors"): why["gguf header unreadable"] += 1; continue
            wmap, _c = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        tot = hit = oldhit = 0.0
        for gname, gm in hg["tensors"].items():
            sz = int(np.prod(gm["dims"]))
            if sz < 4096: continue
            tot += sz
            if any(c in wmap for c in candidates(gname)): hit += sz
            if any(c in wmap for c in old.candidates(gname)): oldhit += sz
        if tot <= 0: why["no sizeable tensors"] += 1; continue
        cov.append(hit / tot); el_all += tot;
        if hit / tot >= 0.8: res_new += 1; el_ok += tot
        else: why[f"still unmapped ({int(100*hit/tot)//25*25}-{int(100*hit/tot)//25*25+25}%)"] += 1
        if oldhit / tot >= 0.8: res_old += 1
    print(f"pairs attempted {att}")
    print(f"  OLD map (llama-style only): resolved {res_old}  = {100*res_old/max(att,1):.1f}%")
    print(f"  NEW map (from the data):    resolved {res_new}  = {100*res_new/max(att,1):.1f}%")
    if el_all: print(f"  by tensor elements, new map: {100*el_ok/el_all:.1f}%")
    if cov:
        c = sorted(cov); q = lambda f: c[min(len(c)-1, int(f*len(c)))]
        print(f"  coverage: p10 {q(.1):.2f}  median {q(.5):.2f}  p90 {q(.9):.2f}")
    print("\nwhat still does not resolve:")
    for r, n in why.most_common(10): print(f"  {n:5d}  {r}")
    json.dump({"att": att, "old": res_old, "new": res_new, "cov": cov, "why": dict(why)},
              open(f"{B}/qgq/resolverate2.json", "w"), indent=1)
    print("RESOLVERATE2_DONE")

if __name__ == "__main__":
    main()
