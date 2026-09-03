#!/usr/bin/env python3
"""Checkpoint series, measured from file trees instead of guessed from file counts.

The first pass flagged 43.6% of weight bytes on 'many files + weights', and the example list shows why
that is wrong: nmatteo3294/...-GGUF with 41 quant files matched. A quant repo is not a checkpoint
series. This pulls the actual tree for every flagged repo and counts only those with >=2 sibling
directories (or file stems) matching checkpoint-N / step-N / epoch-N that each contain weight files.
Byte share is then the weight bytes in those series, and the DELTA-ELIGIBLE share is the bytes in
every checkpoint except the first of each series -- the first has no predecessor to code against."""
import json, re, os, collections, sys, urllib.request
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
TOK = os.environ.get("HF_TOKEN", "")
STEP = re.compile(r"(?:^|/)(?:checkpoint|ckpt|step|epoch|iter|global_step)[-_]?(\d+)(?:/|$|[-_.])", re.I)
WEXT = (".safetensors", ".gguf", ".bin", ".pt", ".pth", ".ckpt")

def api(u):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=60) as f: return json.loads(f.read())
        except Exception: pass
    return None

rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
WEIGHT = ("safetensors", "gguf", "torch-pickle", "other-weights")
flag = []; tot_w = 0.0
for r in rows:
    c = r.get("bytes_in_window") or {}
    w = sum(float(v or 0) for k, v in c.items() if k.split(":")[0] in WEIGHT)
    if w <= 0: continue
    tot_w += w
    if STEP.search(r["id"].split("/")[-1]) or ((r.get("files") or 0) >= 40 and w > 5e9):
        flag.append((r["id"], w))
flag.sort(key=lambda x: -x[1])
LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 120
series_w = delta_w = 0.0; n_series = 0; n_checked = 0; ex = []
for rid, w in flag[:LIMIT]:
    t = api(f"{H}/api/models/{rid}/tree/main?recursive=1&expand=1")
    if not isinstance(t, list): continue
    n_checked += 1
    groups = collections.defaultdict(float)
    for f in t:
        if f.get("type") != "file": continue
        p = f.get("path", "")
        if not p.lower().endswith(WEXT): continue
        m = STEP.search(p)
        if not m: continue
        sz = int(f.get("size") or (f.get("lfs") or {}).get("size") or 0)
        groups[int(m.group(1))] += sz
    if len(groups) >= 2:
        n_series += 1
        ordered = sorted(groups.items())
        s = sum(v for _, v in ordered)
        d = sum(v for _, v in ordered[1:])           # every checkpoint but the first has a predecessor
        series_w += s; delta_w += d
        if len(ex) < 8: ex.append((rid, len(groups), s))
print(f"weight bytes in sample {tot_w/1e12:.2f} TB;  flagged by shape {len(flag)};  trees read {n_checked}")
print(f"REAL checkpoint series (>=2 step-numbered weight groups): {n_series} repos")
print(f"  bytes in series:              {series_w/1e12:.3f} TB = {100*series_w/max(tot_w,1):.1f}% of weight bytes")
print(f"  with a predecessor on the Hub: {delta_w/1e12:.3f} TB = {100*delta_w/max(tot_w,1):.1f}% of weight bytes  <- delta-eligible")
print(f"  (first pass claimed 43.6%; the difference is quant repos and multi-shard files, not checkpoints)")
print("\nexamples:")
for rid, k, s in sorted(ex, key=lambda x: -x[2]): print(f"  {s/1e9:7.1f} GB  {k:3d} checkpoints  {rid}")
json.dump({"tot_w": tot_w, "series_w": series_w, "delta_w": delta_w, "n_series": n_series,
           "n_checked": n_checked}, open(f"{B}/qgq/ckpt.json", "w"), indent=1)
print("CKPTPROBE2_DONE")
