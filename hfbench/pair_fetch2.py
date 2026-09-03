#!/usr/bin/env python3
"""Widen the fine-tune-delta corpus: several tensor TYPES per pair (embedding, attention, MLP,
lm_head) and two 7-8B pairs (sharded safetensors: resolve the shard through the index file).
Each slice is 8 MiB, element-aligned, from the SAME tensor in base and fine-tune."""
import json, struct, subprocess, sys, os, time
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/pairs"; os.makedirs(OUT, exist_ok=True)
PAIRS = [
  ("qwen25-1.5b",  "Qwen/Qwen2.5-1.5B",              "Qwen/Qwen2.5-1.5B-Instruct"),
  ("llama32-1b",   "unsloth/Llama-3.2-1B",           "unsloth/Llama-3.2-1B-Instruct"),
  ("qwen25-7b",    "Qwen/Qwen2.5-7B",                "Qwen/Qwen2.5-7B-Instruct"),
  ("llama31-8b",   "unsloth/Meta-Llama-3.1-8B",      "unsloth/Meta-Llama-3.1-8B-Instruct"),
  ("qwen25-7b-3p", "Qwen/Qwen2.5-7B-Instruct",       "Qwen/Qwen2.5-Coder-7B-Instruct"),   # third-party-style: a further fine-tune of the instruct
]
WANT = [("embed", lambda n: "embed_tokens" in n), ("attn_q", lambda n: ".layers.4." in n and "q_proj.weight" in n),
        ("attn_o", lambda n: ".layers.4." in n and "o_proj.weight" in n), ("mlp_down", lambda n: ".layers.4." in n and "down_proj.weight" in n),
        ("mlp_up_l20", lambda n: ".layers.20." in n and "up_proj.weight" in n), ("lm_head", lambda n: n == "lm_head.weight")]
def curl(url, out, a=None, b=None):
    for t in range(4):
        args = ["curl", "-sSL", "--max-time", "300", "-o", out, url]
        if a is not None: args[1:1] = ["-r", f"{a}-{b}"]
        subprocess.run(args, capture_output=True)
        sz = os.path.getsize(out) if os.path.exists(out) else 0
        if (a is None and sz > 100) or (a is not None and sz == b - a + 1): return True
        time.sleep(4 * (t + 1))
    return False
SM = {}
def shard_map(repo):
    """tensor -> shard filename (index.json) or None if single-file; cached, single attempt"""
    if repo in SM: return SM[repo]
    tmp = f"{OUT}/idx.tmp"
    subprocess.run(["curl", "-sSL", "--max-time", "120", "-o", tmp, f"{H}/{repo}/resolve/main/model.safetensors.index.json"], capture_output=True)
    try: SM[repo] = json.load(open(tmp))["weight_map"]
    except Exception: SM[repo] = None
    return SM[repo]
HDRS = {}
def header(repo, fn):
    key = (repo, fn)
    if key in HDRS: return HDRS[key]
    url = f"{H}/{repo}/resolve/main/{fn}"; tmp = f"{OUT}/hdr.tmp"
    if not curl(url, tmp, 0, 4 * 1024 * 1024 - 1): HDRS[key] = (None, None); return HDRS[key]
    b = open(tmp, "rb").read(); n = struct.unpack("<Q", b[:8])[0]
    if n + 8 > len(b):
        if not curl(url, tmp, 0, n + 8 - 1): HDRS[key] = (None, None); return HDRS[key]
        b = open(tmp, "rb").read()
    HDRS[key] = (json.loads(b[8:8 + n].decode()), 8 + n); return HDRS[key]
def locate(repo, name):
    sm = shard_map(repo); fn = sm.get(name) if sm else "model.safetensors"
    if fn is None: return None
    h, off = header(repo, fn)
    if not h or name not in h: return None
    return fn, h[name], off
for tag, base, ft in PAIRS:
    smb = shard_map(base) or {}; names = list(smb.keys()) if smb else list((header(base, "model.safetensors")[0] or {}).keys())
    if not names: print(f"{tag}: no tensor list"); continue
    for kind, pred in WANT:
        cand = [n for n in names if n != "__metadata__" and pred(n)]
        if not cand: continue
        name = sorted(cand)[0]; out_tag = f"{tag}.{kind}"
        if os.path.exists(f"{OUT}/{out_tag}.json"): print(f"have  {out_tag}"); continue
        lb = locate(base, name); lf = locate(ft, name)
        if not lb or not lf: print(f"skip  {out_tag}: locate failed"); continue
        (fb, mb, ob), (ff, mf, of) = lb, lf
        if mb["dtype"] != mf["dtype"] or mb["shape"] != mf["shape"]: print(f"skip  {out_tag}: shape/dtype differ"); continue
        size = mb["data_offsets"][1] - mb["data_offsets"][0]
        if size < 8 * 1024 * 1024 + 4096: print(f"skip  {out_tag}: tensor only {size} B"); continue
        esz = {"BF16": 2, "F16": 2, "F32": 4}[mb["dtype"]]
        a_b = ob + mb["data_offsets"][0] + 4096 * esz; a_f = of + mf["data_offsets"][0] + 4096 * esz; L = 8 * 1024 * 1024
        okb = curl(f"{H}/{base}/resolve/main/{fb}", f"{OUT}/{out_tag}.base.bin", a_b, a_b + L - 1)
        okf = curl(f"{H}/{ft}/resolve/main/{ff}", f"{OUT}/{out_tag}.ft.bin", a_f, a_f + L - 1)
        json.dump(dict(tag=out_tag, base=base, ft=ft, tensor=name, dtype=mb["dtype"], shape=mb["shape"], tensor_bytes=size,
                       base_file=fb, ft_file=ff, base_offset=a_b, ft_offset=a_f, fetched=bool(okb and okf)), open(f"{OUT}/{out_tag}.json", "w"), indent=1)
        print(f"{'ok   ' if okb and okf else 'FAIL '} {out_tag}: {name} {mb['dtype']} {mb['shape']}")
        time.sleep(2)
print("FETCH2_DONE")
