#!/usr/bin/env python3
"""Ratio-regression oracle over real HF files -- the check the fuzzers cannot do.

The fuzzer oracle is "crashed? / round-tripped?". A lossy encoder whose damage
is absorbed by the uRAW fallback round-trips PERFECTLY and is therefore
invisible to it. This asks a different question: did mzip ship something
absurd?

  CRITICAL : mzip output >= input size (uRAW shipped -- an encoder failed)
  BAD      : mzip loses to the best standard codec by more than 2x
  LOSS     : mzip loses to the best standard codec at all

Hypothesis under test: NUM_EXTRACT fires on "files with many embedded numbers",
and tokenizer.json is huge JSON of mostly numeric token ids -> prime suspect.
"""
import json, os, subprocess, sys

W = "/root/mzip-hfbench/sweep"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
os.makedirs(W, exist_ok=True)

FILES = ["tokenizer.json", "vocab.json", "merges.txt", "vocab.txt",
         "config.json", "tokenizer_config.json", "generation_config.json",
         "special_tokens_map.json", "preprocessor_config.json", "README.md"]

r = subprocess.run(["curl","-sS","--max-time","40",
    "https://huggingface.co/api/models?sort=downloads&direction=-1&limit=90"],
    capture_output=True, text=True)
repos = [m["id"] for m in json.loads(r.stdout)]
print(f"sweeping {len(repos)} repos x {len(FILES)} filenames", flush=True)

got = []
for repo in repos:
    for f in FILES:
        p = f"{W}/{repo.replace('/','__')}__{f}"
        if not os.path.exists(p):
            subprocess.run(["curl","-sSL","--max-time","40","-o",p,
                            f"https://huggingface.co/{repo}/resolve/main/{f}"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if os.path.exists(p):
            n = os.path.getsize(p)
            # reject 404 pages / LFS pointers / empties
            head = open(p,"rb").read(200)
            bad = n < 40 or b"Entry not found" in head or b"<!DOCTYPE" in head \
                  or head.startswith(b"version https://git-lfs")
            if bad: os.remove(p)
            else: got.append(p)

got = sorted(set(got))
print(f"collected {len(got)} files, total {sum(os.path.getsize(g) for g in got):,} B", flush=True)

def sz(cmd, out):
    if os.path.exists(out): os.remove(out)
    subprocess.run(cmd, shell=isinstance(cmd,str), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    s = os.path.getsize(out) if os.path.exists(out) else 0
    if os.path.exists(out): os.remove(out)
    return s

o = f"{W}/o.tmp"
rows, crit, bad = [], [], []
for i, t in enumerate(got):
    n = os.path.getsize(t)
    if n > 40_000_000: continue
    m = sz([MZIP,"c",t,o,"19"], o)
    b = sz(f"brotli -q 11 -c {t} > {o}", o)
    x = sz(f"xz -9e -T1 -c {t} > {o}", o)
    z = sz(f"zstd -19 -T1 -q -f -o {o} {t}", o)
    best = min(v for v in (b,x,z) if v > 0) if any((b,x,z)) else 0
    rows.append(dict(f=os.path.basename(t), n=n, mzip=m, brotli=b, xz=x, zstd=z, best=best))
    if m >= n:
        crit.append(rows[-1]); print(f"  CRITICAL uRAW: {os.path.basename(t)[:60]} "
                                    f"{n:,} -> {m:,} (best std {best:,})", flush=True)
    elif best and m > 2*best:
        bad.append(rows[-1]); print(f"  BAD >2x: {os.path.basename(t)[:60]} "
                                   f"{n:,} mzip {m:,} vs best {best:,}", flush=True)
    if (i+1) % 100 == 0: print(f"  ... {i+1}/{len(got)}", flush=True)

losses = [r for r in rows if r["best"] and r["mzip"] > r["best"]]
tot_m = sum(r["mzip"] for r in rows); tot_b = sum(r["best"] for r in rows)
print(f"\n=== SWEEP RESULT over {len(rows)} real HF files ===")
print(f"  CRITICAL (uRAW shipped)   : {len(crit)}")
print(f"  BAD (>2x worse than best) : {len(bad)}")
print(f"  any loss vs best standard : {len(losses)} / {len(rows)}")
print(f"  totals: mzip {tot_m:,}  best-standard {tot_b:,}  "
      f"({100*(tot_b-tot_m)/tot_b:+.2f}% mzip)")
excl = [r for r in rows if r not in crit]
em, eb = sum(r["mzip"] for r in excl), sum(r["best"] for r in excl)
print(f"  excluding CRITICAL files  : mzip {em:,} vs {eb:,} ({100*(eb-em)/eb:+.2f}% mzip)")
json.dump(dict(rows=rows, critical=crit, bad=bad), open(f"{W}/sweep.json","w"), indent=1)
print("\nALLDONE_SWEEP")
