#!/usr/bin/env python3
"""Undeclared lineage at population scale, byte-weighted.

RESULT 100 found one cluster of 22 undeclared clones among the 92 largest BF16 repos. That is a
sighting, not a rate. This runs the same test over EVERY sampled repo with >=200 MB of BF16/F16
safetensors -- config.json for the architecture key, one 64 KiB slice of one non-embedding tensor,
pairwise correlation within each architecture group -- and weights the answer by bytes, so the output
is a share of the week's weight bytes, not a count of pairs.

Three outcomes per repo in a group: exact twin of another (dedup already has it), perturbed twin
(r>0.99, not identical: a derivative dedup cannot see and no tag admits), or independent. The
byte-weighted share of the middle class, added to the 27.7% that declares a parent, is the honest
floor for how much of the week's weight bytes has a parent on the Hub."""
import json, sys, collections, itertools, time
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json

def arch_key(rid):
    cfg = get_json(f"{H}/{rid}/resolve/main/config.json") or {}
    tc = cfg.get("text_config") or cfg
    k = (cfg.get("model_type"), tc.get("hidden_size"), tc.get("num_hidden_layers"),
         tc.get("num_attention_heads"), tc.get("vocab_size"), tc.get("intermediate_size"))
    return None if k[0] is None or k[1] is None else "|".join(str(x) for x in k)

def slice_of(rid):
    """64 KiB from EACH of two different 2D non-embedding tensors -- the largest, and the largest whose
    name differs in its last two components (a different projection, ideally a different layer).

    One tensor found candidates but did not confirm them: RESULT 104 addendum showed 2 of 6 census
    "twins" at r=0.963 and r=0.001 on a second tensor. A pair is a twin only if BOTH slices agree."""
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json")
    shards = sorted(set((idx or {}).get("weight_map", {}).values())) or ["model.safetensors"]
    for fn in shards[:3]:
        hb, off = st_header(rid, fn)
        if not hb: continue
        cands = []
        for k, v in hb.items():
            if not isinstance(v, dict) or v.get("dtype") not in ("BF16", "F16") or len(v.get("shape", [])) != 2: continue
            if "embed" in k or "lm_head" in k: continue
            sz = v["data_offsets"][1] - v["data_offsets"][0]
            if sz >= 65536: cands.append((sz, k, v))
        if not cands: continue
        cands.sort(reverse=True)
        first = cands[0]
        tail = lambda k: ".".join(k.split(".")[-3:])
        second = next((c for c in cands[1:] if tail(c[1]) != tail(first[1])), cands[1] if len(cands) > 1 else None)
        out = []
        for _sz, k, v in ([first] + ([second] if second else [])):
            raw = grab(rid, fn, off + v["data_offsets"][0], 65536)
            if raw is None: break
            r16 = np.frombuffer(raw.tobytes(), dtype=np.uint16)
            w = bf16_to_f32(r16) if v["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)
            out.append(w.astype(np.float64))
        if len(out) == 2: return first[1] + " + " + second[1], np.concatenate(out)
        if len(out) == 1: return first[1], out[0]
    return None, None

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = []; tot_w = 0.0
    for r in rows:
        c = r.get("bytes_in_window") or {}
        w = sum(float(v or 0) for k, v in c.items() if k.split(":")[0] in ("safetensors", "gguf", "torch-pickle", "other-weights"))
        tot_w += w
        bf = sum(float(v or 0) for k, v in c.items() if k in ("safetensors:BF16", "safetensors:F16"))
        if bf >= 200e6:
            decl = any(x.startswith("base_model:") for x in ((tags.get(r["id"]) or {}).get("tags") or []))
            cand.append((r["id"], bf, decl))
    cand.sort(key=lambda x: -x[1])
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else len(cand)
    print(f"weight bytes in sample {tot_w/1e12:.2f} TB; BF16/F16 repos >=200MB: {len(cand)} "
          f"({sum(x[1] for x in cand)/1e12:.2f} TB); probing {min(LIMIT, len(cand))}", flush=True)
    groups = collections.defaultdict(list); t0 = time.time(); n = 0
    for rid, bf, decl in cand[:LIMIT]:
        n += 1
        if n % 50 == 0: print(f"  ...{n} repos, {time.time()-t0:.0f}s", flush=True)
        try:
            key = arch_key(rid)
            if not key: continue
            k, w = slice_of(rid)
            if w is None or w.size < 4096: continue
            groups[key].append((rid, bf, decl, w))
        except Exception:
            continue
    exact_b = pert_b = indep_b = solo_b = 0.0; n_groups = 0
    cluster_rows = []
    for key, mem in groups.items():
        if len(mem) < 2:
            solo_b += sum(m[1] for m in mem); continue
        n_groups += 1
        # classify each member by its best match to any other member
        cls = {}
        for (ra, ba, da, wa), (rb, bb, dbb, wb) in itertools.combinations(mem, 2):
            m = min(wa.size, wb.size)
            # two slices are concatenated; a twin must agree on BOTH halves, not on the concatenation
            h = m // 2
            def corr(x, y):
                return float(np.corrcoef(x, y)[0, 1]) if np.std(x) > 0 and np.std(y) > 0 else 0.0
            c1 = corr(wa[:h], wb[:h]); c2 = corr(wa[h:m], wb[h:m])
            c = min(c1, c2)
            ident = float(np.mean(wa[:m] == wb[:m]))
            lvl = 2 if (c > 0.99 and ident > 0.999) else (1 if c > 0.99 else 0)
            for r in (ra, rb): cls[r] = max(cls.get(r, 0), lvl)
        ex = [m for m in mem if cls.get(m[0], 0) == 2]; pe = [m for m in mem if cls.get(m[0], 0) == 1]
        ind = [m for m in mem if cls.get(m[0], 0) == 0]
        exact_b += sum(m[1] for m in ex); pert_b += sum(m[1] for m in pe); indep_b += sum(m[1] for m in ind)
        if ex or pe:
            und = sum(1 for m in ex + pe if not m[2])
            cluster_rows.append((key, len(mem), len(ex), len(pe), und, (sum(m[1] for m in ex + pe)) / 1e9,
                                 # member ids by class, so pertcost can price a pair from each cluster
                                 {"exact": [m[0] for m in ex], "perturbed": [m[0] for m in pe],
                                  "tensor": mem[0][2] if len(mem[0]) > 2 else None}))
    probed_b = sum(x[1] for x in cand[:LIMIT])
    print(f"\nprobed {n} repos, {probed_b/1e12:.2f} TB BF16/F16; architecture groups with >=2: {n_groups}")
    print(f"{'class':44s} {'TB':>8s} {'% of probed':>12s} {'% of ALL weight bytes':>22s}")
    for lbl, b in (("exact twin of another repo (dedup has it)", exact_b),
                   ("PERTURBED twin (r>0.99, not identical)", pert_b),
                   ("same architecture, independent weights", indep_b),
                   ("only model of its architecture in sample", solo_b)):
        print(f"{lbl:44s} {b/1e12:8.3f} {100*b/max(probed_b,1):11.1f}% {100*b/max(tot_w,1):21.1f}%")
    print(f"\nclusters (arch key, members, exact, perturbed, undeclared-among-them, GB):")
    for row in sorted(cluster_rows, key=lambda x: -x[5])[:12]:
        print(f"  {row[0][:40]:40s} n={row[1]:3d} exact={row[2]:3d} pert={row[3]:3d} undeclared={row[4]:3d} {row[5]:8.1f} GB")
    json.dump(dict(probed=n, probed_b=probed_b, tot_w=tot_w, exact_b=exact_b, pert_b=pert_b,
                   indep_b=indep_b, solo_b=solo_b, clusters=cluster_rows),
              open(f"{B}/qgq/lineage5.json", "w"), indent=1)
    print("LINEAGE3_DONE")

if __name__ == "__main__":
    main()
