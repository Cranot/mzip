#!/usr/bin/env python3
"""Test the effective-precision hypothesis at the BIT level, not by entropy inference.
For each slice: how many low mantissa bits are IDENTICALLY ZERO across every element?"""
import json, numpy as np
W="/root/mzip-hfbench/weightsB"
for m in json.load(open(f"{W}/slicesB.json")):
    raw=np.fromfile(f"{W}/{m['tag']}.bin",dtype=np.uint8)
    if m["dtype"]=="F32":
        v=raw.view(np.uint32); mant_bits=23
    else:
        v=raw.view(np.uint16).astype(np.uint32); mant_bits=10 if m["dtype"]=="F16" else 7
    dead=0
    for b in range(mant_bits):
        if np.any((v>>b)&1): break
        dead+=1
    live=mant_bits-dead
    print(f"{m['tag']:15s} {m['dtype']:5s} mantissa_bits={mant_bits:2d} "
          f"dead_low_bits={dead:2d} live={live:2d}  -> effective ~{live} mantissa bits")
