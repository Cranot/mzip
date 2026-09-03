#!/usr/bin/env python3
"""Name the unmeasured half of the week's bytes.

The upload census classified files into coarse buckets, and three of the biggest -- archive, other,
torch-pickle -- are counted as zero saving because nothing in this programme has ever been run on them.
A bucket cannot be levered until it is named, so this re-lists a sample of the SAME population and
histograms bytes by full file extension, separately for models and datasets, reporting what the coarse
buckets are actually made of."""
import collections, json, os, random, re, subprocess, time
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/upload-mix"
N = {"models": 220, "datasets": 220}

def get(url):
    for t in range(4):
        hf = f"{OUT}/he.tmp"
        r = subprocess.run(["curl", "-sS", "-g", "--max-time", "120", "-D", hf, url], capture_output=True, text=True)
        st = 0; link = None
        try:
            for l in open(hf):
                if l.startswith("HTTP/"): st = int(l.split()[1])
                if l.lower().startswith("link:"):
                    m = re.search(r"<([^>]+)>;\s*rel=\"next\"", l); link = m.group(1) if m else None
        except Exception: pass
        if st == 429: time.sleep(45 * (t + 1)); continue
        if st in (401, 403, 404): return None, None
        try: return json.loads(r.stdout), link
        except Exception: time.sleep(4)
    return None, None

def ext_of(path):
    b = path.rsplit("/", 1)[-1].lower()
    if "." not in b: return "(no extension)"
    parts = b.split(".")
    e = parts[-1]
    if e in ("gz", "zst", "bz2", "xz", "lz4") and len(parts) > 2:
        return parts[-2] + "." + e          # keep jsonl.gz, tar.gz, parquet.zst distinct
    return e

for kind in ("models", "datasets"):
    pop = json.load(open(f"{OUT}/population_{kind}.json"))["rows"]
    shuffled = list(pop); random.Random(20260902).shuffle(shuffled)
    by = collections.Counter(); files = collections.Counter(); done = 0; capped = 0
    for m in shuffled:
        if done >= N[kind]: break
        url = f"{H}/api/{kind}/{m['id']}/tree/main?recursive=true&limit=1000"
        pages = 0
        while url and pages < 5:
            d, nxt = get(url); pages += 1
            if not isinstance(d, list): break
            for f in d:
                if f.get("type") != "file": continue
                e = ext_of(f.get("path", "")); by[e] += int(f.get("size") or 0); files[e] += 1
            if not nxt: break
            if pages == 5: capped += 1
            url = nxt
        done += 1
        if done % 50 == 0: print(f"  {kind}: {done}/{N[kind]}", flush=True)
        time.sleep(0.1)
    tot = max(sum(by.values()), 1)
    print(f"\n=== {kind}: {done} repos, {tot/1e12:.2f} TB listed, {capped} listings capped")
    print(f"{'extension':22s} {'bytes':>12s} {'share':>7s} {'files':>8s}")
    for e, b in by.most_common(28):
        print(f"{e[:22]:22s} {b/1e9:11.1f}G {100*b/tot:6.2f}% {files[e]:8d}")
    json.dump({e: [b, files[e]] for e, b in by.items()}, open(f"{OUT}/ext_{kind}.json", "w"), indent=1)
print("EXT_CENSUS_DONE")
