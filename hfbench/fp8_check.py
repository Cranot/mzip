#!/usr/bin/env python3
"""Falsify the instrument before trusting its null result.

The FP8-given-base probe returned 0.05-0.96% exact, which is either a real negative or a broken
encoder, and the two look identical from the outside. Two checks settle it:

  1. ROUND-TRIP. Decode all 256 E4M3 byte patterns to float and re-encode them. A correct encoder
     returns every pattern unchanged. This needs no network and no hypothesis.
  2. THE SCALE'S SHAPE. The probe fitted a single per-tensor scale. Real FP8 checkpoints store a scale
     tensor alongside the weights, often per output channel or per 128x128 block -- one repo is named
     "FP8-block". Print what the files actually contain, so the next attempt uses the right one."""
import json, os, struct, subprocess, sys
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from fp8_given_base import f32_to_e4m3, header, api, OUT

def e4m3_to_f32(b):
    b = np.asarray(b, dtype=np.uint8)
    sign = np.where(b >> 7 == 1, -1.0, 1.0)
    e = ((b >> 3) & 0x0F).astype(np.int32)
    m = (b & 0x07).astype(np.float64)
    sub = e == 0
    val = np.where(sub, m / 8.0 * 2.0**-6, (1.0 + m / 8.0) * np.power(2.0, e - 7.0))
    return (sign * val).astype(np.float32)

print("=== 1. does the encoder round-trip all 256 patterns?")
codes = np.arange(256, dtype=np.uint8)
finite = ~(((codes >> 3) & 0x0F) == 0x0F)          # 0x7F/0xFF are NaN in E4M3
f = e4m3_to_f32(codes)
back = f32_to_e4m3(f)
ok = back[finite] == codes[finite]
print(f"    {int(ok.sum())}/{int(finite.sum())} finite patterns survive decode+encode")
if not ok.all():
    bad = np.nonzero(~ok)[0][:12]
    for i in bad:
        c = codes[finite][i]
        print(f"      0x{c:02x} -> {e4m3_to_f32(np.array([c]))[0]:.8g} -> 0x{back[finite][i]:02x}")
    print("    the encoder is WRONG, so the earlier 0.05-0.96% says nothing about the hypothesis")
else:
    print("    encoder verified")

print("\n=== 2. what scale tensors do FP8 checkpoints actually carry?")
for rid in ("TelperionAI/Qwen3.8-27B-FP8-block-AWQ", "VitalyProtasov/Nemotron-3.5-Content-Safety-FP8",
            "eousphoros/qwen3.8-27b-yarn4-mixed-kl-128k-rope-fp8"):
    d = api(f"{H}/api/models/{rid}")
    if not d: print(f"  {rid}: unreachable"); continue
    st = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors")]
    if not st: print(f"  {rid}: no safetensors"); continue
    h, off = header(rid, sorted(st)[0])
    if not h: print(f"  {rid}: header unreadable"); continue
    f8 = [(k, v) for k, v in h.items() if isinstance(v, dict) and v.get("dtype") == "F8_E4M3"]
    sc = [(k, v) for k, v in h.items() if isinstance(v, dict) and "scale" in k.lower()]
    print(f"  {rid[:52]}")
    print(f"    F8 tensors {len(f8)}, scale tensors {len(sc)}")
    if f8:
        k, v = max(f8, key=lambda c: c[1]["data_offsets"][1] - c[1]["data_offsets"][0])
        print(f"    largest F8: {k}  shape {v['shape']}")
        for cand in (k + "_scale", k + "_scale_inv", k.replace(".weight", ".weight_scale"),
                     k.replace(".weight", ".weight_scale_inv")):
            if cand in h:
                print(f"    its scale: {cand}  shape {h[cand]['shape']}  dtype {h[cand]['dtype']}")
                break
        else:
            print(f"    no scale tensor matches that name; scale names present: {[s[0] for s in sc[:3]]}")
print("FP8_CHECK_DONE")
