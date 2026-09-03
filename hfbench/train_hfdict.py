#!/usr/bin/env python3
"""Train HF-metadata zstd dictionaries on half of the 289 real HF config/tokenizer
JSON files; hold the other half out. Baseline the held-out half with zstd-19 +
each dict, zstd-19 alone, brotli-11, and current native mzip (which already
trials DICT_JSON_REAL / DICT_CONFIG)."""
import os, shutil, subprocess
W="/root/mzip-hfbench/small2"; H="/root/mzip-hfbench/hfdict"
got=sorted(f for f in os.listdir(W) if f.endswith(".json"))
train,test=got[0::2],got[1::2]
for d in ("train","test"):
    shutil.rmtree(f"{H}/{d}",ignore_errors=True); os.makedirs(f"{H}/{d}")
for f in train: shutil.copy(f"{W}/{f}",f"{H}/train/{f}")
for f in test:  shutil.copy(f"{W}/{f}",f"{H}/test/{f}")
tb=sum(os.path.getsize(f"{W}/{f}") for f in train); sb=sum(os.path.getsize(f"{W}/{f}") for f in test)
print(f"train {len(train)} files {tb:,} B ; held-out {len(test)} files {sb:,} B")
def run(cmd): return subprocess.run(cmd,shell=True,capture_output=True,text=True)
for sz in (16384,65536,112640):
    r=run(f"zstd --train -q --maxdict={sz} -o {H}/hf_dict_{sz}.bin {H}/train/*.json")
    print(f"hf_dict_{sz}.bin: {os.path.getsize(f'{H}/hf_dict_{sz}.bin') if os.path.exists(f'{H}/hf_dict_{sz}.bin') else 'FAILED '+r.stderr[-120:]}")
def total(cmd_fmt):
    t=0
    for f in sorted(os.listdir(f"{H}/test")):
        p=f"{H}/test/{f}"; out="/tmp/hd.out"
        if os.path.exists(out): os.remove(out)
        run(cmd_fmt.format(p=p,out=out)); t+=os.path.getsize(out) if os.path.exists(out) else 0
    return t
print(f"\nheld-out totals ({sb:,} B raw):")
for sz in (16384,65536,112640):
    if os.path.exists(f"{H}/hf_dict_{sz}.bin"):
        print(f"  zstd-19 + hf_dict_{sz:<6}: {total(f'zstd -19 -q -f -D {H}/hf_dict_{sz}.bin -o {{out}} {{p}}'):>8,}")
print(f"  zstd-19 no dict        : {total('zstd -19 -q -f -o {out} {p}'):>8,}")
print(f"  brotli-11              : {total('brotli -q 11 -c {p} > {out}'):>8,}")
print(f"  mzip native (fixed3)   : {total('/root/mzip-hfbench/build/mzip_fixed3 c {p} {out} 1'):>8,}")
print("ALLDONE_TRAIN")
