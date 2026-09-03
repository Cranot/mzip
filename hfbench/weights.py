#!/usr/bin/env python3
"""Can mzip win on safetensors model weights -- the Hub's dominant data by bytes?

Two arms on identical bytes:
  NATIVE      compress the fp32 tensor bytes as-is
  PLANE-SPLIT deinterleave into 4 byte planes (stride 4), compress each, sum
fp32 LE: byte3 = sign+7 exponent bits (highly skewed), byte2 = 1 exp + 7 mantissa,
bytes 1/0 = low mantissa (near random). Interleaving mixes structured with random.
"""
import hashlib, json, os, subprocess, time

A = "/root/mzip-hfbench/assets"
W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
LPAQ = "/root/mzip-hfbench/build/lpaq_x"
os.makedirs(W, exist_ok=True)

SRC, OFF, N = f"{A}/minilm.safetensors", 11416, 8 * 1024 * 1024
raw = open(SRC, "rb").read()[OFF:OFF + N]
p = f"{W}/weights-fp32.bin"
open(p, "wb").write(raw)
print(f"corpus {len(raw):,} B  sha {hashlib.sha256(raw).hexdigest()[:12]}")

# entropy sanity: per-plane byte distribution
import collections
for k in range(4):
    c = collections.Counter(raw[k::4])
    import math
    tot = sum(c.values())
    H = -sum(v/tot * math.log2(v/tot) for v in c.values())
    print(f"  plane {k}: {len(c):>3d} distinct, entropy {H:.4f} bits/byte")

def csize(path, tool):
    o = path + ".o"
    if os.path.exists(o): os.remove(o)
    t0 = time.perf_counter()
    if tool == "mzip":
        subprocess.run([MZIP,"c",path,o,"19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elif tool == "lpaq":
        subprocess.run([LPAQ,"9",path,o], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elif tool == "zstd":
        subprocess.run(["zstd","-19","-T1","-q","-f","-o",o,path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elif tool == "xz":
        subprocess.run(f"xz -9e -T1 -c {path} > {o}", shell=True)
    elif tool == "brotli":
        subprocess.run(f"brotli -q 11 -c {path} > {o}", shell=True)
    elif tool == "gzip":
        subprocess.run(f"gzip -9 -c {path} > {o}", shell=True)
    elif tool == "ppmd":
        if os.path.exists(o): os.remove(o)
        subprocess.run(["7z","a","-bd","-bso0","-bsp0","-m0=PPMd","-mmem=256m","-mo=16",
                        "-mmt1", o, os.path.basename(path)], cwd=os.path.dirname(path),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dt = time.perf_counter()-t0
    s = os.path.getsize(o) if os.path.exists(o) else 0
    if os.path.exists(o): os.remove(o)
    return s, dt

TOOLS = ["gzip","zstd","xz","brotli","ppmd","lpaq","mzip"]
res = {}
print(f"\n=== NATIVE (interleaved fp32, {N:,} B) ===")
for t in TOOLS:
    s, dt = csize(p, t)
    res[("native",t)] = s
    print(f"  {t:8s} {s:>10,}  {N/s if s else 0:6.4f}x  {dt:6.1f}s")

# what did mzip pick?
env = dict(os.environ); env["MZIP_STATS"]="1"
pr = subprocess.run([MZIP,"c",p,f"{W}/x.mz","19"], capture_output=True, text=True, env=env)
print("  mzip blocks:", ", ".join(l.split("\t")[1] for l in pr.stderr.splitlines()
      if l.startswith("MZSTATS") and len(l.split("\t"))>1))
if os.path.exists(f"{W}/x.mz"): os.remove(f"{W}/x.mz")

print(f"\n=== PLANE-SPLIT (stride 4, 4 planes of {N//4:,} B) ===")
planes = []
for k in range(4):
    pk = f"{W}/plane{k}.bin"
    open(pk,"wb").write(raw[k::4])
    planes.append(pk)
for t in TOOLS:
    tot = 0
    per = []
    for pk in planes:
        s, _ = csize(pk, t)
        tot += s; per.append(s)
    res[("split",t)] = tot
    nat = res[("native",t)]
    d = 100*(nat-tot)/nat if nat else 0
    print(f"  {t:8s} {tot:>10,}  {N/tot if tot else 0:6.4f}x  "
          f"split {d:+.2f}% vs native   planes={[f'{x:,}' for x in per]}")
for pk in planes: os.remove(pk)

json.dump({f"{a}/{b}":v for (a,b),v in res.items()}, open(f"{W}/weights.json","w"), indent=1)
print("\nALLDONE_W")
