#!/usr/bin/env python3
"""Why LZ4 finds nothing in F16 but something in BF16: per-plane order-0 entropy, distinct symbols,
and the density of 4-byte repeats (LZ4's minimum match) in each byte plane."""
import numpy as np, os
B = "/root/mzip-hfbench"
FILES = [("bloom-560m", "weights2/bloom-560m.bin", "F16"), ("llama2-7b", "weights2/llama2-7b.bin", "F16"),
         ("sdxl-unet", "weights2/sdxl-unet.bin", "F16"), ("whisper-lv3", "weights2/whisper-lv3.bin", "F16"),
         ("qwen2.5-1.5b", "weights2/qwen2.5-1.5b.bin", "BF16"), ("tinyllama", "weights2/tinyllama-1.1b.bin", "BF16"),
         ("weights-bf16", "weights/weights-bf16.bin", "BF16")]
def H0(x):
    h = np.bincount(x, minlength=256).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum()), int((h > 0).sum())
def rep4(x):
    """fraction of positions where the next 4 bytes repeat a 4-byte string seen in the last 64 KiB"""
    v = np.frombuffer(x[: (len(x) // 4) * 4].tobytes(), dtype=np.uint32)
    _, counts = np.unique(v, return_counts=True)
    return float((counts[counts > 1].sum() - (counts > 1).sum()) / len(v))
print("%-14s %-5s | %-28s | %-28s | 4B-repeat density" % ("file", "type", "high plane (byte 1)", "low plane (byte 0)"))
print("%-14s %-5s | %8s %6s %8s | %8s %6s %8s | high     low" % ("", "", "H0", "syms", "", "H0", "syms", ""))
for tag, path, dt in FILES:
    a = np.frombuffer(open(os.path.join(B, path), "rb").read(), dtype=np.uint8)
    m = a[: (len(a) // 2) * 2].reshape(-1, 2)
    lo, hi = m[:, 0].copy(), m[:, 1].copy()
    hh, hs = H0(hi); lh, ls = H0(lo)
    print("%-14s %-5s |  %6.3f b %5d          |  %6.3f b %5d          | %7.4f  %7.4f" % (
        tag, dt, hh, hs, lh, ls, rep4(hi), rep4(lo)))
print()
print("order-0 bound of the two planes together, vs what each scheme achieved:")
for tag, path, dt in FILES:
    a = np.frombuffer(open(os.path.join(B, path), "rb").read(), dtype=np.uint8)
    m = a[: (len(a) // 2) * 2].reshape(-1, 2)
    hh, _ = H0(m[:, 1].copy()); lh, _ = H0(m[:, 0].copy())
    bound = len(a) / (len(m) * (hh + lh) / 8)
    print("  %-14s %-5s order-0 bound %.4fx" % (tag, dt, bound))
