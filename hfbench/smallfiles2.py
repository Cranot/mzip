#!/usr/bin/env python3
"""Expanded small-file dictionary test -- the n=8 pilot was too small to bank.

Draws config/tokenizer JSON from the top models by downloads (a real, diverse
sample rather than hand-picked repos), trains a dictionary on a held-out half,
and tests on the other half. Also reports per-file-size behaviour, since the
pilot's files were tiny and self-similar.
"""
import json, os, subprocess, statistics

W = "/root/mzip-hfbench/small2"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
os.makedirs(W, exist_ok=True)

FILES = ["config.json", "tokenizer_config.json", "special_tokens_map.json",
         "generation_config.json", "preprocessor_config.json"]

# real sample: top models by downloads
r = subprocess.run(["curl","-sS","--max-time","40",
    "https://huggingface.co/api/models?sort=downloads&direction=-1&limit=120"],
    capture_output=True, text=True)
repos = [m["id"] for m in json.loads(r.stdout)]
print(f"sampling {len(repos)} top-downloaded models")

got = []
for repo in repos:
    for f in FILES:
        tag = repo.replace("/", "__") + "__" + f
        p = f"{W}/{tag}"
        if not os.path.exists(p):
            subprocess.run(["curl","-sSL","--max-time","15","-o",p,
                            f"https://huggingface.co/{repo}/resolve/main/{f}"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if os.path.exists(p):
            n = os.path.getsize(p)
            ok = 40 < n < 500000 and open(p,"rb").read(1) in (b"{", b"[")
            if not ok: os.remove(p)
            else: got.append(p)

got = sorted(set(got))
sizes = [os.path.getsize(g) for g in got]
print(f"collected {len(got)} files, total {sum(sizes):,} B, "
      f"median {int(statistics.median(sizes)):,}, max {max(sizes):,}")

train, test = got[0::2], got[1::2]
DICT = f"{W}/hf.dict"
subprocess.run(["zstd","--train","--maxdict=65536","-o",DICT]+train,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
have = os.path.exists(DICT)
print(f"dict trained on {len(train)} files -> {os.path.getsize(DICT) if have else 0:,} B; "
      f"held-out {len(test)} files")

def sz(cmd, out):
    if os.path.exists(out): os.remove(out)
    subprocess.run(cmd, shell=isinstance(cmd,str), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    s = os.path.getsize(out) if os.path.exists(out) else 0
    if os.path.exists(out): os.remove(out)
    return s

tot = dict(raw=0, brotli=0, zstd19=0, zstd_dict=0, mzip=0)
wins = dict(brotli=0, zstd_dict=0, mzip=0)
o = f"{W}/o.tmp"
for t in test:
    n = os.path.getsize(t); tot["raw"] += n
    b = sz(f"brotli -q 11 -c {t} > {o}", o); tot["brotli"] += b
    tot["zstd19"] += sz(f"zstd -19 -T1 -q -f -o {o} {t}", o)
    d = sz(f"zstd -19 -T1 -q -f -D {DICT} -o {o} {t}", o) if have else 0
    tot["zstd_dict"] += d
    m = sz([MZIP,"c",t,o,"19"], o); tot["mzip"] += m
    best = min(x for x in (b,d,m) if x>0)
    for k,v in (("brotli",b),("zstd_dict",d),("mzip",m)):
        if v == best: wins[k] += 1

print(f"\nheld-out total {tot['raw']:,} B across {len(test)} files")
base = tot["brotli"]
for k in ("brotli","zstd19","zstd_dict","mzip"):
    if not tot[k]: continue
    print(f"  {k:10s} {tot[k]:>9,}  {tot['raw']/tot[k]:6.4f}x  vs brotli {100*(base-tot[k])/base:+7.2f}%")
print("  per-file wins:", wins)
print("\nALLDONE_S2")
