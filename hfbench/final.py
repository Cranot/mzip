#!/usr/bin/env python3
import json, os, collections
def load(p): return [json.loads(l) for l in open(p)] if os.path.exists(p) else []
A=load("/root/mzip-hfbench/cmresults.jsonl")
Bv=load("/root/mzip-hfbench/cmresultsB.jsonl")
SP=load("/root/mzip-hfbench/splitresults.jsonl")
precA=json.load(open("/root/mzip-hfbench/precA.json"))
precB={m["tag"]:m for m in json.load(open("/root/mzip-hfbench/weightsB/slicesB.json"))}
PB={}
for t,m in precB.items():
    PB[t]=dict(low3=None,low13=None)
try:
    pb=json.load(open("/root/mzip-hfbench/precB.json")); PB=pb
except Exception: pass

def pclass(dt,low3,low13):
    if dt=="F32":
        if low13 is not None and low13>99: return "F32box/10bit"
        if low13 is not None and low13>10: return "F32/mask-mixed"
        return "F32/23bit"
    if dt=="F16": return "F16box/7bit" if (low3 is not None and low3>99) else "F16/10bit"
    return "BF16/7bit"

CO=["mzip","lpaq1","zpaq-m5","bwtcm","xz-9e"]; CM=["lpaq1","zpaq-m5","bwtcm"]

def build(rows,precmap):
    by=collections.defaultdict(dict); meta={}; bad=[]
    for r in rows:
        if r["size"] is None or not r["roundtrip"]:
            bad.append((r["tag"],r["coder"],r["size"],r["roundtrip"])); continue
        by[r["tag"]][r["coder"]]=r["size"]; meta[r["tag"]]=(r["dtype"],r["arch"],r["raw"])
    return by,meta,bad

def show(rows,name,precmap):
    by,meta,bad=build(rows,precmap)
    print("\n"+"="*126); print(name+"  (%d slices)"%len(by)); print("="*126)
    print("%-16s %-5s %-10s %-15s %9s %9s %9s %9s %9s %-8s %8s %8s" %
          ("slice","dt","arch","eff-precision","mzip","lpaq1","zpaq-m5","bwtcm","xz-9e","winner","lpaq v mz","best v mz"))
    tot=collections.Counter(); per=[]
    for tag in sorted(by,key=lambda t:(meta[t][0],meta[t][1])):
        d=by[tag]
        if not all(c in d for c in CO):
            print("%-16s INCOMPLETE %s"%(tag,sorted(d))); continue
        dt,arch,raw=meta[tag]
        pm=precmap.get(tag,{}) or {}
        pc=pclass(dt,pm.get("low3"),pm.get("low13"))
        bestcm=min(CM,key=lambda c:d[c])
        lp=100.0*(d["mzip"]-d["lpaq1"])/d["mzip"]
        gb=100.0*(d["mzip"]-d[bestcm])/d["mzip"]
        print("%-16s %-5s %-10s %-15s %9d %9d %9d %9d %9d %-8s %+7.2f%% %+7.2f%%"%
              (tag,dt,arch,pc,d["mzip"],d["lpaq1"],d["zpaq-m5"],d["bwtcm"],d["xz-9e"],
               min(CO,key=lambda c:d[c]),lp,gb))
        for c in CO: tot[c]+=d[c]
        tot["raw"]+=raw
        per.append((tag,dt,arch,pc,d,lp,gb))
    if per:
        print("-"*126)
        print("%-49s %9d %9d %9d %9d %9d"%("TOTAL (%d slices)"%len(per),
              tot["mzip"],tot["lpaq1"],tot["zpaq-m5"],tot["bwtcm"],tot["xz-9e"]))
        b=min(CM,key=lambda c:tot[c])
        print("raw=%d | mzip %.4fx | %s %.4fx | AGGREGATE best-CM gain over mzip: %+.3f%%  (lpaq1 alone %+.3f%%)"%
              (tot["raw"],tot["raw"]/tot["mzip"],b,tot["raw"]/tot[b],
               100.0*(tot["mzip"]-tot[b])/tot["mzip"],
               100.0*(tot["mzip"]-tot["lpaq1"])/tot["mzip"]))
    if bad:
        print("\nDROPPED (round-trip failed / no size):")
        for x in bad: print("   ",x)
    return per

def groups(per,label):
    for key,idx in (("NOMINAL dtype",1),("EFFECTIVE precision",3),("ARCHITECTURE",2)):
        g=collections.defaultdict(list)
        for row in per: g[row[idx]].append((row[0],row[5],row[6]))
        print("\n  -- %s by %s --"%(label,key))
        for k in sorted(g):
            lw=sum(1 for _,l,_ in g[k] if l>0); bw=sum(1 for _,_,b in g[k] if b>0)
            print("   %-16s n=%d | lpaq1 beats mzip %d/%d | any-CM beats mzip %d/%d | %s"%
                  (k,len(g[k]),lw,len(g[k]),bw,len(g[k]),
                   ", ".join("%s lpaq%+.2f"%(t,l) for t,l,_ in g[k])))

pa=show(A,"FAMILY A - midpoint tensor, as-stored bytes (spans tensor boundaries)",precA)
pb=show(Bv,"FAMILY B - 8MiB from INSIDE one dense weight matrix",PB)
if pa: groups(pa,"A")
if pb: groups(pb,"B")

if SP:
    print("\n"+"="*126); print("PLANE-SPLIT vs NATIVE (family B)"); print("="*126)
    sp=collections.defaultdict(dict)
    for r in SP:
        if r["size"] is not None and r["roundtrip"]: sp[r["tag"]][r["coder"]]=r["size"]
    nb,_,_=build(Bv,PB)
    print("%-16s %10s %10s %10s %10s %10s %10s"%("slice","mzip-nat","mzip-spl","lpaq-nat","lpaq-spl","zpaq-nat","zpaq-spl"))
    T=collections.Counter()
    for tag in sorted(sp):
        if tag not in nb: continue
        n=nb[tag]; s=sp[tag]
        if not all(k in s for k in ("mzip","lpaq1","zpaq-m5")): continue
        print("%-16s %10d %10d %10d %10d %10d %10d"%(tag,n["mzip"],s["mzip"],n["lpaq1"],s["lpaq1"],n["zpaq-m5"],s["zpaq-m5"]))
        T["mn"]+=n["mzip"]; T["ms"]+=s["mzip"]; T["ln"]+=n["lpaq1"]; T["ls"]+=s["lpaq1"]
        T["zn"]+=n["zpaq-m5"]; T["zs"]+=s["zpaq-m5"]
    if T:
        print("-"*126)
        print("TOTAL            %10d %10d %10d %10d %10d %10d"%(T["mn"],T["ms"],T["ln"],T["ls"],T["zn"],T["zs"]))
        print("split effect: mzip %+.2f%% | lpaq1 %+.2f%% | zpaq %+.2f%%   (negative = split is smaller)"%
              (100.0*(T["ms"]-T["mn"])/T["mn"],100.0*(T["ls"]-T["ln"])/T["ln"],100.0*(T["zs"]-T["zn"])/T["zn"]))
        best=min(T["ms"],T["ls"],T["zs"])
        print("BEST split pipeline vs native mzip: %+.3f%% (negative = beats mzip)"%(100.0*(best-T["mn"])/T["mn"]))
