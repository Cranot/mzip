#!/usr/bin/env python3
"""The two readable Qwen3.6-size siblings with r~0 against the base: are they (a) each other's copies
(a second root), (b) the base's weights permuted (same value multiset, different order), or (c) different
weights altogether (different distribution)? One tensor, 128K weights, three tests."""
import sys, time
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
H = "https://huggingface.co"; PACE = 0.4; NEL = 131072
BASE = "Qwen/Qwen3.6-35B-A3B"; K = "model.language_model.layers.19.self_attn.q_proj.weight"
R = ["HarperJane/20260829-050111", "jacob-rojic/mirror-afgod1079-affine-5eqgpsdo6a-cp2700", "Shatoria/Affine-5ghntktyzq-hope3"]

def read(rid, k, nel=NEL):
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    fn = idx["weight_map"][k]; hb, off = st_header(rid, fn); time.sleep(PACE)
    v = hb[k]; raw = grab(rid, fn, off + v["data_offsets"][0], nel * 2); time.sleep(PACE)
    return np.frombuffer(raw.tobytes(), dtype=np.uint16), v["shape"]

w = {}
for rid in [BASE] + R:
    u, shape = read(rid, K); w[rid] = bf16_to_f32(u).astype(np.float64); print(f"{rid[:40]:40s} shape {shape} std {w[rid].std():.5f} mean {w[rid].mean():+.6f} kurtosis {((w[rid]-w[rid].mean())**4).mean()/w[rid].var()**2:.2f}")
ids = list(w)
print("\ncorrelation matrix (raw order):")
for a in ids:
    print(f"  {a[:24]:24s}", " ".join(f"{np.corrcoef(w[a], w[b])[0,1]:+.4f}" for b in ids))
print("\nsorted-value agreement (same multiset => permutation of the same weights): corr of sorted vectors, and max |diff| of sorted vectors")
for a in ids[1:]:
    sa, sb = np.sort(w[BASE]), np.sort(w[a])
    print(f"  base vs {a[:36]:36s} corr(sorted)={np.corrcoef(sa, sb)[0,1]:.6f}  max|diff|={np.abs(sa-sb).max():.5f}  exact-multiset={np.array_equal(sa, sb)}")
print("\nrow-wise: the slice is 64 rows x 2048; best |corr| of each of the first 8 base rows against ALL rows of the other file's slice")
for a in ids[1:]:
    A = w[BASE].reshape(-1, 2048); Bm = w[a].reshape(-1, 2048)
    best = [max(abs(np.corrcoef(A[i], Bm[j])[0, 1]) for j in range(Bm.shape[0])) for i in range(8)]
    print(f"  {a[:36]:36s} {' '.join(f'{x:.3f}' for x in best)}")
print("UNCORR_DONE")
