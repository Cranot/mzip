#!/usr/bin/env python3
"""Lineage of GATED repos from the tree endpoint alone: per-file sizes and LFS sha256 oids are public
even when the bytes are not (HTTP 401 on resolve). An oid shared with the public base = exact copy of
that shard; identical shard-size vectors = same tensor layout (same architecture + dtype); oids shared
between two gated repos = copies of each other. Zero content bytes read."""
import json, sys, time, collections, urllib.request, urllib.error
H = "https://huggingface.co"; PACE = 0.5

def tree(rid):
    req = urllib.request.Request(f"{H}/api/models/{rid}/tree/main", headers={"User-Agent": "mzip-hfbench"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r: lst = json.loads(r.read())
    except urllib.error.HTTPError as e: return e.code, {}
    time.sleep(PACE)
    out = {}
    for x in lst:
        if x.get("type") != "file" or not x["path"].endswith(".safetensors"): continue
        lfs = x.get("lfs") or {}
        out[x["path"]] = (x.get("size"), lfs.get("oid"), x.get("xetHash"))
    return 200, out

bases = sys.argv[1].split(","); repos = sys.argv[2:]
bt = {}
for b in bases:
    st, t = tree(b); bt[b] = t
    print(f"BASE {b}: HTTP {st}, {len(t)} shards, {sum(v[0] or 0 for v in t.values())/1e9:.3f} GB, oids present: {sum(1 for v in t.values() if v[1])}, xetHash present: {sum(1 for v in t.values() if v[2])}")
    if t: print("   first entry:", next(iter(t.items())))
boids = {b: {v[1] for v in t.values() if v[1]} for b, t in bt.items()}
bsizes = {b: sorted(v[0] for v in t.values()) for b, t in bt.items()}
rt = {}
for r in repos:
    st, t = tree(r); rt[r] = t
    tot = sum(v[0] or 0 for v in t.values()); oids = {v[1] for v in t.values() if v[1]}; sizes = sorted(v[0] for v in t.values())
    hits = {b: len(oids & boids[b]) for b in bases}
    lay = [b for b in bases if sizes == bsizes[b]]
    print(f"{r[:46]:46s} HTTP {st} shards={len(t):2d} {tot/1e9:7.3f} GB  oid-hits-vs-base={hits}  same-shard-layout-as={lay}")
print("\npairwise oid overlap among the gated repos (shards shared / shards):")
rs = list(rt)
for i in range(len(rs)):
    for j in range(i + 1, len(rs)):
        a = {v[1] for v in rt[rs[i]].values() if v[1]}; b = {v[1] for v in rt[rs[j]].values() if v[1]}
        if a & b: print(f"  {rs[i][:36]:36s} ~ {rs[j][:36]:36s} {len(a & b)}/{min(len(a), len(b))}")
print("TREECMP_DONE")
