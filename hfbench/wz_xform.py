#!/usr/bin/env python
"""Exactly-invertible transforms for float weight blobs.
usage: xform.py f|i NAME infile outfile
Every transform preserves total byte count and is a bijection.
"""
import sys, numpy as np

def bits_of(arr_u32, width):
    """MSB-first bit matrix (n,width) from integer array, taking low `width` bits."""
    n = arr_u32.size
    b = np.zeros((n, 32), dtype=np.uint8)
    v = arr_u32.astype(np.uint32)
    for i in range(32):
        b[:, 31 - i] = (v >> i) & 1
    return b[:, 32 - width:]

def unbits(bm, dtype=np.uint32):
    n, width = bm.shape
    v = np.zeros(n, dtype=np.uint64)
    for i in range(width):
        v |= bm[:, i].astype(np.uint64) << np.uint64(width - 1 - i)
    return v.astype(dtype)

def pack(bm):
    return np.packbits(bm.reshape(-1))

def unpack(buf, n, width):
    bits = np.unpackbits(buf)[: n * width]
    return bits.reshape(n, width)

# ---------------- bf16 (u16 LE) ----------------
def bf_fields(b):
    u = b.view(np.uint16).astype(np.uint32)
    return (u >> 15).astype(np.uint8), ((u >> 7) & 0xFF).astype(np.uint8), (u & 0x7F).astype(np.uint8)

def bf_join(s, e, m):
    u = (s.astype(np.uint32) << 15) | (e.astype(np.uint32) << 7) | m.astype(np.uint32)
    return u.astype(np.uint16).view(np.uint8)

def f_b_bitsplit(b):
    s, e, m = bf_fields(b)
    return np.concatenate([np.packbits(s), e, pack(bits_of(m.astype(np.uint32), 7))])

def i_b_bitsplit(buf, n):
    o1 = n // 8; o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint8)
    e = buf[o1:o2]
    m = unbits(unpack(buf[o2:], n, 7), np.uint32).astype(np.uint8)
    return bf_join(s, e, m)

def f_b_bucket(b):
    """sign plane, exponent plane, mantissa plane REORDERED by stable-sort on exponent."""
    s, e, m = bf_fields(b)
    order = np.argsort(e, kind="stable")
    return np.concatenate([np.packbits(s), e, pack(bits_of(m[order].astype(np.uint32), 7))])

def i_b_bucket(buf, n):
    o1 = n // 8; o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint8)
    e = buf[o1:o2]
    mb = unbits(unpack(buf[o2:], n, 7), np.uint32).astype(np.uint8)
    order = np.argsort(e, kind="stable")
    m = np.empty(n, dtype=np.uint8); m[order] = mb
    return bf_join(s, e, m)

def f_b_bucket_se(b):
    """bucket mantissa by (sign,exponent) jointly."""
    s, e, m = bf_fields(b)
    key = (s.astype(np.uint32) << 8) | e
    order = np.argsort(key, kind="stable")
    return np.concatenate([np.packbits(s), e, pack(bits_of(m[order].astype(np.uint32), 7))])

def i_b_bucket_se(buf, n):
    o1 = n // 8; o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint8)
    e = buf[o1:o2]
    mb = unbits(unpack(buf[o2:], n, 7), np.uint32).astype(np.uint8)
    key = (s.astype(np.uint32) << 8) | e
    order = np.argsort(key, kind="stable")
    m = np.empty(n, dtype=np.uint8); m[order] = mb
    return bf_join(s, e, m)

def f_b_delta(b):
    u = b.view(np.uint16)
    d = (u - np.concatenate(([np.uint16(0)], u[:-1]))).astype(np.uint16)
    return d.view(np.uint8)

def i_b_delta(buf, n):
    d = buf.view(np.uint16)
    return np.cumsum(d.astype(np.uint64)).astype(np.uint16).view(np.uint8)

def f_b_bg2(b):
    return np.concatenate([b[0::2], b[1::2]])

def i_b_bg2(buf, n):
    out = np.empty(2 * n, dtype=np.uint8)
    out[0::2] = buf[:n]; out[1::2] = buf[n:]
    return out

# ---------------- fp32 (u32 LE) ----------------
def f_f_split19_13(b):
    w = b.view(np.uint32)
    bm = bits_of(w, 32)
    return np.concatenate([pack(bm[:, :19]), pack(bm[:, 19:])])

def i_f_split19_13(buf, n):
    o = (n * 19 + 7) // 8
    hi = unpack(buf[:o], n, 19); lo = unpack(buf[o:], n, 13)
    return unbits(np.concatenate([hi, lo], axis=1), np.uint32).view(np.uint8)

def f_f_sem(b):
    """sign bits packed | exponent bytes | 23-bit mantissa packed"""
    w = b.view(np.uint32)
    s = (w >> 31).astype(np.uint8); e = ((w >> 23) & 0xFF).astype(np.uint8)
    m = (w & 0x7FFFFF).astype(np.uint32)
    return np.concatenate([np.packbits(s), e, pack(bits_of(m, 23))])

def i_f_sem(buf, n):
    o1 = n // 8; o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint32)
    e = buf[o1:o2].astype(np.uint32)
    m = unbits(unpack(buf[o2:], n, 23), np.uint32)
    return ((s << 31) | (e << 23) | m).astype(np.uint32).view(np.uint8)

def f_f_sem_m10_13(b):
    """sign | exponent | mantissa-high-10 | mantissa-low-13  (isolates the fp16 zero field)"""
    w = b.view(np.uint32)
    s = (w >> 31).astype(np.uint8); e = ((w >> 23) & 0xFF).astype(np.uint8)
    m = (w & 0x7FFFFF).astype(np.uint32)
    bm = bits_of(m, 23)
    return np.concatenate([np.packbits(s), e, pack(bm[:, :10]), pack(bm[:, 10:])])

def i_f_sem_m10_13(buf, n):
    o1 = n // 8; o2 = o1 + n
    o3 = o2 + (n * 10 + 7) // 8
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint32)
    e = buf[o1:o2].astype(np.uint32)
    hi = unpack(buf[o2:o3], n, 10); lo = unpack(buf[o3:], n, 13)
    m = unbits(np.concatenate([hi, lo], axis=1), np.uint32)
    return ((s << 31) | (e << 23) | m).astype(np.uint32).view(np.uint8)

def f_f_bg4(b):
    return np.concatenate([b[0::4], b[1::4], b[2::4], b[3::4]])

def i_f_bg4(buf, n):
    out = np.empty(4 * n, dtype=np.uint8)
    for k in range(4):
        out[k::4] = buf[k * n:(k + 1) * n]
    return out

def f_f_bucket(b):
    """fp32: mantissa (23b) reordered by stable sort on exponent."""
    w = b.view(np.uint32)
    s = (w >> 31).astype(np.uint8); e = ((w >> 23) & 0xFF).astype(np.uint8)
    m = (w & 0x7FFFFF).astype(np.uint32)
    order = np.argsort(e, kind="stable")
    return np.concatenate([np.packbits(s), e, pack(bits_of(m[order], 23))])

def i_f_bucket(buf, n):
    o1 = n // 8; o2 = o1 + n
    s = np.unpackbits(buf[:o1])[:n].astype(np.uint32)
    e = buf[o1:o2]
    mb = unbits(unpack(buf[o2:], n, 23), np.uint32)
    order = np.argsort(e, kind="stable")
    m = np.empty(n, dtype=np.uint32); m[order] = mb
    return ((s.astype(np.uint32) << 31) | (e.astype(np.uint32) << 23) | m).astype(np.uint32).view(np.uint8)

BF = {"b_bitsplit": (f_b_bitsplit, i_b_bitsplit), "b_bucket": (f_b_bucket, i_b_bucket),
      "b_bucket_se": (f_b_bucket_se, i_b_bucket_se), "b_delta": (f_b_delta, i_b_delta),
      "b_bg2": (f_b_bg2, i_b_bg2)}
FP = {"f_split19_13": (f_f_split19_13, i_f_split19_13), "f_sem": (f_f_sem, i_f_sem),
      "f_sem_m10_13": (f_f_sem_m10_13, i_f_sem_m10_13), "f_bg4": (f_f_bg4, i_f_bg4),
      "f_bucket": (f_f_bucket, i_f_bucket)}
ALL = {}; ALL.update(BF); ALL.update(FP)
ELEMS = 2  # bf16 default

if __name__ == "__main__":
    mode, name, inp, outp = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    data = np.fromfile(inp, dtype=np.uint8)
    fwd, inv = ALL[name]
    esz = 2 if name.startswith("b_") else 4
    if mode == "f":
        out = fwd(data)
    else:
        nfile = int(sys.argv[5])          # ORIGINAL byte length
        out = inv(data, nfile // esz)
    out = np.asarray(out, dtype=np.uint8)
    out.tofile(outp)
