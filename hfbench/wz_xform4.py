#!/usr/bin/env python
"""Round 3: byte-aligned exponent + exponent-bucketed payload, fp32 analogue of the
bf16 winner; plus a bucketed variant of the fp32 m10/13 split.
usage: xform4.py f|i NAME in out [origbytes]"""
import sys, numpy as np

# ---- fp32: [exponent bytes] [ 3-byte (m<<1|s) payload, stable-bucketed by exponent ] ----
def f_f_rot_bucket(b):
    w = b.view(np.uint32)
    e = ((w >> 23) & 0xFF).astype(np.uint8)
    pay = (((w & 0x7FFFFF) << 1) | (w >> 31)).astype(np.uint32)   # 24 bits
    order = np.argsort(e, kind="stable")
    p = pay[order]
    three = np.empty((p.size, 3), dtype=np.uint8)
    three[:, 0] = p & 0xFF; three[:, 1] = (p >> 8) & 0xFF; three[:, 2] = (p >> 16) & 0xFF
    return np.concatenate([e, three.reshape(-1)])

def i_f_rot_bucket(buf, n):
    e = buf[:n].copy()
    three = buf[n:].reshape(n, 3).astype(np.uint32)
    p = three[:, 0] | (three[:, 1] << 8) | (three[:, 2] << 16)
    pay = np.empty(n, dtype=np.uint32)
    pay[np.argsort(e, kind="stable")] = p
    w = ((pay & 1).astype(np.uint32) << 31) | (e.astype(np.uint32) << 23) | (pay >> 1)
    return w.astype(np.uint32).view(np.uint8)

# ---- fp32: sign|exp|mant_hi10|mant_lo13 but with each mantissa field bucketed by exponent ----
def _bits(v, w):
    n = v.size; b = np.zeros((n, w), dtype=np.uint8); v = v.astype(np.uint32)
    for i in range(w): b[:, w - 1 - i] = (v >> i) & 1
    return b
def _unbits(bm):
    n, w = bm.shape; o = np.zeros(n, dtype=np.uint32)
    for i in range(w): o |= bm[:, i].astype(np.uint32) << np.uint32(w - 1 - i)
    return o

def f_f_semk10_bucket(b):
    w = b.view(np.uint32)
    s = (w >> 31).astype(np.uint8); e = ((w >> 23) & 0xFF).astype(np.uint8)
    m = (w & 0x7FFFFF).astype(np.uint32)
    order = np.argsort(e, kind="stable")
    bm = _bits(m[order], 23)
    return np.concatenate([np.packbits(s[order]), e,
                           np.packbits(bm[:, :10].reshape(-1)),
                           np.packbits(bm[:, 10:].reshape(-1))])
def i_f_semk10_bucket(buf, n):
    o1 = n // 8; o2 = o1 + n; o3 = o2 + (n * 10 + 7) // 8
    ss = np.unpackbits(buf[:o1])[:n].astype(np.uint32)
    e = buf[o1:o2].copy()
    hi = np.unpackbits(buf[o2:o3])[: n * 10].reshape(n, 10)
    lo = np.unpackbits(buf[o3:])[: n * 13].reshape(n, 13)
    mm = _unbits(np.concatenate([hi, lo], axis=1))
    order = np.argsort(e, kind="stable")
    m = np.empty(n, dtype=np.uint32); m[order] = mm
    s = np.empty(n, dtype=np.uint32); s[order] = ss
    return ((s << 31) | (e.astype(np.uint32) << 23) | m).astype(np.uint32).view(np.uint8)

# ---- bf16: winner, re-exported here so batch 3 needs one tool ----
def f_b_rot_bucket(b):
    u = b.view(np.uint16).astype(np.uint32)
    e = ((u >> 7) & 0xFF).astype(np.uint8)
    ms = ((((u & 0x7F) << 1) | (u >> 15)) & 0xFF).astype(np.uint8)
    return np.concatenate([e, ms[np.argsort(e, kind="stable")]])
def i_b_rot_bucket(buf, n):
    e = buf[:n].copy(); msb = buf[n:]
    ms = np.empty(n, dtype=np.uint8); ms[np.argsort(e, kind="stable")] = msb
    u = ((ms >> 1).astype(np.uint32) | (e.astype(np.uint32) << 7) | ((ms & 1).astype(np.uint32) << 15))
    return u.astype(np.uint16).view(np.uint8)
def f_b_rot(b):
    u = b.view(np.uint16).astype(np.uint32)
    return (((u >> 7) | (u << 9)) & 0xFFFF).astype(np.uint16).view(np.uint8)
def i_b_rot(buf, n):
    v = buf.view(np.uint16).astype(np.uint32)
    return (((v << 7) | (v >> 9)) & 0xFFFF).astype(np.uint16).view(np.uint8)

ALL = {k[2:]: (globals()["f_" + k[2:]], globals()["i_" + k[2:]])
       for k in list(globals()) if k.startswith("f_b_") or k.startswith("f_f_")}

if __name__ == "__main__":
    mode, name, i_, o_ = sys.argv[1:5]
    d = np.fromfile(i_, dtype=np.uint8)
    fwd, inv = ALL[name]
    esz = 2 if name.startswith("b_") else 4
    out = fwd(d) if mode == "f" else inv(d, int(sys.argv[5]) // esz)
    np.asarray(out, dtype=np.uint8).tofile(o_)
