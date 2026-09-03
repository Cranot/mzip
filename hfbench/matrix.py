#!/usr/bin/env python3
"""Deep win/loss investigation: shape-stratified matrix + size-crossover ladder.

Every compressor sees byte-identical input within a corpus. Every result is
round-trip verified by sha256; a failure is reported, never silently scored.
mzip's per-block encoder telemetry is captured so each win/loss can be
attributed to a specific detector rather than guessed at.
"""
import hashlib, json, os, subprocess, time

CORP = "/root/mzip-hfbench/corpora"
RUN  = "/root/mzip-hfbench/run2"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
os.makedirs(RUN, exist_ok=True)

STDIO = [
 ("gzip -9",     ["gzip","-9","-c"],                             ["gzip","-dc"]),
 ("bzip2 -9",    ["bzip2","-9","-c"],                            ["bzip2","-dc"]),
 ("xz -9e",      ["xz","-9e","-T1","-c"],                        ["xz","-dc","-T1"]),
 ("zstd -19",    ["zstd","-19","-T1","-c","-q"],                 ["zstd","-dc","-q"]),
 ("brotli -q11", ["brotli","-q","11","-c"],                      ["brotli","-dc"]),
]

def sha(p):
    h = hashlib.sha256()
    with open(p,"rb") as f:
        for b in iter(lambda: f.read(1<<20), b""): h.update(b)
    return h.hexdigest()

def timed(argv, stdin=None, stdout=None, cwd=None, env=None):
    fi = open(stdin,"rb") if stdin else subprocess.DEVNULL
    fo = open(stdout,"wb") if stdout else subprocess.DEVNULL
    t0 = time.perf_counter()
    p = subprocess.run(argv, stdin=fi, stdout=fo, stderr=subprocess.DEVNULL,
                       cwd=cwd, env=env)
    dt = time.perf_counter()-t0
    if stdin: fi.close()
    if stdout: fo.close()
    return dt, p.returncode

def bench(tag, src, rows):
    ssz, ssha = os.path.getsize(src), sha(src)
    print(f"\n=== {tag}  {ssz:,} B", flush=True)
    local = []

    def add(name, cf, df, ct, dt, crc, drc):
        ok = (crc==0 and drc==0 and os.path.exists(cf) and os.path.exists(df)
              and sha(df)==ssha)
        csz = os.path.getsize(cf) if os.path.exists(cf) else 0
        r = dict(corpus=tag, orig=ssz, tool=name, comp=csz,
                 ratio=(ssz/csz if csz else 0), bpb=(csz*8/ssz if ssz else 0),
                 ctime=round(ct,2), dtime=round(dt,2),
                 roundtrip="VERIFIED" if ok else "FAILED")
        rows.append(r); local.append(r)
        print(f"  {name:14s} {csz:>10,} {ssz/csz if csz else 0:7.4f}x "
              f"{r['bpb']:6.4f}bpb c={ct:7.2f}s d={dt:6.2f}s "
              f"{'OK' if ok else 'FAIL'}", flush=True)

    for name, cc, dc in STDIO:
        cf, df = f"{RUN}/{tag}.c", f"{RUN}/{tag}.d"
        ct, crc = timed(cc, stdin=src, stdout=cf)
        dt, drc = timed(dc, stdin=cf, stdout=df)
        add(name, cf, df, ct, dt, crc, drc)
        os.remove(cf); os.remove(df)

    # 7z PPMd, given a budget where it saturates (256m) so it is not a strawman
    arch, df = f"{RUN}/{tag}.7z", f"{RUN}/{tag}.d"
    if os.path.exists(arch): os.remove(arch)
    ct, crc = timed(["7z","a","-bd","-bso0","-bsp0","-m0=PPMd","-mmem=256m",
                     "-mo=16","-mmt1", arch, os.path.basename(src)],
                    cwd=os.path.dirname(src))
    dt, drc = timed(["7z","e","-so","-bd","-bso0","-bsp0",arch], stdout=df)
    add("7z PPMd", arch, df, ct, dt, crc, drc)
    os.remove(arch); os.remove(df)

    # mzip + telemetry
    cf, df = f"{RUN}/{tag}.mz", f"{RUN}/{tag}.d"
    env = dict(os.environ); env["MZIP_STATS"] = "1"
    t0 = time.perf_counter()
    pr = subprocess.run([MZIP,"c",src,cf,"19"], capture_output=True, text=True, env=env)
    ct = time.perf_counter()-t0
    dt, drc = timed([MZIP,"d",cf,df])
    add("mzip L19", cf, df, ct, dt, pr.returncode, drc)
    blocks = [l.split("\t")[1] for l in pr.stderr.splitlines()
              if l.startswith("MZSTATS") and len(l.split("\t")) > 1]
    print(f"  mzip blocks   : {', '.join(blocks) if blocks else '(none)'}")
    open(f"{RUN}/{tag}.stats","w").write(pr.stderr)
    os.remove(cf); os.remove(df)

    best = min((r for r in local if r["comp"]>0), key=lambda r: r["comp"])
    mz   = next(r for r in local if r["tool"]=="mzip L19")
    zs   = next(r for r in local if r["tool"]=="zstd -19")
    vz   = 100*(zs["comp"]-mz["comp"])/zs["comp"] if zs["comp"] else 0
    verdict = "mzip WINS" if best["tool"]=="mzip L19" else f"LOSES to {best['tool']}"
    marg = 100*(mz["comp"]-best["comp"])/mz["comp"] if mz["comp"] else 0
    print(f"  -> {verdict}" + ("" if best['tool']=="mzip L19" else f" by {marg:.2f}%")
          + f" | vs zstd-19 {vz:+.2f}%")

rows = []
for f in sorted(os.listdir(CORP)):
    if f.endswith(".bin"):
        bench(f[:-4], f"{CORP}/{f}", rows)

# ---- size crossover ladder on one corpus ----
print("\n\n=== SIZE CROSSOVER LADDER (prose-wikitext prefixes) ===", flush=True)
full = open("/root/mzip-hfbench/assets/wikitext-2-raw-train.txt","rb").read()
for n in (16*1024, 64*1024, 256*1024, 1024*1024, 4*1024*1024, len(full)):
    d = full[:n]
    cut = d.rfind(b"\n")
    d = d[:cut+1] if cut > 0 else d
    p = f"{CORP}/../ladder_{n}.bin"
    open(p,"wb").write(d)
    bench(f"ladder-{len(d)}", p, rows)
    os.remove(p)

with open(f"{RUN}/matrix.json","w") as f: json.dump(rows,f,indent=1)
print(f"\nwrote {RUN}/matrix.json ({len(rows)} rows)")
print("ALLDONE")
