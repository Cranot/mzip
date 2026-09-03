#!/usr/bin/env python3
"""The datasets frontier: what is inside the week's parquet, read from footers alone.

0.8 PB of the week is datasets and nothing here has looked at their structure beyond the archive class
(RESULT 85, media, dead). Parquet is the format the Hub converts to; its footer names every column,
its type, its codec and the row count. Three category-3 questions, answered without downloading a row:

  * TOKENISED: a dataset whose columns are input_ids / attention_mask / labels is a deterministic
    function of (text dataset, tokenizer). If the text is on the Hub, the tokens are a recipe.
  * RE-SHARDED / SUBSET: two repos with the same schema hash are candidates for the same rows in a
    different container or a filtered subset -- row-level identity a chunk store cannot see.
  * CODEC: what parquet already compresses with. UNCOMPRESSED or PLAIN-encoded numeric columns would be
    free ground for a plane coder; SNAPPY/ZSTD columns are not.

Footer = last 8 bytes (metadata length + PAR1), then the metadata; pyarrow parses it from a BytesIO."""
import json, os, io, struct, sys, hashlib, collections, urllib.request
sys.path.insert(0, "/root/mzip-hfbench")
import pyarrow.parquet as pq
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
TOK = os.environ.get("HF_TOKEN", "")

def req(u, rng=None):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    if rng: r.add_header("Range", f"bytes={rng}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=60) as f: return f.read()
        except Exception: pass
    return None

def api(u):
    b = req(u); return json.loads(b) if b else None

def footer(url, size):
    tail = req(url, f"{max(0, size-8)}-{size-1}")
    if not tail or tail[-4:] != b"PAR1": return None
    mlen, = struct.unpack("<I", tail[:4])
    if mlen > 64 << 20: return None
    meta = req(url, f"{size-8-mlen}-{size-9}")
    if not meta or len(meta) != mlen: return None
    try:
        return pq.read_metadata(io.BytesIO(b"PAR1" + meta + tail))
    except Exception:
        return None

TOKCOLS = {"input_ids", "attention_mask", "token_type_ids", "labels", "tokens", "token_ids"}

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_datasets.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("parquet", 0) or 0) > 0]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["parquet"]))
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    tot_pq = sum(float(r["bytes_in_window"]["parquet"]) for r in cand)
    print(f"dataset repos with parquet in window: {len(cand)} ({tot_pq/1e12:.2f} TB); probing largest {min(LIMIT,len(cand))}", flush=True)
    by_class = collections.Counter(); codec = collections.Counter(); enc_plain = 0.0
    schema_repos = collections.defaultdict(set); schema_bytes = collections.Counter()
    probed = 0.0; nfiles = 0; examples = []
    for i, r in enumerate(cand[:LIMIT]):
        rid = r["id"]; pqb = float(r["bytes_in_window"]["parquet"])
        t = api(f"{H}/api/datasets/{rid}/tree/main?recursive=1&expand=1")
        if not isinstance(t, list): continue
        files = [(f["path"], int(f.get("size") or (f.get("lfs") or {}).get("size") or 0))
                 for f in t if f.get("type") == "file" and f.get("path", "").lower().endswith(".parquet")]
        if not files: continue
        files.sort(key=lambda x: -x[1])
        path, size = files[0]                                   # the largest shard speaks for the repo
        md = footer(f"{H}/datasets/{rid}/resolve/main/{path}", size)
        if md is None: by_class["footer unreadable"] += pqb; continue
        nfiles += 1; probed += pqb
        sch = md.schema.to_arrow_schema()
        names = [f.name for f in sch]
        types = [str(f.type) for f in sch]
        sig = hashlib.sha1(json.dumps(list(zip(names, types))).encode()).hexdigest()[:10]
        schema_repos[sig].add(rid); schema_bytes[sig] += pqb
        lower = {n.lower() for n in names}
        is_tok = len(lower & TOKCOLS) >= 2 or ("input_ids" in lower)
        has_text = any(t == "string" or t.startswith("large_string") for t in types)
        has_img = any("binary" in t for t in types) and any(n.lower() in ("image", "img", "jpg", "png", "bytes") for n in names)
        has_audio = any(n.lower() in ("audio", "wav", "speech") for n in names)
        has_emb = any(t.startswith("list<item: float") or t.startswith("fixed_size_list<item: float") for t in types)
        cls = ("tokenised (input_ids)" if is_tok else "embeddings (float lists)" if has_emb
               else "images (binary)" if has_img else "audio" if has_audio
               else "text" if has_text else "numeric / other")
        by_class[cls] += pqb
        for rg in range(min(md.num_row_groups, 2)):
            g = md.row_group(rg)
            for c in range(g.num_columns):
                col = g.column(c)
                codec[str(col.compression)] += col.total_compressed_size
                if "PLAIN" in [str(e) for e in col.encodings] and "RLE_DICTIONARY" not in [str(e) for e in col.encodings]:
                    enc_plain += col.total_compressed_size
        if len(examples) < 10: examples.append((rid, cls, ",".join(names[:5])[:60], md.num_rows))
        if (i + 1) % 25 == 0: print(f"  ...{i+1} repos", flush=True)
    print(f"\nprobed {nfiles} repos, {probed/1e12:.2f} TB of parquet ({100*probed/max(tot_pq,1):.0f}% of the class)")
    print(f"{'what the parquet holds':28s} {'TB':>7s} {'share':>7s}")
    for k, v in by_class.most_common(): print(f"{k:28s} {v/1e12:7.3f} {100*v/max(probed,1):6.1f}%")
    ctot = sum(codec.values()) or 1
    print(f"\ncolumn codec, by compressed bytes in the first two row groups:")
    for k, v in codec.most_common(): print(f"  {k:14s} {100*v/ctot:5.1f}%")
    print(f"  PLAIN-encoded (no dictionary) share: {100*enc_plain/ctot:.1f}%")
    multi = [(s, len(rs), schema_bytes[s]) for s, rs in schema_repos.items() if len(rs) >= 2]
    multi.sort(key=lambda x: -x[2])
    print(f"\nschemas shared by >=2 repos: {len(multi)}  covering {sum(m[2] for m in multi)/1e12:.3f} TB "
          f"= {100*sum(m[2] for m in multi)/max(probed,1):.1f}% of probed parquet  (re-shard / subset candidates)")
    for s, n, b in multi[:6]:
        rs = sorted(schema_repos[s])[:3]
        print(f"  {s} x{n:3d} {b/1e9:8.1f} GB  e.g. {', '.join(rs)}")
    print("\nexamples:")
    for rid, cls, cols, nr in examples: print(f"  {rid[:40]:40s} {cls:26s} rows={nr:<12,d} {cols}")
    json.dump(dict(by_class=dict(by_class), codec=dict(codec), plain=enc_plain, probed=probed, tot=tot_pq,
                   shared=[(s, n, b) for s, n, b in multi]), open(f"{B}/qgq/dsprobe.json", "w"), indent=1)
    print("DSPROBE_DONE")

if __name__ == "__main__":
    main()
