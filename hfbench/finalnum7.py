#!/usr/bin/env python3
"""Provisional headline, twin lever priced across clusters and the pool discounted for one-tensor
misclassification (RESULT 104 addendum). Reads the two-tensor census if it has run (lineage5.json),
otherwise applies the crude 4/6 discount to RESULT 103's 7.7%. Kept separate from the measured rows."""
import json, os
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
R2 = json.load(open(f"{B}/qgq/resolverate2.json"))
rho_bin = R2["new"] / max(R2["att"], 1); rho_par = float(np.mean(R2["cov"]))
GG = sum(v for k, v in cls.items() if k.startswith("gguf")); ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
WEIGHTS = GG + ST + cls.get("torch-pickle", 0) + cls.get("other-weights", 0)
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
XET_BF16 = 1 / 1.14
PERT_COST = 0.22                       # 18.8-26.1% across three communities
if os.path.exists(f"{B}/qgq/lineage5.json"):
    L = json.load(open(f"{B}/qgq/lineage5.json")); PERT_SHARE = L["pert_b"] / L["tot_w"]; src = "two-tensor census (lineage5)"
else:
    PERT_SHARE = 0.077 * 4 / 6; src = "RESULT 103 x 4/6 crude discount"

def held(rho, pert):
    par = GG * SH_PAR
    gcost = par * rho * 0.001 + par * (1 - rho) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * PERT_SHARE * XET_BF16 * (1 - PERT_COST) if pert else 0.0
    return HELD_NOW - OTHER_SAVE - (GG - gcost) - ft - tw, tw

print(f"perturbed-twin pool: {100*PERT_SHARE:.1f}% of weight bytes ({src}); cost {100*PERT_COST:.0f}% of Xet\n")
print(f"{'resolution':22s} {'measured':>26s} {'+ twins (provisional)':>32s}")
for lbl, rho in (("41.4% binary", rho_bin), ("65.6% partial", rho_par), ("100% ceiling", 1.0)):
    h0_, _ = held(rho, False); h1, tw = held(rho, True)
    print(f"{lbl:22s} {h0_:8.1f} TB {100*(1-h0_/HELD_NOW):5.1f}%      {h1:8.1f} TB {100*(1-h1/HELD_NOW):5.1f}%  (twins {tw:.1f} TB/wk)")
