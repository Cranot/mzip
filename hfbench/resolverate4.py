#!/usr/bin/env python3
"""Resolution rate for the RECIPE route, which needs no tensor-name map.

RESULTs 91-92 measured how often a declared parent resolves through a hand-written GGUF->HF name map:
27.9% then 41.4%. That map exists only to serve the arithmetic route. The recipe route (RESULTs 99-106)
converts the parent with the same converter that produced the published file, so tensor names agree by
construction and the map is irrelevant. What decides resolution for a recipe is:

  1. the declared parent exists and is readable,
  2. it ships safetensors (or another container the converter reads),
  3. its config.json names an architecture convert_hf_to_gguf.py registers.

All three are header-and-config reads. No weights move. Reported by pair and, where the sample has
sizes, by GGUF bytes."""
import json, re, sys, collections
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from hfget import get_json
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def converter_registry():
    src = open("/root/llama.cpp/convert_hf_to_gguf.py").read()
    try:
        src += open("/root/llama.cpp/conversion/base.py").read()
    except OSError:
        pass
    names = set()
    for m in re.finditer(r"@ModelBase\.register\(([^)]*)\)", src):
        for s in re.findall(r"\"([A-Za-z0-9_]+)\"", m.group(1)):
            names.add(s)
    return names

def main():
    reg = converter_registry()
    print(f"converter registers {len(reg)} HF architecture classes")
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    rows = {json.loads(l)["id"]: json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()}
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 140
    why = collections.Counter(); ok = 0; att = 0; ok_b = all_b = 0.0
    unknown_arch = collections.Counter()
    for rid, base in order[:LIMIT]:
        att += 1
        gb = sum(float(v or 0) for k, v in ((rows.get(rid) or {}).get("bytes_in_window") or {}).items() if k.startswith("gguf"))
        all_b += gb
        db = api(f"{H}/api/models/{base}")
        if not db: why["parent gone, gated or private"] += 1; continue
        sib = [s["rfilename"] for s in db.get("siblings", [])]
        has_st = any(f.endswith(".safetensors") for f in sib)
        has_pt = any(f.endswith((".bin", ".pt", ".pth")) for f in sib)
        if not (has_st or has_pt): why["parent has no weight files"] += 1; continue
        cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
        archs = cfg.get("architectures") or []
        if not archs: why["parent config.json missing or has no architectures"] += 1; continue
        if not any(a in reg for a in archs):
            why["architecture not in converter registry"] += 1; unknown_arch[archs[0]] += 1; continue
        if not has_st: why["pytorch .bin only (converter reads it; slower path)"] += 0
        ok += 1; ok_b += gb
    print(f"\ndeclared-parent pairs attempted {att}")
    print(f"  RESOLVABLE BY RECIPE: {ok} = {100*ok/max(att,1):.1f}% by pair"
          + (f", {100*ok_b/all_b:.1f}% by GGUF bytes" if all_b else ""))
    print(f"  (name-map route, RESULT 92: 41.4% by pair)")
    print("\nwhy the rest do not resolve:")
    for r, n in why.most_common():
        if n: print(f"  {n:4d}  {r}")
    if unknown_arch:
        print("  architectures the converter does not register:", dict(unknown_arch.most_common(8)))
    json.dump(dict(att=att, ok=ok, ok_b=ok_b, all_b=all_b, why=dict(why), unknown=dict(unknown_arch)),
              open(f"{B}/qgq/resolverate4.json", "w"), indent=1)
    print("RESOLVERATE4_DONE")

if __name__ == "__main__":
    main()
