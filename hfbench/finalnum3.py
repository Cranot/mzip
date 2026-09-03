#!/usr/bin/env python3
"""The headline on the MEASURED resolution rate, now that the name map is written from the data.

The rate was the only free parameter left. It was 27.9% with a llama-only mapper; with MoE fusion,
DeepSeek MLA, shared experts, vision towers and projectors added it is 41.4% by repo and 36.7% by
tensor elements, on the same 140 pairs with nothing else changed.

Two estimators are reported because the binary one understates. A pair counted as unresolved at 78%
coverage still saves on 78% of its bytes -- a deployment maps what it can and stores the rest -- so the
partial-credit figure (mean coverage over all pairs) is the better estimate of what would collect, and
the binary figure is the conservative floor."""
import json
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
R2 = json.load(open(f"{B}/qgq/resolverate2.json"))
cov = R2["cov"]
rho_bin = R2["new"] / max(R2["att"], 1)
rho_par = float(np.mean(cov)) if cov else 0.0

GG = sum(v for k, v in cls.items() if k.startswith("gguf"))
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485

def run(rho):
    par = GG * SH_PAR
    cost = (par * rho * 0.105 + par * (1 - rho) * 0.97
            + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97)
    q = GG - cost
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    held = HELD_NOW - OTHER_SAVE - q - ft
    return cost, q, ft, held

print(f"gguf {GG:.1f} TB, safetensors {ST:.1f} TB, held today {HELD_NOW:.0f} TB\n")
print(f"resolution rate, binary (>=80% of a repo's tensors map): {100*rho_bin:.1f}%")
print(f"resolution rate, partial credit (mean coverage per pair): {100*rho_par:.1f}%\n")
print(f"{'basis':38s} {'rate':>7s} {'gguf held':>11s} {'quants':>9s} {'finetunes':>11s} "
      f"{'held':>10s} {'vs today':>9s} {'PB/yr':>7s}")
for lbl, rho in (("old mapper, llama names only", 0.279),
                 ("MEASURED, new map, binary", rho_bin),
                 ("MEASURED, new map, partial credit", rho_par),
                 ("ceiling -- not a claim", 1.0)):
    c, q, ft, held = run(rho)
    print(f"{lbl:38s} {100*rho:6.1f}% {c:8.1f} TB {q:6.1f} TB {ft:8.1f} TB {held:7.1f} TB "
          f"{100*(1-held/HELD_NOW):8.1f}% {(HELD_NOW-held)*52/1000:6.2f}")
print("\n  The binary row is the conservative floor: it gives a pair mapping 78% of its tensors nothing")
print("  at all. Partial credit is what a deployment collects, mapping what it can and storing the rest.")
