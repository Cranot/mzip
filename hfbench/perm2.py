#!/usr/bin/env python3
"""The scrambled copies keep every COLUMN of q_proj intact (cos 1.0000) but not byte-exact: B[:, j] =
c_j * A[:, p(j)]. Recover p and c; ask whether c is 1 / a power of two / arbitrary; measure the residual
after undoing the scale and re-rounding to bf16 (the true cost of storing the scrambled file against the
base); and test whether the same p, c apply to the layer's k_proj (a hidden-dimension obfuscation that
leaves the model's function unchanged must apply the same map to every consumer of the residual stream)."""
import sys, time
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from kquant_given import h0
H = "https://huggingface.co"; PACE = 0.4
BASE = "Qwen/Qwen3.6-35B-A3B"; RID = sys.argv[1] if len(sys.argv) > 1 else "HarperJane/20260829-050111"
KQ = "model.language_model.layers.19.self_attn.q_proj.weight"; KK = "model.language_model.layers.19.self_attn.k_proj.weight"
KN = "model.language_model.layers.19.input_layernorm.weight"

def full(rid, k):
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    fn = idx["weight_map"][k]; hb, off = st_header(rid, fn); time.sleep(PACE)
    v = hb[k]; n = v["data_offsets"][1] - v["data_offsets"][0]
    raw = grab(rid, fn, off + v["data_offsets"][0], n); time.sleep(PACE)
    return np.frombuffer(raw.tobytes(), dtype=np.uint16).reshape(v["shape"])

def f32_to_bf16(x):
    u = x.astype(np.float32).view(np.uint32); return ((u + 0x7FFF + ((u >> 16) & 1)) >> 16).astype(np.uint16)

def norm(M):
    M = M.astype(np.float64); M = M - M.mean(axis=0, keepdims=True); return M / (np.linalg.norm(M, axis=0, keepdims=True) + 1e-30)

A = full(BASE, KQ); Bu = full(RID, KQ); Af = bf16_to_f32(A).astype(np.float64); Bf = bf16_to_f32(Bu).astype(np.float64)
S = norm(Af).T @ norm(Bf)                       # (2048 base cols) x (2048 B cols)
p = S.argmax(axis=0)                            # B column j <- base column p[j]
print(f"p is a permutation: {len(set(p.tolist())) == len(p)}; max cos per column min {S.max(axis=0).min():.6f}")
c = np.array([np.median(Bf[:, j][Af[:, p[j]] != 0] / Af[:, p[j]][Af[:, p[j]] != 0]) for j in range(len(p))])
print(f"scale c: min {c.min():.5f} max {c.max():.5f} median {np.median(c):.5f}; ==1 exactly: {np.sum(c == 1.0)}/{len(c)}; power of two: {np.sum(np.abs(np.log2(np.abs(c)) - np.round(np.log2(np.abs(c)))) < 1e-9)}/{len(c)}; sign flips: {np.sum(c < 0)}")
print(f"  c samples: {np.round(c[:8], 5).tolist()}")
print(f"  is p the identity on any column? {np.sum(p == np.arange(len(p)))}/{len(p)}; is p a shift? {np.sum((p - np.arange(len(p))) == (p[0] - 0))}/{len(p)}")
# residual after undoing p and c, re-rounded to bf16
rec = f32_to_bf16(Af[:, p] * c)
d = Bu.astype(np.int32) - rec.astype(np.int32)
print(f"reconstruction A[:,p]*c -> bf16 vs B: exact {100*np.mean(d==0):.2f}%, +-1 ulp {100*np.mean(np.abs(d)==1):.2f}%, h0(residual) {h0(d.ravel()):.3f} b/w, max|d| {np.abs(d).max()}")
# without scale (permutation only)
d0 = Bu.astype(np.int32) - A[:, p].astype(np.int32)
print(f"permutation only (no scale): exact {100*np.mean(d0==0):.2f}%, h0 {h0(d0.ravel()):.3f} b/w")
# same map on k_proj and on the input layernorm
Ak = full(BASE, KK); Bk = full(RID, KK)
dk = Bk.astype(np.int32) - f32_to_bf16(bf16_to_f32(Ak).astype(np.float64)[:, p] * c).astype(np.int32)
print(f"k_proj with the SAME p, c: exact {100*np.mean(dk==0):.2f}%, h0 {h0(dk.ravel()):.3f} b/w   (raw corr {np.corrcoef(bf16_to_f32(Ak).ravel().astype(np.float64), bf16_to_f32(Bk).ravel().astype(np.float64))[0,1]:+.4f})")
An = full(BASE, KN); Bn = full(RID, KN)
an = bf16_to_f32(An).astype(np.float64); bn = bf16_to_f32(Bn).astype(np.float64)
print(f"input_layernorm: base std {an.std():.4f}, B std {bn.std():.4f}; corr(B, base[p]) {np.corrcoef(bn, an[p])[0,1]:+.5f}; corr(B, base[p]/c) {np.corrcoef(bn, an[p]/c)[0,1]:+.5f}; corr(B, base) {np.corrcoef(bn, an)[0,1]:+.5f}")
print("PERM2_DONE")
