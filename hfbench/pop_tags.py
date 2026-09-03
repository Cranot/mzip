#!/usr/bin/env python3
"""Re-page the same lastModified window for MODELS and keep each repo's tags (base_model:finetune:*,
base_model:quantized:*, library, pipeline). 1000 repos per page, ~33 pages. Joined later with the
sample to split model bytes into base / fine-tune / quantized / adapter populations."""
import json, os, re, subprocess, time, datetime as dt
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/upload-mix"
NOW = dt.datetime.fromisoformat(os.environ["UM_NOW"]); START = NOW - dt.timedelta(days=7)
def iso(s): return dt.datetime.fromisoformat(s.replace("Z", "+00:00"))
def get(url):
    for t in range(6):
        hf = f"{OUT}/ht.tmp"
        r = subprocess.run(["curl", "-sS", "-g", "--max-time", "120", "-D", hf, url], capture_output=True, text=True)
        status = 0; link = None
        for l in open(hf):
            if l.startswith("HTTP/"): status = int(l.split()[1])
            if l.lower().startswith("link:"):
                m = re.search(r"<([^>]+)>;\s*rel=\"next\"", l); link = m.group(1) if m else None
        if status == 429: time.sleep(60 * (t + 1)); continue
        try: return json.loads(r.stdout), link
        except Exception: time.sleep(5 * (t + 1))
    return None, None
tags = {}; url = f"{H}/api/models?sort=lastModified&direction=-1&limit=1000"; pages = 0
while url:
    d, nxt = get(url)
    if not isinstance(d, list): print("page failed", pages + 1, flush=True); break
    pages += 1; stop = False
    for m in d:
        lm = iso(m["lastModified"])
        if lm > NOW: continue
        if lm < START: stop = True; break
        t = m.get("tags") or []
        keep = [x for x in t if x.startswith("base_model:") or x in ("peft", "lora", "gguf", "safetensors", "transformers", "diffusers", "sentence-transformers", "llama.cpp", "mlx", "onnx", "unsloth", "adapter-transformers")]
        tags[m["id"]] = dict(tags=keep, pipeline=m.get("pipeline_tag"), library=m.get("library_name"))
    print(f"page {pages}: {len(tags)} repos", flush=True)
    if stop or not nxt: break
    url = nxt; time.sleep(0.3)
json.dump(tags, open(f"{OUT}/population_models_tags.json", "w"))
print("POP_TAGS_DONE", len(tags))
