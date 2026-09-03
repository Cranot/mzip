#!/usr/bin/env python3
"""Does mzip degrade past its 16MB block boundary?

DEFAULT_BLOCK_SIZE = MAX_BLOCK_SIZE = 16MB, so above that mzip must split and
loses whole-file BWT context, while a streaming context model does not. This
is the regime real HF dataset shards live in, so it decides whether the
product claim survives at dataset scale.
"""
import hashlib, json, os, subprocess, time
import pyarrow.parquet as pq

A = "/root/mzip-hfbench/assets"
RUN = "/root/mzip-hfbench/run2"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
LPAQ = "/root/mzip-hfbench/build/lpaq_x"
URL = ("https://huggingface.co/datasets/Salesforce/wikitext/resolve/main/"
       "wikitext-103-raw-v1/train-00000-of-00002.parquet")
src = f"{A}/wikitext103-shard0.parquet"

if not os.path.exists(src):
    print("downloading wikitext-103 shard (157MB) ...", flush=True)
    subprocess.run(["curl","-sSL","--max-time","900","-o",src,URL], check=True)

TARGET = 34 * 1024 * 1024
big = bytearray()
pf = pq.ParquetFile(src)
for b in pf.iter_batches(batch_size=4096, columns=["text"]):
    for v in b.column(0).to_pylist():
        if v: big += v.encode("utf-8", "replace")
    if len(big) >= TARGET: break
print(f"extracted {len(big):,} B of wikitext-103 text", flush=True)

def sha(p):
    h = hashlib.sha256()
    with open(p,"rb") as f:
        for c in iter(lambda: f.read(1<<20), b""): h.update(c)
    return h.hexdigest()

rows = []
for mb in (16, 24, 32):
    n = mb * 1024 * 1024
    d = bytes(big[:n])
    cut = d.rfind(b"\n")
    d = d[:cut+1] if cut > 0 else d
    p = f"{RUN}/big{mb}.bin"
    open(p,"wb").write(d)
    N = len(d)
    ssha = sha(p)
    print(f"\n=== {mb}MB slice: {N:,} B ===", flush=True)

    def run(name, argv, out, dec=None):
        t0 = time.perf_counter()
        r = subprocess.run(argv, capture_output=True)
        t = time.perf_counter()-t0
        s = os.path.getsize(out) if os.path.exists(out) else 0
        ok = "?"
        if dec:
            subprocess.run(dec, capture_output=True)
            rt = dec[-1]
            ok = "OK" if os.path.exists(rt) and sha(rt) == ssha else "FAIL"
            if os.path.exists(rt): os.remove(rt)
        print(f"  {name:12s} {s:>11,} {N/s if s else 0:7.4f}x "
              f"{s*8/N:6.4f}bpb  {t:7.1f}s  {ok}", flush=True)
        rows.append(dict(size=N, tool=name, comp=s, ratio=(N/s if s else 0),
                         bpb=s*8/N, secs=round(t,1), roundtrip=ok))
        if os.path.exists(out): os.remove(out)
        return s

    zs = f"{RUN}/b.zst"
    subprocess.run(f"zstd -19 -T1 -q -f -o {zs} {p}", shell=True)
    s = os.path.getsize(zs); print(f"  {'zstd -19':12s} {s:>11,} {N/s:7.4f}x {s*8/N:6.4f}bpb")
    rows.append(dict(size=N, tool="zstd -19", comp=s, ratio=N/s, bpb=s*8/N, secs=0, roundtrip="?"))
    os.remove(zs)

    a7 = f"{RUN}/b.7z"
    if os.path.exists(a7): os.remove(a7)
    run("7z PPMd", ["7z","a","-bd","-bso0","-bsp0","-m0=PPMd","-mmem=256m","-mo=16",
                    "-mmt1", a7, p], a7)

    run("lpaq1", [LPAQ,"9",p,f"{RUN}/b.lp"], f"{RUN}/b.lp",
        dec=[LPAQ,"d",f"{RUN}/b.lp",f"{RUN}/b.rt"])

    run("mzip L19", [MZIP,"c",p,f"{RUN}/b.mz","19"], f"{RUN}/b.mz",
        dec=[MZIP,"d",f"{RUN}/b.mz",f"{RUN}/b.rt"])
    os.remove(p)

json.dump(rows, open(f"{RUN}/bigladder.json","w"), indent=1)
print("\nwrote bigladder.json")
print("ALLDONE3")
