#!/usr/bin/env python3
"""Is a fine-tune's delta low-rank? If it is a merged LoRA, storing the delta is storing a rank-r matrix.

RESULT 90 codes a fine-tune's changed tensors as a dense bf16 difference and gets 48.5% of standalone.
But most fine-tunes on the Hub are LoRA merges: W = W0 + B A with rank 8-128. Then W - W0 is EXACTLY
rank r up to bf16 rounding, and a rank-r factorisation stores r*(m+n) numbers instead of m*n -- for a
4096x4096 tensor at r=16 that is 1.6% of the delta. The bf16 rounding breaks exactness; what remains
is whether the residual after the best rank-r approximation is a sparse +-1-ulp pattern (cheap) or
dense noise (the lever is dead).

Measured on the fine-tune pairs already in qgq/finetune.json: SVD of the delta on one full 2D tensor,
the singular value spectrum (does it fall off a cliff at some r?), and the cost of coding
(delta - rank_r_approx) in bf16 ulps against coding the delta itself."""
import json, sys
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map

def f32_to_bf16_bits(x):
    u = np.asarray(x, dtype=np.float32).view(np.uint32)
    # round-to-nearest-even on the dropped 16 bits
    lsb = (u >> 16) & 1
    u = (u + 0x7FFF + lsb) >> 16
    return u.astype(np.uint16)

def main():
    rows = json.load(open(f"{B}/qgq/finetune.json"))
    seen = set(); done = 0
    print(f"{'repo':28s} {'tensor':14s} {'shape':>12s} {'ident':>6s} {'sv[0]/sv[r]':>12s} {'r@1%':>5s} "
          f"{'delta bits':>10s} {'rank-r resid':>12s} {'gain':>6s}")
    for r in rows:
        if r["ident"] >= 0.999 or r["ident"] < 0.02: continue      # untouched or unrelated: not our case
        rid = r["rid"]
        if rid in seen: continue
        seen.add(rid)
        if done >= 5: break
        tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
        base = None
        for x in ((tags.get(rid) or {}).get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3: base = ":".join(p[2:]); break
        if not base: continue
        try:
            da = api(f"{H}/api/models/{rid}"); db = api(f"{H}/api/models/{base}")
            sa = sorted(s["rfilename"] for s in da.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            ma, ca = shard_map(rid, sa); mb, cb = shard_map(base, sb)
        except Exception:
            continue
        k = r["k"]
        if k not in ma or k not in mb: continue
        fa, fb = ma[k], mb[k]
        if fa not in ca: ca[fa] = st_header(rid, fa)
        if fb not in cb: cb[fb] = st_header(base, fb)
        ha, oa = ca[fa]; hb, ob = cb[fb]
        if not ha or not hb: continue
        A, Bm = ha[k], hb[k]
        if A["shape"] != Bm["shape"] or len(A["shape"]) != 2 or A["dtype"] != "BF16": continue
        m, n = A["shape"]
        if m * n * 2 > (96 << 20): continue                        # keep the SVD tractable
        ra = grab(rid, fa, oa + A["data_offsets"][0], m * n * 2)
        rb = grab(base, fb, ob + Bm["data_offsets"][0], m * n * 2)
        if ra is None or rb is None: continue
        wa = bf16_to_f32(np.frombuffer(ra.tobytes(), dtype=np.uint16)).astype(np.float64).reshape(m, n)
        wb = bf16_to_f32(np.frombuffer(rb.tobytes(), dtype=np.uint16)).astype(np.float64).reshape(m, n)
        delta = wa - wb
        ident = float(np.mean(wa == wb))
        try:
            U, S, Vt = np.linalg.svd(delta, full_matrices=False)
        except np.linalg.LinAlgError:
            continue
        S = np.maximum(S, 1e-30)
        total = float((S ** 2).sum())
        cum = np.cumsum(S ** 2) / total
        r1 = int(np.searchsorted(cum, 0.99)) + 1                    # rank capturing 99% of delta energy
        r_try = min(r1, 128, min(m, n))
        approx = (U[:, :r_try] * S[:r_try]) @ Vt[:r_try]
        # what does the residual cost, in bf16 ulps of the reconstructed weight?
        recon = wb + approx
        recon_bits = f32_to_bf16_bits(recon.astype(np.float32)).astype(np.int32).ravel()
        true_bits = np.frombuffer(ra.tobytes(), dtype=np.uint16).astype(np.int32)
        ulp_resid = (true_bits - recon_bits)
        delta_bits = (true_bits - np.frombuffer(rb.tobytes(), dtype=np.uint16).astype(np.int32))
        h_delta = h0(delta_bits); h_resid = h0(ulp_resid)
        factor_cost = r_try * (m + n) * 16 / (m * n)               # bits/weight to store B and A in bf16
        total_lr = h_resid + factor_cost
        gain = 1 - total_lr / max(h_delta, 1e-9)
        cliff = S[0] / S[min(r_try, len(S) - 1)]
        print(f"{rid[:28]:28s} {k.split('.')[-2][:14]:14s} {str((m, n)):>12s} {100*ident:5.1f}% "
              f"{cliff:12.1f} {r1:5d} {h_delta:10.3f} {total_lr:12.3f} {100*gain:5.1f}%")
        done += 1
    print("\n  r@1% = rank capturing 99% of the delta's energy. A merged LoRA shows a small r and a large")
    print("  sv[0]/sv[r] cliff; a full fine-tune shows r near min(m,n) and no cliff.")
    print("  'rank-r resid' = bits/weight for (bf16 ulp residual after rank-r) + (the factors themselves).")
    print("LOWRANK_DONE")

if __name__ == "__main__":
    main()
