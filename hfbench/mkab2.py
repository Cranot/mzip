#!/usr/bin/env python3
# Is mzip's compression LEVEL time-inert as well as byte-inert? L1 vs L19, interleaved.
import json, sys
B="/root/mzip-hfbench/timing/bin"; G="/root/mzip-hfbench/weights"; C="/root/mzip-hfbench/corpora"
def mz(b,level):
    return dict(ccmd=[f"{B}/{b}","c","{IN}","{OUT}",level],dcmd=[f"{B}/{b}","d","{IN}","{OUT}"])
plan=[]
for inp in [f"{G}/weights-bf16.bin", f"{G}/weights-fp32.bin"]:
    plan.append(dict(cfg="mzip_balanced_L19",inp=inp,**mz("mzip_ctl","19")))
    plan.append(dict(cfg="mzip_L1",inp=inp,**mz("mzip_ctl","1")))
json.dump(plan,open(sys.argv[1],"w"),indent=1); print(f"{len(plan)} jobs")
