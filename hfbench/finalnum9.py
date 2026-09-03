#!/usr/bin/env python3
"""Row D: BF16 uploads whose parent is a popular root outside the sample week, stored as deltas.

parentfind v3 (RESULT 124) probed keyed "solo" BF16 repos -- the ones the two-tensor census could not
group within the week -- and found half of them derive from a popular Hub root. Exact copies are
dedup's (the root is on the Hub, chunk hashes match); fine-tunes and perturbed twins are ours, coded
against the root with the FSE delta coder (RESULT 123: 1.07x the h0 bound). This prices that pool by
bytes and adds it as row D, separate from row C, with its own caveats printed."""
import json
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
L5 = json.load(open(f"{B}/qgq/lineage5.json")); PERT = L5["pert_b"] / L5["tot_w"]; XET = 1 / 1.14
PF = json.load(open(f"{B}/qgq/parentfind.json"))
UNMATCHED_TB = 14.04; TOT_W_TB = L5["tot_w"] / 1e12

rows = PF["rows"]; probed_b = PF["pb"]
by = {}
for r in rows: by.setdefault(r["kind"], []).append(r)
print("parent discovery, keyed solo BF16 repos (RESULT 124):")
for k, v in by.items():
    b = sum(x["bytes"] for x in v); hs = [x["h"] for x in v]
    print(f"  {k:10s} n={len(v):2d}  {100*b/probed_b:5.1f}% of probed bytes  delta h0 median {np.median(hs):.2f} b/w  untagged {sum(1 for x in v if not x['declared'])}")
ours = [x for x in rows if x["kind"] in ("fine-tune", "perturbed")]
ours_b = sum(x["bytes"] for x in ours); ours_share = ours_b / probed_b
# byte-weighted delta cost, coded at 1.07x the bound (R123), against Xet's 14.0 bits/weight
cost = sum(x["bytes"] * x["h"] * 1.07 for x in ours) / max(ours_b, 1) / 14.0
pool_of_weights = (UNMATCHED_TB / TOT_W_TB) * ours_share          # share of ALL weight bytes
print(f"\n  ours (fine-tune + perturbed): {100*ours_share:.1f}% of probed solo bytes; byte-weighted coded cost {100*cost:.1f}% of Xet")
print(f"  solo BF16 pool is {UNMATCHED_TB:.2f} of {TOT_W_TB:.2f} TB weight bytes in the sample -> discovered-derivative pool = {100*pool_of_weights:.1f}% of ALL weight bytes")
tw_D = WEIGHTS * pool_of_weights * XET * (1 - cost)

def held(rho_, twins, disc):
    par = GG * SH_PAR
    g = par * rho_ * 0.001 + par * (1 - rho_) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * PERT * XET * (1 - 0.264) if twins else 0.0
    d = tw_D if disc else 0.0
    return HELD_NOW - OTHER_SAVE - (GG - g) - ft - tw - d, tw, d

print(f"\n{'row':58s} {'held':>10s} {'less':>6s} {'PB/yr':>6s}")
for lbl, tw, d in (("B  measured (recipe route x correct x converts)", False, False),
                   ("C  B + undeclared twins (two-tensor census, FSE cost)", True, False),
                   ("D  C + solo BF16 resolved to popular roots (this run)", True, True)):
    h, t, dd = held(rho, tw, d)
    print(f"{lbl:58s} {h:8.1f} TB {100*(1-h/HELD_NOW):5.1f}% {(HELD_NOW-h)*52/1000:5.2f}" + (f"   (+{dd:.1f} TB/wk)" if d else ""))
print("\n  row D caveats: n=34 derivatives from 62 keyed solo repos; parentage by ONE 64 KiB slice at r>0.99")
print("  (fine-tunes at r=1.000 with 50-97% identical weights are unambiguous; the 2 perturbed need a second")
print("  tensor); the solo pool was probed by size order (largest 80), so byte weighting favours big repos.")
