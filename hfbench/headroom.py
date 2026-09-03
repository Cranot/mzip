#!/usr/bin/env python3
import json, os, collections
def load(p): return [json.loads(l) for l in open(p)] if os.path.exists(p) else []
precA=json.load(open("/root/mzip-hfbench/precA.json"))
precB=json.load(open("/root/mzip-hfbench/precB.json"))
def pclass(dt,low3,low13):
    if dt=="F32":
        if low13 is not None and low13>99: return "F32box/10bit"
        if low13 is not None and low13>10: return "F32/mask-mixed"
        return "F32/23bit"
    if dt=="F16": return "F16box/7bit" if (low3 is not None and low3>99) else "F16/10bit"
    return "BF16/7bit"
CO=["mzip","lpaq1","zpaq-m5","bwtcm","xz-9e"]; CM=["lpaq1","zpaq-m5","bwtcm"]
for fam,path,pm in (("A","/root/mzip-hfbench/cmresults.jsonl",precA),
                    ("B","/root/mzip-hfbench/cmresultsB.jsonl",precB)):
    rows=load(path)
    by=collections.defaultdict(dict); meta={}
    for r in rows:
        if r["size"] is None or not r["roundtrip"]: continue
        by[r["tag"]][r["coder"]]=r["size"]; meta[r["tag"]]=(r["dtype"],r["arch"],r["raw"])
    grp=collections.defaultdict(lambda: collections.Counter())
    LOW={"BF16/7bit","F16box/7bit","F32box/10bit"}
    agg=collections.Counter()
    for tag,d in by.items():
        if not all(c in d for c in CO): continue
        dt,arch,raw=meta[tag]
        p=pm.get(tag,{}) or {}
        pc=pclass(dt,p.get("low3"),p.get("low13"))
        for c in CO: grp[pc][c]+=d[c]; agg["ALL_"+c]+=d[c]
        grp[pc]["raw"]+=raw; grp[pc]["n"]+=1
        band = "LOW-PRECISION (quantized)" if pc in LOW else "HIGH-PRECISION"
        for c in CO: grp[band][c]+=d[c]
        grp[band]["raw"]+=raw; grp[band]["n"]+=1
        agg["ALL_raw"]+=raw; agg["ALL_n"]+=1
    print("\n########## FAMILY %s : aggregate headroom by precision class ##########"%fam)
    print("%-26s %3s %11s %11s %11s %10s %10s"%("class","n","raw","mzip","best-CM","bestCM name","gain vs mzip"))
    order=[k for k in grp if "PRECISION" not in k]+[k for k in grp if "PRECISION" in k]
    for k in order:
        g=grp[k]
        b=min(CM,key=lambda c:g[c])
        gain=100.0*(g["mzip"]-g[b])/g["mzip"]
        print("%-26s %3d %11d %11d %11d %10s   %+7.3f%%"%(k,g["n"],g["raw"],g["mzip"],g[b],b,gain))
    b=min(CM,key=lambda c:agg["ALL_"+c])
    print("%-26s %3d %11d %11d %11d %10s   %+7.3f%%"%("ALL SLICES",agg["ALL_n"],agg["ALL_raw"],
          agg["ALL_mzip"],agg["ALL_"+b],b,100.0*(agg["ALL_mzip"]-agg["ALL_"+b])/agg["ALL_mzip"]))
