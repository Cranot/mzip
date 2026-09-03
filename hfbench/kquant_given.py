#!/usr/bin/env python3
"""Does one quantisation predict a K-quant? The question that decides the GGUF lever.

RESULT 75 showed Q4_0 costs 3.7% of standalone when coded as a residual on Q8_0, because both derive
their scale from the same block extreme. But the K-quants -- Q4_K_M, Q5_K_M, Q6_K -- carry most of the
Hub's GGUF downloads, and they are built differently: a 256-weight super-block holds a scale and a
minimum, then eight sub-blocks each with their own 6-bit scale and minimum, and the weights are 4-bit
offsets from those.

The prediction still follows, though, from the DEQUANTISATION rule rather than from the quantiser:
Q4_K reconstructs a weight as x = d_j*q - m_j, so given the sub-block's own stored d_j and m_j (which
are 16 of the block's 144 bytes) and an approximation of x from another quantisation,

    q_predicted = round((x + m_j) / d_j)

If that lands, a K-quant costs its scales plus a residual instead of its 128 bytes of codes."""
import json, os, struct, subprocess, sys
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/qgq"
os.makedirs(OUT, exist_ok=True)
NSB = 40000            # 256-weight super-blocks to compare

def sh(a, t=600): return subprocess.run(a, capture_output=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout.decode())
    except Exception: return None

def read_header(repo, path, window=24 * 1024 * 1024):
    tmp = f"{OUT}/kh.tmp"
    sh(["curl", "-sSL", "--max-time", "300", "-r", f"0-{window-1}", "-o", tmp, f"{H}/{repo}/resolve/main/{path}"])
    b = open(tmp, "rb").read()
    if b[:4] != b"GGUF": return None
    n_tensor, n_kv = struct.unpack("<QQ", b[8:24]); p = 24
    def rstr():
        nonlocal p
        (ln,) = struct.unpack("<Q", b[p:p+8]); p += 8
        s = b[p:p+ln]; p += ln; return s.decode("utf-8", "replace")
    T = {0:1,1:1,2:2,3:2,4:4,5:4,6:4,7:1,10:8,11:8,12:8}
    def skip(t):
        nonlocal p
        if t == 8: rstr(); return
        if t == 9:
            (et,) = struct.unpack("<I", b[p:p+4]); p += 4
            (cnt,) = struct.unpack("<Q", b[p:p+8]); p += 8
            for _ in range(cnt):
                if et == 8: rstr()
                elif et == 9: return
                else: p += T.get(et, 4)
            return
        p += T.get(t, 4)
    for _ in range(n_kv):
        rstr(); (t,) = struct.unpack("<I", b[p:p+4]); p += 4; skip(t)
    tens = {}
    for _ in range(n_tensor):
        name = rstr()
        (nd,) = struct.unpack("<I", b[p:p+4]); p += 4
        dims = struct.unpack("<" + "Q"*nd, b[p:p+8*nd]); p += 8*nd
        (tt,) = struct.unpack("<I", b[p:p+4]); p += 4
        (off,) = struct.unpack("<Q", b[p:p+8]); p += 8
        tens[name] = dict(dims=dims, ttype=tt, offset=off)
    return dict(tensors=tens, data_start=(p + 31)//32*32)

def fetch(repo, path, start, length):
    tmp = f"{OUT}/ks.bin"
    sh(["curl", "-sSL", "--max-time", "600", "-r", f"{start}-{start+length-1}", "-o", tmp,
        f"{H}/{repo}/resolve/main/{path}"], t=660)
    if not os.path.exists(tmp) or os.path.getsize(tmp) != length: return None
    return np.fromfile(tmp, dtype=np.uint8)

def dec_q8(raw):
    n = len(raw)//34; a = raw[:n*34].reshape(n, 34)
    d = a[:, :2].copy().view(np.float16).astype(np.float32).ravel()
    q = a[:, 2:].view(np.int8).astype(np.float32)
    return (q * d[:, None]).reshape(-1)          # reconstructed weights

def get_scale_min_k4(scales):
    """ggml's 12-byte packing: 8 sub-blocks, each a 6-bit scale and a 6-bit min"""
    s = scales.astype(np.uint16)
    sc = np.empty(s.shape[:-1] + (8,), dtype=np.uint16)
    mn = np.empty(s.shape[:-1] + (8,), dtype=np.uint16)
    for j in range(8):
        if j < 4:
            sc[..., j] = s[..., j] & 63
            mn[..., j] = s[..., j + 4] & 63
        else:
            sc[..., j] = (s[..., j + 4] & 15) | ((s[..., j - 4] >> 6) << 4)
            mn[..., j] = (s[..., j + 4] >> 4) | ((s[..., j] >> 6) << 4)
    return sc.astype(np.float32), mn.astype(np.float32)

def dec_q4k(raw):
    n = len(raw)//144; a = raw[:n*144].reshape(n, 144)
    d = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    dmin = a[:, 2:4].copy().view(np.float16).astype(np.float32).ravel()
    scales = a[:, 4:16]
    qs = a[:, 16:]
    sc, mn = get_scale_min_k4(scales)
    dj = d[:, None] * sc                                   # [n,8]
    mj = dmin[:, None] * mn
    lo = (qs & 0x0F).astype(np.int32); hi = (qs >> 4).astype(np.int32)
    # ggml order: for each pair of sub-blocks, 32 low nibbles then 32 high nibbles
    q = np.empty((n, 256), dtype=np.int32)
    for p in range(4):
        q[:, p*64:p*64+32] = lo[:, p*32:(p+1)*32]
        q[:, p*64+32:p*64+64] = hi[:, p*32:(p+1)*32]
    return q, dj, mj

def h0(x):
    x = np.asarray(x).ravel()
    v, c = np.unique(x, return_counts=True); p = c/c.sum()
    return float(-(p*np.log2(p)).sum())

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8 = [f for f in files if "q8_0" in f.lower()]
    q4k = [f for f in files if "q4_k_m" in f.lower()]
    if not q8 or not q4k: print("need a Q8_0 and a Q4_K_M"); return
    print(f"{repo}\n  {q8[0]}\n  {q4k[0]}")
    h8 = read_header(repo, q8[0]); hk = read_header(repo, q4k[0])
    common = [n for n in h8["tensors"] if n in hk["tensors"]
              and h8["tensors"][n]["ttype"] == 8 and hk["tensors"][n]["ttype"] == 12]
    if not common: print("no tensor is Q8_0 in one and Q4_K in the other"); return
    common.sort(key=lambda n: -int(np.prod(h8["tensors"][n]["dims"])))
    name = common[0]; t8, tk = h8["tensors"][name], hk["tensors"][name]
    nel = int(np.prod(t8["dims"])); nsb = min(NSB, nel//256)
    print(f"  tensor {name}, {nel:,} elements, {nsb:,} super-blocks of 256")
    r8 = fetch(repo, q8[0], h8["data_start"] + t8["offset"], nsb*8*34)
    rk = fetch(repo, q4k[0], hk["data_start"] + tk["offset"], nsb*144)
    if r8 is None or rk is None: print("fetch failed"); return
    x = dec_q8(r8)[:nsb*256].reshape(nsb, 256)
    q, dj, mj = dec_q4k(rk)
    n = min(len(q), len(x)); q, dj, mj, x = q[:n], dj[:n], mj[:n], x[:n]
    sub = np.repeat(np.arange(8), 32)[None, :]
    D = np.take_along_axis(dj, np.broadcast_to(sub, (n, 256)), axis=1)
    M = np.take_along_axis(mj, np.broadcast_to(sub, (n, 256)), axis=1)
    with np.errstate(divide="ignore", invalid="ignore"):
        pred = np.where(D != 0, np.rint((x + M) / D), 0).astype(np.int32)
    pred = np.clip(pred, 0, 15)
    exact = float(np.mean(pred == q))
    resid = (q - pred).ravel()
    bits_now = h0(q.ravel()) + (16.0 + 16.0 + 12*8) / 256      # codes plus d, dmin and the 12 scale bytes
    bits_res = h0(resid) + (16.0 + 16.0 + 12*8) / 256          # scales still stored; only codes change
    print(f"\n  codes predicted exactly: {100*exact:.2f}%")
    print(f"  residual: {100*float(np.mean(resid==0)):.2f}% zero, "
          f"{100*float(np.mean(np.abs(resid)<=1)):.2f}% within +/-1, range [{resid.min()},{resid.max()}]")
    print(f"  bits per weight: Q4_K_M alone {bits_now:.4f}  |  residual on Q8_0 {bits_res:.4f}")
    print(f"  => {100*bits_res/bits_now:.1f}% of standalone ({bits_now/max(bits_res,1e-9):.2f}x)")
    json.dump(dict(repo=repo, tensor=name, superblocks=int(n), exact=exact,
                   bits_alone=bits_now, bits_residual=bits_res), open(f"{OUT}/kqgq.json", "w"), indent=1)
    print("KQGQ_DONE")

if __name__ == "__main__":
    main()
