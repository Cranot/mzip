#!/usr/bin/env python3
import json, sys
B = "/root/mzip-hfbench/timing/bin"; C = "/root/mzip-hfbench/corpora"; G = "/root/mzip-hfbench/weights"
CORE_IN = [f"{C}/prose-wikitext.bin", f"{C}/code-python.bin", f"{G}/weights-bf16.bin", f"{G}/weights-fp32.bin"]
QWEN_IN = [f"{G}/Qwen2.5-0.5B.slice"]

def mz(b, level="19", env=None):
    return dict(ccmd=[f"{B}/{b}","c","{IN}","{OUT}",level], dcmd=[f"{B}/{b}","d","{IN}","{OUT}"], env=env)
def mx(b, mode, bs="0", level="19", env=None):
    return dict(ccmd=[f"{B}/{b}","c","{IN}","{OUT}",level,mode,bs], dcmd=[f"{B}/{b}","d","{IN}","{OUT}"], env=env)

SPEC = {
 "ref_bg4lz4_HF": dict(kind="bg4", level=None, ccmd=[], dcmd=[]),
 "ref_bg4lz4_9":  dict(kind="bg4", level="-9", ccmd=[], dcmd=[]),
 "ref_lz4":   dict(ccmd=["lz4","-q","-f","{IN}","{OUT}"], dcmd=["lz4","-d","-q","-f","{IN}","{OUT}"]),
 "ref_zstd3": dict(ccmd=["zstd","-3","-T1","-q","-f","-o","{OUT}","{IN}"], dcmd=["zstd","-d","-q","-f","-o","{OUT}","{IN}"]),
 "ref_zstd19":dict(ccmd=["zstd","-19","-T1","-q","-f","-o","{OUT}","{IN}"],dcmd=["zstd","-d","-q","-f","-o","{OUT}","{IN}"]),
 "mzip_balanced_L19": mz("mzip_ctl"),
 "mzip_fast":         mx("mzipx","fast"),
 "mzip_small":        mx("mzipx","small"),
 "mzip_fast_bs1m":    mx("mzipx","fast","1048576"),
 "mzip_fast_noppmd":  mx("mzipx_noppmd","fast"),
 "mzip_noppmd":       mz("mzip_noppmd"),
 "mzip_pa":           mz("mzip_pa"),
 "mzip_ppmd128":      mz("mzip_ppmd128"),
 "mzip_maxratio":     mz("mzip_ctl", env={"MZIP_MAXRATIO":"1"}),
 "mzip_L1":           mz("mzip_ctl", level="1"),
 "mzip_mask_free":    mz("mzip_ctl", env={"MZIP_FAMILY_MASK":"0x13C"}),
 "mzip_mask_none":    mz("mzip_ctl", env={"MZIP_FAMILY_MASK":"0x0"}),
 "mzip_bal_bs1m":     mx("mzipx","balanced","1048576"),
 "mzip_fast_bs4m":    mx("mzipx","fast","4194304"),
 "mzip_fast_pa":      mx("mzipx_pa","fast"),
}
# CORE = the Pareto set (3 reps). SCREEN = "does this lever move time at all" (2 reps).
CORE   = ["ref_bg4lz4_HF","ref_lz4","ref_bg4lz4_9","ref_zstd3","ref_zstd19",
          "mzip_fast","mzip_fast_bs1m","mzip_fast_noppmd","mzip_balanced_L19",
          "mzip_pa","mzip_noppmd","mzip_ppmd128","mzip_maxratio","mzip_small"]
SCREEN = ["mzip_L1","mzip_mask_free","mzip_mask_none","mzip_bal_bs1m","mzip_fast_bs4m","mzip_fast_pa"]
QWEN   = ["ref_bg4lz4_HF","mzip_fast","mzip_balanced_L19","mzip_small","mzip_fast_bs1m"]

sets = {"core":(CORE,CORE_IN), "screen":(SCREEN,CORE_IN), "qwen":(QWEN,QWEN_IN)}
names, inputs = sets[sys.argv[1]]
plan = [dict(cfg=c, inp=i, **SPEC[c]) for c in names for i in inputs]
json.dump(plan, open(sys.argv[2],"w"), indent=1)
print(f"{sys.argv[1]}: {len(names)} configs x {len(inputs)} inputs = {len(plan)} jobs")
