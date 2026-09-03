#!/usr/bin/env python3
import numpy as np, json, os
W2="/root/mzip-hfbench/weights2"; W1="/root/mzip-hfbench/weights"
S=[(m["tag"],f"{W2}/{m['tag']}.bin",m["dtype"]) for m in json.load(open(f"{W2}/slices.json"))]
S+=[("minilm-legacy",f"{W1}/weights-fp32.bin","F32"),
    ("smollm2-legacy",f"{W1}/weights-bf16.bin","BF16"),
    ("qwen0.5b-legacy",f"{W1}/Qwen2.5-0.5B.slice","BF16")]
print("%-16s %-5s %9s %10s %10s" % ("tag","dt","distinct","low3=0 %","low13=0 %"))
out={}
for tag,p,dt in S:
    raw=np.fromfile(p,dtype=np.uint8)
    if dt=="F32":
        v=raw.view(np.uint32); l3=100.0*np.mean((v&7)==0); l13=100.0*np.mean((v&0x1fff)==0)
    else:
        v=raw.view(np.uint16); l3=100.0*np.mean((v&7)==0); l13=float("nan")
    d=len(np.unique(v))
    out[tag]=dict(distinct=int(d),low3=float(l3),low13=float(l13))
    print("%-16s %-5s %9d %10.2f %10.2f" % (tag,dt,d,l3,l13))
json.dump(out,open("/root/mzip-hfbench/precA.json","w"),indent=1)
