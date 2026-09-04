#!/usr/bin/env python3
"""Hidden-basis permutation test over every keyed solo repo the root index called INDEPENDENT (best r < 0.99).

R127: two Qwen3.6-size uploads with r ~ 0 against the base were the base with its 2048 residual-stream
dimensions relabelled (a column permutation of every projection, the same p on the layer norm) -- 77%
byte-identical once un-permuted. Correlation cannot see that; column matching can, even on a slice: 64
rows x width of the largest tensor from both sides, cosine between every column pair, a match is a
column found at cos > 0.999. Random 64-dim columns sit near 0.1. Confirmed on a second tensor with the
SAME p (the R108 lesson, in permutation form). Paced."""
import json, sys, time, os, collections
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from kquant_given import h0
B = "/root/mzip-hfbench"; H = "https://huggingface.co"; PACE = 0.35; ROWS = 64

def slices2(rid, want=None):
    """{name: (uint16 [ROWS, width])} for the largest 2D non-embedding BF16 tensor and one with a different tail"""
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    shards = sorted(set((idx or {}).get("weight_map", {}).values())) or ["model.safetensors"]
    for fn in shards[:4]:
        hb, off = st_header(rid, fn); time.sleep(PACE)
        if not hb: continue
        c = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in hb.items()
             if isinstance(v, dict) and v.get("dtype") == "BF16" and len(v.get("shape", [])) == 2 and "embed" not in k and "lm_head" not in k]
        if not c: continue
        c.sort(reverse=True); tail = lambda k: ".".join(k.split(".")[-3:])
        picks = [c[0]]; s2 = next((x for x in c[1:] if tail(x[1]) != tail(c[0][1])), None)
        if s2: picks.append(s2)
        if want: picks = [x for x in c if x[1] in want] or picks
        out = {}
        for _sz, k, v in picks:
            w = v["shape"][1]; raw = grab(rid, fn, off + v["data_offsets"][0], ROWS * w * 2); time.sleep(PACE)
            if raw is None: continue
            u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
            if u.size < ROWS * w: continue
            out[k] = u.reshape(ROWS, w)
        return out
    return {}

def colmatch(A, Bm):
    """p (B column j <- A column p[j]) and the fraction of B columns with a cos>0.999 match"""
    def nz(M):
        M = bf16_to_f32(M).astype(np.float64); M = M - M.mean(axis=0, keepdims=True); return M / (np.linalg.norm(M, axis=0, keepdims=True) + 1e-30)
    S = nz(A).T @ nz(Bm); p = S.argmax(axis=0); m = S.max(axis=0)
    return p, float(np.mean(m > 0.999)), float(np.mean(p == np.arange(len(p))))

def main():
    RI = json.load(open(f"{B}/qgq/rootindex_result.json")); index = json.load(open(f"{B}/qgq/rootindex.json"))
    keys = json.load(open(f"{B}/qgq/archkeys.json")); found = {x["rid"] for x in RI["found"]}
    L5 = json.load(open(f"{B}/qgq/lineage5.json")); matched = set()
    for c in L5["clusters"]:
        if len(c) >= 7 and isinstance(c[6], dict): matched |= set(c[6].get("exact", [])) | set(c[6].get("perturbed", []))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    solo = []
    for r in rows:
        cb = r.get("bytes_in_window") or {}
        bf = sum(float(v or 0) for k, v in cb.items() if k in ("safetensors:BF16", "safetensors:F16"))
        if bf >= 200e6 and r["id"] not in matched and r["id"] not in found and keys.get(r["id"]) and index.get(keys[r["id"]]): solo.append((r["id"], bf, keys[r["id"]]))
    solo.sort(key=lambda x: -x[1])
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else len(solo); solo = solo[:LIMIT]
    print(f"independent-or-unresolved keyed solos with a root: {len(solo)} ({sum(b for _,b,_ in solo)/1e12:.2f} TB)", flush=True)
    rc = {}; hits = []; why = collections.Counter(); pb = 0; t0 = time.time()
    for i, (rid, bf, sig) in enumerate(solo):
        s = slices2(rid)
        if not s: why["no readable slice"] += 1; continue
        pb += bf; best = None
        for root in index[sig]:
            if root == rid: continue
            if root not in rc: rc[root] = slices2(root, want=set(s))
            rs = rc[root]
            for k in s:
                if k in rs and rs[k].shape == s[k].shape:
                    p, frac, ident = colmatch(rs[k], s[k])
                    if best is None or frac > best[1]: best = (root, frac, k, p, ident)
        if not best: why["no shared tensor name with any root"] += 1; continue
        root, frac, k, p, ident = best
        if frac < 0.9: why["independent (column match < 90%)"] += 1; continue
        # confirm on the second tensor with the same p
        conf = "unconfirmed"; res = float("nan")
        for k2 in s:
            if k2 != k and k2 in rc[root] and rc[root][k2].shape == s[k2].shape and rc[root][k2].shape[1] == len(p):
                d = s[k2].astype(np.int32) - rc[root][k2][:, p].astype(np.int32)
                res = h0(d.ravel()); conf = "confirmed" if np.mean(d == 0) > 0.3 else "second tensor disagrees"; break
        d1 = s[k].astype(np.int32) - rc[root][k][:, p].astype(np.int32)
        hits.append(dict(rid=rid, root=root, frac=frac, ident_after=float(np.mean(d1 == 0)), h=float(h0(d1.ravel())), identity_p=ident, conf=conf, bytes=bf))
        print(f"  {rid[:34]:34s} <- {root[:26]:26s} cols matched {100*frac:5.1f}%  p=identity {100*ident:5.1f}%  exact after unpermute {100*np.mean(d1==0):5.1f}%  h0 {h0(d1.ravel()):.2f}  {conf}", flush=True)
        if (i + 1) % 20 == 0: print(f"  ...{i+1}/{len(solo)} {time.time()-t0:.0f}s", flush=True)
    hb = sum(x["bytes"] for x in hits)
    print(f"\npermuted copies among the independent solos: {len(hits)} repos, {hb/1e12:.2f} of {pb/1e12:.2f} TB probed ({100*hb/max(pb,1):.1f}%); confirmed {sum(1 for x in hits if x['conf']=='confirmed')}")
    for k, v in why.most_common(): print(f"  {v:4d}  {k}")
    json.dump(dict(hits=hits, pb=pb, why=dict(why)), open(f"{B}/qgq/permtest.json", "w"), indent=1)
    print("PERMTEST_DONE")

if __name__ == "__main__":
    main()
