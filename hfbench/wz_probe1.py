import numpy as np, sys, collections

def rep(path, label):
    b = np.fromfile(path, dtype=np.uint8)
    print("==", label, path, "n=", b.size)
    # byte-position histogram of each of 4 lanes and 2 lanes
    for stride in (2,4):
        print("  stride", stride)
        for k in range(stride):
            lane = b[k::stride]
            h = np.bincount(lane, minlength=256).astype(np.float64)
            p = h/h.sum(); p = p[p>0]
            H = -(p*np.log2(p)).sum()
            top = np.argsort(-h)[:5]
            print("    lane%d H=%.3f top=%s" % (k, H, [(int(t), round(float(h[t]/b.size*stride),4)) for t in top]))
    # whole-file entropy
    h = np.bincount(b, minlength=256).astype(np.float64); p=h/h.sum(); p=p[p>0]
    print("  order0 entropy bits/byte = %.4f -> floor bytes %d" % (-(p*np.log2(p)).sum(), int(b.size*(-(p*np.log2(p)).sum())/8)))
    return b

bf = rep("/root/mzip-hfbench/weights/weights-bf16.bin","bf16")
f3 = rep("/root/mzip-hfbench/weights/weights-fp32.bin","fp32")

print()
print("--- bf16 as uint16 LE ---")
u = bf.view(np.uint16)
sign = (u>>15).astype(np.uint8)
expo = ((u>>7)&0xFF).astype(np.uint8)
mant = (u&0x7F).astype(np.uint8)
print(" n elems", u.size)
he = np.bincount(expo, minlength=256).astype(np.float64)
pe = he/he.sum(); pen = pe[pe>0]
print(" exponent entropy %.4f bits ; top10 %s" % (-(pen*np.log2(pen)).sum(), [(int(t), round(float(he[t]/u.size),4)) for t in np.argsort(-he)[:10]]))
print(" exponent top8 coverage %.4f" % float(np.sort(he)[::-1][:8].sum()/u.size))
hm = np.bincount(mant, minlength=128).astype(np.float64); pm=hm/hm.sum(); pmn=pm[pm>0]
print(" mantissa(7b) entropy %.4f bits" % -(pmn*np.log2(pmn)).sum())
print(" sign p(1)=%.4f" % float(sign.mean()))
print(" exact zeros: %d (%.4f%%)" % (int((u==0).sum()), float((u==0).mean()*100)))
print(" denormal/zero exp==0: %d" % int((expo==0).sum()))
print(" distinct u16 values: %d" % int(np.unique(u).size))

print()
print("--- fp32 as uint32 LE ---")
w = f3.view(np.uint32)
print(" n elems", w.size)
e32 = ((w>>23)&0xFF).astype(np.uint8)
he = np.bincount(e32, minlength=256).astype(np.float64); pe=he/he.sum(); pen=pe[pe>0]
print(" exponent entropy %.4f bits ; top10 %s" % (-(pen*np.log2(pen)).sum(), [(int(t), round(float(he[t]/w.size),4)) for t in np.argsort(-he)[:10]]))
print(" exact zeros: %d (%.4f%%)" % (int((w==0).sum()), float((w==0).mean()*100)))
print(" distinct u32: %d of %d" % (int(np.unique(w).size), w.size))
fv = f3.view(np.float32)
print(" finite frac %.6f  min %.4g max %.4g" % (float(np.isfinite(fv).mean()), float(np.nanmin(fv)), float(np.nanmax(fv))))
# is the head an int64 counter run?
q = f3.view(np.uint64)
runlen = 0
for i in range(min(q.size, 4_000_000)):
    if q[i] == i: runlen = i+1
    else: break
print(" leading uint64 counter run length:", runlen)
