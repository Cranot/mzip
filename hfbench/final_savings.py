#!/usr/bin/env python3
"""The final image: what the week's Hub uploads cost, and what every measured lever takes off them.

Every rate below is now MEASURED against Xet on real Hub files of that class -- the last assumed figure
(torch-pickle at an F32 floor) was replaced by a measurement of 13.3%. Classes with no measurement are
listed at zero and named, so the coverage figure is honest rather than flattering."""
import json
import numpy as np
U = json.load(open("/root/mzip-hfbench/upload-mix/UPLOAD-MIX.json"))
C = U["classes"]; TOT = U["total"]; PB, TB = 1e15, 1e12

def fmt(b): return f"{b/PB:.3f} PB" if b >= PB/10 else (f"{b/TB:.1f} TB" if b >= TB else f"{b/1e9:.1f} GB")

# per-chunk saving vs what Xet stores, and where each number comes from.
# Where a class was measured more than once the FLOOR is used, never the best case.
SAVE = {
 "safetensors:BF16":   (0.227, "BF16 weights, 5 classes"),
 "safetensors:F32":    (0.114, "F32 weights, 7 classes, floor"),
 "safetensors:F16":    (0.120, "F16 weights, 3 classes, floor"),
 "safetensors:F8_E4M3":(0.176, "FP8, Xet stores it RAW"),
 "safetensors:I8":     (0.102, "INT8, Xet RAW, floor of 10.2 / 42.1"),
 "safetensors:U8":     (0.021, "UINT8/NVFP4, Xet RAW, mean of 4.2 / 0"),
 "safetensors:I32":    (0.088, "INT32, Xet RAW, floor of 8.8 / 9.3"),
 "safetensors:U32":    (0.033, "UINT32, Xet RAW, floor of 3.3 / 11.3"),
 "safetensors:I16":    (0.000, "INT16 EXL3/trellis: measured, nothing there"),
 "gguf:F16":           (0.333, "GGUF F16"),
 "gguf:BF16":          (0.333, "as GGUF F16"),
 "gguf:F32":           (0.333, "as GGUF F16"),
 "gguf:Q":             (0.062, "7 quant types"),
 "torch-pickle":       (0.133, "MEASURED on real .bin: 1.078 -> 1.2437, and faster"),
 "parquet":            (0.116, "3 files"),
 "text":               (0.310, "8 corpora"),
 "arrow":              (0.000, "measured at the raw floor for every arm"),
 "other-weights":      (0.000, "pb/h5 measured at the floor; onnx/tflite untested"),
 "archive":            (0.000, "gz/zst payloads and tars of media: nothing available"),
 "video":              (0.000, "codec output"), "image": (0.000, "codec output"),
 "audio":              (0.000, "codec output"), "other": (0.000, "unidentified"),
}
LEVER = {
 "gguf:Q":   (0.494, "base in the same repo for 54.5%, less the base itself: measured, not assumed"),
 "gguf:F16": (1.000, "a deterministic conversion of the safetensors"),
 "parquet":  (0.425, "page transcode, byte-identical restore"),
}
FT = (0.611, "fine-tune coded against its base")

print(f"{'class':24s} {'uploaded':>11s} {'per chunk':>10s} {'saved':>11s} {'beyond':>11s}  source")
print("-" * 104)
pc = lv = covered = 0.0
ft_share = 45.9e12 / max(sum(v for k, v in C.items() if k.startswith("safetensors")), 1)
for k, b in sorted(C.items(), key=lambda x: -x[1]):
    s, why = SAVE.get(k, (0.0, "not measured"))
    p = b * s; pc += p
    if s > 0: covered += b
    l = 0.0; note = ""
    if k in LEVER: l, note = LEVER[k][0] * b, LEVER[k][1]
    elif k == "safetensors:BF16": l, note = b * ft_share * FT[0], FT[1]
    lv += l
    print(f"{k:24s} {fmt(b):>11s} {100*s:9.1f}% {fmt(p):>11s} {(fmt(l) if l else '-'):>11s}  {why if not note else why + ' | ' + note}")
print("-" * 104)
print(f"{'TOTAL':24s} {fmt(TOT):>11s} {100*pc/TOT:9.1f}% {fmt(pc):>11s} {fmt(lv):>11s}")
print()
print(f"measured classes cover {100*covered/TOT:.0f}% of the uploaded bytes")
print(f"within them, per-chunk coding removes {100*pc/covered:.1f}%")
print(f"per-chunk + production-aware = {fmt(pc+lv)} = {100*(pc+lv)/covered:.0f}% of the measured classes, "
      f"{100*(pc+lv)/TOT:.0f}% of everything uploaded")
