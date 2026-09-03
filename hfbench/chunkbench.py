#!/usr/bin/env python3
"""mzip vs HF's scheme AT HF'S ACTUAL GRANULARITY.

xet-core compresses each content-defined chunk INDEPENDENTLY
(TARGET_CHUNK_SIZE = 64 KiB, min 8 KiB, max 128 KiB; XorbChunkHeader carries a
per-chunk compression_scheme). Every prior mzip-vs-bg4-lz4 number was measured
on 8 MiB whole slices, which HF never compresses. This re-measures at chunk
granularity. Fixed-size chunks approximate CDC (whose mean is 64 KiB); the
whole-file column is the old, non-drop-in comparison for reference.

Sizes only. All mzip chunks round-trip verified. Uses level 1 (proven
byte-identical to default and faster).
"""
import collections, json, os, struct, subprocess, sys, concurrent.futures as cf
import numpy as np, lz4.frame

W = "/root/mzip-hfbench/weights"
C = "/root/mzip-hfbench/corpora"
OUT = "/root/mzip-hfbench/chunk"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
os.makedirs(OUT, exist_ok=True)

# ---- restore the 6-model weights sample (range requests, ~8MB each) ----
FETCH = [
    ("MiniLM-L6-v2", "sentence-transformers/all-MiniLM-L6-v2"),
    ("gpt2",         "openai-community/gpt2"),
    ("bert-base",    "google-bert/bert-base-uncased"),
    ("bge-small",    "BAAI/bge-small-en-v1.5"),
]
def rng(url, a, b, out):
    r = subprocess.run(["curl","-sSL","--max-time","300","-r",f"{a}-{b}","-o",out,url])
    return r.returncode == 0 and os.path.exists(out) and os.path.getsize(out) > 0

for tag, repo in FETCH:
    sp = f"{W}/{tag}.slice"
    if os.path.exists(sp) and os.path.getsize(sp) == 8*1024*1024: continue
    url = f"https://huggingface.co/{repo}/resolve/main/model.safetensors"
    hp = f"{W}/{tag}.h"
    if not rng(url, 0, 200000, hp): print(f"{tag}: header fetch failed", flush=True); continue
    hb = open(hp,"rb").read(); os.remove(hp)
    n = struct.unpack("<Q", hb[:8])[0]
    if 8+n > len(hb): print(f"{tag}: header too large", flush=True); continue
    if not rng(url, 8+n+4_000_000, 8+n+4_000_000+8*1024*1024-1, sp):
        print(f"{tag}: slice fetch failed", flush=True)
    else:
        print(f"fetched {tag} {os.path.getsize(sp):,} B", flush=True)

INPUTS = [
    ("MiniLM-L6-v2 F32", f"{W}/MiniLM-L6-v2.slice"),
    ("gpt2 F32",         f"{W}/gpt2.slice"),
    ("bert-base F32",    f"{W}/bert-base.slice"),
    ("bge-small F32",    f"{W}/bge-small.slice"),
    ("weights-fp32 F32", f"{W}/weights-fp32.bin"),
    ("weights-bf16",     f"{W}/weights-bf16.bin"),
    ("Qwen2.5-0.5B BF16",f"{W}/Qwen2.5-0.5B.slice"),
    ("prose-wikitext",   f"{C}/prose-wikitext.bin"),
    ("code-python",      f"{C}/code-python.bin"),
    ("tabular-numeric",  f"{C}/tabular-numeric.bin"),
]
INPUTS = [(t,p) for t,p in INPUTS if os.path.exists(p)]

# ---- Xet's BG4Predictor (bg4_prediction.rs): per-position popcount histograms,
#      recommend bg4 if max KL divergence between positions > 0.02 ----
POP = np.array([bin(i).count("1") for i in range(256)], dtype=np.uint8)
def bg4_recommended(data):
    a = np.frombuffer(data, dtype=np.uint8)
    n = (len(a)//4)*4
    if n < 64: return False
    pc = POP[a[:n].reshape(-1,4)]
    hs = [np.bincount(pc[:,k], minlength=9).astype(np.float64) + 1.0 for k in range(4)]  # "a state is never zero"
    ps = [h/h.sum() for h in hs]
    mx = 0.0
    for i in range(4):
        for j in range(4):
            if i != j:
                mx = max(mx, float(np.sum(ps[i]*np.log(ps[i]/ps[j]))))
    return mx > 0.02

def bg4_split(data):
    a = np.frombuffer(data, dtype=np.uint8); n = (len(a)//4)*4
    m = a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4)) + a[n:].tobytes()

def lz4_sz(b):     return len(lz4.frame.compress(b, compression_level=0))
def bg4lz4_sz(b):  return len(lz4.frame.compress(bg4_split(b), compression_level=0))
def auto_sz(b):    return bg4lz4_sz(b) if bg4_recommended(b) else lz4_sz(b)

def mzip_sz(b, tmpbase):
    ip, op, rp = tmpbase+".in", tmpbase+".mz", tmpbase+".rt"
    open(ip,"wb").write(b)
    subprocess.run([MZIP,"c",ip,op,"1"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    s = os.path.getsize(op) if os.path.exists(op) else -1
    subprocess.run([MZIP,"d",op,rp], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ok = os.path.exists(rp) and open(rp,"rb").read() == b
    for f in (ip,op,rp):
        if os.path.exists(f): os.remove(f)
    return s, ok

def job(tag, path, csz):
    raw = open(path,"rb").read()
    N = len(raw)
    chunks = [raw] if csz == 0 else [raw[i:i+csz] for i in range(0, N, csz)]
    lz = bg = au = mz = 0; bad = 0; auto_bg4 = 0
    base = f"{OUT}/{tag.replace(' ','_')}.{csz}"
    for i, ch in enumerate(chunks):
        lz += lz4_sz(ch); bg += bg4lz4_sz(ch)
        rec = bg4_recommended(ch); auto_bg4 += int(rec)
        au += bg4lz4_sz(ch) if rec else lz4_sz(ch)
        s, ok = mzip_sz(ch, f"{base}.{i}")
        if s < 0 or not ok: bad += 1
        mz += max(s, 0)
    return dict(tag=tag, n=N, chunk=csz, nchunks=len(chunks), lz4=lz, bg4lz4=bg, auto=au,
                auto_bg4_frac=auto_bg4/len(chunks), mzip=mz, mzip_bad=bad)

CHUNKS = [32*1024, 64*1024, 128*1024, 1024*1024, 0]
jobs = [(t,p,c) for t,p in INPUTS for c in CHUNKS]
print(f"{len(INPUTS)} inputs x {len(CHUNKS)} chunk sizes = {len(jobs)} jobs", flush=True)

rows = []
with cf.ThreadPoolExecutor(max_workers=4) as ex:
    futs = {ex.submit(job, t, p, c): (t,c) for t,p,c in jobs}
    for f in cf.as_completed(futs):
        r = f.result(); rows.append(r)
        cs = "whole" if r["chunk"]==0 else f"{r['chunk']//1024}K"
        print(f"  {r['tag']:20s} {cs:>6s} n={r['nchunks']:>4}  lz4 {r['lz4']:>9,}  bg4 {r['bg4lz4']:>9,}  "
              f"auto {r['auto']:>9,} (bg4 on {100*r['auto_bg4_frac']:.0f}%)  mzip {r['mzip']:>9,}  "
              f"mzip vs bg4 {100*(r['bg4lz4']-r['mzip'])/r['bg4lz4']:+6.2f}%  "
              f"{'' if r['mzip_bad']==0 else 'RT_FAIL x'+str(r['mzip_bad'])}", flush=True)

json.dump(rows, open(f"{OUT}/chunkbench.json","w"), indent=1)

# ---- summary: weights aggregate per chunk size ----
print("\n=== WEIGHTS AGGREGATE (7 slices) — mzip vs HF per-chunk bg4-lz4, by chunk size ===")
wt = [r for r in rows if "F32" in r["tag"] or "bf16" in r["tag"].lower() or "BF16" in r["tag"]]
for c in CHUNKS:
    rs = [r for r in wt if r["chunk"]==c]
    if not rs: continue
    n=sum(r["n"] for r in rs); bg=sum(r["bg4lz4"] for r in rs); au=sum(r["auto"] for r in rs); mz=sum(r["mzip"] for r in rs)
    cs = "whole" if c==0 else f"{c//1024}K"
    print(f"  chunk {cs:>6s}: raw {n:,}  bg4-lz4 {bg:,} ({n/bg:.4f}x)  auto {au:,} ({n/au:.4f}x)  "
          f"mzip {mz:,} ({n/mz:.4f}x)  -> mzip vs bg4-lz4 {100*(bg-mz)/bg:+.2f}%")
print("\n=== TEXT (per input) mzip vs auto, by chunk size ===")
for r in sorted([r for r in rows if r not in wt], key=lambda r:(r["tag"],r["chunk"])):
    cs = "whole" if r["chunk"]==0 else f"{r['chunk']//1024}K"
    print(f"  {r['tag']:16s} {cs:>6s}  auto {r['auto']:>9,}  mzip {r['mzip']:>9,}  mzip vs auto {100*(r['auto']-r['mzip'])/r['auto']:+.2f}%")
print("\nALLDONE_CHUNK")
