#!/usr/bin/env python3
"""Build a shape-stratified corpus set from real HF files.

Each corpus becomes ONE payload file. Parquet -> length-prefixed serialization
of all columns (the form an unwrapping tool would see). Raw csv/json/jsonl ->
file bytes as shipped. Truncation is line/record aligned and always recorded.
"""
import hashlib, os, struct, subprocess, sys
import pyarrow.parquet as pq

A = "/root/mzip-hfbench/assets"
C = "/root/mzip-hfbench/corpora"
CAP = 8 * 1024 * 1024
os.makedirs(C, exist_ok=True)

SRCS = [
 # tag, url, kind
 ("alpaca-instruct", "https://huggingface.co/datasets/tatsu-lab/alpaca/resolve/main/data/train-00000-of-00001-a09b74b3ef9c3b56.parquet", "parquet"),
 ("code-dockerfile", "https://huggingface.co/datasets/bigcode/the-stack-smol/resolve/main/data/dockerfile/data.json", "raw"),
 ("dialogue-csv",    "https://huggingface.co/datasets/knkarthick/dialogsum/resolve/main/train.csv", "raw"),
 ("tweets-jsonl",    "https://huggingface.co/datasets/mteb/tweet_sentiment_extraction/resolve/main/train.jsonl", "raw"),
 ("tabular-numeric", "https://huggingface.co/datasets/inria-soda/tabular-benchmark/resolve/main/reg_cat/Airlines_DepDelay_1M.csv", "raw"),
]

def ser_parquet(path):
    t = pq.read_table(path)
    try: t = t.flatten()
    except Exception: pass
    out = bytearray()
    for c in t.column_names:
        for v in t.column(c).to_pylist():
            b = b"" if v is None else str(v).encode("utf-8", "replace")
            out += struct.pack("<I", len(b)) + b
    return bytes(out)

def trunc_lines(data, cap):
    if len(data) <= cap: return data, False
    cut = data.rfind(b"\n", 0, cap)
    return data[: (cut + 1 if cut > 0 else cap)], True

for tag, url, kind in SRCS:
    dl = f"{A}/{tag}.src"
    if not os.path.exists(dl):
        print(f"downloading {tag} ...", flush=True)
        r = subprocess.run(["curl", "-sSL", "--max-time", "600", "-o", dl, url])
        if r.returncode != 0: print(f"  FAILED {tag}"); continue
    raw = ser_parquet(dl) if kind == "parquet" else open(dl, "rb").read()
    payload, cut = trunc_lines(raw, CAP)
    op = f"{C}/{tag}.bin"
    open(op, "wb").write(payload)
    print(f"{tag:18s} {len(payload):>10,} B  truncated={cut}  "
          f"sha {hashlib.sha256(payload).hexdigest()[:12]}")

# corpora already prepared earlier in this session
import shutil
for tag, src in [("prose-wikitext", f"{A}/wikitext-2-raw-train.txt"),
                 ("qa-gsm8k",       "/root/mzip-hfbench/percol/gsm8k-main.mono"),
                 ("qa-arc",         "/root/mzip-hfbench/percol/ai2_arc-easy.mono")]:
    if not os.path.exists(src): print(f"  MISSING {src}"); continue
    d, cut = trunc_lines(open(src, "rb").read(), CAP)
    open(f"{C}/{tag}.bin", "wb").write(d)
    print(f"{tag:18s} {len(d):>10,} B  truncated={cut}  "
          f"sha {hashlib.sha256(d).hexdigest()[:12]}")

print("\ncorpora:")
for f in sorted(os.listdir(C)):
    print(f"  {os.path.getsize(C+'/'+f):>10,}  {f}")
