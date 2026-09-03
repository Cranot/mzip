#!/usr/bin/env python3
"""Fine-tune-given-base, all pairs and tensor types. Per pair: today's ratio (plane coder, no base),
then three delta representations coded with FSE planes at 64 KiB blocks via the reference CLI,
EVERY coded stream decoded back and byte-compared: XOR, two's-complement difference, zigzag
difference. Plus the conditional-entropy ceiling of a base-context adaptive coder."""
import numpy as np, subprocess, os, glob, json, sys
P = "/root/mzip-hfbench/pairs"; FSE = "/root/mzip-hfbench/fse/fse"; PE = "/root/mzip-hfbench/final/plane-entropy/bench_pe"
def H0(x):
    h = np.bincount(x, minlength=256).astype(np.float64); p = h[h > 0] / h.sum(); return float(-(p * np.log2(p)).sum())
def Hc(x, y):
    j = np.bincount((y.astype(np.int32) << 8) | x.astype(np.int32), minlength=65536).astype(np.float64).reshape(256, 256)
    tot = j.sum(); py = j.sum(axis=1); h = 0.0
    for yi in np.nonzero(py)[0]:
        p = j[yi][j[yi] > 0] / py[yi]; h += (py[yi] / tot) * float(-(p * np.log2(p)).sum())
    return h
def fse_plane(pl):
    """FSE-coded size of one plane at 64 KiB blocks, round-trip verified; raw if it does not help"""
    fi, fo, fr = f"{P}/pl.tmp", f"{P}/pl.tmp.fse", f"{P}/pl.tmp.rt"; open(fi, "wb").write(pl)
    subprocess.run([FSE, "-B65536", "-f", fi, fo], capture_output=True)
    if not os.path.exists(fo): return len(pl)
    s = os.path.getsize(fo)
    subprocess.run([FSE, "-d", "-f", fo, fr], capture_output=True)
    assert os.path.exists(fr) and open(fr, "rb").read() == pl, "FSE round-trip FAILED"
    return min(s, len(pl))
def planes_fse(a, k):
    m = a[: (len(a) // k) * k].reshape(-1, k)
    return sum(fse_plane(m[:, j].copy().tobytes()) for j in range(k)) + (len(a) - len(m) * k)
def pe_ratio(path):
    r = subprocess.run(["taskset", "-c", "3", PE, path, "65536", "1"], capture_output=True, text=True).stdout
    for l in r.splitlines():
        if "  bytes" in l: return float(l.split()[3])
    return float("nan")
rows = []
for jf in sorted(glob.glob(f"{P}/*.json")):
    rec = json.load(open(jf))
    if not isinstance(rec, dict) or not rec.get("fetched") or rec.get("tag", "").startswith("gguf"): continue
    tag = rec["tag"]; k = {"BF16": 2, "F16": 2, "F32": 4}[rec["dtype"]]
    b = np.frombuffer(open(f"{P}/{tag}.base.bin", "rb").read(), dtype=np.uint8); f = np.frombuffer(open(f"{P}/{tag}.ft.bin", "rb").read(), dtype=np.uint8)
    n = min(len(b), len(f)); b, f = b[:n], f[:n]
    today = pe_ratio(f"{P}/{tag}.ft.bin")
    x = np.bitwise_xor(b, f)
    wt = {2: np.uint16, 4: np.uint32}[k]; it = {2: np.int16, 4: np.int32}[k]
    eb = np.frombuffer(b[: (n // k) * k].tobytes(), dtype=wt).astype(np.int64); ef = np.frombuffer(f[: (n // k) * k].tobytes(), dtype=wt).astype(np.int64)
    d = ef - eb
    tc = d.astype(it).view(np.uint8)
    zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(wt).view(np.uint8)
    r_xor = n / planes_fse(x, k); r_tc = n / planes_fse(tc, k); r_zig = n / planes_fse(zig, k)
    same = float(np.mean(eb == ef)); same_hi = float(np.mean((eb >> (8 * (k - 1))) == (ef >> (8 * (k - 1)))))
    # base-context ceiling on the zigzag planes (adaptive coder with the base's high byte as context)
    mz = zig[: (n // k) * k].reshape(-1, k); bh = (eb >> (8 * (k - 1))).astype(np.uint8)
    ceil0 = sum(H0(mz[:, j].copy()) * len(mz) for j in range(k)) / 8
    ceilc = sum(Hc(mz[:, j].copy(), bh) * len(mz) for j in range(k)) / 8
    best = max(r_xor, r_tc, r_zig)
    rows.append((tag, rec["base"].split("/")[-1], rec["ft"].split("/")[-1], rec["tensor"], rec["dtype"], today, r_xor, r_tc, r_zig, n / ceil0, n / ceilc, same, same_hi))
    print(f"{tag:26s} today {today:.4f}  XOR {r_xor:.4f}  2c {r_tc:.4f}  zigzag {r_zig:.4f}  | bound zig {n/ceil0:.4f}  +base-ctx {n/ceilc:.4f} | same {100*same:5.2f}%  same-hi {100*same_hi:5.2f}%  | ft costs {100*today/best:.0f}% of today", flush=True)
for t in glob.glob(f"{P}/pl.tmp*"): os.remove(t)
json.dump(rows, open(f"{P}/delta_all.json", "w"), indent=1)
print("DELTA_ALL_DONE")
