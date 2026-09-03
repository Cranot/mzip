#!/usr/bin/env python3
"""mzip vs what HuggingFace ACTUALLY runs: ratio + compress time + decompress time.

Baseline is xet-core's CompressionScheme::ByteGrouping4LZ4 -- 4-byte grouping
then LZ4 -- which is what the Hub stores. lz4_flex's FrameEncoder default is
FAST mode, so `lz4` at default level is the faithful simulation; -9 is shown
too as the strongest LZ4 the scheme could use.

Every arm is round-trip verified. Timings are single-threaded wall clock.
"""
import collections, json, os, struct, subprocess, time
import numpy as np

W = "/root/mzip-hfbench/weights"
MZIP = "/root/mzip-hfbench/build/mzip_fixed"
SLICE = 8 * 1024 * 1024
os.makedirs(W, exist_ok=True)

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

def sh(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def bg4_split(raw):
    a = np.frombuffer(raw, dtype=np.uint8)
    n = (len(a) // 4) * 4
    m = a[:n].reshape(-1, 4)
    planes = [m[:, k].tobytes() for k in range(4)]
    return planes, a[n:].tobytes()

def bg4_regroup(planes, tail, total):
    m = np.empty((len(planes[0]), 4), dtype=np.uint8)
    for k in range(4):
        m[:, k] = np.frombuffer(planes[k], dtype=np.uint8)
    return m.tobytes() + tail

def time_lz4_arm(raw, base, level):
    """Xet: bg4 split -> lz4 each plane. Returns (bytes, ctime, dtime, ok)."""
    t0 = time.perf_counter()
    planes, tail = bg4_split(raw)
    tot = 0
    for k, p in enumerate(planes):
        fp = f"{base}.p{k}"
        open(fp, "wb").write(p)
        sh(f"lz4 {level} -q -f {fp} {fp}.lz4")
        tot += os.path.getsize(f"{fp}.lz4")
    ct = time.perf_counter() - t0

    t0 = time.perf_counter()
    back = []
    for k in range(4):
        fp = f"{base}.p{k}"
        sh(f"lz4 -d -q -f {fp}.lz4 {fp}.out")
        back.append(open(f"{fp}.out", "rb").read())
    got = bg4_regroup(back, tail, len(raw))
    dt = time.perf_counter() - t0

    for k in range(4):
        for s in (f"{base}.p{k}", f"{base}.p{k}.lz4", f"{base}.p{k}.out"):
            if os.path.exists(s): os.remove(s)
    return tot, ct, dt, got == raw

def time_file_arm(path, raw, kind):
    o, r = path + ".c", path + ".r"
    for f in (o, r):
        if os.path.exists(f): os.remove(f)
    t0 = time.perf_counter()
    if kind == "lz4":       sh(f"lz4 -q -f {path} {o}")
    elif kind == "zstd":    sh(f"zstd -19 -T1 -q -f -o {o} {path}")
    elif kind == "mzip":    subprocess.run([MZIP,"c",path,o,"19"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ct = time.perf_counter() - t0
    s = os.path.getsize(o) if os.path.exists(o) else 0
    t0 = time.perf_counter()
    if kind == "lz4":       sh(f"lz4 -d -q -f {o} {r}")
    elif kind == "zstd":    sh(f"zstd -d -q -f -o {r} {o}")
    elif kind == "mzip":    subprocess.run([MZIP,"d",o,r], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dt = time.perf_counter() - t0
    ok = os.path.exists(r) and open(r,"rb").read() == raw
    for f in (o, r):
        if os.path.exists(f): os.remove(f)
    return s, ct, dt, ok

rows = []
for tag, repo in MODELS:
    url = f"https://huggingface.co/{repo}/resolve/main/model.safetensors"
    hp = f"{W}/{tag}.h"
    if not rng(url, 0, 200000, hp): print(f"{tag}: hdr fail", flush=True); continue
    hb = open(hp,"rb").read(); os.remove(hp)
    n = struct.unpack("<Q", hb[:8])[0]
    if 8+n > len(hb): print(f"{tag}: hdr big", flush=True); continue
    hdr = json.loads(hb[8:8+n])
    dtype = collections.Counter(v["dtype"] for k,v in hdr.items() if k!="__metadata__").most_common(1)[0][0]

    sp = f"{W}/{tag}.s"
    if not rng(url, 8+n+4_000_000, 8+n+4_000_000+SLICE-1, sp): print(f"{tag}: slice fail", flush=True); continue
    raw = open(sp,"rb").read(); N = len(raw)

    lz_s, lz_c, lz_d, lz_ok = time_file_arm(sp, raw, "lz4")
    bg_s, bg_c, bg_d, bg_ok = time_lz4_arm(raw, sp, "")      # Xet default (fast)
    b9_s, b9_c, b9_d, b9_ok = time_lz4_arm(raw, sp, "-9")    # strongest LZ4
    zs_s, zs_c, zs_d, zs_ok = time_file_arm(sp, raw, "zstd")
    mz_s, mz_c, mz_d, mz_ok = time_file_arm(sp, raw, "mzip")
    os.remove(sp)

    rows.append(dict(tag=tag, dtype=dtype, n=N,
                     lz4=lz_s, lz4_c=lz_c, lz4_d=lz_d,
                     bg4=bg_s, bg4_c=bg_c, bg4_d=bg_d,
                     bg4_9=b9_s, bg4_9_c=b9_c, bg4_9_d=b9_d,
                     zstd=zs_s, zstd_c=zs_c, zstd_d=zs_d,
                     mzip=mz_s, mzip_c=mz_c, mzip_d=mz_d,
                     ok=all([lz_ok,bg_ok,b9_ok,zs_ok,mz_ok])))
    print(f"\n{tag} ({dtype}, {N:,} B)  roundtrips={'ALL OK' if rows[-1]['ok'] else 'FAIL'}", flush=True)
    print(f"  {'scheme':16s} {'bytes':>10s} {'ratio':>7s} {'comp s':>8s} {'decomp s':>9s}", flush=True)
    for nm,s,c,d in (("lz4 (plain)",lz_s,lz_c,lz_d), ("bg4-lz4  <-HF",bg_s,bg_c,bg_d),
                     ("bg4-lz4 -9",b9_s,b9_c,b9_d), ("zstd-19 (ref)",zs_s,zs_c,zs_d),
                     ("mzip",mz_s,mz_c,mz_d)):
        print(f"  {nm:16s} {s:>10,} {N/s:>6.3f}x {c:>8.2f} {d:>9.2f}", flush=True)
    print(f"  mzip vs bg4-lz4: {100*(bg_s-mz_s)/bg_s:+.2f}% size, "
          f"{mz_c/bg_c:.0f}x slower to compress, {mz_d/bg_d:.0f}x slower to decompress", flush=True)

if rows:
    k = lambda f: sum(r[f] for r in rows)
    tn = k("n")
    print(f"\n=== TOTALS, {len(rows)} model slices, {tn:,} B raw ===")
    print(f"  {'scheme':16s} {'bytes':>11s} {'ratio':>7s} {'comp s':>8s} {'decomp s':>9s} {'MB/s c':>8s} {'MB/s d':>8s}")
    for nm,fs,fc,fd in (("lz4 (plain)","lz4","lz4_c","lz4_d"), ("bg4-lz4  <-HF","bg4","bg4_c","bg4_d"),
                        ("bg4-lz4 -9","bg4_9","bg4_9_c","bg4_9_d"), ("zstd-19 (ref)","zstd","zstd_c","zstd_d"),
                        ("mzip","mzip","mzip_c","mzip_d")):
        s,c,d = k(fs),k(fc),k(fd)
        print(f"  {nm:16s} {s:>11,} {tn/s:>6.3f}x {c:>8.1f} {d:>9.1f} "
              f"{tn/1048576/c:>8.1f} {tn/1048576/d:>8.1f}")
    print(f"\n  mzip vs Xet bg4-lz4 : {100*(k('bg4')-k('mzip'))/k('bg4'):+.2f}% smaller")
    print(f"  compress  : mzip {k('mzip_c')/k('bg4_c'):.0f}x slower than Xet")
    print(f"  decompress: mzip {k('mzip_d')/k('bg4_d'):.0f}x slower than Xet")
    json.dump(rows, open(f"{W}/xet_full.json","w"), indent=1)
print("\nALLDONE_XF")
