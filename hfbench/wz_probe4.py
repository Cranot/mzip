import numpy as np

b3 = np.fromfile("/root/mzip-hfbench/weights/weights-fp32.bin", dtype=np.uint8)
w = b3.view(np.uint32)
m3 = (w & 0x7FFFFF).astype(np.uint32)

def ctz24(m):
    r = np.zeros(m.size, dtype=np.uint8); mask = m != 0; vv = m.copy()
    for i in range(24):
        add = mask & ((vv & 1) == 0); r[add] += 1; vv = np.where(add, vv >> 1, vv)
    r[~mask] = 24; return r

t = ctz24(m3); lowp = (t >= 13)
# find exact boundary of the low-precision region (in BYTES)
idx = np.flatnonzero(np.diff(lowp.astype(np.int8)) != 0)
print("first 12 transition element-indices (byte = idx*4):")
for i in idx[:12]:
    print("   elem %8d  byte %9d  -> %s" % (i + 1, (i + 1) * 4, "LOWP" if lowp[i + 1] else "FULL"))
print("last 6 transitions:", [(int(i + 1), int((i + 1) * 4)) for i in idx[-6:]])

# per-tensor view using the real header layout
segs = [("position_ids I64", 0, 4096), ("LN.bias F32", 4096, 5632), ("LN.weight F32", 5632, 7168),
        ("position_embeddings F32", 7168, 793600), ("token_type_emb F32", 793600, 796672),
        ("word_embeddings F32 (trunc)", 796672, 8388608)]
print()
print("%-30s %9s %9s %8s %8s %9s" % ("segment", "start", "bytes", "lowpfrac", "H0byte", "distinct"))
for name, a, bb in segs:
    seg = b3[a:bb]
    ws = w[a // 4:bb // 4]
    lp = lowp[a // 4:bb // 4]
    h = np.bincount(seg, minlength=256).astype(np.float64); p = h[h > 0] / h.sum()
    print("%-30s %9d %9d %8.4f %8.4f %9d" % (name, a, bb - a, float(lp.mean()),
          float(-(p * np.log2(p)).sum()), int(np.unique(ws).size)))

# word_embeddings rows: 384 floats = 1536 bytes per row
we = w[796672 // 4:]
rows = we.size // 384
wr = we[:rows * 384].reshape(rows, 384)
lpr = lowp[796672 // 4:][:rows * 384].reshape(rows, 384).mean(axis=1)
print()
print("word_embeddings rows=%d ; rows fully low-precision: %d" % (rows, int((lpr > 0.99).sum())))
ch = np.flatnonzero(np.diff((lpr > 0.5).astype(np.int8)) != 0)
print("row-level transitions at rows:", [int(x + 1) for x in ch[:10]], "...total", len(ch))
# are those rows identical / zero / duplicated?
print("row 5 uniq vals:", int(np.unique(wr[5]).size), " row 5 first 6 u32:", [hex(int(x)) for x in wr[5][:6]])
print("row 1100 uniq vals:", int(np.unique(wr[1100]).size), " row 1100 first 6:", [hex(int(x)) for x in wr[1100][:6]])
f = wr.view(np.float32) if wr.dtype == np.uint32 else None
fr = we.view(np.float32)[:rows * 384].reshape(rows, 384)
print("row  5 std %.6g mean %.6g" % (float(fr[5].std()), float(fr[5].mean())))
print("row 1100 std %.6g mean %.6g" % (float(fr[1100].std()), float(fr[1100].mean())))
print("dup rows among first 1017:", rows - int(np.unique(wr[:1017], axis=0).shape[0]))
