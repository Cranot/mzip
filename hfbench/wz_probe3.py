import numpy as np

def ctz24(m3):
    r = np.zeros(m3.size, dtype=np.uint8)
    mask = m3 != 0
    vv = m3.copy()
    for i in range(24):
        add = mask & ((vv & 1) == 0)
        r[add] += 1
        vv = np.where(add, vv >> 1, vv)
    r[~mask] = 24
    return r

b3 = np.fromfile("/root/mzip-hfbench/weights/weights-fp32.bin", dtype=np.uint8)
w = b3.view(np.uint32)
n3 = w.size
m3 = (w & 0x7FFFFF).astype(np.uint32)
t = ctz24(m3)
lowp = (t >= 13)
print("fp32 n=%d  fp16-exact(tz>=13) = %d (%.2f%%)" % (n3, lowp.sum(), 100 * lowp.mean()))

# spatial clustering: fraction of low-precision elems in each 1/64 block
nb = 64
bs = n3 // nb
print("block profile (frac fp16-exact per 1/64 of file):")
fr = [float(lowp[i * bs:(i + 1) * bs].mean()) for i in range(nb)]
for i in range(0, nb, 4):
    print("  " + " ".join("%5.3f" % fr[j] for j in range(i, min(i + 4, nb))))
# run-length structure of the boolean
tr = np.diff(lowp.astype(np.int8))
print("transitions:", int((tr != 0).sum()), " => mean run length %.2f" % (n3 / max(1, int((tr != 0).sum()) + 1)))
print("bitmap order0 entropy: %.5f bits/elem -> %d B" %
      (-(lowp.mean() * np.log2(lowp.mean()) + (1 - lowp.mean()) * np.log2(1 - lowp.mean())),
       int(n3 * -(lowp.mean() * np.log2(lowp.mean()) + (1 - lowp.mean()) * np.log2(1 - lowp.mean())) / 8)))

# Is low-precision aligned to a stride? check period
for period in (2, 3, 4, 8, 16, 64, 128, 256, 384, 768):
    if n3 % period == 0:
        v = lowp[:n3 // period * period].reshape(-1, period).mean(axis=0)
        print("  period %4d: min %.3f max %.3f spread %.3f" % (period, v.min(), v.max(), v.max() - v.min()))

print()
print("### autocorrelation of magnitude (bf16) ###")
b = np.fromfile("/root/mzip-hfbench/weights/weights-bf16.bin", dtype=np.uint8)
u = b.view(np.uint16).astype(np.uint32)
expo = ((u >> 7) & 0xFF).astype(np.float64)
e = expo - expo.mean()
den = float((e * e).sum())
for lag in (1, 2, 3, 4, 8, 16, 64, 128, 256, 384, 512, 768, 1024, 1536, 2048, 4096):
    print("  lag %5d  r=%+.5f" % (lag, float((e[:-lag] * e[lag:]).sum()) / den))

print()
print("### bf16 mantissa distribution within top exponent buckets ###")
mant = (u & 0x7F).astype(np.uint8)
ex = ((u >> 7) & 0xFF).astype(np.uint8)
for e0 in [123, 122, 124, 121, 120, 125]:
    m = mant[ex == e0]
    h = np.bincount(m, minlength=128).astype(np.float64)
    p = h[h > 0] / h.sum()
    print("  exp=%3d n=%8d H=%.4f  first-quarter mass %.4f last-quarter %.4f"
          % (e0, m.size, float(-(p * np.log2(p)).sum()),
             float(h[:32].sum() / h.sum()), float(h[96:].sum() / h.sum())))
