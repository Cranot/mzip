#!/usr/bin/env python3
"""HarperJane and jacob-rojic carry the base's value multiset in a different order. Which permutation?
Full tensor (layers.19 q_proj, 8192 x 2048 bf16 = 32 MiB) from the base and from each: exact multiset on
the full tensor; row permutation (each base row appears intact somewhere); column permutation; both;
else element-level. A structured permutation costs a few KB beside the parent; an element shuffle costs
more than the raw file."""
import sys, time
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
H = "https://huggingface.co"; PACE = 0.4
BASE = "Qwen/Qwen3.6-35B-A3B"; K = "model.language_model.layers.19.self_attn.q_proj.weight"
R = ["HarperJane/20260829-050111", "jacob-rojic/mirror-afgod1079-affine-5eqgpsdo6a-cp2700"]

def full(rid, k):
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    fn = idx["weight_map"][k]; hb, off = st_header(rid, fn); time.sleep(PACE)
    v = hb[k]; n = v["data_offsets"][1] - v["data_offsets"][0]
    raw = grab(rid, fn, off + v["data_offsets"][0], n); time.sleep(PACE)
    return np.frombuffer(raw.tobytes(), dtype=np.uint16).reshape(v["shape"])

def norm(M):
    M = M.astype(np.float64); M = M - M.mean(axis=1, keepdims=True); return M / (np.linalg.norm(M, axis=1, keepdims=True) + 1e-30)

A = full(BASE, K); Af = bf16_to_f32(A).astype(np.float64)
print(f"base {K} {A.shape}")
for rid in R:
    Bu = full(rid, K); Bf = bf16_to_f32(Bu).astype(np.float64)
    print(f"\n{rid}")
    print(f"  exact multiset on the full tensor: {np.array_equal(np.sort(A.ravel()), np.sort(Bu.ravel()))}   raw corr {np.corrcoef(Af.ravel(), Bf.ravel())[0,1]:+.5f}")
    # row permutation: for 64 random base rows, best cosine against all rows of B
    rng = np.random.default_rng(0); ri = rng.choice(A.shape[0], 64, replace=False)
    nb = norm(Bf); best = (norm(Af[ri]) @ nb.T).max(axis=1)
    print(f"  row match: {np.sum(best > 0.9999)}/64 base rows found intact in B (best cos min {best.min():.4f} median {np.median(best):.4f})")
    ci = rng.choice(A.shape[1], 64, replace=False)
    nbc = norm(Bf.T); bestc = (norm(Af.T[ci]) @ nbc.T).max(axis=1)
    print(f"  column match: {np.sum(bestc > 0.9999)}/64 base columns found intact in B (best cos min {bestc.min():.4f} median {np.median(bestc):.4f})")
    # row-sorted match: same rows as multisets? (row perm + within-row column perm)
    sa = np.sort(A, axis=1); sb = np.sort(Bu, axis=1)
    import hashlib
    hb_ = {hashlib.md5(sb[j].tobytes()).hexdigest() for j in range(sb.shape[0])}
    hits = sum(1 for i in ri if hashlib.md5(sa[i].tobytes()).hexdigest() in hb_)
    print(f"  row-multiset match: {hits}/64 base rows exist in B as the same multiset (=> row perm + column perm, or global column perm)")
    # global column permutation test: is there a single column permutation p with B[:, j] == A[:, p[j]]? use sorted column signature
    sac = {hashlib.md5(np.sort(A[:, j]).tobytes()).hexdigest(): j for j in range(A.shape[1])}
    colhits = sum(1 for j in range(Bu.shape[1]) if hashlib.md5(np.sort(Bu[:, j]).tobytes()).hexdigest() in sac)
    print(f"  column-multiset match: {colhits}/{Bu.shape[1]} B columns exist in A as the same multiset")
    # transposition / reshape? compare B with A.T reshaped
    if A.size == Bu.size:
        print(f"  corr(B, A.T.reshape): {np.corrcoef(Bf.ravel(), Af.T.reshape(Bf.shape).ravel())[0,1]:+.5f}   corr(B, A reversed): {np.corrcoef(Bf.ravel(), Af.ravel()[::-1])[0,1]:+.5f}")
    # head-block permutation: q_proj rows grouped by head (head_dim 128 or 256): permute 64-row blocks?
    for hd in (64, 128, 256):
        ba = norm(Af.reshape(-1, hd * A.shape[1])); bb = norm(Bf.reshape(-1, hd * A.shape[1]))
        m = (ba @ bb.T).max(axis=1)
        print(f"  {hd}-row block permutation: {np.sum(m > 0.9999)}/{len(m)} blocks found intact")
print("PERM_DONE")
