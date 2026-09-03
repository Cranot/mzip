#!/usr/bin/env python
"""Chunked rotate+exponent-bucket: applies the transform independently inside each
chunk of C elements, so locality is preserved and no single plane spans the file.
usage: xform5.py f|i b_rbc<C_in_K> in out [origbytes]"""
import sys, numpy as np

def fwd(b, C):
    u = b.view(np.uint16).astype(np.uint32)
    n = u.size
    out = np.empty(2 * n, dtype=np.uint8)
    p = 0
    for a in range(0, n, C):
        c = u[a:a + C]
        e = ((c >> 7) & 0xFF).astype(np.uint8)
        ms = ((((c & 0x7F) << 1) | (c >> 15)) & 0xFF).astype(np.uint8)
        k = e.size
        out[p:p + k] = e
        out[p + k:p + 2 * k] = ms[np.argsort(e, kind="stable")]
        p += 2 * k
    return out

def inv(buf, n, C):
    u = np.empty(n, dtype=np.uint32)
    p = 0
    for a in range(0, n, C):
        k = min(C, n - a)
        e = buf[p:p + k].copy()
        msb = buf[p + k:p + 2 * k]
        ms = np.empty(k, dtype=np.uint8)
        ms[np.argsort(e, kind="stable")] = msb
        u[a:a + k] = ((ms >> 1).astype(np.uint32) | (e.astype(np.uint32) << 7)
                      | ((ms & 1).astype(np.uint32) << 15))
        p += 2 * k
    return u.astype(np.uint16).view(np.uint8)

if __name__ == "__main__":
    mode, name, i_, o_ = sys.argv[1:5]
    C = int(name.replace("b_rbc", "")) * 1024
    d = np.fromfile(i_, dtype=np.uint8)
    out = fwd(d, C) if mode == "f" else inv(d, int(sys.argv[5]) // 2, C)
    np.asarray(out, dtype=np.uint8).tofile(o_)
