#!/usr/bin/env python3
"""Why does the plane coder sit 14-16% above the order-0 bound on the XOR delta, and can a
different entropy coder close it? On each pair's XOR stream: per-plane H0, huff0 vs FSE (tANS)
coded size via the reference CLI, and the conditional entropy of the delta's high byte given the
base's high byte (is context on the base worth anything?)."""
import numpy as np, subprocess, os, glob, json
P = "/root/mzip-hfbench/pairs"; FSE = "/root/mzip-hfbench/fse/fse"
def H0(x):
    h = np.bincount(x, minlength=256).astype(np.float64); p = h[h > 0] / h.sum(); return float(-(p * np.log2(p)).sum())
def Hc(x, y):
    j = np.bincount((y.astype(np.int32) << 8) | x.astype(np.int32), minlength=65536).astype(np.float64).reshape(256, 256)
    tot = j.sum(); py = j.sum(axis=1); h = 0.0
    for yi in np.nonzero(py)[0]:
        p = j[yi][j[yi] > 0] / py[yi]; h += (py[yi] / tot) * float(-(p * np.log2(p)).sum())
    return h
def coded(plane_bytes, mode):
    """reference FSE CLI: -h = huff0, default = FSE; sizes summed over 64 KiB blocks (-B65536)"""
    fi = f"{P}/plane.tmp"; fo = f"{P}/plane.tmp.out"; open(fi, "wb").write(plane_bytes)
    args = [FSE] + (["-h"] if mode == "huff0" else []) + ["-B65536", "-f", fi, fo]
    subprocess.run(args, capture_output=True); s = os.path.getsize(fo) if os.path.exists(fo) else len(plane_bytes)
    return min(s, len(plane_bytes))
for jf in sorted(glob.glob(f"{P}/*.json")):
    rec = json.load(open(jf))
    if not rec.get("fetched") or "." in rec["tag"]: continue   # the three original single-tensor pairs
    tag = rec["tag"]; k = 2
    b = np.frombuffer(open(f"{P}/{tag}.base.bin", "rb").read(), dtype=np.uint8); f = np.frombuffer(open(f"{P}/{tag}.ft.bin", "rb").read(), dtype=np.uint8)
    n = min(len(b), len(f)); b, f = b[:n], f[:n]; x = np.bitwise_xor(b, f)
    m = x[: (n // k) * k].reshape(-1, k); mb = b[: (n // k) * k].reshape(-1, k)
    lo, hi = m[:, 0].copy(), m[:, 1].copy(); bhi = mb[:, 1].copy(); blo = mb[:, 0].copy()
    print(f"=== {tag}  XOR delta planes ===")
    tot = {"bound": 0.0, "huff0": 0, "fse": 0}
    for name, pl in (("high(sign+exp)", hi), ("low(mantissa)", lo)):
        h = H0(pl); bound = h * len(pl) / 8; hu = coded(pl.tobytes(), "huff0"); fs = coded(pl.tobytes(), "fse")
        tot["bound"] += bound; tot["huff0"] += hu; tot["fse"] += fs
        print(f"  {name:15s} H0 {h:.3f} b/B  bound {bound:9.0f}  huff0 {hu:9d} (+{100*(hu/bound-1):.1f}%)  FSE {fs:9d} (+{100*(fs/bound-1):.1f}%)   P(0)={np.mean(pl==0):.3f}")
    print(f"  TOTAL ratio: bound {n/tot['bound']:.4f}x  huff0-planes {n/tot['huff0']:.4f}x  FSE-planes {n/tot['fse']:.4f}x")
    print(f"  context: H(xor_hi) {H0(hi):.3f}  H(xor_hi | base_hi) {Hc(hi, bhi):.3f}  H(xor_lo) {H0(lo):.3f}  H(xor_lo | xor_hi) {Hc(lo, hi):.3f}  H(xor_lo | base_hi) {Hc(lo, bhi):.3f}")
    # a smarter delta: signed integer difference of the bit patterns, coded as sign+magnitude bytes
    eb = np.frombuffer(b[: (n // 2) * 2].tobytes(), dtype=np.uint16).astype(np.int64); ef = np.frombuffer(f[: (n // 2) * 2].tobytes(), dtype=np.uint16).astype(np.int64)
    d = ef - eb; zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(np.uint16)   # zigzag -> small unsigned
    zl, zh = (zig & 0xFF).astype(np.uint8), (zig >> 8).astype(np.uint8)
    zb = (H0(zl) + H0(zh)) * len(zig) / 8; zf = coded(zl.tobytes(), "fse") + coded(zh.tobytes(), "fse")
    print(f"  zigzag(ft-base) planes: bound {n/zb:.4f}x  FSE {n/zf:.4f}x   |d|<=8: {100*np.mean(np.abs(d)<=8):.1f}%")
for t in glob.glob(f"{P}/plane.tmp*"): os.remove(t)
print("GAP_DONE")
