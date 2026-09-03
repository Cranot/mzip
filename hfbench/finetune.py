#!/usr/bin/env python3
"""Fine-tunes are the case chunk dedup is structurally worst at, and no lever here has touched them.

7.2% of the week's weight bytes declare a finetune, adapter or merge parent. A fine-tuned weight
differs from its base in the low mantissa bits, so every 64 KiB chunk hash differs and dedup records
pure novelty for a file that is almost entirely its parent -- the worst possible case for a
content-addressed store, and invisible in any dedup statistic.

Three things are measured against the declared base, same tensor, same shape:
  1. how many weights are BYTE-IDENTICAL to the base -- a LoRA merge touches only the projections it
     targeted, so whole tensors are often untouched (dedup does catch those, and they must not be
     counted as our win);
  2. what the raw bf16 words cost on their own;
  3. what the difference from the base costs -- as an integer difference of the bf16 words, and as a
     byte-plane view of that difference, which is what plane-entropy would actually code."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] in ("finetune", "merge", "adapter"):
                pairs.append((rid, ":".join(p[2:]), p[1])); break
    print(f"declared finetune/merge/adapter pairs: {len(pairs)}\n")
    print(f"{'repo':26s} {'rel':9s} {'tensor':16s} {'identical':>10s} {'raw':>7s} {'diff':>7s} "
          f"{'planes':>7s} {'cost':>7s}")
    why = collections.Counter(); rows = []; done = 0; tried = 0
    for rid, base, rel in pairs:
        if done >= 8 or tried >= 200: break
        tried += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            sa = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sa: why["no safetensors in repo"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base not reachable"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base has no safetensors"] += 1; continue
            ma, ca = shard_map(rid, sa)
            mb, cb = shard_map(base, sb)
            if not ma or not mb: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        common = [k for k in ma if k in mb and k.endswith(".weight") and "layers." in k]
        if not common: why["no common tensor"] += 1; continue
        common.sort()
        picked = 0
        for kname in common[:60]:
            if picked >= 3: break
            try:
                fa, fb = ma[kname], mb[kname]
                if fa not in ca: ca[fa] = st_header(rid, fa)
                if fb not in cb: cb[fb] = st_header(base, fb)
                ha, oa = ca[fa]; hb, ob = cb[fb]
                if not ha or not hb or kname not in ha or kname not in hb: continue
                A, Bm = ha[kname], hb[kname]
                if A.get("dtype") != Bm.get("dtype") or A.get("dtype") not in ("BF16", "F16"): continue
                if A["shape"] != Bm["shape"]: continue
                nby = min(4 << 20, A["data_offsets"][1] - A["data_offsets"][0])
                nby -= nby % 2
                ra = grab(rid, fa, oa + A["data_offsets"][0], nby)
                rb = grab(base, fb, ob + Bm["data_offsets"][0], nby)
                if ra is None or rb is None: continue
            except Exception:
                continue
            wa = np.frombuffer(ra.tobytes()[:nby], dtype=np.uint16).astype(np.int32)
            wb = np.frombuffer(rb.tobytes()[:nby], dtype=np.uint16).astype(np.int32)
            n = min(len(wa), len(wb))
            if n < 4096: continue
            wa, wb = wa[:n], wb[:n]
            ident = float(np.mean(wa == wb))
            ba = wa.astype(np.uint16).view(np.uint8) if False else np.frombuffer(ra.tobytes()[:n*2], dtype=np.uint8)
            raw = h0(ba) * 2                      # bits per weight, coding the raw bytes
            diff = (wa - wb).astype(np.int32)
            dh = h0(diff)                         # bits per weight, coding the integer difference
            d16 = diff.astype(np.int32) & 0xFFFF
            hi = (d16 >> 8).astype(np.uint8); lo = (d16 & 0xFF).astype(np.uint8)
            pl = h0(hi) + h0(lo)                  # bits per weight, coding the difference by byte plane
            best = min(dh, pl)
            rows.append(dict(rid=rid, rel=rel, k=kname, ident=ident, raw=raw, diff=dh, pl=pl))
            print(f"{rid[:26]:26s} {rel:9s} {kname.split('.')[-2][:16]:16s} {100*ident:9.2f}% "
                  f"{raw:7.3f} {dh:7.3f} {pl:7.3f} {100*best/raw:6.1f}%")
            picked += 1
        if picked: done += 1
    if rows:
        R = sum(r["raw"] for r in rows); Dv = sum(min(r["diff"], r["pl"]) for r in rows)
        print(f"\n  {len(rows)} tensors across {done} repos")
        print(f"  mean byte-identical weights: {100*np.mean([r['ident'] for r in rows]):.2f}%")
        print(f"  raw {R/len(rows):.3f} bits/weight -> given the base {Dv/len(rows):.3f} "
              f"({100*Dv/R:.1f}% of standalone, {R/Dv:.2f}x)")
    print("\nwhy the rest produced nothing:")
    for r, c in why.most_common(10): print(f"  {c:5d}  {r}")
    json.dump(rows, open(f"{B}/qgq/finetune.json", "w"), indent=1, default=float)
    print("FINETUNE_DONE")

if __name__ == "__main__":
    main()
