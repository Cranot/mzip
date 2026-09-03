#!/usr/bin/env python3
"""Recompute the fine-tune saving with dedup's share removed.

A tensor that is 100% byte-identical to its base is contiguous identical bytes, which Xet's chunk
dedup already collapses. Counting it as a compression win would credit us with dedup's work. Only
tensors the fine-tune actually CHANGED are ours -- there the identical weights are scattered among
changed ones, so no 64 KiB chunk repeats and dedup sees pure novelty."""
import json
import numpy as np
rows = json.load(open("/root/mzip-hfbench/qgq/finetune.json"))
chg = [r for r in rows if r["ident"] < 0.999]
unt = [r for r in rows if r["ident"] >= 0.999]
print(f"tensors measured {len(rows)}: {len(unt)} untouched (dedup's), {len(chg)} changed (ours)\n")
print(f"{'repo':30s} {'tensor':13s} {'identical':>10s} {'raw':>7s} {'given base':>11s} {'cost':>7s}")
for r in sorted(chg, key=lambda x: x["raw"] and (min(x["diff"], x["pl"]) / x["raw"])):
    b = min(r["diff"], r["pl"])
    print(f"{r['rid'][:30]:30s} {r['k'].split('.')[-2][:13]:13s} {100*r['ident']:9.2f}% "
          f"{r['raw']:7.3f} {b:11.3f} {100*b/r['raw']:6.1f}%")
R = sum(r["raw"] for r in chg); C = sum(min(r["diff"], r["pl"]) for r in chg)
print(f"\n  CHANGED tensors only: {R/len(chg):.3f} bits/weight -> {C/len(chg):.3f}"
      f"  = {100*C/R:.1f}% of standalone ({R/C:.2f}x)")
print(f"  identical-weight share within changed tensors: "
      f"{100*np.mean([r['ident'] for r in chg]):.1f}% (scattered, so dedup cannot see it)")
allR = sum(r["raw"] for r in rows); allC = sum(min(r["diff"], r["pl"]) for r in rows)
print(f"  for contrast, counting untouched tensors too: {100*allC/allR:.1f}% ({allR/allC:.2f}x)"
      f"  <- inflated by dedup's share, not reported")
