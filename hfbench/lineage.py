#!/usr/bin/env python3
"""Undeclared lineage: how many models with NO base_model tag are derivatives anyway?

Every provenance lever so far starts from the Hub's own tags -- 27.7% of weight bytes declare a
parent. The other 72% is treated as original. But a tag is a courtesy, not a fact: a fine-tune of
Llama-3-8B is a fine-tune whether or not its author said so. Weights tell the truth: at fixed
positions in the same tensor, a fine-tune correlates with its base above 0.99 (the change is in the
low mantissa bits); two independent trainings of the same architecture correlate near 0.

So: group the sample's untagged BF16/F16 safetensors repos by architecture fingerprint (the set of
tensor names and shapes), fetch the same 64 KiB of the same large tensor from each, and correlate
every pair. A cluster of >0.99 correlations is a family with one root and undeclared children.
Only same-shape models can be compared, which is also the only case where the delta lever applies."""
import json, sys, collections, hashlib, itertools
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map
from hfget import get_json

def fingerprint(wmap, cache, base):
    """architecture signature from config.json -- what the model IS, not how it was sharded.

    The first version hashed the largest 12 tensor names and shapes, and 36 repos fell into 36
    groups: shard layout, a renamed prefix, or one extra tensor changes that hash while leaving the
    architecture identical. model_type + hidden + layers + heads + vocab is what two models must
    share for a weight-by-weight comparison to be meaningful at all."""
    cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
    tc = cfg.get("text_config") or cfg
    key = (cfg.get("model_type"), tc.get("hidden_size"), tc.get("num_hidden_layers"),
           tc.get("num_attention_heads"), tc.get("vocab_size"), tc.get("intermediate_size"))
    sig = "|".join(str(x) for x in key)
    ents = []
    for fn, (hb, _o) in cache.items():
        if not hb: continue
        for k, v in hb.items():
            if isinstance(v, dict) and "shape" in v:
                ents.append((v["data_offsets"][1] - v["data_offsets"][0], k, tuple(v["shape"]), v["dtype"]))
    ents.sort(reverse=True)
    return sig, ents[:12]

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = []
    for r in rows:
        c = r.get("bytes_in_window") or {}
        bf = sum(float(v or 0) for k, v in c.items() if k in ("safetensors:BF16", "safetensors:F16"))
        if bf < 1e9: continue
        t = (tags.get(r["id"]) or {}).get("tags") or []
        declared = any(x.startswith("base_model:") for x in t)
        cand.append((r["id"], bf, declared))
    cand.sort(key=lambda x: -x[1])
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 80
    groups = collections.defaultdict(list); nread = 0
    for rid, bf, declared in cand[:LIMIT]:
        d = api(f"{H}/api/models/{rid}")
        if not d: continue
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        if not sts: continue
        wmap, cache = shard_map(rid, sts)
        if not cache:
            for fn in sts[:3]: cache[fn] = st_header(rid, fn)
        sig, top = fingerprint(wmap, cache, rid)
        if not top: continue
        nread += 1
        # pick the largest 2D BF16/F16 tensor that is NOT an embedding (embeddings drift on vocab changes)
        pick = next(((k, s, dt) for _, k, s, dt in top if len(s) == 2 and dt in ("BF16", "F16")
                     and "embed" not in k and "lm_head" not in k), None)
        if not pick: continue
        k, s, dt = pick
        fn = wmap.get(k) or next(iter(cache))
        hb, off = cache.get(fn) or st_header(rid, fn)
        if not hb or k not in hb: continue
        m = hb[k]
        raw = grab(rid, fn, off + m["data_offsets"][0], 65536)
        if raw is None: continue
        r16 = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        w = (bf16_to_f32(r16) if dt == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float64)
        groups[sig].append((rid, declared, k, w))
    print(f"untagged-or-tagged BF16/F16 repos read: {nread}; architecture groups with >=2 members: "
          f"{sum(1 for g in groups.values() if len(g) >= 2)}\n")
    fam_untagged = 0; fam_total = 0; solo_untagged = 0
    for sig, mem in groups.items():
        if len(mem) < 2:
            solo_untagged += sum(1 for _, dcl, _k, _w in mem if not dcl); continue
        print(f"--- architecture {sig}: {len(mem)} models, tensor {mem[0][2][:40]}")
        for (ra, da, _, wa), (rb, dbb, _, wb) in itertools.combinations(mem, 2):
            n = min(len(wa), len(wb))
            if n < 1024: continue
            c = float(np.corrcoef(wa[:n], wb[:n])[0, 1])
            ident = float(np.mean(wa[:n] == wb[:n]))
            flag = "FAMILY" if c > 0.99 else ("related?" if c > 0.5 else "independent")
            tag = ("both declare" if da and dbb else "one declares" if (da or dbb) else "NEITHER declares")
            print(f"   r={c:+.4f}  identical={100*ident:5.1f}%  {flag:12s} {tag:16s} {ra[:28]} ~ {rb[:28]}")
            if c > 0.99:
                fam_total += 1
                if not (da and dbb): fam_untagged += 1
    print(f"\n  family pairs (r>0.99): {fam_total};  of which at least one side undeclared: {fam_untagged}")
    print("  An undeclared family pair is a derivative the tag census cannot see and Xet cannot dedup.")
    print("LINEAGE_DONE")

if __name__ == "__main__":
    main()
