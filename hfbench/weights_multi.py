#!/usr/bin/env python3
"""Widen the model-weights sample before any public extrapolation.

n=2 is far too thin to put under a founder's post. Uses HTTP range requests so
each model costs ~8MB, not gigabytes. Reports mzip vs zstd-19 (the number that
matters if HF already compresses) AND mzip vs raw (the number that matters if
weights are stored uncompressed).
"""
import json, os, struct, subprocess, sys

W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
os.makedirs(W, exist_ok=True)
SLICE = 8 * 1024 * 1024

MODELS = [
    ("Qwen2.5-0.5B",    "Qwen/Qwen2.5-0.5B"),
    ("gpt2",            "openai-community/gpt2"),
    ("bert-base",       "google-bert/bert-base-uncased"),
    ("bge-small",       "BAAI/bge-small-en-v1.5"),
]

def rng(url, a, b, out):
    r = subprocess.run(["curl","-sSL","--max-time","300","-r",f"{a}-{b}","-o",out,url])
    return r.returncode == 0 and os.path.exists(out) and os.path.getsize(out) > 0

def csize(path, tool):
    o = path + f".{tool}"
    if os.path.exists(o): os.remove(o)
    if tool == "mzip":
        subprocess.run([MZIP,"c",path,o,"19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        subprocess.run(["zstd","-19","-T1","-q","-f","-o",o,path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    s = os.path.getsize(o) if os.path.exists(o) else 0
    if os.path.exists(o): os.remove(o)
    return s

rows = []
for tag, repo in MODELS:
    url = f"https://huggingface.co/{repo}/resolve/main/model.safetensors"
    hp = f"{W}/{tag}.hdr"
    if not rng(url, 0, 200000, hp):
        print(f"{tag}: header fetch failed"); continue
    raw = open(hp,"rb").read()
    n = struct.unpack("<Q", raw[:8])[0]
    if 8 + n > len(raw):
        print(f"{tag}: header {n} larger than probe"); continue
    hdr = json.loads(raw[8:8+n])
    import collections
    dt = collections.Counter(v["dtype"] for k,v in hdr.items() if k != "__metadata__")
    dtype = dt.most_common(1)[0][0]
    start = 8 + n
    os.remove(hp)

    # take the slice from ~40% into the tensor region (avoid embeddings-only head)
    sp = f"{W}/{tag}.slice"
    if not rng(url, start + 4_000_000, start + 4_000_000 + SLICE - 1, sp):
        print(f"{tag}: slice fetch failed"); continue
    N = os.path.getsize(sp)
    m = csize(sp, "mzip")
    z = csize(sp, "zstd")
    os.remove(sp)
    rows.append((tag, dtype, N, m, z))
    print(f"  {tag:14s} {dtype:5s} N={N:>9,}  mzip {m:>9,} ({N/m:5.3f}x, {100*(1-m/N):5.1f}% saved)  "
          f"zstd {z:>9,} ({N/z:5.3f}x)  mzip vs zstd {100*(z-m)/z:+5.2f}%", flush=True)

# fold in the two already measured this session
prior = [("MiniLM(prior)","F32",8388608,6155434,6931352),
         ("SmolLM2(prior)","BF16",8388608,5571614,6576407)]
allr = rows + prior
print("\n=== SUMMARY (all model-weight slices) ===")
tn = sum(r[2] for r in allr); tm = sum(r[3] for r in allr); tz = sum(r[4] for r in allr)
for tag,dtype,N,m,z in allr:
    print(f"  {tag:14s} {dtype:5s} mzip {100*(1-m/N):5.1f}% saved vs raw | {100*(z-m)/z:+5.2f}% vs zstd")
print(f"\n  TOTAL  raw {tn:,}  mzip {tm:,}  zstd {tz:,}")
print(f"  mzip saves {100*(1-tm/tn):.2f}% vs RAW ; {100*(tz-tm)/tz:.2f}% vs ZSTD-19")
json.dump([dict(tag=t,dtype=d,n=N,mzip=m,zstd=z) for t,d,N,m,z in allr],
          open(f"{W}/weights_multi.json","w"), indent=1)
print("ALLDONE_WM")
