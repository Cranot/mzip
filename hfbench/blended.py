#!/usr/bin/env python3
"""Twin delta cost blended by cluster BYTES, not by cluster count.

RESULT 111 has seven priced communities with a median near 20%. The headline uses 22%. But the Affine
cluster is the largest by an order of magnitude and prices at 25%, so a count-median understates the
blended cost. Weight each priced cluster's cost by its perturbed bytes from the two-tensor census;
clusters not priced take the priced median."""
import json
import numpy as np
B = "/root/mzip-hfbench"
L = json.load(open(f"{B}/qgq/lineage5.json"))
priced = {"qwen3_5_moe|2048": 0.25, "qwen3|2560|36": 0.193, "llama|4096|32|32|128256": 0.188,
          "qwen3|4096|36": 0.061, "qwen3|2048|28": 0.075, "llama|4096|32|32|102400": 0.218, "gpt_neox|4096": 0.368}
med = float(np.median(list(priced.values())))
tot_b = 0.0; cost_b = 0.0; rows = []
for c in L["clusters"]:
    key, n, ex, pe, und, gb = c[:6]
    if pe == 0: continue
    pert_gb = gb * pe / max(ex + pe, 1)
    cost = next((v for k, v in priced.items() if key.startswith(k)), None)
    src = "priced" if cost is not None else "median"
    if cost is None: cost = med
    tot_b += pert_gb; cost_b += pert_gb * cost
    rows.append((pert_gb, key, cost, src))
rows.sort(reverse=True)
print(f"{'perturbed GB':>13s} {'cluster':34s} {'cost':>6s} {'src':>7s}")
for gb, k, cost, src in rows[:10]:
    print(f"{gb:13.1f} {k[:34]:34s} {100*cost:5.1f}% {src:>7s}")
print(f"\npriced clusters cover {100*sum(r[0] for r in rows if r[3]=='priced')/tot_b:.0f}% of perturbed bytes")
print(f"count-median cost {100*med:.1f}%   BYTE-BLENDED cost {100*cost_b/tot_b:.1f}%   headline uses 22.0%")
tw22 = 864 * 0.078 / 1.14 * (1 - 0.22); twb = 864 * 0.078 / 1.14 * (1 - cost_b / tot_b)
print(f"twins saving/week at 22%: {tw22:.1f} TB   at blended: {twb:.1f} TB   delta {tw22-twb:+.1f} TB")
