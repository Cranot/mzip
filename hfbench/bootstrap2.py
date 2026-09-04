#!/usr/bin/env python3
"""The baseline, derived; and sampling intervals for rows B..E against it (RESULT 130).

finalnum2..10 carry `HELD_NOW = 1444.0` as a literal (2026-09-03) with no derivation in the record. Here
held-today is BUILT: per-class uploaded bytes / Xet's per-class ratio, minus what Xet's chunk dedup
removes (exact copies), with every ratio a measurement -- the BF16, F32, GGUF-Q and parquet ratios from
xet-core's own chunker + scheme choice + encoders run on this box (xetratio, R130), the rest from
before_after.py's table -- and the dedup shares from the census (R108 exact twins within the week) and
the root index (R128 exact copies of out-of-week roots). Then the rows, then 4,000 bootstrap replicates
over the week composition and the factor records."""
import json, sys, collections
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from upload_mix_report import klass, OUT, jl
import before_after as BA
B = "/root/mzip-hfbench"; TB = 1e12
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
rng = np.random.default_rng(7)

# per-class Xet ratio: xet-core measured where we hold files of the class, else the table
XET_MEASURED = {"safetensors:BF16": 1.151, "safetensors:F32": 1.086, "gguf:Q": 1.001, "gguf:F16": 1.001, "gguf:BF16": 1.001, "gguf:F32": 1.001, "parquet": 1.001}
M = {k: (XET_MEASURED.get(k, v[0]), v[1], v[2], v[3]) for k, v in BA.M.items()}
M["safetensors:F32"] = (M["safetensors:F32"][0], 1.229, 0.0, M["safetensors:F32"][3])   # Fano on gpt2-f32, bench_pe, R130
FT_SHARE, FT_RATIO = BA.FT_SHARE, BA.FT_RATIO
SAVE = {"safetensors:BF16": 0.227, "safetensors:F32": 0.114, "safetensors:F16": 0.120, "safetensors:F8_E4M3": 0.176, "safetensors:I8": 0.102, "safetensors:U8": 0.021,
        "safetensors:I32": 0.088, "safetensors:U32": 0.033, "torch-pickle": 0.133, "parquet": 0.116, "text": 0.310}   # final_savings.py, measured floors

kinds = {}
for kind in ("models", "datasets"):
    pop = json.load(open(f"{OUT}/population_{kind}.json")); samp = jl(f"{OUT}/sample_{kind}.jsonl")
    scale = pop["n"] / len(samp); rows = []
    for r in samp:
        c = collections.Counter()
        if r.get("ok"):
            for k, b in (r.get("bytes_in_window") or {}).items(): c[klass(k)] += b
        rows.append(c)
    kinds[kind] = (rows, scale)
classes = sorted({k for rows, _ in kinds.values() for c in rows for k in c})
mats = {kind: (np.array([[c.get(k, 0.0) for k in classes] for c in rows]), scale) for kind, (rows, scale) in kinds.items()}
def class_totals(idx=None):
    tot = np.zeros(len(classes))
    for kind, (Mx, scale) in mats.items():
        tot += (Mx if idx is None else Mx[idx[kind]]).sum(axis=0) * scale
    return dict(zip(classes, tot / TB))

R4 = json.load(open(f"{B}/qgq/resolverate4.json")); R3 = json.load(open(f"{B}/qgq/resolverate3.json")); CC = json.load(open(f"{B}/qgq/convcheck.json"))
L5 = json.load(open(f"{B}/qgq/lineage5.json")); RI = json.load(open(f"{B}/qgq/rootindex_result.json")); PT = json.load(open(f"{B}/qgq/permtest.json"))
XET_BF16 = 1 / M["safetensors:BF16"][0]; XBITS = 16 / M["safetensors:BF16"][0]
SH_PAR, SH_MULTI, SH_SINGLE = 0.805, 0.172, 0.023
FT_POOL, FT_CHANGED, FT_COST = 0.103, 13 / 24.0, 0.485
SOLO_KEYED_TB, SOLO_GATED_TB = 3.90, 9.20; TOT_W = L5["tot_w"] / TB
PROXY = np.array([12.04, 1.635, 1.635, 4.30, 9.41, 7.17])
clusters = [c for c in L5["clusters"] if len(c) >= 7 and isinstance(c[6], dict)]
pw = np.array([len(c[6].get("perturbed", [])) for c in clusters], float); pert_bytes = pw / max(pw.sum(), 1) * L5["pert_b"]
ew = np.array([len(c[6].get("exact", [])) for c in clusters], float); exact_bytes = ew / max(ew.sum(), 1) * L5["exact_b"]
found = RI["found"]; seen = {x["rid"] for x in found}
found = found + [dict(rid=x["rid"], kind="fine-tune", h=x["h"], bytes=x["bytes"]) for x in PT["hits"] if x["conf"] == "confirmed" and x["ident_after"] < 0.999 and x["rid"] not in seen]
fb = np.array([x["bytes"] for x in found]); fh = np.array([x["h"] for x in found])
fo = np.array([x["kind"] in ("fine-tune", "perturbed") for x in found]); fx = np.array([x["kind"] == "exact" for x in found])
probed_b = RI["pb"]

def model(C, rho, pert, exact_in, exact_out, oursD, costD, costE):
    GG = sum(v for k, v in C.items() if k.startswith("gguf")); ST = sum(v for k, v in C.items() if k.startswith("safetensors"))
    WEIGHTS = GG + ST + C.get("torch-pickle", 0) + C.get("other-weights", 0)
    chunk = sum(b / M[k][0] if k in M else b for k, b in C.items())          # Xet per-chunk only
    dedup = WEIGHTS * (exact_in + exact_out)                                  # Xet's cross-repo exact-copy dedup
    held_now = chunk - dedup
    # non-GGUF per-chunk saving from final_savings.py's documented floors (fraction of what Xet holds), plus its parquet transcode lever
    other = sum((b * (1 - FT_SHARE) if k == "safetensors:BF16" else b) / M[k][0] * SAVE.get(k, 0.0) for k, b in C.items() if k in M and not k.startswith("gguf"))
    other += C.get("parquet", 0.0) * 0.425
    par = GG * SH_PAR
    g = par * rho * 0.001 + par * (1 - rho) * 0.97 + GG * SH_MULTI * 0.252 + GG * SH_SINGLE * 0.97
    ft = ST * FT_POOL * FT_CHANGED * (1 - FT_COST)
    tw = WEIGHTS * pert * XET_BF16 * (1 - 0.264)
    d = WEIGHTS * (SOLO_KEYED_TB / TOT_W) * oursD * XET_BF16 * (1 - costD)
    e = WEIGHTS * (SOLO_GATED_TB / TOT_W) * XET_BF16 * (1 - costE)
    b_ = held_now - other - (GG - g) - ft
    return held_now, chunk, dedup, np.array([b_, b_ - tw, b_ - tw - d, b_ - tw - d - e])

def factors(j=None, r4=None, r3=None, cc=None, pj=None, ej=None, xj=None):
    rho = (R4["ok_b"] / R4["all_b"] if r4 is None else r4) * (R3["correct"] / (R3["correct"] + R3["wrong"]) if r3 is None else r3) * (CC["ok"] / CC["att"] if cc is None else cc)
    pert = (L5["pert_b"] if pj is None else pert_bytes[pj].sum()) / L5["tot_w"]
    exact_in = (L5["exact_b"] if ej is None else exact_bytes[ej].sum()) / L5["tot_w"]
    jj = np.arange(len(fb)) if j is None else j
    ob = fb[jj][fo[jj]].sum(); xb = fb[jj][fx[jj]].sum()
    exact_out = (xb / probed_b) * SOLO_KEYED_TB / TOT_W
    oursD = ob / probed_b; costD = (fb[jj][fo[jj]] * np.minimum(1.0, fh[jj][fo[jj]] * 1.07 / XBITS)).sum() / max(ob, 1)
    costE = np.mean(np.minimum(1.0, (PROXY if xj is None else PROXY[xj]) / XBITS))
    return rho, pert, exact_in, exact_out, oursD, costD, costE

C0 = class_totals(); held0, chunk0, dedup0, pt = model(C0, *factors())
print(f"BASELINE, derived: uploaded {sum(C0.values()):.0f} TB; Xet per-chunk holds {chunk0:.1f} TB; exact-copy dedup {dedup0:.1f} TB "
      f"(within-week {100*L5['exact_b']/L5['tot_w']:.1f}% + out-of-week {100*factors()[3]:.2f}% of {sum(v for k,v in C0.items() if k.startswith(('gguf','safetensors','torch','other-w'))):.0f} TB weight bytes)")
print(f"  held today = {held0:.1f} TB   (the literal in finalnum2..10: 1,444.0)")
print("  measured Xet ratios in use: " + ", ".join(f"{k} {v}" for k, v in XET_MEASURED.items()))
print("\nrows against the derived baseline (TB saved unchanged; the percentage moves with the baseline):")
for k, lbl in enumerate("BCDE"): print(f"  {lbl}  held {pt[k]:8.1f} TB   less {100*(1-pt[k]/held0):5.1f}%   {(held0-pt[k])*52/1000:5.2f} PB/yr")

res = np.zeros((REPS, 4)); H = np.zeros(REPS)
for i in range(REPS):
    idx = {kind: rng.integers(0, Mx.shape[0], Mx.shape[0]) for kind, (Mx, _) in mats.items()}
    r4 = rng.binomial(R4["att"], R4["ok"] / R4["att"]) / R4["att"] * (R4["ok_b"] / R4["all_b"]) / (R4["ok"] / R4["att"])
    n3 = R3["correct"] + R3["wrong"]; r3 = rng.binomial(n3, R3["correct"] / n3) / n3
    cc = rng.binomial(CC["att"], CC["ok"] / CC["att"]) / CC["att"]
    f = factors(j=rng.integers(0, len(fb), len(fb)), r4=r4, r3=r3, cc=cc, pj=rng.integers(0, len(clusters), len(clusters)), ej=rng.integers(0, len(clusters), len(clusters)), xj=rng.integers(0, 6, 6))
    H[i], _, _, res[i] = model(class_totals(idx), *f)
pct = 100 * (1 - res / H[:, None])
print(f"\n{REPS} replicates -- held today 5-95%: {np.percentile(H,5):.0f}-{np.percentile(H,95):.0f} TB")
print(f"{'row':62s} {'point':>7s} {'5%':>7s} {'95%':>7s}   {'TB/wk 5%':>9s} {'95%':>7s}")
for k, lbl in enumerate(("B  measured (recipe route x correct x converts)", "C  B + undeclared twins", "D  C + keyed solo BF16 resolved to popular roots (measured)", "E  D + gated Qwen3.6-35B-A3B cluster via readable siblings [I]")):
    sv = H - res[:, k]
    print(f"{lbl:62s} {100*(1-pt[k]/held0):6.1f}% {np.percentile(pct[:,k],5):6.1f}% {np.percentile(pct[:,k],95):6.1f}%   {np.percentile(sv,5):8.0f} {np.percentile(sv,95):7.0f}")
# width attribution for row B
w = {}
for name in ("composition", "factors"):
    v = []
    for i in range(1000):
        if name == "composition":
            idx = {kind: rng.integers(0, Mx.shape[0], Mx.shape[0]) for kind, (Mx, _) in mats.items()}; h, _, _, r = model(class_totals(idx), *factors())
        else:
            r4 = rng.binomial(R4["att"], R4["ok"] / R4["att"]) / R4["att"] * (R4["ok_b"] / R4["all_b"]) / (R4["ok"] / R4["att"]); r3 = rng.binomial(n3, R3["correct"] / n3) / n3; cc = rng.binomial(CC["att"], CC["ok"] / CC["att"]) / CC["att"]
            h, _, _, r = model(C0, *factors(r4=r4, r3=r3, cc=cc, pj=rng.integers(0, len(clusters), len(clusters)), ej=rng.integers(0, len(clusters), len(clusters))))
        v.append(100 * (1 - r[0] / h))
    w[name] = (np.percentile(v, 5), np.percentile(v, 95))
print("\nrow B width by source: " + "; ".join(f"{k} {a:.1f}-{b:.1f}%" for k, (a, b) in w.items()))
print("BOOTSTRAP2_DONE")
