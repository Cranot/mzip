#!/usr/bin/env python3
"""Headline rows with the solo BF16 pool partitioned by what could actually be READ (RESULT 126/127).

RESULT 124's row D extrapolated the keyed-solo derivative rate over the whole 14.04 TB solo pool. That
pool is three things: 3.90 TB of keyed, readable repos (the rate was measured there), 9.20 TB of GATED
repos at Qwen3.6-35B-A3B's byte size (HTTP 401 -- never readable by us; priced here through their six
readable siblings), and 0.94 TB of other unkeyed repos (unknown, priced at zero). Row D now covers only
the keyed part, measured over ALL keyed solos by the root index when its result file exists (else the
R124 largest-80 run); row E is the gated cluster, tagged inferred."""
import json, os
import numpy as np
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json")); cls = {k: v / 1e12 for k, v in U["classes"].items()}
GG = sum(v for k, v in cls.items() if k.startswith("gguf")); ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
WEIGHTS = GG + ST + cls.get("torch-pickle", 0) + cls.get("other-weights", 0)
HELD_NOW, OTHER_SAVE = 1444.0, 270.0 - 104.3
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
R4 = json.load(open(f"{B}/qgq/resolverate4.json")); R3 = json.load(open(f"{B}/qgq/resolverate3.json")); CC = json.load(open(f"{B}/qgq/convcheck.json"))
rho = (R4["ok_b"] / R4["all_b"]) * (R3["correct"] / (R3["correct"] + R3["wrong"])) * (CC["ok"] / CC["att"])
L5 = json.load(open(f"{B}/qgq/lineage5.json")); PERT = L5["pert_b"] / L5["tot_w"]; XET = 1 / 1.14; XBITS = 14.0
TOT_W_TB = L5["tot_w"] / 1e12
SOLO_KEYED_TB, SOLO_GATED_TB, SOLO_OTHER_TB = 3.90, 9.20, 0.94        # RESULT 126 partition of the 14.04 TB

# --- row D: keyed solos, measured ---
src = f"{B}/qgq/rootindex_result.json" if os.path.exists(f"{B}/qgq/rootindex_result.json") else f"{B}/qgq/parentfind.json"
PF = json.load(open(src)); rows = PF["rows"] if isinstance(PF.get("rows"), list) else PF["found"]; probed_b = PF["pb"]
ours = [x for x in rows if x["kind"] in ("fine-tune", "perturbed")]
# R129: permtest.py recovered name-mismatch cases by fetching the root's tensors under the solo's names;
# only its second-tensor-CONFIRMED non-exact hits join the measured row (one tensor is not the model)
if os.path.exists(f"{B}/qgq/permtest.json"):
    PT = json.load(open(f"{B}/qgq/permtest.json")); seen = {x["rid"] for x in rows}
    extra = [dict(rid=x["rid"], kind="fine-tune", h=x["h"], bytes=x["bytes"]) for x in PT["hits"]
             if x["conf"] == "confirmed" and x["ident_after"] < 0.999 and x["rid"] not in seen]
    ours += extra; print(f"  + {len(extra)} confirmed fine-tunes recovered by permtest.py (R129): {[x['rid'][:24] for x in extra]}")
ours_b = sum(x["bytes"] for x in ours); ours_share = ours_b / probed_b
costD = sum(x["bytes"] * min(1.0, x["h"] * 1.07 / XBITS) for x in ours) / max(ours_b, 1)
poolD = (SOLO_KEYED_TB / TOT_W_TB) * ours_share
tw_D = WEIGHTS * poolD * XET * (1 - costD)
print(f"row D source: {os.path.basename(src)} -- probed {PF.get('probed', len(rows))} repos, {probed_b/1e12:.2f} TB; ours {100*ours_share:.1f}% of probed bytes at {100*costD:.1f}% of Xet")
print(f"  keyed solo pool {SOLO_KEYED_TB:.2f} of {TOT_W_TB:.2f} TB weight bytes -> {100*poolD:.2f}% of ALL weight bytes; +{tw_D:.1f} TB/week")

# --- row E: gated Qwen3.6-35B-A3B cluster, priced through six readable siblings (proxy.py R126; the two permuted copies at their un-permuted residual, R127) ---
PROXY_BW = {"Shatoria/Affine-5ghntktyzq-hope3": 12.04, "HarperJane/20260829-050111": 1.635, "jacob-rojic/mirror-afgod1079-affine-5eqgpsdo6a-cp2700": 1.635,
            "ammazon/albedo-qwen3.6-35b-ant": 4.30, "leary-criste/affine-5g4yy75zuz-v30": 9.41, "topcatmax/mirror-leary-criste-affine-5g4yy75zuz-q2": 7.17}
costs = [min(1.0, v / XBITS) for v in PROXY_BW.values()]; costE = float(np.mean(costs))
poolE = SOLO_GATED_TB / TOT_W_TB
tw_E = WEIGHTS * poolE * XET * (1 - costE)
print(f"row E: gated cluster {SOLO_GATED_TB:.2f} TB = {100*poolE:.1f}% of ALL weight bytes; six readable siblings cost {', '.join(f'{100*c:.0f}%' for c in costs)} of Xet -> mean {100*costE:.1f}%; +{tw_E:.1f} TB/week [inferred]")
print(f"  unpriced: {SOLO_OTHER_TB:.2f} TB other unkeyed solos (0 assumed)")

def held(rho_, twins, d, e):
    par = GG * SH_PAR
    g = par * rho_ * 0.001 + par * (1 - rho_) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * PERT * XET * (1 - 0.264) if twins else 0.0
    return HELD_NOW - OTHER_SAVE - (GG - g) - ft - tw - (tw_D if d else 0) - (tw_E if e else 0)

print(f"\n{'row':66s} {'held':>10s} {'less':>6s} {'PB/yr':>6s}")
for lbl, tw, d, e in (("B  measured (recipe route x correct x converts)", False, False, False),
                      ("C  B + undeclared twins (two-tensor census, FSE cost)", True, False, False),
                      ("D  C + keyed solo BF16 resolved to popular roots (measured)", True, True, False),
                      ("E  D + gated Qwen3.6-35B-A3B cluster via readable siblings [I]", True, True, True)):
    h = held(rho, tw, d, e)
    print(f"{lbl:66s} {h:8.1f} TB {100*(1-h/HELD_NOW):5.1f}% {(HELD_NOW-h)*52/1000:5.2f}")
print(f"\nrho = {rho:.3f}; WEIGHTS = {WEIGHTS:.0f} TB; Xet reference {XBITS} bits/weight on bf16 (1/1.14)")
