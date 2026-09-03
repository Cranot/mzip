#!/usr/bin/env python3
# High-n interleaved A/B: does the periodic_approx early-out move END-TO-END wall clock?
# Also re-checks noppmd on text at higher n. Reps are the outer loop -> configs interleave.
import json, sys
B="/root/mzip-hfbench/timing/bin"; C="/root/mzip-hfbench/corpora"; G="/root/mzip-hfbench/weights"
def mz(b,level="19",env=None):
    return dict(ccmd=[f"{B}/{b}","c","{IN}","{OUT}",level],dcmd=[f"{B}/{b}","d","{IN}","{OUT}"],env=env)
which=sys.argv[1]
if which=="ab_fp32":
    plan=[dict(cfg=c,inp=f"{G}/weights-fp32.bin",**mz(b))
          for c,b in [("mzip_balanced_L19","mzip_ctl"),("mzip_pa","mzip_pa")]]
elif which=="ab_prose":
    plan=[dict(cfg=c,inp=f"{C}/prose-wikitext.bin",**mz(b))
          for c,b in [("mzip_balanced_L19","mzip_ctl"),("mzip_pa","mzip_pa"),("mzip_noppmd","mzip_noppmd")]]
json.dump(plan,open(sys.argv[2],"w"),indent=1); print(f"{which}: {len(plan)} jobs")
