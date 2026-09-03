import numpy as np

files = [("weights-bf16.bin", "/root/mzip-hfbench/weights/weights-bf16.bin"),
         ("bf16-layers.bin", "/root/mzip-hfbench/wz/bf16-layers.bin"),
         ("Qwen2.5-0.5B.slice", "/root/mzip-hfbench/weights/Qwen2.5-0.5B.slice")]

def H(a, ml=0):
    h = np.bincount(np.asarray(a).ravel(), minlength=ml).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

for name, path in files:
    b = np.fromfile(path, dtype=np.uint8)
    u = b.view(np.uint16).astype(np.uint32)
    n = u.size
    e = ((u >> 7) & 0xFF).astype(np.uint8)
    m = (u & 0x7F).astype(np.uint8)
    s = (u >> 15).astype(np.uint8)
    # duplicate structure: how repetitive is the raw stream?
    uq = np.unique(u)
    # how many 8-byte blocks repeat?
    blk = b[: (b.size // 8) * 8].reshape(-1, 8)
    _, cnt = np.unique(blk, axis=0, return_counts=True)
    dupfrac = 1.0 - cnt.size / blk.shape[0]
    # conditional saving that bucketing targets
    tot = 0.0
    for ev in np.unique(e):
        mm = m[e == ev]
        tot += mm.size * H(mm, 128)
    Hm, Hme = H(m, 128), tot / n
    print("%-22s n=%d  H(exp)=%.4f H(mant)=%.4f H(mant|exp)=%.4f  cond-saving=%d B"
          % (name, n, H(e, 256), Hm, Hme, int(n * (Hm - Hme) / 8)))
    print("    distinct u16=%d  dup-8B-block frac=%.4f  H(u16 joint)=%.4f -> %d B"
          % (uq.size, dupfrac, H(np.unique(u, return_inverse=True)[1].astype(np.int64)),
             int(n * H(np.unique(u, return_inverse=True)[1].astype(np.int64)) / 8)))
    # LOCAL repetition: fraction of elements equal to one of the previous 64
    win = 64
    hit = 0
    step = max(1, n // 200000)
    idx = np.arange(win, n, step)
    prev = np.stack([u[idx - k] for k in range(1, win + 1)])
    hit = float((prev == u[idx]).any(axis=0).mean())
    print("    P(elem repeats within previous %d elems) = %.4f" % (win, hit))
    # run structure of exponent
    print("    exponent transitions/elem = %.4f" % (float((np.diff(e) != 0).mean())))
