#!/usr/bin/env python3
"""modelprint: a hidden-basis-invariant canonical form and fingerprint for transformer weights.

A transformer's function is unchanged when its residual-stream dimensions are relabelled (R127: two Hub
uploads were the base under such a relabelling, r ~ 0 against it, 77% byte-identical once undone). A
content-addressed store that chunks bytes in storage order cannot dedup or delta such copies, and a
correlation-based lineage check calls them unrelated.

Canonical form: for one layer L, take the input_layernorm weight g (one value per hidden dim, permuted by
the same p as every projection's input columns) and order the hidden dims by (g_j, ||W_q[:, j]||). Every
input-side projection of that layer is re-ordered by the same key. The result is byte-comparable across
copies with NO pairwise search -- hashable, chunkable, delta-able. Ties in g are broken by the column
norm; the tie rate is measured and printed. Scale attacks (W[:, j] * c_j with g_j / c_j) are undone by
normalising each column to unit norm before keying -- also measured.

Fingerprint: the canonical order's key vector itself (2048 x 2 numbers) plus the canonical-form hash of
a 64-row slice: a few KB that identifies a model up to relabelling.

Usage: modelprint.py canon <base> <other> [layer]   -- byte agreement after canonicalising each side alone
       modelprint.py search <base> <other> [layer]  -- the pairwise column search (R127), for comparison"""
import sys, time, hashlib
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, bf16_to_f32
from hfget import get_json
from kquant_given import h0
H = "https://huggingface.co"; PACE = 0.4; ROWS = 64

def index_of(rid):
    idx = get_json(f"{H}/{rid}/resolve/main/model.safetensors.index.json"); time.sleep(PACE)
    return (idx or {}).get("weight_map", {})

def read(rid, wm, k, rows=None, _cache={}):
    fn = wm[k]
    if (rid, fn) not in _cache: _cache[(rid, fn)] = st_header(rid, fn); time.sleep(PACE)
    hb, off = _cache[(rid, fn)]; v = hb[k]; shape = v["shape"]
    n = (v["data_offsets"][1] - v["data_offsets"][0]) if rows is None or len(shape) < 2 else rows * shape[1] * 2
    raw = grab(rid, fn, off + v["data_offsets"][0], n); time.sleep(PACE)
    u = np.frombuffer(raw.tobytes(), dtype=np.uint16)
    return u.reshape(-1, shape[1]) if len(shape) == 2 else u

def layer_tensors(wm, L):
    pre = [k for k in wm if f".layers.{L}." in k and ".layers." in k]
    norm = next((k for k in pre if k.endswith("input_layernorm.weight")), None)
    proj = sorted(k for k in pre if any(k.endswith(s) for s in ("q_proj.weight", "k_proj.weight", "v_proj.weight", "in_proj_qkv.weight", "gate_proj.weight", "up_proj.weight")))
    return norm, proj

def canon_key(g, W, unit=False):
    """key per hidden dim: (norm weight, column norm of the first projection); unit=True keys on the normalised column instead"""
    Wf = bf16_to_f32(W).astype(np.float64); cn = np.linalg.norm(Wf, axis=0)
    gf = bf16_to_f32(g).astype(np.float64)
    if unit:  # scale-invariant: key on g * cn (invariant to W[:,j]*c, g/c) and on the sorted normalised column's top value
        return np.lexsort((Wf.max(axis=0) / (cn + 1e-30), gf * cn))
    return np.lexsort((cn, gf))

def canonical(rid, L, unit=False):
    wm = index_of(rid); norm, proj = layer_tensors(wm, L)
    if not norm or not proj: return None
    g = read(rid, wm, norm); W0 = read(rid, wm, proj[0], ROWS)
    order = canon_key(g, W0, unit)
    gf = bf16_to_f32(g); ties = 1 - len(np.unique(gf)) / len(gf)
    out = {"order": order, "g": g[order], "ties_in_g": ties, "proj": {}}
    for k in proj[:3]:
        W = read(rid, wm, k, ROWS); out["proj"][k] = W[:, order]
    out["hash"] = hashlib.sha256(b"".join(v.tobytes() for v in out["proj"].values())).hexdigest()[:16]
    return out

def compare_canon(a, b, label):
    print(f"\n{label}")
    print(f"  ties in norm weight: {100*a['ties_in_g']:.1f}% / {100*b['ties_in_g']:.1f}% of dims share a value with another dim (bf16)")
    print(f"  canonical hash: {a['hash']} vs {b['hash']}")
    for k in a["proj"]:
        if k not in b["proj"]: continue
        d = b["proj"][k].astype(np.int32) - a["proj"][k].astype(np.int32)
        print(f"  {k.split('.')[-2]:12s} exact {100*np.mean(d==0):5.1f}%  +-1ulp {100*np.mean(np.abs(d)==1):5.1f}%  h0 {h0(d.ravel()):.3f} b/w")
    dg = b["g"].astype(np.int32) - a["g"].astype(np.int32)
    print(f"  norm weight  exact {100*np.mean(dg==0):5.1f}%")

def search(base, other, L):
    wm_a, wm_b = index_of(base), index_of(other); _, pa = layer_tensors(wm_a, L); _, pb = layer_tensors(wm_b, L)
    k = pa[0]; A = read(base, wm_a, k, ROWS); Bm = read(other, wm_b, k, ROWS)
    def nz(M):
        M = bf16_to_f32(M).astype(np.float64); M = M - M.mean(axis=0, keepdims=True); return M / (np.linalg.norm(M, axis=0, keepdims=True) + 1e-30)
    S = nz(A).T @ nz(Bm); p = S.argmax(axis=0); m = S.max(axis=0)
    d = Bm.astype(np.int32) - A[:, p].astype(np.int32)
    print(f"\nsearch {other} vs {base} layer {L}: columns matched (cos>0.999) {100*np.mean(m>0.999):.1f}%, exact after un-permute {100*np.mean(d==0):.1f}%, h0 {h0(d.ravel()):.3f}")

if __name__ == "__main__":
    mode, base, other = sys.argv[1], sys.argv[2], sys.argv[3]; L = int(sys.argv[4]) if len(sys.argv) > 4 else 19
    if mode == "search": search(base, other, L)
    else:
        for unit in (False, True):
            a, b = canonical(base, L, unit), canonical(other, L, unit)
            if a and b: compare_canon(a, b, f"canonical form ({'scale-invariant key' if unit else 'plain key: (g, column norm)'}) -- {other} vs {base}, layer {L}")
    print("MODELPRINT_DONE")
