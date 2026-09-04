#!/usr/bin/env python3
"""Parent discovery against a ROOT INDEX, over the whole solo pool, with a second tensor per match.

RESULT 124 guessed ONE root per upload -- the most-downloaded model of its architecture signature --
and still found derivatives in half the keyed solos; 20 were "independent" against that single guess
and 14 had no guess at all. This removes the single-guess barrier:

  1. ROOT INDEX. For every architecture signature in the solo pool, list the top 60 models of that
     model_type by downloads, keep up to 8 whose config matches the signature, and fingerprint each once
     (64 KiB of the largest non-embedding 2D BF16 tensor, by name). Each solo repo's DECLARED base, if it
     has one, joins the index too. Fingerprints cache to disk.
  2. EVERY keyed solo repo (not the largest 80) is correlated against every root sharing its signature
     and tensor name; best r wins.
  3. Every match above 0.99 fetches a SECOND tensor from both sides and must agree there too -- the
     R108 lesson: one slice finds candidates, two confirm them.
Paced to the Hub's 500-per-5-minute window. Output is byte-weighted over the whole solo pool."""
import json, sys, time, collections, os
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from lineage5 import arch_key
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
PACE = 0.35

def paced(x):
    time.sleep(PACE); return x

def slices(rid, nel=32768, second=False):
    """(name1, w1, name2, w2): the largest and the largest-with-different-tail 2D non-embedding BF16 tensors"""
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    shards = sorted(set((idx or {}).get("weight_map", {}).values())) or ["model.safetensors"]
    for fn in shards[:4]:
        hb, off = st_header(rid, fn); time.sleep(PACE)
        if not hb: continue
        c = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in hb.items()
             if isinstance(v, dict) and v.get("dtype") == "BF16" and len(v.get("shape", [])) == 2
             and "embed" not in k and "lm_head" not in k]
        if not c: continue
        c.sort(reverse=True)
        tail = lambda k: ".".join(k.split(".")[-3:])
        picks = [c[0]]
        if second:
            s2 = next((x for x in c[1:] if tail(x[1]) != tail(c[0][1])), None)
            if s2: picks.append(s2)
        out = []
        for _sz, k, v in picks:
            raw = grab(rid, fn, off + v["data_offsets"][0], nel * 2); time.sleep(PACE)
            if raw is None: return None
            out.append((k, np.frombuffer(raw.tobytes(), dtype=np.uint16).astype(np.int32)))
        return out
    return None

def corr(wa, wb):
    n = min(wa.size, wb.size)
    a = bf16_to_f32(wa[:n].astype(np.uint16)).astype(np.float64); b = bf16_to_f32(wb[:n].astype(np.uint16)).astype(np.float64)
    a = np.nan_to_num(a); b = np.nan_to_num(b)
    return (float(np.corrcoef(a, b)[0, 1]) if a.std() > 0 and b.std() > 0 else 0.0), wa[:n] - wb[:n]

def main():
    keys = json.load(open(f"{B}/qgq/archkeys.json"))
    L5 = json.load(open(f"{B}/qgq/lineage5.json"))
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    matched = set()
    for c in L5["clusters"]:
        if len(c) >= 7 and isinstance(c[6], dict): matched |= set(c[6].get("exact", [])) | set(c[6].get("perturbed", []))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    solo = []
    for r in rows:
        cb = r.get("bytes_in_window") or {}
        bf = sum(float(v or 0) for k, v in cb.items() if k in ("safetensors:BF16", "safetensors:F16"))
        if bf >= 200e6 and r["id"] not in matched and keys.get(r["id"]): solo.append((r["id"], bf, keys[r["id"]]))
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else len(solo)
    solo.sort(key=lambda x: -x[1]); solo = solo[:LIMIT]
    sigs = collections.Counter(k for _, _, k in solo)
    print(f"keyed solo BF16 repos: {len(solo)} ({sum(b for _, b, _ in solo)/1e12:.2f} TB), {len(sigs)} architecture signatures", flush=True)

    # 1. root index
    ipath = f"{B}/qgq/rootindex.json"; cpath = f"{B}/qgq/rootslices.npz"
    index = json.load(open(ipath)) if os.path.exists(ipath) else {}
    slc = dict(np.load(cpath, allow_pickle=True)) if os.path.exists(cpath) else {}
    t0 = time.time()
    # one keyed top-60 list per MODEL TYPE (many signatures share a type); keys persist across runs
    kpath = f"{B}/qgq/rootkeys.json"
    rootkeys = json.load(open(kpath)) if os.path.exists(kpath) else {}
    for i, (sig, n) in enumerate(sigs.most_common()):
        if sig in index: continue
        mt = sig.split("|")[0]
        if mt not in rootkeys:
            lst = api(f"{H}/api/models?filter={mt}&sort=downloads&direction=-1&limit=60") or []; time.sleep(PACE)
            keyed = []
            for m in lst:
                rid = m.get("id") or m.get("modelId")
                if not rid: continue
                try: k = arch_key(rid)
                except Exception: k = None
                time.sleep(PACE)
                keyed.append((rid, k))
            rootkeys[mt] = keyed; json.dump(rootkeys, open(kpath, "w"))
        index[sig] = [rid for rid, k in rootkeys[mt] if k == sig][:8]
        if (i + 1) % 10 == 0:
            json.dump(index, open(ipath, "w")); print(f"  index {i+1}/{len(sigs)} signatures, {time.time()-t0:.0f}s", flush=True)
    json.dump(index, open(ipath, "w"))
    # declared bases join the index for their own repo
    declared = {}
    for rid, _, _ in solo:
        for x in ((tags.get(rid) or {}).get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3: declared[rid] = ":".join(p[2:]); break
    allroots = set(r for v in index.values() for r in v) | set(declared.values())
    print(f"root index: {sum(len(v) for v in index.values())} roots over {len(index)} signatures, +{len(declared)} declared bases; fingerprinting {len(allroots - set(slc.keys()))} new", flush=True)
    for j, root in enumerate(sorted(allroots)):
        if root in slc: continue
        s = slices(root, second=True)
        if s: slc[root] = np.array([(k, w) for k, w in s], dtype=object)
        if (j + 1) % 25 == 0:
            np.savez(cpath, **{k: v for k, v in slc.items()}); print(f"  fingerprints {j+1}/{len(allroots)}, {time.time()-t0:.0f}s", flush=True)
    np.savez(cpath, **{k: v for k, v in slc.items()})

    # 2./3. every solo against every root of its signature (+ its declared base), second tensor to confirm
    found = []; why = collections.Counter(); probed = 0; pb = 0.0
    for i, (rid, bf, sig) in enumerate(solo):
        if (i + 1) % 25 == 0: print(f"  ...{i+1}/{len(solo)}, found {len(found)}, {time.time()-t0:.0f}s", flush=True)
        cands = list(index.get(sig, [])) + ([declared[rid]] if rid in declared else [])
        cands = [c for c in cands if c != rid and c in slc]
        if not cands: why["no root fingerprinted for this signature"] += 1; continue
        s = slices(rid, second=True)
        if not s: why["no readable tensor"] += 1; continue
        probed += 1; pb += bf
        (k1, w1) = s[0]
        best = None
        for c in cands:
            rs = slc[c]
            rk1, rw1 = rs[0]
            if rk1 != k1: continue
            r, d = corr(w1, rw1)
            if best is None or r > best[0]: best = (r, c, d, rs)
        if not best: why["tensor name mismatch with every root"] += 1; continue
        r, root, d, rs = best
        if r <= 0.99: why["independent (best root r<0.99)"] += 1; continue
        # second tensor
        confirmed = True; r2 = float("nan")
        if len(s) > 1 and len(rs) > 1 and s[1][0] == rs[1][0]:
            r2, _ = corr(s[1][1], rs[1][1]); confirmed = r2 > 0.99
        if not confirmed: why["first tensor matched, second did not"] += 1; continue
        ident = float(np.mean(d == 0)); one = float(np.mean(np.abs(d) == 1)); hd = h0(d)
        kind = "exact" if ident > 0.999 else ("perturbed" if one > 0.3 else "fine-tune")
        found.append(dict(rid=rid, root=root, r=r, r2=r2, ident=ident, one=one, h=hd, kind=kind, declared=rid in declared, bytes=bf))
        print(f"  {rid[:30]:30s} <- {root[:26]:26s} r={r:.4f} r2={r2:.3f} ident={100*ident:5.1f}% h={hd:5.2f} {kind:9s} {'tagged' if rid in declared else 'UNTAGGED'}", flush=True)
    fb = sum(x["bytes"] for x in found)
    print(f"\nprobed {probed} of {len(solo)} keyed solo repos ({pb/1e12:.2f} TB); DERIVATIVE (two tensors): {len(found)} = {100*len(found)/max(probed,1):.1f}% by repo, {100*fb/max(pb,1):.1f}% by bytes")
    kb = collections.defaultdict(float); kn = collections.Counter()
    for x in found: kb[x["kind"]] += x["bytes"]; kn[x["kind"]] += 1
    for k in ("exact", "fine-tune", "perturbed"):
        if kn[k]: print(f"  {k:10s} n={kn[k]:3d}  {100*kb[k]/max(pb,1):5.1f}% of probed bytes  h median {np.median([x['h'] for x in found if x['kind']==k]):.2f}  untagged {sum(1 for x in found if x['kind']==k and not x['declared'])}")
    print("why the rest did not:")
    for k, v in why.most_common(): print(f"  {v:4d}  {k}")
    json.dump(dict(probed=probed, pb=pb, found=found, why=dict(why), n_solo=len(solo), solo_b=sum(b for _, b, _ in solo)), open(f"{B}/qgq/rootindex_result.json", "w"), indent=1, default=float)
    print("ROOTINDEX_DONE")

if __name__ == "__main__":
    main()
