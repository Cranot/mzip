import glob, os, collections
FAM = {}
rows_by_file = {}
for p in sorted(glob.glob('/root/mzip-hfbench/verify-a4/cand/*.cand')):
    n = os.path.basename(p)[:-5]
    rows = []
    for line in open(p):
        f = line.rstrip('\n').split('\t')
        if len(f) != 5 or f[0] != 'MZCAND':
            continue
        magic, fam, avail, sz = f[1], f[2], f[3], int(f[4])
        rows.append((magic, fam, avail, sz))
    if rows:
        rows_by_file[n] = rows

print(f"{'input':20s} {'best':>10s} {'winner':>14s}   per-family cost if masked (bytes)")
worst = []
for n, rows in rows_by_file.items():
    avail = [(m, fa, s) for (m, fa, a, s) in rows if a == 'avail']
    best = min(s for (_, _, s) in avail)
    win = [m for (m, _, s) in avail if s == best]
    fams = sorted(set(fa for (_, fa, _) in avail if fa != '--FALLBACK'))
    costs = {}
    for fam in fams:
        rem = [s for (m, fa, s) in avail if fa != fam]
        costs[fam] = min(rem) - best
        worst.append((costs[fam], n, fam))
    cs = '  '.join(f"{fam}={costs[fam]}" for fam in fams if costs[fam] != 0) or '(all families cost 0)'
    print(f"{n:20s} {best:>10d} {','.join(win):>14s}   {cs}")

print("\n=== ranked: single-family masking cost, all inputs x all families ===")
for c, n, fam in sorted(worst, reverse=True)[:12]:
    print(f"  {c:>9d} B   {n:20s} {fam}")
