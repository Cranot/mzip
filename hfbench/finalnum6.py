#!/usr/bin/env python3
"""The headline with undeclared perturbed twins priced in -- PROVISIONAL until RESULT 104 is n>1 cluster.

RESULT 103: perturbed twins are 7.7% of all weight bytes. RESULT 104: they cost ~25% of Xet's BF16
storage against their sibling, measured on one cluster. This row is what the week looks like if that
price holds across clusters, kept separate from the measured rows so nobody quotes it as measured."""
import json
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
R2 = json.load(open(f"{B}/qgq/resolverate2.json"))
rho_bin = R2["new"] / max(R2["att"], 1); rho_par = float(np.mean(R2["cov"]))
GG = sum(v for k, v in cls.items() if k.startswith("gguf"))
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
WEIGHTS = GG + ST + cls.get("torch-pickle", 0) + cls.get("other-weights", 0)
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
PERT_SHARE = 0.077          # RESULT 103, of all weight bytes
PERT_COST = 0.25            # RESULT 104, vs Xet BF16, one cluster
XET_BF16 = 1 / 1.14         # what Xet holds a BF16 byte at

def held(rho, cross, pert):
    par = GG * SH_PAR
    gcost = par * rho * cross + par * (1 - rho) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * PERT_SHARE * XET_BF16 * (1 - PERT_COST) if pert else 0.0
    return HELD_NOW - OTHER_SAVE - (GG - gcost) - ft - tw, tw

print(f"weights in the week {WEIGHTS:.0f} TB; perturbed-twin pool {WEIGHTS*PERT_SHARE:.1f} TB "
      f"(held today at ~{WEIGHTS*PERT_SHARE*XET_BF16:.1f} TB)\n")
print(f"{'resolution':26s} {'measured (R99 recipe)':>30s} {'+ perturbed twins (R104, provisional)':>40s}")
for lbl, rho in (("41.4% binary", rho_bin), ("65.6% partial", rho_par), ("100% ceiling", 1.0)):
    h0_, _ = held(rho, 0.001, False); h1, tw = held(rho, 0.001, True)
    print(f"{lbl:26s} {h0_:8.1f} TB {100*(1-h0_/HELD_NOW):5.1f}% {(HELD_NOW-h0_)*52/1000:5.2f}PB/y"
          f"      {h1:8.1f} TB {100*(1-h1/HELD_NOW):5.1f}% {(HELD_NOW-h1)*52/1000:5.2f}PB/y   (twins save {tw:.1f} TB/wk)")
print("\nthe right-hand column is PROVISIONAL: twin cost measured on one cluster (RESULT 104). It is not in the artifact.")
