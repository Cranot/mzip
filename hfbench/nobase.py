#!/usr/bin/env python3
"""The 16 bases that 'publish no safetensors': what ARE they?

They were counted as the genuine ceiling -- unavailable, not unmapped. But 'no safetensors' is my
filter, not the Hub's. A base that ships GGUF F16 or BF16 is a perfectly usable parent (the quant was
probably made FROM it). A base that ships only pytorch_model.bin holds the same tensors in a
different wrapper. A gated base exists but needs a token. Only a base that is truly gone or
weights-free is a ceiling. This looks at each one."""
import json, sys, collections
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import api

tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
pairs = []
for rid, t in tags.items():
    for x in (t.get("tags") or []):
        p = x.split(":")
        if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
            pairs.append((rid, ":".join(p[2:]))); break
order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
kinds = collections.Counter(); shown = 0; att = 0
print(f"{'base':44s} {'what it actually holds':40s}")
for rid, base in order[:140]:
    att += 1
    db = api(f"{H}/api/models/{base}")
    if not db:
        kinds["gone, gated, or private"] += 1
        if shown < 20: print(f"{base[:44]:44s} (api returned nothing: gone/gated/private)"); shown += 1
        continue
    sib = [s["rfilename"] for s in db.get("siblings", [])]
    if any(f.endswith(".safetensors") for f in sib): continue      # these resolve; not our subject
    exts = collections.Counter()
    for f in sib:
        l = f.lower()
        if l.endswith(".gguf"): exts["gguf"] += 1
        elif l.endswith((".bin", ".pt", ".pth", ".ckpt")): exts["pickle"] += 1
        elif l.endswith(".onnx"): exts["onnx"] += 1
        elif l.endswith((".msgpack", ".h5", ".npz")): exts["other-weights"] += 1
    ggufs = [f for f in sib if f.lower().endswith(".gguf")]
    f16 = [f for f in ggufs if any(k in f.lower() for k in ("f16", "bf16", "f32"))]
    if f16: k = "GGUF with an F16/BF16/F32 file -- USABLE parent"
    elif ggufs: k = "GGUF quantised only -- chainable via sibling Q8_0 if present"
    elif exts["pickle"]: k = "pytorch .bin/.pt only -- same tensors, different wrapper"
    elif exts["onnx"]: k = "onnx only"
    elif exts["other-weights"]: k = "flax/h5/npz only"
    elif not sib: k = "empty repo"
    else: k = "no weight files at all (card/config only)"
    kinds[k] += 1
    if shown < 20:
        print(f"{base[:44]:44s} {k[:40]:40s}  [{len(sib)} files]"); shown += 1
print(f"\npairs scanned {att}; bases without safetensors, by what they hold:")
for k, v in kinds.most_common(): print(f"  {v:3d}  {k}")
print("NOBASE_DONE")
