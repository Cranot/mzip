#!/usr/bin/env python3
"""What does the Hub's model volume actually look like by format/dtype?

Every weights measurement so far is F32 and BF16 safetensors. If GGUF, FP8 or
INT4 quantized files carry a large share of bytes, the sample is unrepresentative
and the extrapolation is wrong. Survey the top-downloaded models' file lists.
"""
import json, subprocess, collections, re

def api(url):
    r = subprocess.run(["curl","-sS","--max-time","40",url], capture_output=True, text=True)
    try: return json.loads(r.stdout)
    except Exception: return None

models = api("https://huggingface.co/api/models?sort=downloads&direction=-1&limit=100") or []
fmt_bytes = collections.Counter(); fmt_files = collections.Counter()
dtype_bytes = collections.Counter(); gguf_quants = collections.Counter()
n_ok = 0
for m in models:
    mid = m["id"]
    tree = api(f"https://huggingface.co/api/models/{mid}/tree/main?recursive=true&expand=true")
    if not isinstance(tree, list): continue
    n_ok += 1
    cfg = api(f"https://huggingface.co/{mid}/resolve/main/config.json")
    td = (cfg or {}).get("torch_dtype") or (cfg or {}).get("dtype") or "?"
    for x in tree:
        if x.get("type") != "file": continue
        p = x["path"]; s = (x.get("lfs") or {}).get("size") or x.get("size", 0)
        ext = p.rsplit(".",1)[-1].lower() if "." in p else "(none)"
        if ext in ("safetensors","bin","gguf","pt","pth","onnx","msgpack","h5","ckpt"):
            fmt_bytes[ext] += s; fmt_files[ext] += 1
            if ext == "safetensors": dtype_bytes[td] += s
            if ext == "gguf":
                q = re.search(r"[-_.](Q\d[_A-Za-z0-9]*|F16|F32|BF16|IQ\d\S*)", p, re.I)
                gguf_quants[q.group(1).upper() if q else "?"] += s

tot = sum(fmt_bytes.values())
print(f"surveyed {n_ok} of {len(models)} top-downloaded models; {tot/1e9:.1f} GB of weight files\n")
print("BYTES BY FORMAT:")
for k,v in fmt_bytes.most_common():
    print(f"  {k:12s} {v/1e9:9.1f} GB  {100*v/tot:5.1f}%   ({fmt_files[k]} files)")
st = sum(dtype_bytes.values())
print("\nSAFETENSORS BYTES BY torch_dtype (from config.json):")
for k,v in dtype_bytes.most_common():
    print(f"  {k:12s} {v/1e9:9.1f} GB  {100*v/st if st else 0:5.1f}%")
if gguf_quants:
    gt = sum(gguf_quants.values())
    print("\nGGUF BYTES BY QUANT:")
    for k,v in gguf_quants.most_common(12):
        print(f"  {k:12s} {v/1e9:9.1f} GB  {100*v/gt:5.1f}%")
print("\nNOTE: top-100 by download COUNT, not by bytes; a byte-weighted census of the")
print("whole Hub is not available via this API. Treat as indicative of what is SERVED.")
