#!/usr/bin/env python3
"""The time axis: how much of the week is a checkpoint whose predecessor is also on the Hub?

Every lever so far crosses the REPO boundary. None crosses TIME. But a training run uploads
checkpoint-1000, checkpoint-2000, ... and consecutive checkpoints differ in exactly the way a fine-tune
differs from its base: low mantissa bits, every chunk hash different, dedup sees pure novelty. If a
meaningful share of the week's weight bytes is the Nth checkpoint of a run whose (N-1)th is on the Hub,
that is the fine-tune lever again, with the parent guaranteed present and same-shaped.

File trees only. A repo counts when it holds >=2 sibling directories or files matching a step/epoch
pattern with weight files inside, or when a repo named *-step-N / *-ckpt-N has a sibling repo one
step earlier under the same owner."""
import json, re, collections
B = "/root/mzip-hfbench"
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
STEP = re.compile(r"(checkpoint|ckpt|step|epoch|iter|global_step)[-_]?(\d+)", re.I)
WEIGHT = ("safetensors", "gguf", "torch-pickle", "other-weights")

tot_w = 0.0; series_w = 0.0; n_series = 0; n = 0
by_kind = collections.Counter()
examples = []
for r in rows:
    c = r.get("bytes_in_window") or {}
    w = sum(float(v or 0) for k, v in c.items() if k.split(":")[0] in WEIGHT)
    if w <= 0: continue
    n += 1; tot_w += w
    rid = r["id"]
    in_name = STEP.search(rid.split("/")[-1])
    files = r.get("files", 0) or 0
    # the sample carries no file list, only counts; a repo with many weight files and a step-named id,
    # or a very high file count with weights, is the checkpoint-series shape
    hit = None
    if in_name: hit = "step in repo name"
    elif files >= 40 and w > 5e9: hit = "many files + weights (checkpoint dirs)"
    if hit:
        n_series += 1; series_w += w; by_kind[hit] += w
        if len(examples) < 10: examples.append((rid, files, w))
print(f"sampled repos with weight bytes: {n}   weight bytes {tot_w/1e12:.2f} TB")
print(f"checkpoint-series shape: {n_series} repos ({100*n_series/max(n,1):.1f}%)  "
      f"{series_w/1e12:.2f} TB = {100*series_w/max(tot_w,1):.1f}% of weight bytes")
for k, v in by_kind.most_common(): print(f"  {100*v/max(tot_w,1):5.1f}%  {k}")
print("\nexamples:")
for rid, f, w in sorted(examples, key=lambda x: -x[2]):
    print(f"  {w/1e9:7.1f} GB  {f:4d} files  {rid}")
print("\n  This is a SHAPE census from repo names and file counts, not a measurement of delta cost.")
print("  It bounds how much of the week the time-axis lever could reach; the delta rate is the")
print("  fine-tune rate (48.5% on changed tensors) until measured on consecutive checkpoints.")
print("CKPTPROBE_DONE")
