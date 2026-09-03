#!/usr/bin/env python3
"""The week's numbers, with the GGUF class rebuilt from scratch rather than patched.

The first attempt read the class table as terabytes when it is bytes, and tried to fold the cross-repo
lever in as a delta against the old within-repo model. That delta is wrong: the old model only ever
priced MULTI-FILE repos, so a single-file GGUF with a public parent was previously worth nothing and
its improvement is the whole saving, not the difference between two rates.

So the class is rebuilt. Every sampled repo's GGUF bytes land in exactly one bucket:
  * a declared parent that resolves      -> 10.5% of standalone (RESULTs 88, 89)
  * no usable parent, several GGUF files -> the sibling model, 25.2% (RESULTs 84, 88)
  * no usable parent, one file           -> per-chunk only; quantised weights are near-incompressible
                                            (RESULT 87), so 97%
The resolution rate is measured at 6/20 and 8/28 and the rest is UNKNOWN, so it stays a parameter."""
import json, collections
B = "/root/mzip-hfbench"
U = json.load(open(f"{B}/upload-mix/UPLOAD-MIX.json"))
tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
cls = {k: v / 1e12 for k, v in U["classes"].items()}
TOT = U["total"] / 1e12

prov = {}
try:
    for x in json.load(open(f"{B}/gguf-prov/gguf_prov.json")):
        k = x.get("repo") or x.get("id")
        if k: prov[k] = int(x.get("files") or 0)
except Exception:
    pass

def rels(rid):
    out = set()
    for x in ((tags.get(rid) or {}).get("tags") or []):
        p = x.split(":")
        if x.startswith("base_model:") and len(p) >= 3: out.add(p[1])
    return out

g_parent = g_multi = g_single = 0.0
s_all = s_ft = 0.0
for r in rows:
    c = r.get("bytes_in_window") or {}
    g = sum(float(v or 0) for k, v in c.items() if k.startswith("gguf"))
    s = sum(float(v or 0) for k, v in c.items() if k.startswith("safetensors"))
    s_all += s
    rl = rels(r["id"])
    if rl & {"finetune", "merge", "adapter"}: s_ft += s
    if g <= 0: continue
    if "quantized" in rl: g_parent += g
    elif prov.get(r["id"], 0) > 1: g_multi += g
    else: g_single += g

gt = g_parent + g_multi + g_single
GG = sum(v for k, v in cls.items() if k.startswith("gguf"))
ST = sum(v for k, v in cls.items() if k.startswith("safetensors"))
sh = lambda x: x / max(gt, 1)
f_ft = s_ft / max(s_all, 1)
print(f"week uploaded {TOT:.1f} TB;  gguf {GG:.1f} TB;  safetensors {ST:.1f} TB")
print(f"  gguf bytes by bucket:  declared parent {100*sh(g_parent):.1f}%   "
      f"multi-file, no parent {100*sh(g_multi):.1f}%   single file, no parent {100*sh(g_single):.1f}%")
print(f"  safetensors in finetune/merge/adapter repos: {100*f_ft:.1f}%\n")

HELD_NOW = 1444.0
OTHER_SAVE = 270.0 - 104.3      # the week's other levers, with the old quant lever removed
FT_CHANGED = 13 / 24.0
FT_COST = 0.485
print(f"{'resolve':>9s} {'gguf held':>11s} {'quant saves':>13s} {'finetune saves':>16s} "
      f"{'held w/ mzip':>14s} {'vs today':>10s} {'PB/yr':>8s}")
for rho in (0.30, 0.50, 0.75, 1.00):
    par = GG * sh(g_parent)
    cost = (par * rho * 0.105 + par * (1 - rho) * 0.97
            + GG * sh(g_multi) * 0.252 + GG * sh(g_single) * 0.97)
    q_save = GG - cost
    ft_save = ST * f_ft * FT_CHANGED * (1 - FT_COST)
    held = HELD_NOW - OTHER_SAVE - q_save - ft_save
    print(f"{rho:8.0%} {cost:10.1f} TB {q_save:10.1f} TB {ft_save:13.1f} TB {held:11.1f} TB "
          f"{100*(1-held/HELD_NOW):9.1f}% {(HELD_NOW-held)*52/1000:7.2f}")
print("\n  resolution rate = share of declared-parent repos whose base resolves to public safetensors")
print("  with mappable tensor names. Measured 6/20 and 8/28; the failures were dominated by a")
print("  llama-only name mapper, which is an instrument limit, so the rest is UNKNOWN.")
