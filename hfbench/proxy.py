#!/usr/bin/env python3
"""Price the gated Qwen3.6-35B-A3B cluster through its READABLE siblings: the sample repos that share
the base's architecture key and byte total (Affine-subnet fine-tunes that were not gated). For each,
three tensors against the public base -- an early attention projection, a mid-layer one, and the
largest tensor (an expert stack) -- 128K weights each: r, identical fraction, one-ulp fraction, delta
h0 on the remapped alphabet (R123's FSE coder lands at 1.07x that). Also: what the 70.214 GB variant
drops relative to 71.904 GB (header prefix sums), and which shard every gated tree shares."""
import json, sys, time, collections, urllib.request
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from kquant_given import h0
H = "https://huggingface.co"; PACE = 0.4; NEL = 131072
BASE = "Qwen/Qwen3.6-35B-A3B"
SIBS = ["Shatoria/Affine-5ghntktyzq-hope3", "HarperJane/20260829-050111", "jacob-rojic/mirror-afgod1079-affine-5eqgpsdo6a-cp2700",
        "ammazon/albedo-qwen3.6-35b-ant", "leary-criste/affine-5g4yy75zuz-v30", "topcatmax/mirror-leary-criste-affine-5g4yy75zuz-q2"]

def headers(rid):
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    shards = sorted(set((idx or {}).get("weight_map", {}).values())) or ["model.safetensors"]
    out = {}
    for fn in shards:
        hb, off = st_header(rid, fn); time.sleep(PACE)
        if not hb: continue
        for k, v in hb.items():
            if isinstance(v, dict) and "data_offsets" in v: out[k] = (fn, off, v)
    return out

def summary(rid, hd):
    pre = collections.Counter()
    for k, (fn, off, v) in hd.items():
        sz = v["data_offsets"][1] - v["data_offsets"][0]
        p = k.split(".")
        pre[".".join(p[:2]) if not p[0].startswith("model") else (p[1] if len(p) > 1 else p[0])] += sz
    layers = sorted({int(k.split(".layers.")[1].split(".")[0]) for k in hd if ".layers." in k})
    tot = sum(v["data_offsets"][1] - v["data_offsets"][0] for _, _, v in hd.values())
    print(f"  {rid[:44]:44s} {tot/1e9:.3f} GB  layers 0..{layers[-1] if layers else '-'}  mtp={sum(sz for k, sz in pre.items() if 'mtp' in k)/1e9:.3f} GB  visual={sum(sz for k, sz in pre.items() if 'visual' in k or 'vision' in k)/1e9:.3f} GB  dtypes={collections.Counter(v['dtype'] for _,_,v in hd.values()).most_common(3)}")

def read(rid, hd, k, nel=NEL):
    fn, off, v = hd[k]
    raw = grab(rid, fn, off + v["data_offsets"][0], nel * 2); time.sleep(PACE)
    return None if raw is None else np.frombuffer(raw.tobytes(), dtype=np.uint16)

def remap_h0(d):
    """h0 of the delta on a remapped alphabet: the 254 most frequent deltas + escape (R123's construction)"""
    vals, cnt = np.unique(d, return_counts=True)
    order = np.argsort(-cnt); keep = cnt[order[:254]]; esc = cnt[order[254:]].sum()
    c = np.concatenate([keep, [esc]]) if esc else keep
    p = c / c.sum(); hs = -(p * np.log2(p)).sum()
    return hs + (esc / c.sum()) * 16  # escaped symbols carry the raw 16-bit delta

bh = headers(BASE); print("base:"); summary(BASE, bh)
two = [k for k, (_, _, v) in bh.items() if v["dtype"] == "BF16" and len(v["shape"]) == 2 and "embed" not in k and "lm_head" not in k and "visual" not in k]
qs = sorted([k for k in two if "q_proj" in k and ".layers." in k], key=lambda k: int(k.split(".layers.")[1].split(".")[0]))
big = max(two, key=lambda k: bh[k][2]["data_offsets"][1] - bh[k][2]["data_offsets"][0])
picks = [qs[0], qs[len(qs) // 2], big]
print("tensors:", picks)
bw = {k: read(BASE, bh, k) for k in picks}
print(f"\n{'sibling':44s} {'tensor':10s} {'r':>8s} {'ident':>7s} {'1-ulp':>7s} {'h0 remap':>9s} {'FSE est':>8s} {'% of Xet':>8s}")
rows = []
for rid in SIBS:
    hd = headers(rid); summary(rid, hd)
    for k in picks:
        if k not in hd: print(f"    {k} absent"); continue
        w = read(rid, hd, k)
        if w is None: print("    unreadable"); continue
        a = bf16_to_f32(bw[k]).astype(np.float64); b = bf16_to_f32(w).astype(np.float64)
        r = float(np.corrcoef(np.nan_to_num(a), np.nan_to_num(b))[0, 1])
        d = w.astype(np.int32) - bw[k].astype(np.int32)
        ident = float(np.mean(d == 0)); one = float(np.mean(np.abs(d) == 1)); hh = remap_h0(d); fse = 1.07 * hh
        rows.append((rid, k, r, ident, one, hh, fse))
        print(f"  {rid[:42]:42s} {k.split('.')[-2][:10]:10s} {r:8.5f} {100*ident:6.1f}% {100*one:6.1f}% {hh:9.3f} {fse:8.3f} {100*fse/16:7.1f}%")
if rows:
    fs = [x[6] for x in rows]
    print(f"\nacross {len(rows)} tensor pairs: FSE-est median {np.median(fs):.2f} b/w = {100*np.median(fs)/16:.1f}% of Xet; mean {np.mean(fs):.2f} b/w = {100*np.mean(fs)/16:.1f}%; range {min(fs):.2f}-{max(fs):.2f}")
    print(f"  (R123: the FSE coder lands at 1.07x the remapped h0; Xet = 16 bits/weight flat for these files, BG4+LZ4 gains ~0% on bf16 weights, R7)")

# shared shard across gated trees
def tree(rid):
    req = urllib.request.Request(f"{H}/api/models/{rid}/tree/main", headers={"User-Agent": "mzip-hfbench"})
    with urllib.request.urlopen(req, timeout=30) as r: lst = json.loads(r.read())
    time.sleep(PACE); return {x["path"]: (x.get("size"), (x.get("lfs") or {}).get("oid")) for x in lst if x["path"].endswith(".safetensors")}
t1, t2 = tree("iionai/1787981879"), tree("goal612/realmadrid9999-king-father")
o2 = {v[1]: p for p, v in t2.items()}
print("\nshard(s) shared by two gated repos with different layouts:", [(p, v[0], o2[v[1]]) for p, v in t1.items() if v[1] in o2])
print("PROXY_DONE")
