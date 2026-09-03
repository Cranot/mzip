#!/usr/bin/env python3
"""Why did --find return nothing? Print what the GGUF repos actually list instead of guessing."""
import json, sys, collections
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import api
tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
cands = [r for r in tags if "gguf" in r.lower()]
cands.sort(key=lambda r: (0 if r.startswith(("bartowski/", "mradermacher/", "unsloth/", "QuantFactory/", "lmstudio-community/")) else 1, r))
why = collections.Counter(); shown = 0
for rid in cands[:120]:
    d = api(f"{H}/api/models/{rid}")
    if not d: why["api none"] += 1; continue
    sib = {s["rfilename"]: (s.get("size") or (s.get("lfs") or {}).get("size") or 0) for s in d.get("siblings", [])}
    gg = [f for f in sib if f.lower().endswith(".gguf")]
    lo = [f.lower() for f in gg]
    f16 = [f for f in gg if any(k in f.lower() for k in ("f16", "fp16", "bf16", "f32", "fp32"))]
    q4 = [f for f in gg if "q4_k_m" in f.lower()]
    im = [f for f in sib if "imatrix" in f.lower()]
    if not gg: why["no gguf files"] += 1; continue
    if not q4: why["no q4_k_m"] += 1
    if not f16: why["no f16/bf16/f32 gguf"] += 1
    if f16 and q4:
        sz = max(sib[f] for f in f16)
        if sz > 4.5e9: why[f"f16 too big (>4.5GB)"] += 1
        elif sz < 200e6: why["f16 too small"] += 1
        else: why["OK candidate"] += 1
    if shown < 12:
        shown += 1
        print(f"{rid[:50]:50s} gguf={len(gg):2d} f16={len(f16)} q4km={len(q4)} imatrix={len(im)} "
              f"maxf16={max([sib[f] for f in f16], default=0)/1e9:.1f}GB  e.g. {gg[0][:40] if gg else ''}")
print("\nwhy (per repo, non-exclusive):")
for k, v in why.most_common(): print(f"  {v:4d}  {k}")
print("FINDDIAG_DONE")
