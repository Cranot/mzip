#!/usr/bin/env python3
"""How many BYTES of quantized derivatives does one base model have on the Hub, and what share is
GGUF (llama.cpp-derivable) vs GPTQ/AWQ/bnb (calibration-dependent, not derivable)? Samples the
'quantized' derivative repos of a base via the Hub API and sums their file sizes by format."""
import json, subprocess, sys, time, collections
def api(url):
    for t in range(3):
        r = subprocess.run(["curl", "-sS", "--max-time", "60", url], capture_output=True, text=True)
        try: return json.loads(r.stdout)
        except Exception: time.sleep(3)
    return None
def classify(fn):
    f = fn.lower()
    if f.endswith(".gguf"): return "gguf"
    if any(k in f for k in ("gptq", "awq", "bnb", "exl2", "marlin", "hqq")): return "other-quant"
    if f.endswith((".safetensors", ".bin", ".pt", ".pth")): return "safetensors/other"
    return "misc"
BASES = ["Qwen/Qwen2.5-7B-Instruct", "meta-llama/Llama-3.1-8B-Instruct", "Qwen/Qwen2.5-1.5B-Instruct"]
SAMPLE = 60
for base in BASES:
    lst = api(f"https://huggingface.co/api/models?filter=base_model:quantized:{base}&limit=1000&sort=downloads&direction=-1")
    if not lst: print(f"{base}: list failed"); continue
    n_total = len(lst); repos = [m["id"] for m in lst[:SAMPLE]]
    by = collections.Counter(); files = collections.Counter(); gguf_repos = 0; dl = 0
    for rid in repos:
        d = api(f"https://huggingface.co/api/models/{rid}?blobs=true")
        if not d: continue
        dl += d.get("downloads", 0) or 0
        has_gguf = False
        for s in d.get("siblings", []):
            c = classify(s.get("rfilename", "")); sz = s.get("size") or 0
            by[c] += sz; files[c] += 1; has_gguf |= (c == "gguf")
        gguf_repos += has_gguf
        time.sleep(0.3)
    tot = sum(by.values()) or 1
    print(f"=== {base}: {n_total} quantized-derivative repos listed (API cap 1000); sampled top-{len(repos)} by downloads, {dl:,} downloads among them ===")
    for c in ("gguf", "other-quant", "safetensors/other", "misc"):
        print(f"   {c:18s} {by[c]/1e9:9.1f} GB  {files[c]:5d} files  {100*by[c]/tot:5.1f}% of sampled bytes")
    print(f"   repos with GGUF files: {gguf_repos}/{len(repos)};  projected GGUF bytes over all {n_total} repos (linear): {by['gguf']/1e9*n_total/len(repos):,.0f} GB")
    # the base itself
    b = api(f"https://huggingface.co/api/models/{base}?blobs=true")
    if b: print(f"   the base model's own safetensors: {sum((s.get('size') or 0) for s in b.get('siblings', []) if s.get('rfilename','').endswith('.safetensors'))/1e9:.1f} GB")
print("QUANT_BYTES_DONE")
