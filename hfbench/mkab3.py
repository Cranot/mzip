#!/usr/bin/env python3
# The untested combination: FAST mode AT LEVEL 1. FAST buys ~2x, level-1 buys ~4-16%.
import json, sys
B="/root/mzip-hfbench/timing/bin"; G="/root/mzip-hfbench/weights"; C="/root/mzip-hfbench/corpora"
def mx(b,mode,bs,level):
    return dict(ccmd=[f"{B}/{b}","c","{IN}","{OUT}",level,mode,bs],dcmd=[f"{B}/{b}","d","{IN}","{OUT}"])
plan=[]
for inp in [f"{G}/weights-bf16.bin", f"{G}/weights-fp32.bin", f"{G}/Qwen2.5-0.5B.slice",
            f"{C}/prose-wikitext.bin", f"{C}/code-python.bin"]:
    plan.append(dict(cfg="mzip_fast",    inp=inp, **mx("mzipx","fast","0","19")))
    plan.append(dict(cfg="mzip_fast_L1", inp=inp, **mx("mzipx","fast","0","1")))
json.dump(plan,open(sys.argv[1],"w"),indent=1); print(f"{len(plan)} jobs")
