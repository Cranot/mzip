#!/usr/bin/env python3
"""Build HELD-OUT corpora: same distributions, DISJOINT rows from the ones
the claimant measured on. Decisive generalization test for the PPMd-mem lever."""
import hashlib, os, struct
import pyarrow.parquet as pq

A = "/root/mzip-hfbench/assets"
D = open("/root/mzip-hfbench/.advver_dir").read().strip()
H = os.path.join(D, "heldout")
os.makedirs(H, exist_ok=True)
CAP = 8 * 1024 * 1024

def emit(tag, payload):
    p = f"{H}/{tag}.bin"
    open(p, "wb").write(payload)
    print(f"{tag:22s} {len(payload):>10,} B  sha {hashlib.sha256(payload).hexdigest()[:16]}")

# ---- code-python: replicate add_code.py serialization, then take slices
#      AFTER the bytes that became corpora/code-python.bin (disjoint rows).
pf = pq.ParquetFile(f"{A}/code-python.parquet")
cand = [n for n in ["original_string"]
        ]
col = cand[0] if cand else pf.schema_arrow.names[0]
print("code column:", col)
out = bytearray()
NEED = CAP * 3          # want ~24MB so we can carve B and C past the first 8MB
for batch in pf.iter_batches(batch_size=512, columns=[col]):
    for v in batch.column(0).to_pylist():
        if v is None: continue
        out += str(v).encode("utf-8","replace") + b"\n"
    if len(out) >= NEED: break

orig = open("/root/mzip-hfbench/corpora/code-python.bin","rb").read()
n0 = len(orig)
assert bytes(out[:n0]) == orig, "serialization does not reproduce the shipped corpus"
print(f"reproduced shipped code-python prefix exactly ({n0:,} B) -> slices are truly disjoint")

def cut_at_nl(buf, start, cap):
    seg = buf[start:start+cap]
    c = seg.rfind(b"\n")
    return bytes(seg[:c+1]) if c > 0 else bytes(seg)

emit("code-python-B", cut_at_nl(out, n0, CAP))
emit("code-python-C", cut_at_nl(out, n0 + CAP, CAP))

# ---- qa-gsm8k: whole mono is the shipped corpus; carve disjoint halves so the
#      "new adoption" claim can be tested on data it was not selected on.
g = open("/root/mzip-hfbench/percol/gsm8k-main.mono","rb").read()
mid = g.rfind(b"\n", 0, len(g)//2) + 1
emit("qa-gsm8k-H1", g[:mid])
emit("qa-gsm8k-H2", g[mid:])

# ---- held-out prose from the OTHER wikitext shard (103, not 2)
pf2 = pq.ParquetFile(f"{A}/wikitext103-shard0.parquet")
buf = bytearray()
for batch in pf2.iter_batches(batch_size=1024, columns=["text"]):
    for v in batch.column(0).to_pylist():
        if v is None: continue
        buf += str(v).encode("utf-8","replace")
    if len(buf) >= CAP + 4096: break
emit("prose-wt103-B", cut_at_nl(buf, 0, CAP))

# ---- held-out alpaca (disjoint tail of the same source parquet)
t = pq.read_table(f"{A}/alpaca-instruct.src")
try: t = t.flatten()
except Exception: pass
ab = bytearray()
for c in t.column_names:
    for v in t.column(c).to_pylist():
        b = b"" if v is None else str(v).encode("utf-8","replace")
        ab += struct.pack("<I", len(b)) + b
shipped_alpaca = os.path.getsize("/root/mzip-hfbench/corpora/alpaca-instruct.bin")
if len(ab) > shipped_alpaca + 1024:
    emit("alpaca-B", bytes(ab[shipped_alpaca:shipped_alpaca+CAP]))
else:
    print("alpaca: no disjoint remainder")
print("HELDOUT_DONE")
