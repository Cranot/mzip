import os, glob

rows = []
for fn in ["results.tsv", "results2.tsv", "results3.tsv", "results4.tsv"]:
    p = "/root/mzip-hfbench/wz/" + fn
    if not os.path.exists(p): continue
    for ln in open(p):
        f = ln.rstrip("\n").split("\t")
        if f[0].startswith("DONE"): continue
        rows.append((fn, f))

# baselines (mzip) per source file
BASE = {
    "weights-bf16.bin":   5571614,
    "weights-fp32.bin":   6155434,
    "bf16-layers.bin":    5630778,
    "fp32-enc.bin":       7017702,
    "Qwen2.5-0.5B.slice": 5526346,
}
# map tag -> (source, label)
def src_of(fn, tag):
    if tag.startswith("HO_bf16lay") or tag.startswith("HOa_bf16layers") or tag.startswith("layers_"):
        return "bf16-layers.bin"
    if tag.startswith("HO_fp32enc") or tag.startswith("HOc_fp32enc"):
        return "fp32-enc.bin"
    if tag.startswith("HOb_qwen") or tag.startswith("qwen_"):
        return "Qwen2.5-0.5B.slice"
    if tag.startswith("deep_"):
        return "bf16-deep.bin"
    if tag.startswith("b_") or tag.startswith("identity_bf16") or tag.startswith("embed_"):
        return "weights-bf16.bin"
    return "weights-fp32.bin"

print("%-28s %-22s %-6s %12s %10s  %s" % ("transform", "source", "coder", "bytes", "vs plain", "roundtrip"))
out = []
for fn, f in rows:
    if len(f) == 5: tag, coder, size, ver, _ = f
    elif len(f) == 4: tag, coder, size, ver = f
    else: tag, size, ver = f; coder = "mzip"
    s = src_of(fn, tag)
    try: size = int(size)
    except: print("%-28s %-22s %-6s %12s %10s  %s" % (tag, s, coder, size, "-", ver)); continue
    out.append((s, coder, tag, size, ver))

for s in ["weights-bf16.bin", "bf16-layers.bin", "bf16-deep.bin", "Qwen2.5-0.5B.slice",
          "weights-fp32.bin", "fp32-enc.bin"]:
    grp = [r for r in out if r[0] == s]
    if not grp: continue
    for coder in ["mzip", "zstd"]:
        g = sorted([r for r in grp if r[1] == coder], key=lambda r: r[3])
        if not g: continue
        base = None
        for r in g:
            if "identity" in r[2]: base = r[3]
        if base is None: base = BASE.get(s)
        print()
        for r in g:
            d = "" if base is None else "%+.3f%%" % (100.0 * (r[3] - base) / base)
            print("%-28s %-22s %-6s %12d %10s  %s" % (r[2], s, coder, r[3], d, r[4]))
