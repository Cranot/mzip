#!/usr/bin/env python3
"""Price perturbed twins across clusters, not just the Affine one.

RESULT 104 measured 25% of Xet on three pairs from one community's re-uploads of one base. Whether the
7.7% pool (RESULT 103) is a re-cast at 25% or a genuine fine-tune at 70-96% is decided cluster by
cluster. Reads the member lists lineage3.py now saves, takes the largest non-Affine clusters with a
perturbed member, pairs each perturbed member with an exact member of the same cluster (or another
perturbed one), and measures the bf16 delta on one full tensor. Same statistics as pertcost.py."""
import json, sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from pertcost import tensor_slice, h0

B = "/root/mzip-hfbench"
L = json.load(open(f"{B}/qgq/lineage3.json"))
clusters = [c for c in L["clusters"] if len(c) >= 7 and isinstance(c[6], dict)]
clusters.sort(key=lambda c: -c[5])
done = 0
print(f"{'cluster':34s} {'pair':52s} {'ident':>6s} {'|d|=1':>6s} {'|d|<=2':>7s} {'h(d)':>6s} {'vs Xet':>7s}")
for c in clusters:
    key, mem = c[0], c[6]
    if key.startswith("qwen3_5_moe"): continue                      # RESULT 104 already priced this one
    pe, ex = mem.get("perturbed", []), mem.get("exact", [])
    if not pe: continue
    pairs = []
    if ex: pairs.append((pe[0], ex[0]))
    if len(pe) >= 2: pairs.append((pe[0], pe[1]))
    if len(pe) >= 3 and ex: pairs.append((pe[2], ex[0]))
    got = 0
    for a, b in pairs[:2]:
        A = tensor_slice(a)
        if not A: continue
        k, wa = A
        Bt = tensor_slice(b, want_key=k)
        if not Bt: continue
        _, wb = Bt
        n = min(wa.size, wb.size); d = wa[:n] - wb[:n]
        if n < 4096: continue
        ident = float(np.mean(d == 0)); ad = np.abs(d)
        one = float(np.mean(ad == 1)); two = float(np.mean(ad <= 2)); hd = h0(d)
        r = float(np.corrcoef(wa[:n].astype(np.float64), wb[:n].astype(np.float64))[0, 1])
        tag = "twin" if r > 0.99 else "NOT a twin (r=%.3f)" % r
        print(f"{key[:34]:34s} {(a[:24]+' ~ '+b[:24]):52s} {100*ident:5.1f}% {100*one:5.1f}% {100*two:6.1f}% "
              f"{hd:6.3f} {100*hd/(16/1.14):6.1f}%  {tag}")
        got += 1
    if got: done += 1
    if done >= 5: break
print("\n  vs Xet = h(delta) against Xet's ~14.0 bits/weight for BF16. RESULT 104's cluster measured 24.7-26.1%.")
print("PERTCOST2_DONE")
