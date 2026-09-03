#!/usr/bin/env python3
"""Can we rebuild HF's exact parquet bytes after unwrapping it?

This gates the whole product: byte-exact => mirror/archival format;
not byte-exact => dataset cache with semantic-equality guarantees only.
"""
import hashlib, io, os
import pyarrow as pa
import pyarrow.parquet as pq

SRC = "/root/mzip-hfbench/assets/wikitext-2-raw-train.parquet"
orig = open(SRC, "rb").read()
pf = pq.ParquetFile(SRC)
md = pf.metadata
tbl = pf.read()

print(f"original         : {len(orig):,} bytes  sha {hashlib.sha256(orig).hexdigest()[:16]}")
print(f"created_by       : {md.created_by}")
print(f"row_groups       : {md.num_row_groups}   rows: {md.num_rows:,}")

# Attempt 1: naive re-encode with matching codec
buf = io.BytesIO()
pq.write_table(tbl, buf, compression="snappy")
a1 = buf.getvalue()
print(f"\nnaive re-encode  : {len(a1):,} bytes  identical={a1 == orig}")

# Attempt 2: match row-group sizing and disable extras
rg_rows = md.row_group(0).num_rows
buf = io.BytesIO()
pq.write_table(tbl, buf, compression="snappy", row_group_size=rg_rows,
               write_statistics=False, store_schema=False,
               use_dictionary=False, data_page_version="1.0")
a2 = buf.getvalue()
print(f"tuned re-encode  : {len(a2):,} bytes  identical={a2 == orig}  (rg_rows={rg_rows})")

# How close? first divergence tells us whether a "recipe + diff" would be small.
def first_diff(x, y):
    n = min(len(x), len(y))
    for i in range(n):
        if x[i] != y[i]:
            return i
    return n if len(x) != len(y) else -1

for nm, cand in (("naive", a1), ("tuned", a2)):
    d = first_diff(orig, cand)
    print(f"  {nm}: first byte difference at offset {d:,} of {len(orig):,} "
          f"({100*d/len(orig):.4f}% in), size delta {len(cand)-len(orig):+,}")

# Semantic equality — what a dataset cache actually needs
back = pq.read_table(io.BytesIO(a1))
print(f"\nsemantic equality: tables equal = {back.equals(tbl)}  rows={back.num_rows:,}")
payload = b"".join((x or "").encode() for x in tbl.column("text").to_pylist())
print(f"payload sha256   : {hashlib.sha256(payload).hexdigest()[:16]} "
      f"({len(payload):,} bytes)")
