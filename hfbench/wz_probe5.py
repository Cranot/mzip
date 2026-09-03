import json, struct, numpy as np

def load(path):
    with open(path, "rb") as f:
        nh = struct.unpack("<Q", f.read(8))[0]
        hdr = json.loads(f.read(nh))
    return path, 8 + nh, hdr

def get(path, base, hdr, name):
    a, b = hdr[name]["data_offsets"]
    return np.fromfile(path, dtype=np.uint8, count=b - a, offset=base + a)

def H(a, minlength=0):
    h = np.bincount(np.asarray(a).ravel(), minlength=minlength).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def Hjoint(v):
    _, iv = np.unique(v, return_inverse=True)
    return H(iv.astype(np.int64))

p, base, hdr = load("/root/mzip-hfbench/assets/smollm2.safetensors")
pairs = [("model.layers.0.mlp.up_proj.weight", "model.layers.1.mlp.up_proj.weight"),
         ("model.layers.0.mlp.gate_proj.weight", "model.layers.1.mlp.gate_proj.weight"),
         ("model.layers.0.self_attn.q_proj.weight", "model.layers.1.self_attn.q_proj.weight"),
         ("model.layers.0.mlp.up_proj.weight", "model.layers.0.mlp.gate_proj.weight")]
print("### BF16 cross-tensor delta / xor (entropy of the u16 element alphabet) ###")
for a, b in pairs:
    if a not in hdr or b not in hdr: print("  missing", a, b); continue
    A = get(p, base, hdr, a).view(np.uint16)
    B = get(p, base, hdr, b).view(np.uint16)
    n = min(A.size, B.size); A = A[:n]; B = B[:n]
    hA = Hjoint(A)
    hD = Hjoint((B - A).astype(np.uint16))
    hX = Hjoint((B ^ A).astype(np.uint16))
    print("  %-46s vs %-46s  H(B)=%.4f  H(B-A)=%.4f  H(B^A)=%.4f"
          % (a.replace("model.layers.", "L"), b.replace("model.layers.", "L"), Hjoint(B), hD, hX))

print()
print("### BF16 within-tensor 2-D structure: row vs column traversal ###")
T = get(p, base, hdr, "model.layers.0.mlp.up_proj.weight").view(np.uint16)
shape = hdr["model.layers.0.mlp.up_proj.weight"]["shape"]
print("  tensor shape", shape, "elems", T.size)
M = T.reshape(shape)
print("  H(row-major)=%.4f  H(col-major)=%.4f" % (Hjoint(M.ravel()), Hjoint(M.T.ravel())))
e = ((M.astype(np.uint32) >> 7) & 0xFF).astype(np.float64)
print("  exponent: per-row std of row-means %.5f ; per-col std of col-means %.5f ; overall std %.5f"
      % (float(e.mean(axis=1).std()), float(e.mean(axis=0).std()), float(e.std())))
# delta along rows vs columns
dr = Hjoint((np.diff(M.astype(np.int32), axis=1) & 0xFFFF).astype(np.uint16).ravel())
dc = Hjoint((np.diff(M.astype(np.int32), axis=0) & 0xFFFF).astype(np.uint16).ravel())
print("  H(delta along row)=%.4f  H(delta along col)=%.4f  (baseline H=%.4f)" % (dr, dc, Hjoint(M.ravel())))

print()
print("### FP32 (minilm) same checks on an encoder weight ###")
p2, base2, hdr2 = load("/root/mzip-hfbench/assets/minilm.safetensors")
A = get(p2, base2, hdr2, "encoder.layer.0.attention.self.query.weight").view(np.uint32)
B = get(p2, base2, hdr2, "encoder.layer.1.attention.self.query.weight").view(np.uint32)
n = min(A.size, B.size)
eA = ((A[:n] >> 23) & 0xFF).astype(np.uint8); eB = ((B[:n] >> 23) & 0xFF).astype(np.uint8)
print("  exponent H(A)=%.4f H(B)=%.4f H(B-A)=%.4f" % (H(eA, 256), H(eB, 256), H(((eB.astype(np.int16) - eA) & 0xFF).astype(np.uint8), 256)))
