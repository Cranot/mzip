#!/usr/bin/env python3
"""The week's numbers with the cross-repo levers folded in, and the double-counting kept out.

Two traps. The within-repo quant model already prices multi-file GGUF repos, so a repo that ALSO has a
public parent must be counted once, at the better rate, not twice. And a fine-tune's untouched tensors
are contiguous identical bytes that Xet's dedup already collapses inside the 'held today' baseline --
only the CHANGED tensors are ours to compress.

The cross-repo lever also carries a resolution rate: of the repos declaring a quantized parent, only
some resolve to a base that is present, public, in safetensors, and whose tensor names map. That rate
was measured at 6/20 and 8/28, and the rest is UNKNOWN, not zero and not one. It is carried as an
explicit parameter and the headline is reported as a range."""
import json
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
cls = U["classes"]
TOT = U["total"]

def rel_of(rid):
    out = set()
    for x in ((tags.get(rid) or {}).get("tags") or []):
        p = x.split(":")
        if x.startswith("base_model:") and len(p) >= 3: out.add(p[1])
    return out

g_all = g_parent = 0.0
s_all = s_ft = 0.0
for r in rows:
    c = r.get("bytes_in_window") or {}
    g = sum(float(v or 0) for k, v in c.items() if k.startswith("gguf"))
    s = sum(float(v or 0) for k, v in c.items() if k.startswith("safetensors"))
    rel = rel_of(r["id"])
    g_all += g; s_all += s
    if "quantized" in rel: g_parent += g
    if rel & {"finetune", "merge", "adapter"}: s_ft += s

f_q = g_parent / max(g_all, 1)
f_ft = s_ft / max(s_all, 1)
GG = cls.get("gguf:Q", 0) + cls.get("gguf:BF16", 0)
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
print(f"week uploaded            {TOT:9.1f} TB")
print(f"  gguf, all types        {GG:9.1f} TB   of which {100*f_q:.1f}% sits in repos declaring a quantized parent")
print(f"  safetensors, all types {ST:9.1f} TB   of which {100*f_ft:.1f}% sits in repos declaring finetune/merge/adapter\n")

HELD_NOW = 1444.0          # TB held after Xet dedup + LZ4/BG4, RESULT 79
BASE_SAVE = 270.0          # TB saved by the levers priced before today
WITHIN = 0.252             # within-repo quant model: a repo costs 25.2% of standalone (RESULT 84 era)
CROSS_Q8 = 0.077           # RESULT 88
CROSS_K = 0.115            # RESULT 89
CROSS = 0.105              # a repo is a mix of both; the midpoint is used
FT_CHANGED = 13/24.0       # share of a fine-tune's tensors that are not byte-identical (RESULT 90)
FT_COST = 0.485            # what those cost given the base (RESULT 90)

print(f"{'resolution rate':>16s} {'extra from quants':>19s} {'extra from fine-tunes':>23s} "
      f"{'held with mzip':>16s} {'vs today':>10s} {'PB/yr':>8s}")
for rho in (0.30, 0.50, 0.75, 1.00):
    q_pool = GG * f_q * rho
    q_extra = q_pool * (WITHIN - CROSS)
    ft_pool = ST * f_ft
    ft_extra = ft_pool * FT_CHANGED * (1 - FT_COST)
    held = HELD_NOW - BASE_SAVE - q_extra - ft_extra
    print(f"{rho:15.0%} {q_extra:16.1f} TB {ft_extra:20.1f} TB {held:13.1f} TB "
          f"{100*(1-held/HELD_NOW):9.1f}% {(HELD_NOW-held)*52/1000:7.2f}")
print("\n  resolution rate = share of declared-parent repos whose base actually resolves;")
print("  measured 6/20 and 8/28 in RESULTs 88/89, so 30% is the measured floor and 100% is the ceiling.")
print("  The remainder is UNKNOWN -- the failures were dominated by my llama-only tensor-name mapper,")
print("  which is an instrument limit, not evidence either way.")
