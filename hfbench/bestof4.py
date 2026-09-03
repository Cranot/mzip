#!/usr/bin/env python3
"""The honest cheap-upgrade number for HF: per-chunk best-of over codecs that run
at lz4-class speed -- {lz4, bg4-lz4, zstd-1, bg4-zstd-1} (+ lz4hc-12 and
bg4-lz4hc-12 as the zero-decoder-change option). At 64K on all 8 MiB slices,
both 256 MiB slices, and the GGUF slices. Also reports the SINGLE-arm winner so
the failure of any one arm is visible. Sizes only, fast, no mzip.
Run AFTER the gated timing pass (do not perturb it)."""
import os, json, numpy as np, lz4.frame, zstandard as zstd
W="/root/mzip-hfbench/weights"; B="/root/mzip-hfbench/big"; OUT="/root/mzip-hfbench/chunk"; CS=65536
def bg4(d):
    a=np.frombuffer(d,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4); return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
ARMS={
 "lz4":        lambda c: lz4.frame.compress(c,compression_level=0),
 "bg4-lz4":    lambda c: lz4.frame.compress(bg4(c),compression_level=0),
 "zstd1":      lambda c: zstd.ZstdCompressor(level=1).compress(c),
 "bg4-zstd1":  lambda c: zstd.ZstdCompressor(level=1).compress(bg4(c)),
 "lz4hc12":    lambda c: lz4.frame.compress(c,compression_level=12),
 "bg4-lz4hc12":lambda c: lz4.frame.compress(bg4(c),compression_level=12),
}
INPUTS=[("MiniLM F32",f"{W}/MiniLM-L6-v2.slice"),("gpt2 F32",f"{W}/gpt2.slice"),("bert F32",f"{W}/bert-base.slice"),
 ("bge F32",f"{W}/bge-small.slice"),("wfp32 F32",f"{W}/weights-fp32.bin"),("wbf16 BF16",f"{W}/weights-bf16.bin"),
 ("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice"),("gguf-q8_0",f"{W}/gguf-q8_0.slice"),
 ("qwen256 BF16",f"{B}/qwen-bf16-256M.bin"),("gpt2-256 F32",f"{B}/gpt2-f32-256M.bin")]
rows=[]; T={k:0 for k in ARMS}; TB4=TB6=THF=TN=0
print(f"{'input':13s} {'HF bg4-lz4':>11s} {'best single':>22s} {'best-of-4/chunk':>15s} {'vs HF':>7s} {'best-of-6/chunk':>15s} {'vs HF':>7s}")
for tag,p in INPUTS:
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); N=len(raw); ch=[raw[i:i+CS] for i in range(0,N,CS)]
    per={k:0 for k in ARMS}; b4=0; b6=0; win4={k:0 for k in ARMS}
    for c in ch:
        s={k:len(f(c)) for k,f in ARMS.items()}
        for k in ARMS: per[k]+=s[k]
        k4=min(("lz4","bg4-lz4","zstd1","bg4-zstd1"),key=lambda k:s[k]); b4+=s[k4]; win4[k4]+=1
        b6+=min(s.values())
    hf=per["bg4-lz4"]; bs=min(per,key=per.get)
    rows.append(dict(tag=tag,n=N,per=per,bestof4=b4,bestof6=b6,win4=win4))
    for k in ARMS: T[k]+=per[k]
    TB4+=b4; TB6+=b6; THF+=hf; TN+=N
    print(f"{tag:13s} {hf:>11,} {bs+' '+f'{per[bs]:,}':>22s} {b4:>15,} {100*(hf-b4)/hf:>+6.2f}% {b6:>15,} {100*(hf-b6)/hf:>+6.2f}%   chunk winners: {win4}",flush=True)
print(f"\nTOTAL {TN:,} B: HF {THF:,}  best-of-4 {TB4:,} ({100*(THF-TB4)/THF:+.2f}%)  best-of-6 {TB6:,} ({100*(THF-TB6)/THF:+.2f}%)")
for k in ARMS: print(f"  single-arm {k:12s} {T[k]:>12,}  vs HF {100*(THF-T[k])/THF:+.2f}%")
json.dump(rows,open(f"{OUT}/bestof4.json","w"),indent=1)
print("ALLDONE_B4")
