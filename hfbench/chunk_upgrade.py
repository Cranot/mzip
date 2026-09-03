#!/usr/bin/env python3
"""At HF's real 64 KiB chunk granularity: what does a CHEAP upgrade to Xet's
scheme buy, and does a shared dictionary recover the context that chunking
throws away? This is the bar mzip must clear at chunk level.

Arms (all per-64K-chunk, sizes summed):
  bg4-lz4                 HF today
  bg4-zstd-1/3/9/19       swap lz4 for zstd (zstd-1 is ~lz4-class speed)
  bg2-lz4, bg2-zstd-3     stride-2 grouping (natural for BF16/F16)
  bg4-zstd-3 +selfdict    dict trained on THIS file's chunks; dict bytes ADDED to total
  bg4-zstd-3 +crossdict   dict trained on a DIFFERENT model of the same dtype; no per-file cost
Sizes only. Deterministic, no timing.
"""
import os, json, numpy as np, lz4.frame, zstandard as zstd

W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"
CS=64*1024
INPUTS=[("MiniLM-L6-v2","F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2","F32",f"{W}/gpt2.slice"),
 ("bert-base","F32",f"{W}/bert-base.slice"),("bge-small","F32",f"{W}/bge-small.slice"),
 ("weights-fp32","F32",f"{W}/weights-fp32.bin"),("weights-bf16","BF16",f"{W}/weights-bf16.bin"),
 ("Qwen2.5-0.5B","BF16",f"{W}/Qwen2.5-0.5B.slice")]
INPUTS=[x for x in INPUTS if os.path.exists(x[2])]

def bg(data, k):
    a=np.frombuffer(data,dtype=np.uint8); n=(len(a)//k)*k; m=a[:n].reshape(-1,k)
    return b"".join(m[:,j].tobytes() for j in range(k))+a[n:].tobytes()
lz=lambda b: len(lz4.frame.compress(b,compression_level=0))
def zs(level, dict_data=None):
    cd = zstd.ZstdCompressionDict(dict_data) if dict_data else None
    c = zstd.ZstdCompressor(level=level, dict_data=cd) if cd else zstd.ZstdCompressor(level=level)
    return lambda b: len(c.compress(b))

def chunks_of(raw): return [raw[i:i+CS] for i in range(0,len(raw),CS)]

# cross-dict sources: train on one F32 file for the other F32 files, one BF16 for the other BF16
def train_dict(chunks_bg4, size=112*1024):
    try: return zstd.train_dictionary(size, chunks_bg4).as_bytes()
    except Exception as e: return None

data={t:open(p,"rb").read() for t,_,p in INPUTS}
bg4chunks={t:[bg(c,4) for c in chunks_of(data[t])] for t in data}
cross_src={"F32":"MiniLM-L6-v2","BF16":"weights-bf16"}
cross_dict={dt:train_dict(bg4chunks[src]) for dt,src in cross_src.items() if src in bg4chunks}

rows=[]
print(f"{'input':14s} {'dt':4s} {'bg4-lz4':>9s} {'bg4-z1':>9s} {'bg4-z3':>9s} {'bg4-z9':>9s} {'bg4-z19':>9s} {'bg2-lz4':>9s} {'bg2-z3':>9s} {'z3+self':>9s} {'(+dict)':>9s} {'z3+cross':>9s}")
for tag,dt,p in INPUTS:
    raw=data[tag]; N=len(raw); ch=chunks_of(raw); b4=bg4chunks[tag]; b2=[bg(c,2) for c in ch]
    r=dict(tag=tag,dtype=dt,n=N)
    r["bg4_lz4"]=sum(lz(c) for c in b4)
    for L in (1,3,9,19): r[f"bg4_z{L}"]=sum(map(zs(L),b4))
    r["bg2_lz4"]=sum(lz(c) for c in b2)
    r["bg2_z3"]=sum(map(zs(3),b2))
    sd=train_dict(b4)
    r["bg4_z3_selfdict"]=sum(map(zs(3,sd),b4)) if sd else -1
    r["selfdict_bytes"]=len(sd) if sd else 0
    cdd=cross_dict.get(dt)
    r["bg4_z3_crossdict"]=sum(map(zs(3,cdd),b4)) if (cdd and cross_src[dt]!=tag) else -1
    rows.append(r)
    print(f"{tag:14s} {dt:4s} {r['bg4_lz4']:>9,} {r['bg4_z1']:>9,} {r['bg4_z3']:>9,} {r['bg4_z9']:>9,} {r['bg4_z19']:>9,} "
          f"{r['bg2_lz4']:>9,} {r['bg2_z3']:>9,} {r['bg4_z3_selfdict']:>9,} {r['bg4_z3_selfdict']+r['selfdict_bytes'] if sd else -1:>9,} "
          f"{r['bg4_z3_crossdict']:>9,}",flush=True)

json.dump(rows,open(f"{OUT}/upgrade.json","w"),indent=1)
n=sum(r["n"] for r in rows); base=sum(r["bg4_lz4"] for r in rows)
print(f"\n=== AGGREGATE over {len(rows)} slices ({n:,} B), 64K chunks, vs HF bg4-lz4 {base:,} ===")
for k,lab in (("bg4_z1","bg4+zstd-1"),("bg4_z3","bg4+zstd-3"),("bg4_z9","bg4+zstd-9"),("bg4_z19","bg4+zstd-19"),
              ("bg2_lz4","bg2+lz4"),("bg2_z3","bg2+zstd-3")):
    s=sum(r[k] for r in rows); print(f"  {lab:14s} {s:>10,}  {n/s:.4f}x  vs HF {100*(base-s)/base:+.2f}%")
sd=sum(r["bg4_z3_selfdict"]+r["selfdict_bytes"] for r in rows if r["bg4_z3_selfdict"]>0)
sn=sum(r["n"] for r in rows if r["bg4_z3_selfdict"]>0); sb=sum(r["bg4_lz4"] for r in rows if r["bg4_z3_selfdict"]>0)
print(f"  {'bg4+z3+selfdict(+dict bytes)':14s} {sd:>10,}  {sn/sd:.4f}x  vs HF {100*(sb-sd)/sb:+.2f}%")
cd=[r for r in rows if r["bg4_z3_crossdict"]>0]
if cd:
    s=sum(r["bg4_z3_crossdict"] for r in cd); n2=sum(r["n"] for r in cd); b2=sum(r["bg4_lz4"] for r in cd); z3=sum(r["bg4_z3"] for r in cd)
    print(f"  bg4+z3+crossdict (on {len(cd)} held-out files) {s:>10,} vs their bg4-lz4 {b2:,} ({100*(b2-s)/b2:+.2f}%), vs their bg4-z3 {z3:,} ({100*(z3-s)/z3:+.2f}%)")
print("\nBF16 only — does stride-2 beat stride-4?")
for r in rows:
    if r["dtype"]=="BF16": print(f"  {r['tag']:14s} bg4-lz4 {r['bg4_lz4']:,}  bg2-lz4 {r['bg2_lz4']:,} ({100*(r['bg4_lz4']-r['bg2_lz4'])/r['bg4_lz4']:+.2f}%)   bg4-z3 {r['bg4_z3']:,}  bg2-z3 {r['bg2_z3']:,} ({100*(r['bg4_z3']-r['bg2_z3'])/r['bg4_z3']:+.2f}%)")
print("ALLDONE_UPG")
