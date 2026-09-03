#!/usr/bin/env python3
"""Aggregate timing jsonl -> medians, spread, Pareto frontier.
MB/s convention matches xet_full.py: bytes / 1048576 / seconds."""
import json, statistics as st, sys, glob, os

MIB = 1048576.0
rows = []
for p in sys.argv[1:]:
    for f in glob.glob(p):
        for line in open(f):
            line = line.strip()
            if line:
                rows.append(json.loads(line))

by = {}
for r in rows:
    by.setdefault((r["cfg"], r["inp"]), []).append(r)

print(f"# {len(rows)} measurements, {len(by)} (config,input) cells\n")

bad = [k for k, v in by.items() if any(x.get("rt") != "OK" for x in v)]
print(f"ROUND-TRIP: {sum(1 for r in rows if r.get('rt')=='OK')}/{len(rows)} OK"
      + (f"   FAILURES: {bad}" if bad else "   (zero failures)"))
nondet = [k for k, v in by.items() if len({x["comp"] for x in v}) > 1]
print(f"DETERMINISM: {'all cells byte-size-stable across reps' if not nondet else 'VARIES: '+str(nondet)}\n")

agg = {}
for (cfg, inp), v in by.items():
    ct = sorted(x["ct"] for x in v); dt = sorted(x["dt"] for x in v)
    raw = v[0]["raw"]; comp = v[0]["comp"]
    agg[(cfg, inp)] = dict(cfg=cfg, inp=inp, n=len(v), raw=raw, comp=comp,
        ct_med=st.median(ct), ct_min=ct[0], ct_max=ct[-1],
        dt_med=st.median(dt), dt_min=dt[0], dt_max=dt[-1],
        cmbs=raw/MIB/st.median(ct), dmbs=raw/MIB/st.median(dt),
        ct_spread=100*(ct[-1]-ct[0])/st.median(ct) if st.median(ct) else 0,
        dt_spread=100*(dt[-1]-dt[0])/st.median(dt) if st.median(dt) else 0)

inputs = sorted({k[1] for k in agg})
ORDER = ["ref_lz4","ref_bg4lz4_HF","ref_bg4lz4_9","ref_zstd3","ref_zstd19",
         "mzip_fast","mzip_fast_bs1m","mzip_fast_bs4m","mzip_fast_noppmd","mzip_fast_pa",
         "mzip_balanced_L19","mzip_L1","mzip_pa","mzip_noppmd","mzip_mask_free","mzip_mask_none",
         "mzip_bal_bs1m","mzip_ppmd128","mzip_maxratio","mzip_small"]
def keyf(c): return ORDER.index(c) if c in ORDER else 99

for inp in inputs:
    cells = [a for k, a in agg.items() if k[1] == inp]
    cells.sort(key=lambda a: keyf(a["cfg"]))
    raw = cells[0]["raw"]
    print(f"\n{'='*118}\n{inp}   raw {raw:,} B\n{'='*118}")
    print(f"{'config':20s} {'n':>2s} {'bytes':>10s} {'ratio':>6s} "
          f"{'c_med s':>9s} {'c min-max':>15s} {'sp%':>5s} {'MB/s c':>8s}  "
          f"{'d_med s':>8s} {'d min-max':>14s} {'sp%':>5s} {'MB/s d':>8s}")
    for a in cells:
        print(f"{a['cfg']:20s} {a['n']:>2d} {a['comp']:>10,} {raw/a['comp']:>5.3f}x "
              f"{a['ct_med']:>9.3f} {a['ct_min']:>7.3f}-{a['ct_max']:<7.3f} {a['ct_spread']:>4.1f} {a['cmbs']:>8.2f}  "
              f"{a['dt_med']:>8.4f} {a['dt_min']:>6.4f}-{a['dt_max']:<6.4f} {a['dt_spread']:>4.1f} {a['dmbs']:>8.1f}")

    # Pareto on (smaller bytes, higher compress MB/s, higher decompress MB/s)
    def dominated(x, ys):
        for y in ys:
            if y is x: continue
            if (y["comp"] <= x["comp"] and y["cmbs"] >= x["cmbs"] and y["dmbs"] >= x["dmbs"]
                    and (y["comp"] < x["comp"] or y["cmbs"] > x["cmbs"] or y["dmbs"] > x["dmbs"])):
                return y
        return None
    print("\n  PARETO-OPTIMAL (bytes, compress MB/s, decompress MB/s):")
    for a in sorted(cells, key=lambda z: z["comp"]):
        d = dominated(a, cells)
        if d is None:
            print(f"    * {a['cfg']:20s} {a['comp']:>10,} B  c {a['cmbs']:>7.2f} MB/s  d {a['dmbs']:>7.1f} MB/s")
    print("  STRICTLY DOMINATED:")
    for a in sorted(cells, key=lambda z: z["comp"]):
        d = dominated(a, cells)
        if d is not None:
            print(f"      {a['cfg']:20s} dominated by {d['cfg']}")
