#!/usr/bin/env python3
"""The headline with RESULT 93 folded in: Q8_0 given its parent costs 1.4%, not 7.7%.

Only one constant changes from finalnum3: the cross-repo class rate. A GGUF repo with a public parent
was priced at 10.5% of standalone (the midpoint of Q8_0 7.7% and K-quants 11.5%). With Q8_0 at 1.4%
and its ~40% share of repo bytes (8 bits/weight against 4-6), the repo rate is 0.4*0.014 + 0.6*0.115
= 0.075. Everything else is held fixed so the delta is attributable to one result."""
import json
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
R2 = json.load(open(f"{B}/qgq/resolverate2.json"))
rho_bin = R2["new"] / max(R2["att"], 1)
rho_par = float(np.mean(R2["cov"]))
GG = sum(v for k, v in cls.items() if k.startswith("gguf"))
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
Q8_SHARE = 0.40
CROSS_OLD = 0.105
CROSS_NEW = Q8_SHARE * 0.014 + (1 - Q8_SHARE) * 0.115

def run(rho, cross):
    par = GG * SH_PAR
    cost = (par * rho * cross + par * (1 - rho) * 0.97
            + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97)
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    held = HELD_NOW - OTHER_SAVE - (GG - cost) - ft
    return cost, held

print(f"cross-repo class rate: {CROSS_OLD:.3f} (scales stored) -> {CROSS_NEW:.3f} (Q8_0 scales predicted)\n")
print(f"{'basis':36s} {'rate':>6s} {'held, R91':>10s} {'held, R93':>10s} {'vs today':>9s} {'PB/yr':>6s}")
for lbl, rho in (("measured, binary floor", rho_bin), ("measured, partial credit", rho_par),
                 ("ceiling, not a claim", 1.0)):
    _c1, h1 = run(rho, CROSS_OLD)
    _c2, h2 = run(rho, CROSS_NEW)
    print(f"{lbl:36s} {100*rho:5.1f}% {h1:7.1f} TB {h2:7.1f} TB {100*(1-h2/HELD_NOW):8.1f}% "
          f"{(HELD_NOW-h2)*52/1000:5.2f}")
