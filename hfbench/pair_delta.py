#!/usr/bin/env python3
"""How compressible is a fine-tune GIVEN its base model? For each tensor-aligned pair:
 (1) what we get today, no base knowledge: plane-entropy (bench_pe) on base and on ft alone;
 (2) element statistics: exact-equal fraction, same-exponent fraction, |delta| histogram;
 (3) delta representations at 64 KiB chunk granularity (the Xet-realistic unit, chunk i of ft
     against chunk i of base): XOR bytes, integer difference of the bit patterns, and zstd-19 with
     the base chunk as dictionary (what a 'reference chunk' scheme would do); each coded with
     order-0 planes (bound) and with the real coders; plus whole-file zstd --patch-from as the
     ceiling for a file-level reference scheme.
Every coded size is round-trip verified where a real coder is used."""
import json, os, subprocess, sys, numpy as np
P = "/root/mzip-hfbench/pairs"; PE = "/root/mzip-hfbench/final/plane-entropy/bench_pe"
CS = 65536
def H0bits(x):
    h = np.bincount(x, minlength=256).astype(np.float64); p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())
def plane_bound(a, k):
    """order-0 bound (bytes) of a byte array split into k planes"""
    m = a[: (len(a) // k) * k].reshape(-1, k)
    return sum(H0bits(m[:, j].copy()) * len(m) / 8 for j in range(k)) + (len(a) - len(m) * k)
def pe_ratio(path):
    r = subprocess.run(["taskset", "-c", "3", PE, path, str(CS), "1"], capture_output=True, text=True).stdout
    for l in r.splitlines():
        if "  bytes" in l:
            f = l.split(); return int(f[1]), float(f[3]), l.strip().endswith("OK")
    return None
def zstd_size(data, level=19, dict_=None):
    """zstd CLI; a dictionary file without the zstd magic is used as raw content. Round-trip verified."""
    import tempfile
    d = tempfile.mkdtemp(prefix="pd_", dir=P)
    fi, fo, fd, fr = f"{d}/in", f"{d}/out.zst", f"{d}/dict", f"{d}/rt"
    open(fi, "wb").write(data)
    args = ["zstd", f"-{level}", "-q", "-f", "-o", fo, fi]
    if dict_ is not None: open(fd, "wb").write(dict_); args += ["-D", fd]
    subprocess.run(args, check=True, capture_output=True)
    sz = os.path.getsize(fo)
    dargs = ["zstd", "-d", "-q", "-f", "-o", fr, fo] + (["-D", fd] if dict_ is not None else [])
    subprocess.run(dargs, check=True, capture_output=True)
    assert open(fr, "rb").read() == data, "zstd round-trip mismatch"
    subprocess.run(["rm", "-rf", d])
    return sz
HAVE_Z = True
for jf in sorted(os.listdir(P)):
    if not jf.endswith(".json"): continue
    rec = json.load(open(f"{P}/{jf}"))
    if not rec.get("fetched"): print(f"{rec['tag']}: not fetched"); continue
    tag, dt = rec["tag"], rec["dtype"]; k = {"BF16": 2, "F16": 2, "F32": 4}[dt]
    b = np.frombuffer(open(f"{P}/{tag}.base.bin", "rb").read(), dtype=np.uint8)
    f = np.frombuffer(open(f"{P}/{tag}.ft.bin", "rb").read(), dtype=np.uint8)
    n = min(len(b), len(f)); b, f = b[:n], f[:n]
    print(f"\n=== {tag}: {rec['base']} -> {rec['ft']}  tensor {rec['tensor']} {dt} {rec['shape']}  ({n} B) ===")
    # (1) today
    rb = pe_ratio(f"{P}/{tag}.base.bin"); rf = pe_ratio(f"{P}/{tag}.ft.bin")
    print(f"  today, no base knowledge:  base {rb[1]:.4f}x   fine-tune {rf[1]:.4f}x  (plane-entropy, 64K chunks, rt {rf[2]})")
    # (2) element stats
    wt = {2: np.uint16, 4: np.uint32}[k]
    eb = np.frombuffer(b[: (n // k) * k].tobytes(), dtype=wt); ef = np.frombuffer(f[: (n // k) * k].tobytes(), dtype=wt)
    same = float((eb == ef).mean())
    hi_b = (eb >> (8 * (k - 1))).astype(np.uint8); hi_f = (ef >> (8 * (k - 1))).astype(np.uint8)
    same_hi = float((hi_b == hi_f).mean())
    diff = (ef.astype(np.int64) - eb.astype(np.int64))
    ad = np.abs(diff)
    print(f"  elements identical: {100*same:.2f}%   same high byte (sign+exponent): {100*same_hi:.2f}%   |bit-pattern delta| <= 1: {100*float((ad<=1).mean()):.1f}%  <= 4: {100*float((ad<=4).mean()):.1f}%  <= 64: {100*float((ad<=64).mean()):.1f}%")
    # (3) delta representations, order-0 plane bounds
    x = np.bitwise_xor(b, f)
    dd = diff.astype({2: np.int16, 4: np.int32}[k]).view(np.uint8)
    print(f"  order-0 plane bound:  ft alone {n/plane_bound(f,k):.4f}x   XOR(ft,base) {n/plane_bound(x,k):.4f}x   int-delta(ft-base) {n/plane_bound(dd,k):.4f}x")
    # real coders on the delta streams, chunked
    open(f"{P}/{tag}.xor.bin", "wb").write(x.tobytes()); open(f"{P}/{tag}.idelta.bin", "wb").write(dd.tobytes())
    rx = pe_ratio(f"{P}/{tag}.xor.bin"); rd = pe_ratio(f"{P}/{tag}.idelta.bin")
    print(f"  plane-entropy coder:  XOR {rx[1]:.4f}x (rt {rx[2]})   int-delta {rd[1]:.4f}x (rt {rd[2]})")
    if HAVE_Z:
        tot_plain = tot_dict = tot_xor = 0
        for i in range(0, n, CS):
            cb = b[i:i+CS].tobytes(); cf = f[i:i+CS].tobytes()
            tot_plain += zstd_size(cf, 19); tot_dict += zstd_size(cf, 19, dict_=cb); tot_xor += zstd_size(x[i:i+CS].tobytes(), 19)
        print(f"  zstd-19 per 64K chunk: ft alone {n/tot_plain:.4f}x   ft with base chunk as dictionary {n/tot_dict:.4f}x   XOR {n/tot_xor:.4f}x   (all verified)")
    # whole-file patch-from ceiling
    r = subprocess.run(["zstd", "-19", "--patch-from", f"{P}/{tag}.base.bin", "-f", "-o", f"{P}/{tag}.patch.zst", f"{P}/{tag}.ft.bin"], capture_output=True, text=True)
    if os.path.exists(f"{P}/{tag}.patch.zst"):
        ps = os.path.getsize(f"{P}/{tag}.patch.zst")
        rr = subprocess.run(["zstd", "-d", "--patch-from", f"{P}/{tag}.base.bin", "-f", "-o", f"{P}/{tag}.patch.out", f"{P}/{tag}.patch.zst"], capture_output=True)
        ok = open(f"{P}/{tag}.patch.out", "rb").read() == f.tobytes()
        print(f"  whole-file zstd-19 --patch-from base: {n/ps:.4f}x  (round-trip {'OK' if ok else 'FAIL'})  [file-level reference ceiling]")
    else:
        print("  zstd --patch-from unavailable:", r.stderr.strip()[:120])
print("\nPAIR_DELTA_DONE")
