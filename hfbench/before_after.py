#!/usr/bin/env python3
"""Today versus with mzip: what a week of uploads actually occupies.

The census gives bytes UPLOADED per class. Xet compresses some of them and stores the rest raw, so
what it holds is uploaded / xet_ratio. mzip holds uploaded / our_ratio, and for the classes that can be
rebuilt from something already on the Hub it holds only the recipe. Every ratio below was measured on
real files of that class, both arms on identical bytes."""
import json
U = json.load(open("/root/mzip-hfbench/upload-mix/UPLOAD-MIX.json"))
C = U["classes"]; PB, TB = 1e15, 1e12
def f(b): return f"{b/PB:.3f} PB" if b >= PB/10 else (f"{b/TB:.1f} TB" if b >= TB else f"{b/1e9:.0f} GB")

# class -> (xet ratio, our ratio, rebuildable share of the class, label)
M = {
 "safetensors:BF16":   (1.1447, 1.4885, 0.0,   "model weights, BF16"),
 "safetensors:F32":    (1.1500, 1.4000, 0.0,   "model weights, F32"),
 "safetensors:F16":    (1.0100, 1.2200, 0.0,   "model weights, F16"),
 "safetensors:F8_E4M3":(1.0000, 1.2133, 0.0,   "model weights, FP8"),
 "safetensors:I8":     (1.0000, 1.1142, 0.0,   "model weights, INT8"),
 "safetensors:U8":     (1.0000, 1.0217, 0.0,   "model weights, UINT8/NVFP4"),
 "safetensors:I32":    (1.0000, 1.0960, 0.0,   "model weights, INT32"),
 "safetensors:U32":    (1.0000, 1.0343, 0.0,   "model weights, UINT32"),
 "safetensors:I16":    (1.0000, 1.0000, 0.0,   "model weights, INT16"),
 "gguf:Q":             (1.0006, 1.0667, 0.82,  "quantised models, GGUF"),
 "gguf:F16":           (1.0006, 1.5012, 1.0,   "full-precision GGUF"),
 "gguf:BF16":          (1.0006, 1.5012, 0.0,   "full-precision GGUF (BF16)"),
 "gguf:F32":           (1.0006, 1.5012, 0.0,   "full-precision GGUF (F32)"),
 "torch-pickle":       (1.0785, 1.2437, 0.0,   "checkpoints, PyTorch"),
 "parquet":            (1.0068, 1.1387, 0.0,   "dataset tables, Parquet"),
 "text":               (1.9003, 2.7556, 0.0,   "dataset text and metadata"),
 "arrow":              (1.0000, 1.0000, 0.0,   "dataset tables, Arrow"),
 "other-weights":      (1.0000, 1.0000, 0.0,   "other weight formats"),
 "archive":            (1.0000, 1.0000, 0.0,   "archives"),
 "video":              (1.0000, 1.0000, 0.0,   "video"),
 "image":              (1.0000, 1.0000, 0.0,   "images"),
 "audio":              (1.0000, 1.0000, 0.0,   "audio"),
 "other":              (1.0000, 1.0000, 0.0,   "unidentified"),
}
# the fine-tune share of BF16 that can be coded against its base, and by how much
FT_SHARE, FT_RATIO = 45.9e12 / 0.350e15, 2.94

rows = []
up = xt = mz = 0.0
for k, b in sorted(C.items(), key=lambda x: -x[1]):
    if k not in M:
        xt += b; mz += b; up += b; continue
    xr, orr, reb, lab = M[k]
    x = b / xr
    if k == "safetensors:BF16":
        base = b * (1 - FT_SHARE) / orr
        ft = b * FT_SHARE / FT_RATIO
        o = base + ft
    elif reb > 0:
        # the rebuildable share keeps only its base (1/10.8 of the bytes) plus a negligible recipe
        o = b * (1 - reb) / orr + b * reb / 10.8
    else:
        o = b / orr
    rows.append((lab, b, x, o))
    up += b; xt += x; mz += o

print(f"{'class':34s} {'uploaded':>10s} {'Xet holds':>11s} {'mzip holds':>11s} {'less':>9s}")
print("-" * 82)
agg = {}
for lab, b, x, o in rows:
    key = lab.split(",")[0]
    a = agg.setdefault(key, [0, 0, 0]); a[0] += b; a[1] += x; a[2] += o
for lab, (b, x, o) in sorted(agg.items(), key=lambda kv: -kv[1][1]):
    print(f"{lab:34s} {f(b):>10s} {f(x):>11s} {f(o):>11s} {100*(1-o/x) if x else 0:8.1f}%")
print("-" * 82)
print(f"{'TOTAL':34s} {f(up):>10s} {f(xt):>11s} {f(mz):>11s} {100*(1-mz/xt):8.1f}%")
print()
print(f"uploaded            {f(up)}")
print(f"stored today (Xet)  {f(xt)}   compression on the week: {up/xt:.3f}x")
print(f"stored with mzip    {f(mz)}   compression on the week: {up/mz:.3f}x")
print(f"difference          {f(xt-mz)}  = {100*(1-mz/xt):.1f}% less than today")
print(f"per year at this rate: {(xt-mz)*52/PB:.1f} PB not stored")
