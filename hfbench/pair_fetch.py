#!/usr/bin/env python3
"""Fetch TENSOR-ALIGNED 8 MiB slices of a base model and its fine-tune from the Hub, so the two
slices cover the same tensor region element for element. Parses each safetensors header (8-byte
length + JSON) to locate a named tensor's absolute byte offset in EACH file (the headers differ in
length, so file offsets differ), then range-fetches from those offsets. Verifies dtype/shape match."""
import json, struct, subprocess, sys, os, time
H = "https://huggingface.co"
OUT = "/root/mzip-hfbench/pairs"
os.makedirs(OUT, exist_ok=True)
PAIRS = [
  ("qwen25-1.5b", "Qwen/Qwen2.5-1.5B", "Qwen/Qwen2.5-1.5B-Instruct", "model.safetensors", "model.safetensors"),
  ("llama32-1b",  "unsloth/Llama-3.2-1B", "unsloth/Llama-3.2-1B-Instruct", "model.safetensors", "model.safetensors"),
  ("smollm2-135m","HuggingFaceTB/SmolLM2-135M", "HuggingFaceTB/SmolLM2-135M-Instruct", "model.safetensors", "model.safetensors"),
]
def curl_range(url, a, b, out):
    for t in range(4):
        r = subprocess.run(["curl", "-sSL", "--max-time", "300", "-r", f"{a}-{b}", "-o", out, url], capture_output=True)
        if os.path.exists(out) and os.path.getsize(out) == b - a + 1: return True
        time.sleep(4 * (t + 1))
    return False
def header(repo, fn):
    url = f"{H}/{repo}/resolve/main/{fn}"
    tmp = f"{OUT}/hdr.tmp"
    if not curl_range(url, 0, 8 * 1024 * 1024 - 1, tmp): return None, None
    b = open(tmp, "rb").read()
    n = struct.unpack("<Q", b[:8])[0]
    if n + 8 > len(b):
        if not curl_range(url, 0, n + 8 - 1, tmp): return None, None
        b = open(tmp, "rb").read()
    return json.loads(b[8:8 + n].decode()), 8 + n
for tag, base, ft, fb, ff in PAIRS:
    hb, ob = header(base, fb); hf, of = header(ft, ff)
    if not hb or not hf: print(f"{tag}: header fetch FAILED"); continue
    # pick the largest tensor present in both with identical dtype+shape, preferring a mid-stack weight
    cands = []
    for name, m in hb.items():
        if name == "__metadata__" or name not in hf: continue
        n2 = hf[name]
        if m["dtype"] != n2["dtype"] or m["shape"] != n2["shape"]: continue
        size = m["data_offsets"][1] - m["data_offsets"][0]
        if size >= 8 * 1024 * 1024 + 4096: cands.append((size, name))
    if not cands: print(f"{tag}: no common tensor >= 8 MiB"); continue
    cands.sort()
    # prefer a mid-depth layer weight over the embedding table (embeddings are a special case)
    pick = None
    for size, name in cands:
        if ".layers." in name and "weight" in name: pick = (size, name)
    if not pick: pick = cands[len(cands) // 2]
    size, name = pick
    esz = {"BF16": 2, "F16": 2, "F32": 4}[hb[name]["dtype"]]
    # element-aligned offset 4096 elements into the tensor, in EACH file
    a_b = ob + hb[name]["data_offsets"][0] + 4096 * esz
    a_f = of + hf[name]["data_offsets"][0] + 4096 * esz
    L = 8 * 1024 * 1024
    okb = curl_range(f"{H}/{base}/resolve/main/{fb}", a_b, a_b + L - 1, f"{OUT}/{tag}.base.bin")
    okf = curl_range(f"{H}/{ft}/resolve/main/{ff}", a_f, a_f + L - 1, f"{OUT}/{tag}.ft.bin")
    rec = dict(tag=tag, base=base, ft=ft, tensor=name, dtype=hb[name]["dtype"], shape=hb[name]["shape"],
               tensor_bytes=size, base_offset=a_b, ft_offset=a_f, header_len_base=ob, header_len_ft=of,
               fetched=(okb and okf), common_tensors=len(cands))
    json.dump(rec, open(f"{OUT}/{tag}.json", "w"), indent=1)
    print(f"{tag}: {'OK' if okb and okf else 'FAILED'}  tensor={name} {hb[name]['dtype']} {hb[name]['shape']}  hdr base={ob} ft={of}  offsets {a_b} / {a_f}")
