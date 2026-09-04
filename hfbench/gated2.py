#!/usr/bin/env python3
"""Three checks on the gated Qwen3.6-35B-A3B cluster, zero gated bytes read:
 1. what single LFS oid do all gated trees share, and do they expose xetHash;
 2. does the public base's vision tower account for 71.904 - 70.214 GB (header of the public base, readable);
 3. which READABLE repos in the sample share the base's architecture key -- the proxy for pricing the gated class."""
import json, sys, time, re, urllib.request, urllib.error, collections
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header
from hfget import get_json
from lineage5 import arch_key
B = "/root/mzip-hfbench"; H = "https://huggingface.co"; PACE = 0.5
BASE = "Qwen/Qwen3.6-35B-A3B"

def tree(rid):
    req = urllib.request.Request(f"{H}/api/models/{rid}/tree/main", headers={"User-Agent": "mzip-hfbench"})
    with urllib.request.urlopen(req, timeout=30) as r: lst = json.loads(r.read())
    time.sleep(PACE); return lst

# 1
for rid in ("iionai/1787981879", "goal612/realmadrid9999-king-father"):
    lst = tree(rid)
    st = [x for x in lst if x["path"].endswith(".safetensors")]
    print(f"{rid}: {len(st)} safetensors entries; with lfs: {sum(1 for x in st if x.get('lfs'))}, with xetHash: {sum(1 for x in st if x.get('xetHash'))}")
    for x in st[:2]: print("   ", {k: (v if k != 'lfs' else {kk: str(vv)[:20] for kk, vv in v.items()}) for k, v in x.items() if k in ('path', 'size', 'lfs', 'xetHash')})
    for x in st:
        if x.get("lfs"): print("    lfs entry:", x["path"], x["size"], str(x["lfs"].get("oid"))[:16]); break

# 2
idx = get_json(f"{H}/{BASE}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
shards = sorted(set(idx["weight_map"].values()))
vis = 0; txt = 0; kinds = collections.Counter()
for fn in shards:
    hb, off = st_header(BASE, fn); time.sleep(PACE)
    for k, v in hb.items():
        if not isinstance(v, dict) or "data_offsets" not in v: continue
        sz = v["data_offsets"][1] - v["data_offsets"][0]
        if "visual" in k or "vision" in k: vis += sz; kinds[k.split(".")[0] + "." + k.split(".")[1]] += sz
        else: txt += sz
print(f"\n{BASE}: text tensors {txt/1e9:.3f} GB, vision tensors {vis/1e9:.3f} GB (71.904 - 70.214 = 1.690 GB expected)  {dict(kinds)}")

# 3
k = json.load(open(f"{B}/qgq/archkeys.json"))
bk = arch_key(BASE); time.sleep(PACE)
rows = {json.loads(l)["id"]: json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()}
def bf(r):
    cb = r.get("bytes_in_window") or {}
    return sum(float(v or 0) for kk, v in cb.items() if kk in ("safetensors:BF16", "safetensors:F16"))
same = [(rid, bf(rows[rid])) for rid, kk in k.items() if kk == bk and rid in rows]
print(f"\nbase key {bk}\nreadable sample repos with the SAME key: {len(same)}, {sum(b for _, b in same)/1e12:.2f} TB")
for rid, b in sorted(same, key=lambda x: -x[1])[:15]: print(f"   {b/1e9:7.1f} GB  {rid}")
gated = [(rid, bf(r)) for rid, r in rows.items() if rid in k and not k[rid] and int(bf(r)) in (71903775952, 70214493024)]
print(f"gated repos at exactly the base's byte total or the text-only total: {len(gated)}, {sum(b for _, b in gated)/1e12:.2f} TB")
near = [(rid, bf(r)) for rid, r in rows.items() if rid in k and not k[rid] and 69e9 <= bf(r) <= 73e9 and int(bf(r)) not in (71903775952, 70214493024)]
print(f"gated repos within 69-73 GB but NOT at those two totals: {len(near)}, {sum(b for _, b in near)/1e12:.2f} TB  e.g. {[(r, round(b/1e9,3)) for r, b in near[:6]]}")
print("GATED2_DONE")
