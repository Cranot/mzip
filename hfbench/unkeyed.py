#!/usr/bin/env python3
"""Why do 208 solo BF16 repos (10.14 TB, 72% of solo BF16 bytes) have no architecture key?
For the largest N: is there a config.json at all (HTTP status, not a silent None), what model_type /
nesting does it use, and what does the repo tree look like. A 429 recorded as 'no key' would be an
instrument fault, not a fact about the pool."""
import json, sys, time, collections, urllib.request, urllib.error
sys.path.insert(0, "/root/mzip-hfbench")
B = "/root/mzip-hfbench"; H = "https://huggingface.co"; PACE = 0.5
N = int(sys.argv[1]) if len(sys.argv) > 1 else 24

def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "mzip-hfbench"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r: return r.status, r.read()
    except urllib.error.HTTPError as e: return e.code, b""
    except Exception as e: return -1, str(e).encode()

k = json.load(open(f"{B}/qgq/archkeys.json"))
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
un = []
for r in rows:
    if r["id"] in k and not k[r["id"]]:
        cb = r.get("bytes_in_window") or {}
        un.append((sum(float(v or 0) for kk, v in cb.items() if kk in ("safetensors:BF16", "safetensors:F16")), r["id"]))
un.sort(reverse=True)
tot = sum(b for b, _ in un)
print(f"unkeyed solo repos: {len(un)}, {tot/1e12:.2f} TB; largest {N} = {sum(b for b,_ in un[:N])/tot*100:.0f}% of that")
why = collections.Counter(); wb = collections.Counter()
for b, rid in un[:N]:
    st, body = fetch(f"{H}/{rid}/resolve/main/config.json"); time.sleep(PACE)
    st2, tree = fetch(f"{H}/api/models/{rid}/tree/main"); time.sleep(PACE)
    names = [x.get("path") for x in (json.loads(tree) if st2 == 200 and tree else [])]
    top = [n for n in names if "/" not in n][:12]; dirs = sorted(set(n.split("/")[0] for n in names if "/" in n))[:8]
    if st == 200:
        try: cfg = json.loads(body)
        except Exception: cfg = {}
        mt = cfg.get("model_type"); nested = [x for x in cfg if isinstance(cfg[x], dict) and "hidden_size" in cfg[x]]
        tag = f"config ok: model_type={mt} hidden_size={'yes' if 'hidden_size' in cfg else 'no'} nested={nested[:3]} keys={list(cfg)[:8]}"
        cls = "config present, key rule missed it"
    else:
        tag = f"config.json HTTP {st}"; cls = f"config.json HTTP {st}"
    why[cls] += 1; wb[cls] += b
    print(f"{b/1e9:7.1f} GB  {rid[:44]:44s} {tag}")
    print(f"           top: {top}  dirs: {dirs}")
print("\nby class:")
for c, n in why.most_common(): print(f"  {n:3d}  {wb[c]/1e12:5.2f} TB  {c}")
print("UNKEYED_DONE")
