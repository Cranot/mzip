#!/usr/bin/env python3
"""bf16 weights -- what modern LLMs actually ship, unlike MiniLM's fp32.

bf16 = 1 sign + 8 exponent + 7 mantissa in TWO bytes. So byte1 (LE high) is
sign+exponent (structured) and byte0 is pure mantissa (near random). Half the
file is structured vs a quarter for fp32, so the plane split should matter more.
"""
import collections, hashlib, json, math, os, struct, subprocess, time

A = "/root/mzip-hfbench/assets"
W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
LPAQ = "/root/mzip-hfbench/build/lpaq_x"
URL = "https://huggingface.co/HuggingFaceTB/SmolLM2-135M/resolve/main/model.safetensors"
src = f"{A}/smollm2.safetensors"

if not os.path.exists(src):
    print("downloading SmolLM2-135M (269MB) ...", flush=True)
    subprocess.run(["curl","-sSL","--max-time","900","-o",src,URL], check=True)

f = open(src,"rb"); n = struct.unpack("<Q", f.read(8))[0]
hdr = json.loads(f.read(n))
dt = collections.Counter(v["dtype"] for k,v in hdr.items() if k != "__metadata__")
print(f"dtypes: {dict(dt)}   data starts at {8+n}")

N = 8*1024*1024
raw = open(src,"rb").read()[8+n : 8+n+N]
p = f"{W}/weights-bf16.bin"
open(p,"wb").write(raw)
print(f"corpus {len(raw):,} B  sha {hashlib.sha256(raw).hexdigest()[:12]}")
for k in range(2):
    c = collections.Counter(raw[k::2]); tot = sum(c.values())
    H = -sum(v/tot*math.log2(v/tot) for v in c.values())
    print(f"  plane {k}: {len(c):>3d} distinct, entropy {H:.4f} bits/byte")

def csize(path, tool):
    o = path + ".o"
    if os.path.exists(o): os.remove(o)
    t0 = time.perf_counter()
    if tool=="mzip":   subprocess.run([MZIP,"c",path,o,"19"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    elif tool=="lpaq": subprocess.run([LPAQ,"9",path,o],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    elif tool=="zstd": subprocess.run(["zstd","-19","-T1","-q","-f","-o",o,path],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    elif tool=="xz":   subprocess.run(f"xz -9e -T1 -c {path} > {o}",shell=True)
    elif tool=="brotli": subprocess.run(f"brotli -q 11 -c {path} > {o}",shell=True)
    elif tool=="gzip": subprocess.run(f"gzip -9 -c {path} > {o}",shell=True)
    dt = time.perf_counter()-t0
    s = os.path.getsize(o) if os.path.exists(o) else 0
    if os.path.exists(o): os.remove(o)
    return s, dt

TOOLS=["gzip","zstd","xz","brotli","lpaq","mzip"]
res={}
print(f"\n=== NATIVE bf16 interleaved ({N:,} B) ===")
for t in TOOLS:
    s,d = csize(p,t); res[("native",t)]=s
    print(f"  {t:8s} {s:>10,}  {N/s if s else 0:6.4f}x  {d:6.1f}s", flush=True)
env=dict(os.environ); env["MZIP_STATS"]="1"
pr=subprocess.run([MZIP,"c",p,f"{W}/y.mz","19"],capture_output=True,text=True,env=env)
print("  mzip blocks:", ", ".join(l.split("\t")[1] for l in pr.stderr.splitlines()
      if l.startswith("MZSTATS") and len(l.split("\t"))>1))
if os.path.exists(f"{W}/y.mz"): os.remove(f"{W}/y.mz")

print(f"\n=== PLANE-SPLIT (stride 2, 2 planes of {N//2:,} B) ===")
pls=[]
for k in range(2):
    pk=f"{W}/bp{k}.bin"; open(pk,"wb").write(raw[k::2]); pls.append(pk)
for t in TOOLS:
    tot=0; per=[]
    for pk in pls:
        s,_=csize(pk,t); tot+=s; per.append(s)
    res[("split",t)]=tot
    nat=res[("native",t)]
    print(f"  {t:8s} {tot:>10,}  {N/tot if tot else 0:6.4f}x  "
          f"split {100*(nat-tot)/nat:+.2f}% vs native  planes={[f'{x:,}' for x in per]}", flush=True)
for pk in pls: os.remove(pk)
json.dump({f"{a}/{b}":v for (a,b),v in res.items()}, open(f"{W}/bf16.json","w"), indent=1)
print("\nALLDONE_BF")
