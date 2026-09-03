#!/usr/bin/env python3
"""The headline with the recipe route's own resolution rate (RESULT 109) and undeclared twins (RESULT 108).

Three rows, each one assumption further along, each labelled:
  A  name-map resolution 41.4%  -- what every earlier headline used; the arithmetic route's rate
  B  recipe-route resolution 94.3% by bytes x correct-parent factor -- the rate the recipe actually has
  C  B + undeclared perturbed twins delta-coded against their siblings
The correct-parent factor comes from resolverate3 (norm check) when it has run; until then 1.0 with a
flag, because RESULT 105 showed a resolvable parent can be the wrong one."""
import json, os
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
cls = {k: v / 1e12 for k, v in U["classes"].items()}
GG = sum(v for k, v in cls.items() if k.startswith("gguf")); ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
WEIGHTS = GG + ST + cls.get("torch-pickle", 0) + cls.get("other-weights", 0)
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
R4 = json.load(open(f"{B}/qgq/resolverate4.json")); rho_recipe = R4["ok_b"] / max(R4["all_b"], 1)
correct = 1.0; cflag = "not yet measured (resolverate3 in flight); RESULT 105 shows it is < 1"
if os.path.exists(f"{B}/qgq/resolverate3.json"):
    R3 = json.load(open(f"{B}/qgq/resolverate3.json"))
    if R3.get("correct") is not None and (R3["correct"] + R3["wrong"]) > 0:
        correct = R3["correct"] / (R3["correct"] + R3["wrong"]); cflag = f"measured: {R3['correct']} correct / {R3['wrong']} wrong by norm check"
L5 = json.load(open(f"{B}/qgq/lineage5.json")); PERT = L5["pert_b"] / L5["tot_w"]; PERT_COST = 0.22; XET_BF16 = 1 / 1.14
# RESULT 112: converter acceptance on resolvable parents, measured by --vocab-only. Conservative: the
# proxy figure, though 4 of its 6 failures are the proxy or my fetch, not the converter.
conv = 1.0; convflag = "not measured"
if os.path.exists(f"{B}/qgq/convcheck.json"):
    CC = json.load(open(f"{B}/qgq/convcheck.json"))
    if CC.get("att"): conv = CC["ok"] / CC["att"]; convflag = f"measured: {CC['ok']}/{CC['att']} accept (--vocab-only)"

def held(rho, twins):
    par = GG * SH_PAR
    gcost = par * rho * 0.001 + par * (1 - rho) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * PERT * XET_BF16 * (1 - PERT_COST) if twins else 0.0
    h = HELD_NOW - OTHER_SAVE - (GG - gcost) - ft - tw
    return h, gcost, tw

print(f"recipe-route resolution by bytes {100*rho_recipe:.1f}%; correct-parent {correct:.3f} ({cflag}); converts {conv:.3f} ({convflag})")
print(f"effective rho = {rho_recipe*correct*conv:.3f};  undeclared twin pool {100*PERT:.1f}% of weight bytes at {100*PERT_COST:.0f}% of Xet\n")
print(f"{'row':56s} {'held':>10s} {'less':>7s} {'PB/yr':>7s} {'gguf held':>10s}")
for lbl, rho, tw in (("A  name-map resolution 41.4% (all earlier headlines)", 0.414, False),
                     ("B  recipe-route x correct-parent x converts", rho_recipe * correct * conv, False),
                     ("C  B + undeclared twins against siblings", rho_recipe * correct * conv, True),
                     ("   ceiling: every parent resolves, + twins", 1.0, True)):
    h, g, t = held(rho, tw)
    print(f"{lbl:56s} {h:8.1f} TB {100*(1-h/HELD_NOW):6.1f}% {(HELD_NOW-h)*52/1000:6.2f} {g:8.1f} TB" + (f"   twins {t:.1f} TB/wk" if tw else ""))
