#!/usr/bin/env python
"""Mantissa split-point sweep: sign | exponent | mant_hi_k | mant_lo_(23-k).
usage: xform3.py f|i semk<K> in out [origbytes]     (fp32 only)"""
import sys, numpy as np

def bits_of(v, width):
    n = v.size
    b = np.zeros((n, width), dtype=np.uint8)
    v = v.astype(np.uint32)
    for i in range(width):
        b[:, width - 1 - i] = (v >> i) & 1
    return b

def unbits(bm):
    n, width = bm.shape
    out = np.zeros(n, dtype=np.uint32)
    for i in range(width):
        out |= bm[:, i].astype(np.uint32) << np.uint32(width - 1 - i)
    return out

def fwd(b, k):
    w = b.view(np.uint32)
    s = (w >> 31).astype(np.uint8)
    e = ((w >> 23) & 0xFF).astype(np.uint8)
    m = (w & 0x7FFFFF).astype(np.uint32)
    bm = bits_of(m, 23)
    parts = [np.packbits(s), e]
    if k > 0:
        parts.append(np.packbits(bm[:, :k].reshape(-1)))
    if k < 23:
        parts.append(np.packbits(bm[:, k:].reshape(-1)))
    return np.concatenate(parts)

def inv(buf, n, k):
    o1 = n // 8
    o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint32)
    e = buf[o1:o2].astype(np.uint32)
    p = o2
    cols = []
    if k > 0:
        nb = (n * k + 7) // 8
        cols.append(np.unpackbits(buf[p:p + nb])[: n * k].reshape(n, k)); p += nb
    if k < 23:
        r = 23 - k
        nb = (n * r + 7) // 8
        cols.append(np.unpackbits(buf[p:p + nb])[: n * r].reshape(n, r)); p += nb
    m = unbits(np.concatenate(cols, axis=1))
    return ((s << 31) | (e << 23) | m).astype(np.uint32).view(np.uint8)

if __name__ == "__main__":
    mode, name, i_, o_ = sys.argv[1:5]
    k = int(name.replace("semk", ""))
    d = np.fromfile(i_, dtype=np.uint8)
    out = fwd(d, k) if mode == "f" else inv(d, int(sys.argv[5]) // 4, k)
    np.asarray(out, dtype=np.uint8).tofile(o_)
