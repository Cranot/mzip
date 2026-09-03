import glob, os
D='/root/mzip-hfbench/verify-a4/cand'
print(f"{'input':20s} {'archive':>9s} {'telem':>9s} {'winner':>8s}  cost-if-masked, per family")
worst=[]
for p in sorted(glob.glob(D+'/*.cand')):
    n=os.path.basename(p)[:-5]
    mz=os.path.join(D,n+'.mz')
    if not os.path.exists(mz): continue
    arc=os.path.getsize(mz)
    groups=[]
    for line in open(p):
        f=line.rstrip('\n').split('\t')
        if len(f)!=5 or f[0]!='MZCAND': continue
        magic,fam,avail,sz=f[1],f[2],f[3],int(f[4])
        if magic=='BLOCKS': groups.append([])
        if not groups: continue
        groups[-1].append((magic,fam,avail,sz))
    if not groups: continue
    top=groups[-1]                       # top level is emitted LAST
    if len(top)!=16:
        print(f"{n:20s}  INCOMPLETE top-level group ({len(top)} lines) -- run still in flight, skipped")
        continue
    av=[(m,fa,s) for (m,fa,a,s) in top if a=='avail']
    best=min(s for _,_,s in av)
    if best!=arc:
        print(f"{n:20s}  MISMATCH telem_best={best} archive={arc} -- skipped")
        continue
    win=','.join(m for m,_,s in av if s==best)
    fams=sorted(set(fa for _,fa,_ in av if fa!='--FALLBACK'))
    costs={fam: min(s for m,fa,s in av if fa!=fam)-best for fam in fams}
    for fam in fams: worst.append((costs[fam],n,fam))
    cs='  '.join(f"{fam}={costs[fam]}" for fam in fams if costs[fam]) or '(every family costs 0)'
    print(f"{n:20s} {arc:>9d} {best:>9d} {win:>8s}  {cs}")
print("\n=== ranked single-family masking cost across every input x family ===")
for c,n,fam in sorted(worst,reverse=True)[:15]:
    print(f"  {c:>9d} B   {n:20s} {fam}")
