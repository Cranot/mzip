#!/usr/bin/env python
"""Round 2: BYTE-ALIGNED field isolation. usage: xform2.py f|i NAME in out [origbytes]"""
import sys, numpy as np

# bf16: u = s<<15 | e<<7 | m.  rot-right-7 -> low byte = exponent EXACTLY,
#       high byte = (m<<1)|s.  Zero bit-packing, so mzip's byte contexts stay aligned.
def _rr7(u):   return (((u >> 7) | (u << 9)) & 0xFFFF).astype(np.uint16)
def _rl7(v):   return (((v << 7) | (v >> 9)) & 0xFFFF).astype(np.uint16)

def f_b_rot(b):      return _rr7(b.view(np.uint16).astype(np.uint32)).view(np.uint8)
def i_b_rot(buf, n): return _rl7(buf.view(np.uint16).astype(np.uint32)).view(np.uint8)

def f_b_rot_bg2(b):
    r = _rr7(b.view(np.uint16).astype(np.uint32)).view(np.uint8)
    return np.concatenate([r[0::2], r[1::2]])
def i_b_rot_bg2(buf, n):
    r = np.empty(2 * n, dtype=np.uint8); r[0::2] = buf[:n]; r[1::2] = buf[n:]
    return _rl7(r.view(np.uint16).astype(np.uint32)).view(np.uint8)

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

# fp32: rot-right-23 -> low byte = exponent EXACTLY
def _rr23(w): return ((w >> 23) | (w << 9)).astype(np.uint32)
def _rl23(v): return ((v << 23) | (v >> 9)).astype(np.uint32)

def f_f_rot(b):      return _rr23(b.view(np.uint32)).view(np.uint8)
def i_f_rot(buf, n): return _rl23(buf.view(np.uint32)).view(np.uint8)

def f_f_rot_bg4(b):
    r = _rr23(b.view(np.uint32)).view(np.uint8)
    return np.concatenate([r[0::4], r[1::4], r[2::4], r[3::4]])
def i_f_rot_bg4(buf, n):
    r = np.empty(4 * n, dtype=np.uint8)
    for k in range(4): r[k::4] = buf[k * n:(k + 1) * n]
    return _rl23(r.view(np.uint32)).view(np.uint8)

# fp32 per-TENSOR split, using the real safetensors layout of the MiniLM slice.
# Tensor boundaries are a property of the file format, carried as a tiny fixed table.
FP32_SEGS = [0, 4096, 5632, 7168, 793600, 796672, 8388608]
def f_f_tensor(b):
    # reorder: int64 index tensor, layernorms, then the two embedding tables
    order = [(0, 4096), (4096, 5632), (5632, 7168), (793600, 796672), (7168, 793600), (796672, 8388608)]
    return np.concatenate([b[a:c] for a, c in order])
def i_f_tensor(buf, n):
    sizes = [4096, 1536, 1536, 3072, 786432, 7591936]
    off = np.cumsum([0] + sizes)
    p = [buf[off[i]:off[i + 1]] for i in range(6)]
    return np.concatenate([p[0], p[1], p[2], p[4], p[3], p[5]])

# fp32 BG4 applied WITHIN each tensor rather than across the whole slice
def f_f_bg4_pertensor(b):
    out = []
    for a, c in zip(FP32_SEGS[:-1], FP32_SEGS[1:]):
        s = b[a:c]
        out.append(np.concatenate([s[0::4], s[1::4], s[2::4], s[3::4]]))
    return np.concatenate(out)
def i_f_bg4_pertensor(buf, n):
    out = []; p = 0
    for a, c in zip(FP32_SEGS[:-1], FP32_SEGS[1:]):
        L = c - a; q = L // 4; s = buf[p:p + L]; p += L
        r = np.empty(L, dtype=np.uint8)
        for k in range(4): r[k::4] = s[k * q:(k + 1) * q]
        out.append(r)
    return np.concatenate(out)

ALL = {n[2:]: (globals()["f_" + n[2:]], globals()["i_" + n[2:]])
       for n in list(globals()) if n.startswith("f_b_") or n.startswith("f_f_")}

if __name__ == "__main__":
    mode, name, inp, outp = sys.argv[1:5]
    data = np.fromfile(inp, dtype=np.uint8)
    fwd, inv = ALL[name]
    esz = 2 if name.startswith("b_") else 4
    out = fwd(data) if mode == "f" else inv(data, int(sys.argv[5]) // esz)
    np.asarray(out, dtype=np.uint8).tofile(outp)
