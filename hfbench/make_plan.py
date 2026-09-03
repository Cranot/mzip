#!/usr/bin/env python3
"""Emit the measurement plan as JSON. Usage: make_plan.py <which> <out.json>
   which = core | screen | weights"""
import json, sys

B = "/root/mzip-hfbench/timing/bin"
C = "/root/mzip-hfbench/corpora"
G = "/root/mzip-hfbench/weights"

TEXT = [f"{C}/prose-wikitext.bin", f"{C}/code-python.bin"]
WGT  = [f"{G}/weights-bf16.bin", f"{G}/weights-fp32.bin"]
CORE = TEXT + WGT
QWEN = [f"{G}/Qwen2.5-0.5B.slice"]


def mz(binname, level="19", env=None):
    return dict(ccmd=[f"{B}/{binname}", "c", "{IN}", "{OUT}", level],
                dcmd=[f"{B}/{binname}", "d", "{IN}", "{OUT}"], env=env)


def mx(binname, mode, bs="0", level="19", env=None):
    return dict(ccmd=[f"{B}/{binname}", "c", "{IN}", "{OUT}", level, mode, bs],
                dcmd=[f"{B}/{binname}", "d", "{IN}", "{OUT}"], env=env)


REF = {
    "ref_lz4":       dict(ccmd=["lz4", "-q", "-f", "{IN}", "{OUT}"],
                          dcmd=["lz4", "-d", "-q", "-f", "{IN}", "{OUT}"]),
    "ref_bg4lz4_HF": dict(kind="bg4", level=None, ccmd=[], dcmd=[]),
    "ref_bg4lz4_9":  dict(kind="bg4", level="-9", ccmd=[], dcmd=[]),
    "ref_zstd19":    dict(ccmd=["zstd", "-19", "-T1", "-q", "-f", "-o", "{OUT}", "{IN}"],
                          dcmd=["zstd", "-d", "-q", "-f", "-o", "{OUT}", "{IN}"]),
    "ref_zstd3":     dict(ccmd=["zstd", "-3", "-T1", "-q", "-f", "-o", "{OUT}", "{IN}"],
                          dcmd=["zstd", "-d", "-q", "-f", "-o", "{OUT}", "{IN}"]),
}

# Configurations that could plausibly move wall-clock time.
MZIP = {
    "mzip_balanced_L19": mz("mzip_ctl"),                                   # shipped default
    "mzip_L1":           mz("mzip_ctl", level="1"),                        # level is byte-inert; time?
    "mzip_maxratio":     mz("mzip_ctl", env={"MZIP_MAXRATIO": "1"}),       # lifts brotli-11 cap
    "mzip_mask_free":    mz("mzip_ctl", env={"MZIP_FAMILY_MASK": "0x13C"}),# 4 "free" families off
    "mzip_mask_none":    mz("mzip_ctl", env={"MZIP_FAMILY_MASK": "0x0"}),  # all families off = mask time floor
    "mzip_noppmd":       mz("mzip_noppmd"),
    "mzip_ppmd128":      mz("mzip_ppmd128"),
    "mzip_pa":           mz("mzip_pa"),                                    # periodic_approx early-out
    "mzip_fast":         mx("mzipx", "fast"),
    "mzip_small":        mx("mzipx", "small"),
    "mzip_fast_bs1m":    mx("mzipx", "fast", "1048576"),
    "mzip_fast_bs4m":    mx("mzipx", "fast", "4194304"),
    "mzip_bal_bs1m":     mx("mzipx", "balanced", "1048576"),
    "mzip_fast_noppmd":  mx("mzipx_noppmd", "fast"),
    "mzip_fast_pa":      mx("mzipx_pa", "fast"),
    "mzip_bal_pa_x":     mx("mzipx_pa", "balanced"),
}

which = sys.argv[1]
plan = []
if which == "core":
    order = (["ref_bg4lz4_HF", "ref_lz4", "ref_bg4lz4_9", "ref_zstd3", "ref_zstd19"] +
             ["mzip_fast", "mzip_fast_bs1m", "mzip_fast_bs4m", "mzip_fast_noppmd",
              "mzip_fast_pa", "mzip_balanced_L19", "mzip_pa", "mzip_noppmd",
              "mzip_mask_free", "mzip_mask_none", "mzip_L1", "mzip_bal_bs1m",
              "mzip_ppmd128", "mzip_maxratio", "mzip_small", "mzip_bal_pa_x"])
    for cfg in order:
        spec = REF.get(cfg) or MZIP[cfg]
        for inp in CORE:
            plan.append(dict(cfg=cfg, inp=inp, **spec))
elif which == "qwen":
    for cfg in ["ref_bg4lz4_HF", "mzip_fast", "mzip_balanced_L19", "mzip_small"]:
        spec = REF.get(cfg) or MZIP[cfg]
        for inp in QWEN:
            plan.append(dict(cfg=cfg, inp=inp, **spec))
json.dump(plan, open(sys.argv[2], "w"), indent=1)
print(f"{len(plan)} jobs -> {sys.argv[2]}")
