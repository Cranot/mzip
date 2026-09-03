#!/usr/bin/env python3
"""Add a code corpus. the-stack-smol is gated; use a public code-search-net
mirror and stream only enough row batches to fill the 8MB cap."""
import hashlib, io, os, struct, subprocess
import pyarrow.parquet as pq

C = "/root/mzip-hfbench/corpora"
A = "/root/mzip-hfbench/assets"
CAP = 8 * 1024 * 1024
URL = ("https://huggingface.co/datasets/Nan-Do/code-search-net-python/"
       "resolve/main/data/train-00000-of-00004-ee77a7de79eb2ab2.parquet")
dl = f"{A}/code-python.parquet"

if not os.path.exists(dl):
    print("downloading code parquet (155MB) ...", flush=True)
    subprocess.run(["curl", "-sSL", "--max-time", "900", "-o", dl, URL], check=True)
print(f"downloaded {os.path.getsize(dl):,} B")

pf = pq.ParquetFile(dl)
print("schema:", pf.schema_arrow.names)

# Prefer the column that actually holds source code.
cand = [n for n in pf.schema_arrow.names
        if any(k in n.lower() for k in ("code", "func", "whole", "body", "source"))]
col = cand[0] if cand else pf.schema_arrow.names[0]
print("using column:", col)

out = bytearray()
for batch in pf.iter_batches(batch_size=512, columns=[col]):
    for v in batch.column(0).to_pylist():
        if v is None: continue
        b = str(v).encode("utf-8", "replace")
        out += b + b"\n"
        if len(out) >= CAP: break
    if len(out) >= CAP: break

cut = out.rfind(b"\n", 0, CAP)
payload = bytes(out[: cut + 1 if cut > 0 else CAP])
open(f"{C}/code-python.bin", "wb").write(payload)
print(f"code-python        {len(payload):>10,} B  "
      f"sha {hashlib.sha256(payload).hexdigest()[:12]}")
os.remove(f"{C}/code-dockerfile.bin") if os.path.exists(f"{C}/code-dockerfile.bin") else None
