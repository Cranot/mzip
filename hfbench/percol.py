#!/usr/bin/env python3
"""Does per-COLUMN compression beat monolithic, for mzip vs zstd?

Thesis under test: mzip's differentiator is per-column structure detection, and
wikitext (one homogeneous prose column) was its worst case. Same bytes in both
arms -- only the granularity differs -- so the comparison is like-for-like.
"""
import os, struct, subprocess, sys
import pyarrow.parquet as pq

MZIP = "/root/mzip-hfbench/build/mzip_cli"
WORK = "/root/mzip-hfbench/percol"

def ser(vals):
    """Length-prefixed serialization of one column. Lossless enough to measure
    compressibility; identical in both arms so the comparison stays fair."""
    out = bytearray()
    for v in vals:
        b = (b"" if v is None else str(v).encode("utf-8", "replace"))
        out += struct.pack("<I", len(b)) + b
    return bytes(out)

def csize(path, tool):
    o = path + (".mz" if tool == "mzip" else ".zst")
    if os.path.exists(o): os.remove(o)
    if tool == "mzip":
        subprocess.run([MZIP, "c", path, o, "19"], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False)
    else:
        subprocess.run(["zstd", "-19", "-T1", "-q", "-f", "-o", o, path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    n = os.path.getsize(o) if os.path.exists(o) else 0
    os.remove(o)
    return n

def run(tag, src):
    os.makedirs(WORK, exist_ok=True)
    tbl = pq.read_table(src)
    try: tbl = tbl.flatten()          # expand struct columns into children
    except Exception: pass
    cols = tbl.column_names
    blobs = []
    for c in cols:
        blobs.append((c, ser(tbl.column(c).to_pylist())))

    mono = b"".join(b for _, b in blobs)
    mp = f"{WORK}/{tag}.mono"
    open(mp, "wb").write(mono)

    print(f"\n=== {tag}  ({os.path.getsize(src):,} B parquet as shipped) ===")
    print(f"    columns: {len(cols)}  serialized payload: {len(mono):,} B")

    res = {}
    for tool in ("zstd", "mzip"):
        res[(tool, "mono")] = csize(mp, tool)
        tot = 0
        per = []
        for c, b in blobs:
            p = f"{WORK}/{tag}.col.{c.replace('/','_')}"
            open(p, "wb").write(b)
            s = csize(p, tool)
            tot += s
            per.append((c, len(b), s))
            os.remove(p)
        res[(tool, "percol")] = tot
        if tool == "mzip":
            print("    per-column detail (mzip):")
            for c, raw, s in sorted(per, key=lambda r: -r[1]):
                r = raw / s if s else 0
                print(f"      {c[:26]:26s} {raw:>9,} -> {s:>8,}  {r:7.2f}x")

    # also: the shipped parquet recompressed, for the container-cost number
    res[("mzip", "parquet")] = csize(src, "mzip")
    os.remove(mp)

    print(f"    {'':22s} {'monolithic':>12s} {'per-column':>12s}")
    for tool in ("zstd", "mzip"):
        m, p = res[(tool, "mono")], res[(tool, "percol")]
        d = 100 * (m - p) / m if m else 0
        print(f"    {tool+' -19/L19':22s} {m:>12,} {p:>12,}   per-col {d:+.2f}%")
    best = min(res[("mzip", "mono")], res[("mzip", "percol")])
    bz = min(res[("zstd", "mono")], res[("zstd", "percol")])
    print(f"    mzip best vs zstd best : {100*(bz-best)/bz:+.2f}%  "
          f"({best:,} vs {bz:,})")
    print(f"    recompress parquet only: {res[('mzip','parquet')]:,}  "
          f"-> unwrapping is worth {res[('mzip','parquet')]/best:.2f}x")

for tag, url in [
    ("ai2_arc-easy", "https://huggingface.co/datasets/allenai/ai2_arc/resolve/main/ARC-Easy/train-00000-of-00001.parquet"),
    ("gsm8k-main",   "https://huggingface.co/datasets/openai/gsm8k/resolve/main/main/train-00000-of-00001.parquet"),
]:
    p = f"/root/mzip-hfbench/assets/{tag}.parquet"
    if not os.path.exists(p):
        subprocess.run(["curl", "-sSL", "--max-time", "300", "-o", p, url], check=True)
    run(tag, p)
