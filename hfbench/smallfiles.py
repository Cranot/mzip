#!/usr/bin/env python3
"""Attack mzip's one measured loss: small files.

Every HF model repo ships the same handful of small JSON files. mzip loses
below ~64KB because it wraps brotli (whose 120KB English/web dictionary it
cannot match per-file) and then pays container framing on top.

Test: does a dictionary TRAINED ON HF SMALL FILES close that gap? mzip already
has a ZSTD_DICT block type (0x37) and mzip_dicts.h, so this is a question about
dictionary CONTENT, not new machinery.
"""
import json, os, subprocess, sys

W = "/root/mzip-hfbench/small"
MZIP = "/root/mzip-hfbench/build/mzip_cli"
os.makedirs(W, exist_ok=True)

REPOS = ["bert-base-uncased", "gpt2", "distilbert-base-uncased",
         "roberta-base", "sentence-transformers/all-MiniLM-L6-v2",
         "google/flan-t5-small", "facebook/bart-base", "albert-base-v2",
         "microsoft/deberta-v3-small", "xlm-roberta-base"]
FILES = ["config.json", "tokenizer_config.json", "special_tokens_map.json"]

got = []
for r in REPOS:
    for f in FILES:
        tag = (r.replace("/", "_") + "." + f)
        p = f"{W}/{tag}"
        if not os.path.exists(p):
            url = f"https://huggingface.co/{r}/resolve/main/{f}"
            subprocess.run(["curl","-sSL","--max-time","30","-o",p,url],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if os.path.exists(p) and 40 < os.path.getsize(p) < 200000:
            head = open(p,"rb").read(1)
            if head in (b"{", b"["):
                got.append(p)
            else:
                os.remove(p)
        elif os.path.exists(p):
            os.remove(p)

print(f"collected {len(got)} real HF small files, "
      f"total {sum(os.path.getsize(g) for g in got):,} B")
sizes = sorted(os.path.getsize(g) for g in got)
print(f"  size range {sizes[0]:,} .. {sizes[-1]:,}  median {sizes[len(sizes)//2]:,}")

# hold out half for testing, train the dictionary on the other half
train = got[0::2]
test  = got[1::2]
DICT = f"{W}/hf.dict"
subprocess.run(["zstd","--train","--maxdict=16384","-o",DICT]+train,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
have_dict = os.path.exists(DICT)
print(f"trained dict on {len(train)} files -> "
      f"{os.path.getsize(DICT) if have_dict else 0:,} B; testing on {len(test)} held-out files")

def size_of(cmd, out):
    if os.path.exists(out): os.remove(out)
    subprocess.run(cmd, shell=isinstance(cmd,str), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    s = os.path.getsize(out) if os.path.exists(out) else 0
    if os.path.exists(out): os.remove(out)
    return s

tot = {"raw":0,"brotli":0,"zstd19":0,"zstd_dict":0,"mzip":0}
for t in test:
    n = os.path.getsize(t); tot["raw"] += n
    o = f"{W}/o.tmp"
    tot["brotli"]    += size_of(f"brotli -q 11 -c {t} > {o}", o)
    tot["zstd19"]    += size_of(f"zstd -19 -T1 -q -f -o {o} {t}", o)
    if have_dict:
        tot["zstd_dict"] += size_of(f"zstd -19 -T1 -q -f -D {DICT} -o {o} {t}", o)
    tot["mzip"]      += size_of([MZIP,"c",t,o,"19"], o)

print(f"\nheld-out total: {tot['raw']:,} B across {len(test)} files")
base = tot["brotli"]
for k in ("brotli","zstd19","zstd_dict","mzip"):
    if not tot[k]: continue
    print(f"  {k:10s} {tot[k]:>9,}  {tot['raw']/tot[k]:6.4f}x  "
          f"vs brotli {100*(base-tot[k])/base:+7.2f}%")
print("\nALLDONE_S")
