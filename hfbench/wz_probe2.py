import numpy as np

def H(a, minlength=None):
    a = np.asarray(a).ravel()
    h = np.bincount(a, minlength=(minlength or 0)).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def bytes_at(bits_total):
    return int(bits_total / 8)

print("############ BF16 ############")
b = np.fromfile("/root/mzip-hfbench/weights/weights-bf16.bin", dtype=np.uint8)
u = b.view(np.uint16).astype(np.uint32)
n = u.size
sign = (u >> 15).astype(np.uint8)
expo = ((u >> 7) & 0xFF).astype(np.uint8)
mant = (u & 0x7F).astype(np.uint8)

Hs, He, Hm = H(sign, 2), H(expo, 256), H(mant, 128)
print("n=%d  H(sign)=%.4f H(exp)=%.4f H(mant)=%.4f  sum=%.4f -> %d B" %
      (n, Hs, He, Hm, Hs + He + Hm, bytes_at(n * (Hs + He + Hm))))

# joint symbol entropy over the full u16 element (memoryless element model)
uniq, inv = np.unique(u, return_inverse=True)
Hu16 = H(inv.astype(np.int64), uniq.size)
print("H(u16 joint)=%.4f -> %d B   [captures sign/exp/mant dependence]" % (Hu16, bytes_at(n * Hu16)))

# conditional entropy of mantissa given exponent  == "mantissa bucketed by exponent"
tot = 0.0
for e in np.unique(expo):
    m = mant[expo == e]
    tot += m.size * H(m, 128)
Hm_given_e = tot / n
print("H(mant|exp)=%.4f  (vs H(mant)=%.4f)  saving %.4f bits/elem = %d B"
      % (Hm_given_e, Hm, Hm - Hm_given_e, bytes_at(n * (Hm - Hm_given_e))))

# conditional entropy of mantissa given (sign,exponent)
tot = 0.0
key = (sign.astype(np.uint32) << 8) | expo
for k in np.unique(key):
    m = mant[key == k]
    tot += m.size * H(m, 128)
print("H(mant|sign,exp)=%.4f" % (tot / n))

# INTER-ELEMENT: does the previous element's exponent predict this one?
e_prev = np.concatenate(([expo[0]], expo[:-1]))
tot = 0.0
for e in np.unique(e_prev):
    m = expo[e_prev == e]
    tot += m.size * H(m, 256)
print("H(exp_i | exp_{i-1})=%.4f  (vs H(exp)=%.4f) saving=%d B"
      % (tot / n, He, bytes_at(n * (He - tot / n))))

# delta / xor of adjacent u16
d = ((u - np.concatenate(([0], u[:-1]))) & 0xFFFF).astype(np.uint32)
x = (u ^ np.concatenate(([0], u[:-1]))).astype(np.uint32)
for name, arr in (("delta_u16", d), ("xor_u16", x)):
    uq, iv = np.unique(arr, return_inverse=True)
    hh = H(iv.astype(np.int64), uq.size)
    print("H(%s joint)=%.4f -> %d B" % (name, hh, bytes_at(n * hh)))
    bl = (arr & 0xFF).astype(np.uint8); bh = (arr >> 8).astype(np.uint8)
    print("    byte-split: H(lo)=%.4f H(hi)=%.4f sum=%.4f -> %d B"
          % (H(bl, 256), H(bh, 256), H(bl, 256) + H(bh, 256), bytes_at(n * (H(bl, 256) + H(bh, 256)))))

# exponent delta
ed = ((expo.astype(np.int32) - np.concatenate(([0], expo[:-1])).astype(np.int32)) & 0xFF).astype(np.uint8)
print("H(exp_delta)=%.4f (vs H(exp)=%.4f)" % (H(ed, 256), He))

print()
print("############ FP32 ############")
b3 = np.fromfile("/root/mzip-hfbench/weights/weights-fp32.bin", dtype=np.uint8)
w = b3.view(np.uint32)
n3 = w.size
s3 = (w >> 31).astype(np.uint8)
e3 = ((w >> 23) & 0xFF).astype(np.uint8)
m3 = (w & 0x7FFFFF).astype(np.uint32)
print("n=%d H(sign)=%.4f H(exp)=%.4f" % (n3, H(s3, 2), H(e3, 256)))
# mantissa byte planes
mb = [(m3 >> 16).astype(np.uint8), ((m3 >> 8) & 0xFF).astype(np.uint8), (m3 & 0xFF).astype(np.uint8)]
hs = [H(x, 256) for x in mb]
print("mantissa byte-plane H: hi=%.4f mid=%.4f lo=%.4f  sum=%.4f" % (hs[0], hs[1], hs[2], sum(hs)))
tot0 = H(s3, 2) + H(e3, 256) + sum(hs)
print("field-split order0 total = %.4f bits/elem -> %d B" % (tot0, bytes_at(n3 * tot0)))
# how many trailing mantissa bits are always zero?
nz = m3[m3 > 0]
tz = np.zeros(24, dtype=np.int64)
mm = m3.copy()
cnt = np.zeros(n3, dtype=np.uint8)
tmp = m3.copy()
for i in range(24):
    stillz = (tmp & 1) == 0
    cnt += stillz.astype(np.uint8) * 0
    tmp = tmp >> 1
# simpler: count trailing zeros per element
def ctz(v):
    v = v.astype(np.uint64)
    r = np.zeros(v.size, dtype=np.uint8)
    mask = v != 0
    vv = v.copy()
    for i in range(24):
        bit0 = (vv & 1) == 0
        add = mask & bit0
        r[add] += 1
        vv = np.where(add, vv >> np.uint64(1), vv)
    r[~mask] = 24
    return r
t = ctz(m3)
print("mantissa trailing-zero-bit histogram (count of elems):")
hb = np.bincount(t, minlength=25)
for i in range(25):
    if hb[i]:
        print("   tz=%2d  %8d  (%.4f%%)" % (i, hb[i], 100.0 * hb[i] / n3))
print("median tz =", int(np.median(t)))
