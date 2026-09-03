#!/usr/bin/env python3
"""mzip vs standard compressors on HuggingFace wikitext-2-raw.

Every tool is run at its max setting, single-threaded, and every result is
round-trip verified by sha256 against the original bytes. A tool whose
round-trip fails is reported as FAILED, never silently scored.
"""
import hashlib, json, os, shutil, subprocess, sys, time

RUN = "/root/mzip-hfbench/run"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
ASSETS = "/root/mzip-hfbench/assets"

INPUTS = [
    ("parquet", f"{ASSETS}/wikitext-2-raw-train.parquet"),
    ("text",    f"{ASSETS}/wikitext-2-raw-train.txt"),
]

# name, compress argv (stdin->stdout), decompress argv (stdin->stdout)
STDIO = [
    ("gzip -9",                       ["gzip","-9","-c"],                                   ["gzip","-dc"]),
    ("bzip2 -9",                      ["bzip2","-9","-c"],                                  ["bzip2","-dc"]),
    ("xz -9e -T1",                    ["xz","-9e","-T1","-c"],                              ["xz","-dc","-T1"]),
    ("zstd -19 -T1",                  ["zstd","-19","-T1","-c","-q"],                       ["zstd","-dc","-q"]),
    ("zstd -22 --ultra --long=27",    ["zstd","--ultra","-22","--long=27","-T1","-c","-q"], ["zstd","-dc","--long=27","-q"]),
    ("brotli -q 11",                  ["brotli","-q","11","-c"],                            ["brotli","-dc"]),
    ("brotli -q 11 -w 30",            ["brotli","-q","11","--large_window=30","-c"],        ["brotli","-dc","--large_window=30"]),
]

def sha(path):
    h = hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda: f.read(1<<20), b""):
            h.update(b)
    return h.hexdigest()

def timed(argv, stdin=None, stdout=None, cwd=None):
    """Run argv wrapped in /usr/bin/time for peak RSS. Returns (secs, rss_kb, rc)."""
    rf = f"{RUN}/.rss"
    full = ["/usr/bin/time","-f","%M","-o",rf] + argv
    fi = open(stdin,"rb") if stdin else subprocess.DEVNULL
    fo = open(stdout,"wb") if stdout else subprocess.DEVNULL
    t0 = time.perf_counter()
    p = subprocess.run(full, stdin=fi, stdout=fo, stderr=subprocess.DEVNULL, cwd=cwd)
    dt = time.perf_counter() - t0
    if stdin: fi.close()
    if stdout: fo.close()
    rss = 0
    try:
        rss = int(open(rf).read().strip().splitlines()[-1])
    except Exception:
        pass
    return dt, rss, p.returncode

def record(rows, tag, name, src, src_sha, src_sz, cfile, ct, crss, crc, dfile, dt, drc):
    ok = (crc == 0 and drc == 0 and os.path.exists(cfile)
          and os.path.exists(dfile) and sha(dfile) == src_sha)
    csz = os.path.getsize(cfile) if os.path.exists(cfile) else 0
    rows.append(dict(input=tag, tool=name, orig=src_sz, comp=csz,
                     ratio=(src_sz/csz if csz else 0),
                     bpb=(csz*8/src_sz if src_sz else 0),
                     ctime=round(ct,2), dtime=round(dt,2), crss_kb=crss,
                     roundtrip=("VERIFIED" if ok else "FAILED")))
    print(f"  {name:32s} {csz:>10,}  {src_sz/csz if csz else 0:6.4f}x  "
          f"c={ct:7.2f}s d={dt:6.2f}s  {'OK' if ok else 'FAIL'}", flush=True)

def main():
    os.makedirs(RUN, exist_ok=True)
    rows = []
    for tag, src in INPUTS:
        src_sz, src_sha = os.path.getsize(src), sha(src)
        print(f"\n=== INPUT {tag}: {os.path.basename(src)}  {src_sz:,} bytes")
        print(f"    sha256 {src_sha}", flush=True)

        for name, cc, dc in STDIO:
            cf, df = f"{RUN}/{tag}.{len(rows)}.c", f"{RUN}/{tag}.{len(rows)}.d"
            ct, crss, crc = timed(cc, stdin=src, stdout=cf)
            dt, _, drc = timed(dc, stdin=cf, stdout=df)
            record(rows, tag, name, src, src_sha, src_sz, cf, ct, crss, crc, df, dt, drc)
            os.remove(df)

        # 7-Zip: archive-based, needs real files. Run from the asset dir so the
        # stored path is a bare basename.
        for name, margs in [
            ("7z LZMA2 max", ["-m0=LZMA2","-mx=9","-md=64m","-mfb=273","-mmt1"]),
            ("7z PPMd o=16", ["-m0=PPMd","-mmem=1024m","-mo=16","-mmt1"]),
        ]:
            arch, df = f"{RUN}/a{len(rows)}.7z", f"{RUN}/{tag}.{len(rows)}.d"
            if os.path.exists(arch): os.remove(arch)
            ct, crss, crc = timed(["7z","a","-bd","-bso0","-bsp0"]+margs+[arch, os.path.basename(src)],
                                  cwd=os.path.dirname(src))
            dt, _, drc = timed(["7z","e","-so","-bd","-bso0","-bsp0",arch], stdout=df)
            record(rows, tag, name, src, src_sha, src_sz, arch, ct, crss, crc, df, dt, drc)
            os.remove(df); os.remove(arch)

        # mzip: file-in/file-out CLI
        for lvl in (1, 19, 22):
            cf, df = f"{RUN}/{tag}.mzip{lvl}", f"{RUN}/{tag}.mzip{lvl}.d"
            ct, crss, crc = timed([MZIP,"c",src,cf,str(lvl)])
            dt, _, drc = timed([MZIP,"d",cf,df])
            record(rows, tag, f"mzip L{lvl}", src, src_sha, src_sz, cf, ct, crss, crc, df, dt, drc)
            os.remove(df)

    with open(f"{RUN}/results.json","w") as f:
        json.dump(rows, f, indent=1)
    print(f"\nwrote {RUN}/results.json  ({len(rows)} rows)")

main()
