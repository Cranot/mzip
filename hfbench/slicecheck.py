#!/usr/bin/env python3
"""Sanity-check every slice: zero fraction, distinct bytes, per-plane entropy.
Guards against reporting headroom that is really a padding region."""
import collections, json, math, os
W2="/root/mzip-hfbench/weights2"; W1="/root/mzip-hfbench/weights"
S=[(m["tag"],f"{W2}/{m['tag']}.bin",m["dtype"]) for m in json.load(open(f"{W2}/slices.json"))]
S+=[("minilm-legacy",f"{W1}/weights-fp32.bin","F32"),
    ("smollm2-legacy",f"{W1}/weights-bf16.bin","BF16"),
    ("qwen0.5b-legacy",f"{W1}/Qwen2.5-0.5B.slice","BF16")]
def H(b):
    c=collections.Counter(b); t=sum(c.values())
    return -sum(v/t*math.log2(v/t) for v in c.values()), len(c)
print(f"{'tag':16s} {'dt':5s} {'zero%':>6s} {'H0':>6s} {'planes (entropy bits/byte)'}")
for tag,p,dt in S:
    raw=open(p,"rb").read()
    z=100.0*raw.count(0)/len(raw)
    h,dn=H(raw)
    w = 4 if dt=="F32" else 2
    pl=" ".join(f"{H(raw[k::w])[0]:.3f}" for k in range(w))
    print(f"{tag:16s} {dt:5s} {z:6.2f} {h:6.3f} distinct={dn:3d}  planes: {pl}")
