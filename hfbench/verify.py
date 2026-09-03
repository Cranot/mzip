#!/usr/bin/env python3
import json, os
tot=ok=0; bad=[]
for p in ("cmresults.jsonl","cmresultsB.jsonl","splitresults.jsonl"):
    f="/root/mzip-hfbench/"+p
    if not os.path.exists(f): print(p,"MISSING"); continue
    n=v=0
    for l in open(f):
        r=json.loads(l); n+=1
        if r["size"] is not None and r["roundtrip"]: v+=1
        else: bad.append((p,r["tag"],r["coder"],r["size"],r["roundtrip"]))
    print("%-22s rows=%3d round-trip-verified=%3d" % (p,n,v)); tot+=n; ok+=v
print("TOTAL rows=%d verified=%d" % (tot,ok))
print("UNVERIFIED:", bad if bad else "none - every reported size round-tripped")
