#!/usr/bin/env python3
"""mzip vs HF's ACTUAL storage scheme, not a strawman.

xet-core's CompressionScheme is {None, LZ4, ByteGrouping4LZ4, Auto}, so the Hub
already applies 4-byte grouping + LZ4 to stored chunks. Any public savings claim
must be measured against THAT, not against raw bytes (which would overclaim by
~27-34%) and not against zstd-19 (which HF does not use here).

Reports, per model slice:
  raw            -- the uncompressed bytes
  lz4            -- Xet CompressionScheme::LZ4
  bg4-lz4        -- Xet CompressionScheme::ByteGrouping4LZ4  <-- the real baseline
  zstd-19        -- reference only
  mzip           -- ours
"""
import collections, json, os, struct, subprocess

W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
os.makedirs(W, exist_ok=True)
SLICE = 8 * 1024 * 1024

MODELS = [
    ("MiniLM-L6-v2",  "sentence-transformers/all-MiniLM-L6-v2"),
    ("SmolLM2-135M",  "HuggingFaceTB/SmolLM2-135M"),
    ("Qwen2.5-0.5B",  "Qwen/Qwen2.5-0.5B"),
    ("gpt2",          "openai-community/gpt2"),
    ("bert-base",     "google-bert/bert-base-uncased"),
    ("bge-small",     "BAAI/bge-small-en-v1.5"),
]

def rng(url, a, b, out):
    r = subprocess.run(["curl","-sSL","--max-time","300","-r",f"{a}-{b}","-o",out,url])
    return r.returncode == 0 and os.path.exists(out) and os.path.getsize(out) > 0

def run_sz(cmd, out):
    if os.path.exists(out): os.remove(out)
    subprocess.run(cmd, shell=isinstance(cmd,str), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    s = os.path.getsize(out) if os.path.exists(out) else 0
    if os.path.exists(out): os.remove(out)
    return s

def lz4_size(path):
    return run_sz(f"lz4 -9 -q -f {path} {path}.lz4", f"{path}.lz4")

def bg4_lz4_size(raw, tmp):
    """Xet ByteGrouping4LZ4: split into 4 byte groups, LZ4 each, sum."""
    tot = 0
    for k in range(4):
        p = f"{tmp}.bg{k}"
        open(p,"wb").write(raw[k::4])
        tot += lz4_size(p)
        os.remove(p)
    return tot

rows = []
for tag, repo in MODELS:
    url = f"https://huggingface.co/{repo}/resolve/main/model.safetensors"
    hp = f"{W}/{tag}.hdr"
    if not rng(url, 0, 200000, hp):
        print(f"{tag}: header fetch failed", flush=True); continue
    hb = open(hp,"rb").read(); os.remove(hp)
    n = struct.unpack("<Q", hb[:8])[0]
    if 8 + n > len(hb):
        print(f"{tag}: header too large ({n})", flush=True); continue
    hdr = json.loads(hb[8:8+n])
    dt = collections.Counter(v["dtype"] for k,v in hdr.items() if k != "__metadata__")
    dtype = dt.most_common(1)[0][0]
    start = 8 + n

    sp = f"{W}/{tag}.slice"
    if not rng(url, start + 4_000_000, start + 4_000_000 + SLICE - 1, sp):
        print(f"{tag}: slice fetch failed", flush=True); continue
    raw = open(sp,"rb").read()
    N = len(raw)

    lz  = lz4_size(sp)
    bg  = bg4_lz4_size(raw, sp)
    zs  = run_sz(["zstd","-19","-T1","-q","-f","-o",f"{sp}.zst",sp], f"{sp}.zst")
    mz  = run_sz([MZIP,"c",sp,f"{sp}.mz","19"], f"{sp}.mz")
    os.remove(sp)

    rows.append(dict(tag=tag, dtype=dtype, n=N, lz4=lz, bg4lz4=bg, zstd=zs, mzip=mz))
    print(f"  {tag:14s} {dtype:5s} N={N:,}", flush=True)
    print(f"      lz4      {lz:>9,} ({N/lz:5.3f}x)   bg4-lz4 {bg:>9,} ({N/bg:5.3f}x)  <- Xet", flush=True)
    print(f"      zstd-19  {zs:>9,} ({N/zs:5.3f}x)   mzip    {mz:>9,} ({N/mz:5.3f}x)", flush=True)
    print(f"      mzip vs bg4-lz4: {100*(bg-mz)/bg:+.2f}%   mzip vs raw: {100*(1-mz/N):+.2f}%", flush=True)

if rows:
    tn = sum(r["n"] for r in rows); tb = sum(r["bg4lz4"] for r in rows)
    tm = sum(r["mzip"] for r in rows); tl = sum(r["lz4"] for r in rows)
    tz = sum(r["zstd"] for r in rows)
    print(f"\n=== TOTALS over {len(rows)} model slices ({tn:,} B raw) ===")
    print(f"  lz4      {tl:>10,}  ({tn/tl:.4f}x)")
    print(f"  bg4-lz4  {tb:>10,}  ({tn/tb:.4f}x)   <-- what HF actually stores")
    print(f"  zstd-19  {tz:>10,}  ({tn/tz:.4f}x)")
    print(f"  mzip     {tm:>10,}  ({tn/tm:.4f}x)")
    print(f"\n  mzip vs Xet bg4-lz4 : {100*(tb-tm)/tb:+.2f}%  (THE honest headline)")
    print(f"  mzip vs raw         : {100*(1-tm/tn):+.2f}%  (only if stored uncompressed)")
    json.dump(rows, open(f"{W}/xet_baseline.json","w"), indent=1)
print("ALLDONE_XET")
