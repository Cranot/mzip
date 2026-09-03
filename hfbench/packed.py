#!/usr/bin/env python3
"""31.75 TB/wk of the week is uint32 safetensors, and uint32 is not a natural weight dtype.

It is how GPTQ and AWQ ship 4-bit weights: eight codes packed into one int32 word. That packing is
category 4 of the frame -- badly encoded. The codes underneath are a narrow, bell-shaped distribution
(quantised weights cluster near the middle of their range), but packed into 32-bit words they present
as near-uniform bytes, which is precisely the shape a generic compressor cannot exploit and precisely
what plane-entropy was built for.

This measures the format question alone, with no base model and no prediction: what do the raw packed
bytes cost, and what do the same codes cost once unpacked? Any gap is free -- it needs nothing but
knowing what the container holds."""
import json, os, sys, subprocess, collections
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from fp8_given_base import header as st_header, grab, api

def h0(a):
    a = np.asarray(a).ravel()
    if a.size == 0: return 0.0
    _, c = np.unique(a, return_counts=True)
    p = c / c.sum()
    return float(-(p * np.log2(p)).sum())

def order1(sym, alpha):
    """conditional entropy H(x_i | x_{i-1}) in bits, the ceiling for a context-1 coder"""
    sym = np.asarray(sym).ravel()
    if sym.size < 2: return 0.0
    prev = sym[:-1].astype(np.int64); cur = sym[1:].astype(np.int64)
    idx = prev * alpha + cur
    cnt = np.bincount(idx, minlength=alpha*alpha).reshape(alpha, alpha).astype(np.float64)
    tot = cnt.sum(); rows = cnt.sum(axis=1, keepdims=True)
    with np.errstate(divide="ignore", invalid="ignore"):
        p = np.where(rows > 0, cnt / np.maximum(rows, 1), 0.0)
        lg = np.where(p > 0, np.log2(np.maximum(p, 1e-300)), 0.0)
    return float(-(cnt * lg).sum() / max(tot, 1))

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("safetensors:U32", 0) or 0) > 0]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["safetensors:U32"]))
    print(f"repos with U32 safetensors in the sample: {len(cand)}\n")
    print(f"{'repo':34s} {'tensor':22s} {'bits/code':>10s} {'packed':>8s} {'nibble':>8s} "
          f"{'ord-1':>7s} {'planes':>8s} {'gain':>7s}")
    tot_p = tot_b = 0.0; n = 0
    for r in cand[:14]:
        rid = r["id"]
        d = api(f"{H}/api/models/{rid}")
        if not d: continue
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        if not sts: continue
        h, off = st_header(rid, sts[0])
        if not h: continue
        u32 = [(k, v) for k, v in h.items()
               if isinstance(v, dict) and v.get("dtype") in ("U32", "I32")
               and (v["data_offsets"][1] - v["data_offsets"][0]) > (1 << 20)
               and "qweight" in k.lower()]
        if not u32:
            u32 = [(k, v) for k, v in h.items()
                   if isinstance(v, dict) and v.get("dtype") in ("U32", "I32")
                   and (v["data_offsets"][1] - v["data_offsets"][0]) > (1 << 20)]
        if not u32: continue
        name, meta = max(u32, key=lambda c: c[1]["data_offsets"][1] - c[1]["data_offsets"][0])
        nby = min(8 << 20, meta["data_offsets"][1] - meta["data_offsets"][0])
        raw = grab(rid, sts[0], off + meta["data_offsets"][0], nby)
        if raw is None: continue
        b = np.frombuffer(raw.tobytes()[:(nby // 4) * 4], dtype=np.uint8)
        w = b.view(np.uint32)
        # infer the code width: try 4 and 8 bits per code, keep the one with lower unpacked entropy
        best = None
        for bits in (4, 8, 2, 3):
            per = 32 // bits
            if per * bits != 32: continue
            codes = np.empty((w.size, per), dtype=np.uint8)
            for i in range(per):
                codes[:, i] = (w >> (i * bits)) & ((1 << bits) - 1)
            c = codes.ravel()
            hc = h0(c)
            if best is None or hc / bits < best[1] / best[0]: best = (bits, hc, c)
        bits, hc, codes = best
        packed_bits = h0(b) * 4 / (32 / bits)          # bits of packed bytes, per code
        # byte-plane view of the packed words: plane k of every 4-byte word
        planes = b[:(b.size // 4) * 4].reshape(-1, 4)
        pl = sum(h0(planes[:, i]) for i in range(4)) / (32 / bits)
        o1 = order1(codes, 1 << bits)
        gain = 1 - min(hc, o1, pl) / max(packed_bits, 1e-9)
        print(f"{rid[:34]:34s} {name.split('.')[-2][:22]:22s} {bits:10d} {packed_bits:8.3f} "
              f"{hc:8.3f} {o1:7.3f} {pl:8.3f} {100*gain:6.1f}%")
        tot_p += packed_bits; tot_b += min(hc, o1, pl); n += 1
    if n:
        print(f"\n  across {n} tensors: packed {tot_p/n:.3f} bits/code -> {tot_b/n:.3f} "
              f"({100*(1-tot_b/tot_p):.1f}% smaller, {tot_p/tot_b:.2f}x)")
    print("PACKED_DONE")

if __name__ == "__main__":
    main()
