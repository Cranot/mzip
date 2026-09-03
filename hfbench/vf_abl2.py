#!/usr/bin/env python3
"""(1) Why are the k variants byte-identical in size?  Ask mzip what it picked.
(2) Ablate the fp16-signature elements out of MiniLM and re-sweep k.
(3) Does semk10 beat the ALREADY-KNOWN byte-plane split (bg4) on clean fp32?
"""
import os, subprocess, numpy as np, json
W = "/root/mzip-hfbench/vfy-semk10"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
PY = "/root/mzip-hfbench/venv/bin/python"
X3 = "/root/mzip-hfbench/wz_xform3.py"

def mz(path, tag, stats=False):
    o = f"{W}/a2.{tag}.mz"
    if os.path.exists(o): os.remove(o)
    env = dict(os.environ)
    if stats: env["MZIP_STATS"] = "1"
    r = subprocess.run([MZIP,"c",path,o,"19"], capture_output=True, text=True, env=env)
    s = os.path.getsize(o) if os.path.exists(o) else 0
    return s, r.stderr

print("== (1) what does mzip pick on the split planes? ==", flush=True)
enc = "/root/mzip-hfbench/wz/fp32-enc.bin"
for k in (6, 10):
    tp = f"{W}/a2.enc.k{k}.t"
    subprocess.run([PY,X3,"f",f"semk{k}",enc,tp], check=True)
    s, err = mz(tp, f"enc.k{k}", stats=True)
    meth = [l.split("\t")[1] for l in err.splitlines() if l.startswith("MZSTATS") and len(l.split("\t"))>1]
    print(f"  enc k={k}: {s}  blocks={meth}", flush=True)
    os.remove(tp)

print("\n== (2) ablate fp16-signature elements from MiniLM, then sweep k ==", flush=True)
src = "/root/mzip-hfbench/weights/weights-fp32.bin"
w = np.fromfile(src, dtype="<u4")
m = (w & 0x1FFF) == 0
print(f"  fp16-sig elements {int(m.sum())}/{w.size} = {100*m.mean():.2f}%", flush=True)
clean = w[~m]; dirty = w[m]
cp = f"{W}/a2.minilm_clean.bin"; clean.tofile(cp)
dp = f"{W}/a2.minilm_dirty.bin"; dirty.tofile(dp)
print(f"  clean {os.path.getsize(cp)} B, dirty {os.path.getsize(dp)} B", flush=True)

for lbl, path in (("CLEAN(no fp16 elems)", cp), ("DIRTY(fp16 elems only)", dp)):
    base, _ = mz(path, f"{lbl[:5]}.base")
    print(f"  {lbl}: plain {base:,}", flush=True)
    out = {"plain": base}
    for k in (6,8,10,12,14):
        tp = f"{W}/a2.abl.k{k}.t"
        subprocess.run([PY,X3,"f",f"semk{k}",path,tp], check=True)
        s, _ = mz(tp, f"{lbl[:5]}.k{k}")
        out[f"k{k}"] = s
        print(f"      k={k:<3d} {s:>10,}  {100*(s-base)/base:+7.4f}%", flush=True)
        os.remove(tp)
    json.dump(out, open(f"{W}/abl_{lbl[:5]}.json","w"), indent=1)

print("\n== (3) semk10 vs the already-known byte-plane split (bg4) on clean fp32 ==", flush=True)
for tag, path in (("gpt2.head", f"{W}/gpt2.head.bin"),
                  ("fp32enc",   enc)):
    if not os.path.exists(path): 
        print(f"  {tag}: missing"); continue
    d = np.fromfile(path, dtype=np.uint8)
    bg4 = np.concatenate([d[0::4], d[1::4], d[2::4], d[3::4]])
    bp = f"{W}/a2.{tag}.bg4.t"; bg4.tofile(bp)
    sb, _ = mz(bp, f"{tag}.bg4")
    base, _ = mz(path, f"{tag}.plainX")
    print(f"  {tag}: plain {base:,}  bg4 {sb:,} ({100*(sb-base)/base:+.4f}%)", flush=True)
    os.remove(bp)
print("ABL2_DONE", flush=True)
