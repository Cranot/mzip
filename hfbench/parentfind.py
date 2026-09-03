#!/usr/bin/env python3
"""Parent discovery: for BF16 uploads with no parent in the sample, guess the root and test it.

Every derivative lever so far started from what the Hub declares (base_model tags) or from siblings
in the same upload week (the twin census). But most fine-tunes descend from a few popular bases that
were uploaded months ago, so a within-week census calls them "solo". The guess: the most-downloaded
model on the Hub with the same architecture signature (model_type, hidden, layers, heads, vocab,
intermediate) is the root. Fetch its slice, correlate. r > 0.99 means the upload is a derivative and
codes against the root at 0.9-3.5 bits/weight instead of 10.7.

Also, on each matched pair: is the changed-weight mask clustered by row (neuron)? h(mask | row) vs
h(mask) says whether a position-aware delta coder beats order-0."""
import json, sys, collections, time
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from lineage5 import arch_key
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def slice2d(rid, want=None, nel=32768):
    """(name, uint16 words) of the largest 2D non-embedding BF16 tensor, or the named one"""
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json")
    shards = sorted(set((idx or {}).get("weight_map", {}).values())) or ["model.safetensors"]
    for fn in shards[:4]:
        hb, off = st_header(rid, fn)
        if not hb: continue
        if want and want in hb:
            v = hb[want]; k = want
        else:
            if want: continue
            c = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in hb.items()
                 if isinstance(v, dict) and v.get("dtype") == "BF16" and len(v.get("shape", [])) == 2
                 and "embed" not in k and "lm_head" not in k]
            if not c: continue
            _, k, v = max(c)
        raw = grab(rid, fn, off + v["data_offsets"][0], nel * 2)
        if raw is None: continue
        return k, np.frombuffer(raw.tobytes(), dtype=np.uint16).astype(np.int32), tuple(v["shape"])
    return None, None, None

def root_candidates(mt, key):
    """most-downloaded Hub models tagged with this model_type whose config matches the key"""
    out = []
    lst = api(f"{H}/api/models?filter={mt}&sort=downloads&direction=-1&limit=40")
    for m in (lst or []):
        rid = m.get("id") or m.get("modelId")
        if not rid: continue
        try:
            if arch_key(rid) == key: out.append(rid)
        except Exception: pass
        if len(out) >= 3: break
    return out

def main():
    L = json.load(open(f"{B}/qgq/lineage5.json"))
    matched = set()
    for c in L["clusters"]:
        if len(c) >= 7 and isinstance(c[6], dict):
            matched |= set(c[6].get("exact", [])) | set(c[6].get("perturbed", []))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    cand = []
    for r in rows:
        c = r.get("bytes_in_window") or {}
        bf = sum(float(v or 0) for k, v in c.items() if k in ("safetensors:BF16", "safetensors:F16"))
        if bf >= 200e6 and r["id"] not in matched: cand.append((r["id"], bf))
    cand.sort(key=lambda x: -x[1])
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 80
    print(f"BF16 repos >=200MB not matched within the sample: {len(cand)} ({sum(b for _, b in cand)/1e12:.2f} TB); probing {min(LIMIT, len(cand))}", flush=True)
    keycache = {}; rootcache = {}
    found = 0; fb = 0.0; probed = 0; pb = 0.0; why = collections.Counter(); rows_out = []
    t0 = time.time()
    for i, (rid, bf) in enumerate(cand[:LIMIT]):
        if (i + 1) % 20 == 0: print(f"  ...{i+1}, {time.time()-t0:.0f}s, found {found}", flush=True)
        try:
            key = arch_key(rid)
            if not key: why["no config"] += 1; continue
            mt = key.split("|")[0]
            if key not in rootcache: rootcache[key] = root_candidates(mt, key)
            roots = [x for x in rootcache[key] if x != rid]
            if not roots: why["no popular root with this signature"] += 1; continue
            k, w, shp = slice2d(rid)
            if w is None: why["no readable tensor"] += 1; continue
            probed += 1; pb += bf
            best = None
            for root in roots:
                _, wr, _ = slice2d(root, want=k)
                if wr is None: continue
                n = min(w.size, wr.size)
                a, b = bf16_to_f32(w[:n].astype(np.uint16)).astype(np.float64), bf16_to_f32(wr[:n].astype(np.uint16)).astype(np.float64)
                r = float(np.corrcoef(a, b)[0, 1]) if a.std() > 0 and b.std() > 0 else 0.0
                if best is None or r > best[0]: best = (r, root, w[:n] - wr[:n])
            if not best: why["root slice unreadable"] += 1; continue
            r, root, dlt = best
            decl = any(x.startswith("base_model:") for x in ((tags.get(rid) or {}).get("tags") or []))
            if r > 0.99:
                found += 1; fb += bf
                ident = float(np.mean(dlt == 0)); one = float(np.mean(np.abs(dlt) == 1)); hd = h0(dlt)
                kind = "exact" if ident > 0.999 else ("perturbed" if one > 0.3 else "fine-tune")
                # row clustering of the change mask, if the slice spans several rows
                cols = shp[1] if shp and len(shp) == 2 else 0
                rowgain = float("nan")
                if cols and dlt.size >= 4 * cols:
                    mask = (dlt[: (dlt.size // cols) * cols] != 0).astype(np.int64).reshape(-1, cols)
                    hm = h0(mask.ravel()); hr = sum(h0(mask[j]) for j in range(mask.shape[0])) / mask.shape[0]
                    rowgain = hm - hr
                rows_out.append(dict(rid=rid, root=root, r=r, ident=ident, one=one, h=hd, kind=kind, declared=decl, bytes=bf, rowgain=rowgain))
                print(f"  {rid[:34]:34s} <- {root[:30]:30s} r={r:.4f} ident={100*ident:5.1f}% |d|=1 {100*one:5.1f}% h={hd:5.2f} {kind:9s} {'tagged' if decl else 'UNTAGGED'}", flush=True)
            else:
                why[f"best root r<0.99 (independent)"] += 1
        except Exception as e:
            why["exception"] += 1
    print(f"\nprobed {probed} unmatched repos ({pb/1e12:.2f} TB); DERIVATIVE of a popular root: {found} = {100*found/max(probed,1):.1f}% by repo, {100*fb/max(pb,1):.1f}% by bytes")
    kinds = collections.Counter(x["kind"] for x in rows_out)
    print(f"  kinds: {dict(kinds)};  untagged among found: {sum(1 for x in rows_out if not x['declared'])}")
    if rows_out:
        hs = [x["h"] for x in rows_out]
        print(f"  delta cost bits/weight: median {np.median(hs):.2f}  (vs 10.7 flat)")
        rg = [x["rowgain"] for x in rows_out if x["rowgain"] == x["rowgain"]]
        if rg: print(f"  row-conditioned change-mask gain: median {np.median(rg):.3f} bits/weight over {len(rg)} pairs")
    print("why the rest did not match:")
    for k, v in why.most_common(): print(f"  {v:4d}  {k}")
    json.dump(dict(probed=probed, pb=pb, found=found, fb=fb, rows=rows_out, why=dict(why)), open(f"{B}/qgq/parentfind.json", "w"), indent=1)
    print("PARENTFIND_DONE")

if __name__ == "__main__":
    main()
