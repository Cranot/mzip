#!/usr/bin/env python3
"""Inspect the HF wikitext parquet and extract its text payload verbatim."""
import hashlib, os, sys
import pyarrow.parquet as pq

SRC = "/root/mzip-hfbench/assets/wikitext-2-raw-train.parquet"
OUT = "/root/mzip-hfbench/assets/wikitext-2-raw-train.txt"

pf = pq.ParquetFile(SRC)
md = pf.metadata
print("=== parquet metadata ===")
print("created_by      :", md.created_by)
print("num_rows        :", f"{md.num_rows:,}")
print("num_row_groups  :", md.num_row_groups)
print("schema          :", pf.schema_arrow)

tot_c = tot_u = 0
codecs = set()
for g in range(md.num_row_groups):
    rg = md.row_group(g)
    for c in range(rg.num_columns):
        col = rg.column(c)
        codecs.add(col.compression)
        tot_c += col.total_compressed_size
        tot_u += col.total_uncompressed_size
print("page codec(s)   :", ", ".join(sorted(codecs)))
print(f"col compressed  : {tot_c:,} bytes")
print(f"col uncompressed: {tot_u:,} bytes")
print(f"parquet internal ratio: {tot_u/tot_c:.4f}x")

# Extract the text column exactly as `datasets` would hand it to a user:
# one row per element, rows joined by newline (wikitext rows already carry
# their own trailing newlines, so this reproduces the original corpus text).
tbl = pf.read(columns=["text"])
col = tbl.column("text").to_pylist()
data = "".join(x if x is not None else "" for x in col).encode("utf-8")
with open(OUT, "wb") as f:
    f.write(data)

print("\n=== extracted text ===")
print("path            :", OUT)
print(f"bytes           : {len(data):,}")
print("sha256          :", hashlib.sha256(data).hexdigest())
print("first 200 chars :", repr(data[:200].decode("utf-8", "replace")))
