#!/usr/bin/env python3
"""Twin delta cost across MORE communities, with the pairing done right this time.

RESULT 104's addendum priced three communities and mis-paired two others (a member classified as a
twin of X was priced against Y). This takes each cluster from the two-tensor census, fetches one tensor
slice from up to five members, finds the pair with the HIGHEST correlation, and prices that pair --
which is, by construction, a pair the census would have called twins. Up to eight clusters, skipping
the three already priced."""
import json, sys, itertools
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from pertcost import tensor_slice, h0
B = "/root/mzip-hfbench"

def main():
    L = json.load(open(f"{B}/qgq/lineage5.json"))
    clusters = [c for c in L["clusters"] if len(c) >= 7 and isinstance(c[6], dict)]
    clusters.sort(key=lambda c: -c[5])
    priced = ("qwen3_5_moe|2048", "qwen3|2560|36", "llama|4096|32|32|128256")
    print(f"{'cluster':30s} {'best pair':50s} {'r':>7s} {'ident':>6s} {'|d|<=2':>7s} {'h(d)':>6s} {'vs Xet':>7s}")
    rows = []; done = 0
    for c in clusters:
        key, mem = c[0], c[6]
        if any(key.startswith(p) for p in priced): continue
        ids = (mem.get("perturbed") or [])[:4] + (mem.get("exact") or [])[:2]
        if len(ids) < 2: continue
        got = []
        for rid in ids[:5]:
            s = tensor_slice(rid, nel_cap=2_000_000)
            if s: got.append((rid, s[0], s[1]))
        if len(got) < 2: continue
        # all members must share the tensor name for a fair pair
        best = None
        for (ra, ka, wa), (rb, kb, wb) in itertools.combinations(got, 2):
            if ka != kb: continue
            n = min(wa.size, wb.size)
            r = float(np.corrcoef(wa[:n].astype(np.float64), wb[:n].astype(np.float64))[0, 1])
            if best is None or r > best[0]: best = (r, ra, rb, wa[:n] - wb[:n])
        if not best or best[0] < 0.99:
            print(f"{key[:30]:30s} no pair above r=0.99 among {len(got)} members fetched (best r={best[0] if best else float('nan'):.3f})"); continue
        r, ra, rb, d = best
        ident = float(np.mean(d == 0)); two = float(np.mean(np.abs(d) <= 2)); hd = h0(d)
        print(f"{key[:30]:30s} {(ra[:23]+' ~ '+rb[:23]):50s} {r:7.4f} {100*ident:5.1f}% {100*two:6.1f}% {hd:6.3f} {100*hd/(16/1.14):6.1f}%")
        rows.append(dict(key=key, a=ra, b=rb, r=r, ident=ident, two=two, h=hd, vs_xet=hd / (16 / 1.14)))
        done += 1
        if done >= 8: break
    if rows:
        v = [x["vs_xet"] for x in rows]
        print(f"\n  {len(rows)} new clusters priced: cost vs Xet median {100*np.median(v):.1f}%, range {100*min(v):.1f}-{100*max(v):.1f}%")
        print("  previously priced: Affine 25%, Qwen3-2560 19%, Llama-3-8B 19%")
    json.dump(rows, open(f"{B}/qgq/pertcost3.json", "w"), indent=1)
    print("PERTCOST3_DONE")

if __name__ == "__main__":
    main()
