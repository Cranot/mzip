#!/usr/bin/env python3
"""Was the head count ever read? The permuted column was blank on every Q row, and that branch is
gated on num_attention_heads from the base's config.json. A blank is not a refutation -- it means the
test did not run. This checks the fetch itself, on the exact pairs the run used."""
import json, sys, urllib.request, os
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
H = "https://huggingface.co"
TOK = os.environ.get("HF_TOKEN", "")

tags = json.load(open("/root/mzip-hfbench/upload-mix/population_models_tags.json"))
pairs = []
for rid, t in tags.items():
    for x in (t.get("tags") or []):
        p = x.split(":")
        if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
            pairs.append((rid, ":".join(p[2:]))); break
want = [p for p in pairs if any(s in p[0] for s in ("TinyLlama", "FrenchLla", "ibm-granite"))]
print(f"pairs matching the measured repos: {len(want)}")
for rid, base in want[:6]:
    u = f"{H}/{base}/resolve/main/config.json"
    viaapi = api(u)
    raw = None
    try:
        r = urllib.request.Request(u)
        if TOK: r.add_header("Authorization", f"Bearer {TOK}")
        with urllib.request.urlopen(r, timeout=45) as f:
            raw = json.loads(f.read())
    except Exception as e:
        raw = f"ERR {type(e).__name__}: {e}"
    nh_api = (viaapi or {}).get("num_attention_heads") if isinstance(viaapi, dict) else None
    nh_raw = raw.get("num_attention_heads") if isinstance(raw, dict) else raw
    print(f"  {rid[:38]:38s} base={base[:34]:34s}")
    print(f"      api() -> {type(viaapi).__name__}, heads={nh_api}   direct -> heads={nh_raw}")
