#!/usr/bin/env python3
"""The headline if resolved quantised GGUFs are stored as recipes (RESULT 99) instead of residuals.

Only one constant changes from finalnum4: a GGUF repo whose parent resolves costs a recipe -- call it
0.1% of standalone for the pointer, versions and KV metadata -- instead of 7.5% (RESULT 93's blended
Q8_0 1.4% / K-quant 11.5%). Everything else, including the unresolved remainder at 97% and the
multi-file sibling route at 25.2%, is held fixed so the delta is attributable to one result.

Two honesty constraints carried in the output: the recipe is exact on n=1 (one publisher, static
quant, RESULT 101 will widen it), and the saving is bought with ingest compute and a pinned toolchain,
which the table cannot show but the text must."""
import json
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
R2 = json.load(open(f"{B}/qgq/resolverate2.json"))
rho_bin = R2["new"] / max(R2["att"], 1); rho_par = float(np.mean(R2["cov"]))
GG = sum(v for k, v in cls.items() if k.startswith("gguf"))
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
RATES = {"R91 arithmetic, scales stored": 0.105, "R93 Q8_0 scales predicted": 0.075, "R99 RECIPE (exact)": 0.001}

def held(rho, cross):
    par = GG * SH_PAR
    cost = par * rho * cross + par * (1 - rho) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    return HELD_NOW - OTHER_SAVE - (GG - cost) - ft, cost

print(f"gguf {GG:.1f} TB of the week; held today {HELD_NOW:.0f} TB\n")
print(f"{'resolution':28s} " + " ".join(f"{k:>30s}" for k in RATES))
for lbl, rho in (("41.4% measured, binary", rho_bin), ("65.6% measured, partial", rho_par), ("100% ceiling", 1.0)):
    row = []
    for cross in RATES.values():
        h, _ = held(rho, cross)
        row.append(f"{h:7.1f} TB  {100*(1-h/HELD_NOW):5.1f}%  {(HELD_NOW-h)*52/1000:5.2f}PB/y")
    print(f"{lbl:28s} " + " ".join(f"{r:>30s}" for r in row))
h_b, c_b = held(rho_bin, 0.001); h_p, c_p = held(rho_par, 0.001)
print(f"\nGGUF class held with recipes: {c_b:.1f} TB (binary) / {c_p:.1f} TB (partial), of 177.4 TB uploaded")
print("the recipe row is exact on n=1 (RESULT 99); it is bought with ingest compute and a pinned toolchain.")
