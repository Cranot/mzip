#!/usr/bin/env python3
"""Codec x column-type: is any of the 41.6% UNCOMPRESSED parquet actually text?

RESULT 102 found 41.6% of parquet column bytes stored UNCOMPRESSED but did not cross that with the
column's type. If it is image/audio binary, uncompressed is correct -- the payload is already JPEG or
FLAC and a second codec buys nothing. If any of it is string columns, that is free ground for zstd
(text is LZ territory; a plane coder has nothing to offer). Same footers, same repos, one more tally."""
import json, os, io, struct, sys, collections, urllib.request
sys.path.insert(0, "/root/mzip-hfbench")
import pyarrow.parquet as pq
from dsprobe import req, api, footer
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def tclass(t):
    t = str(t)
    if t in ("string", "large_string", "utf8", "large_utf8"): return "text"
    if "binary" in t: return "binary (image/audio bytes)"
    if t.startswith(("int", "uint")): return "integer"
    if t.startswith(("float", "double", "half")): return "float"
    if t.startswith(("list", "large_list", "fixed_size_list")):
        return "list<float>" if "float" in t or "double" in t else "list<int>" if "int" in t else "list<other>"
    return "other"

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_datasets.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("parquet", 0) or 0) > 0]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["parquet"]))
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    cross = collections.Counter(); n = 0
    for r in cand[:LIMIT]:
        rid = r["id"]
        t = api(f"{H}/api/datasets/{rid}/tree/main?recursive=1&expand=1")
        if not isinstance(t, list): continue
        files = sorted(((f["path"], int(f.get("size") or (f.get("lfs") or {}).get("size") or 0))
                        for f in t if f.get("type") == "file" and f.get("path", "").lower().endswith(".parquet")),
                       key=lambda x: -x[1])
        if not files: continue
        path, size = files[0]
        md = footer(f"{H}/datasets/{rid}/resolve/main/{path}", size)
        if md is None: continue
        n += 1
        sch = md.schema.to_arrow_schema()
        # map leaf column index -> arrow field class via the parquet schema path root
        leaf_cls = []
        pqs = md.schema
        for i in range(len(pqs)):
            root = pqs.column(i).path.split(".")[0]
            try: leaf_cls.append(tclass(sch.field(root).type))
            except Exception: leaf_cls.append("other")
        # weight by ALL row groups so the tally reflects the file, not its first two groups
        for rg in range(md.num_row_groups):
            g = md.row_group(rg)
            for c in range(g.num_columns):
                col = g.column(c)
                cross[(str(col.compression), leaf_cls[c] if c < len(leaf_cls) else "other")] += col.total_compressed_size
    tot = sum(cross.values()) or 1
    print(f"repos with readable footer: {n};  column bytes tallied: {tot/1e9:.1f} GB (all row groups)\n")
    print(f"{'codec':14s} {'column type':28s} {'share':>7s}")
    for (codec, cls), v in cross.most_common(14):
        print(f"{codec:14s} {cls:28s} {100*v/tot:6.1f}%")
    unc_text = sum(v for (c, k), v in cross.items() if c == "UNCOMPRESSED" and k == "text")
    unc_all = sum(v for (c, k), v in cross.items() if c == "UNCOMPRESSED")
    print(f"\nUNCOMPRESSED total {100*unc_all/tot:.1f}%  of which TEXT {100*unc_text/tot:.1f}% of all column bytes "
          f"({100*unc_text/max(unc_all,1):.0f}% of the uncompressed)")
    print("  uncompressed text = zstd ground (not Fano's); uncompressed binary = already-compressed media, correct as is")
    json.dump({f"{c}|{k}": v for (c, k), v in cross.items()}, open(f"{B}/qgq/dsprobe2.json", "w"), indent=1)
    print("DSPROBE2_DONE")

if __name__ == "__main__":
    main()
