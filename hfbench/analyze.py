#!/usr/bin/env python3
"""Build the final tables. Any cell whose round-trip failed is dropped and named."""
import json, os, collections

def load(p):
    return [json.loads(l) for l in open(p)] if os.path.exists(p) else []

A = load("/root/mzip-hfbench/cmresults.jsonl")
Bv= load("/root/mzip-hfbench/cmresultsB.jsonl")
precA = json.load(open("/root/mzip-hfbench/precA.json"))
precB = {m["tag"]: m for m in json.load(open("/root/mzip-hfbench/weightsB/slicesB.json"))}
import numpy as np

def pclass(dtype, low3, low13):
    if dtype == "F32":
        if low13 > 99: return "F32box/10bit"
        if low13 > 10: return "F32/mask-mixed"
        return "F32/23bit"
    if dtype == "F16":
        return "F16box/7bit" if low3 > 99 else "F16/10bit"
    return "BF16/7bit"

CO = ["mzip","lpaq1","zpaq-m5","bwtcm","xz-9e"]
CM = ["lpaq1","zpaq-m5","bwtcm"]

def table(rows, name, precmap):
    by = collections.defaultdict(dict)
    meta = {}
    bad = []
    for r in rows:
        if r["size"] is None or not r["roundtrip"]:
            bad.append((r["tag"], r["coder"], r["size"], r["roundtrip"])); continue
        by[r["tag"]][r["coder"]] = r["size"]
        meta[r["tag"]] = (r["dtype"], r["arch"], r["raw"])
    print(f"\n{'='*118}\n{name}  ({len(by)} slices)\n{'='*118}")
    print("%-16s %-5s %-10s %-15s %9s %9s %9s %9s %9s  %-8s %7s" %
          ("slice","dt","arch","precision","mzip","lpaq1","zpaq-m5","bwtcm","xz-9e","winner","CM gain"))
    tot = collections.Counter(); n_ok = 0
    per = []
    for tag in sorted(by, key=lambda t: (meta[t][0], meta[t][1])):
        d = by[tag]
        if not all(c in d for c in CO): 
            print("%-16s INCOMPLETE: have %s" % (tag, sorted(d))); continue
        dt, arch, raw = meta[tag]
        if tag in precmap and isinstance(precmap[tag], dict) and "low3" in precmap[tag]:
            pc = pclass(dt, precmap[tag]["low3"], precmap[tag].get("low13", 0) or 0)
        else:
            pm = precmap.get(tag, {})
            pc = pclass(dt, pm.get("low3", 12.5), pm.get("low13", 0) or 0)
        best = min(CO, key=lambda c: d[c])
        bestcm = min(CM, key=lambda c: d[c])
        gain = 100.0 * (d["mzip"] - d[bestcm]) / d["mzip"]
        print("%-16s %-5s %-10s %-15s %9d %9d %9d %9d %9d  %-8s %+6.2f%%" %
              (tag, dt, arch, pc, d["mzip"], d["lpaq1"], d["zpaq-m5"], d["bwtcm"], d["xz-9e"], best, gain))
        for c in CO: tot[c] += d[c]
        tot["raw"] += raw; n_ok += 1
        per.append((tag, dt, arch, pc, d, gain, best))
    if n_ok:
        print("-"*118)
        print("%-49s %9d %9d %9d %9d %9d   raw=%d" %
              ("TOTAL (%d slices)" % n_ok, tot["mzip"], tot["lpaq1"], tot["zpaq-m5"], tot["bwtcm"], tot["xz-9e"], tot["raw"]))
        bcm = min(CM, key=lambda c: tot[c])
        print("%-49s mzip %.3fx | best-CM=%s %.3fx | aggregate CM gain over mzip: %+.2f%%" %
              ("", tot["raw"]/tot["mzip"], bcm, tot["raw"]/tot[bcm], 100.0*(tot["mzip"]-tot[bcm])/tot["mzip"]))
    if bad:
        print("\nDROPPED (round-trip failed or no size):")
        for b in bad: print("   ", b)
    return per

perA = table(A, "FAMILY A - slice at midpoint tensor (as-stored bytes, spans tensor boundaries)", precA)
perB = table(Bv, "FAMILY B - 8MiB from INSIDE one dense weight matrix", precB)

for nm, per in (("A", perA), ("B", perB)):
    if not per: continue
    print(f"\n--- family {nm}: grouped by NOMINAL dtype ---")
    g = collections.defaultdict(list)
    for tag, dt, arch, pc, d, gain, best in per: g[dt].append((tag, gain, best))
    for k in sorted(g):
        w = sum(1 for _, gg, _ in g[k] if gg > 0)
        print("  %-6s n=%d  CM beats mzip on %d/%d  gains: %s" %
              (k, len(g[k]), w, len(g[k]), ", ".join("%s %+.2f%%" % (t, gg) for t, gg, _ in g[k])))
    print(f"--- family {nm}: grouped by EFFECTIVE PRECISION ---")
    g = collections.defaultdict(list)
    for tag, dt, arch, pc, d, gain, best in per: g[pc].append((tag, gain))
    for k in sorted(g):
        w = sum(1 for _, gg in g[k] if gg > 0)
        print("  %-16s n=%d  CM beats mzip on %d/%d  gains: %s" %
              (k, len(g[k]), w, len(g[k]), ", ".join("%s %+.2f%%" % (t, gg) for t, gg in g[k])))
    print(f"--- family {nm}: grouped by ARCHITECTURE ---")
    g = collections.defaultdict(list)
    for tag, dt, arch, pc, d, gain, best in per: g[arch].append((tag, gain))
    for k in sorted(g):
        w = sum(1 for _, gg in g[k] if gg > 0)
        print("  %-10s n=%d  CM beats mzip on %d/%d  gains: %s" %
              (k, len(g[k]), w, len(g[k]), ", ".join("%s %+.2f%%" % (t, gg) for t, gg in g[k])))
