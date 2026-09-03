#!/usr/bin/env python3
"""Is an FP8 model a deterministic cast of its base? Second attempt, with both errors fixed.

The first attempt failed twice over and the falsification caught both: the E4M3 encoder mishandled
subnormals (14 of 240 patterns), and the scale was fitted as one value per tensor when real checkpoints
store one per 128x128 BLOCK -- a [5120, 17408] weight carries a [40, 136] scale tensor.

Both are fixed here. The encoder is built as an exact nearest-representable mapping derived from the
decoder itself, so it cannot disagree with the format's definition; ties go to even, as IEEE rounding
requires. The scale is read from the file and broadcast over its blocks."""
import json, os, struct, subprocess, sys
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/fp8"
sys.path.insert(0, B)
from fp8_given_base import header, api, grab, bf16_to_f32

def e4m3_to_f32(b):
    b = np.asarray(b, dtype=np.uint8)
    sign = np.where(b >> 7 == 1, -1.0, 1.0)
    e = ((b >> 3) & 0x0F).astype(np.int32)
    m = (b & 0x07).astype(np.float64)
    val = np.where(e == 0, m / 8.0 * 2.0**-6, (1.0 + m / 8.0) * np.power(2.0, e - 7.0))
    return (sign * val).astype(np.float64)

# the exact table of finite E4M3 values, from the decoder, so encoder and decoder cannot disagree
_codes = np.array([c for c in range(256) if ((c >> 3) & 0x0F) != 0x0F], dtype=np.uint8)
_vals = e4m3_to_f32(_codes)
_order = np.argsort(_vals, kind="stable")
_sv = _vals[_order]; _sc = _codes[_order]

def f32_to_e4m3(x):
    """nearest representable E4M3, ties to even, saturating"""
    x = np.asarray(x, dtype=np.float64).ravel()
    x = np.clip(x, _sv[0], _sv[-1])
    i = np.searchsorted(_sv, x)
    i = np.clip(i, 1, len(_sv) - 1)
    lo, hi = _sv[i - 1], _sv[i]
    dlo, dhi = x - lo, hi - x
    take_hi = dhi < dlo
    tie = dhi == dlo
    if tie.any():                     # ties to even mantissa
        even_hi = (_sc[i] & 1) == 0
        take_hi = np.where(tie, even_hi, take_hi)
    return np.where(take_hi, _sc[i], _sc[i - 1]).astype(np.uint8)

def main():
    rid = sys.argv[1] if len(sys.argv) > 1 else "TelperionAI/Qwen3.8-27B-FP8-block-AWQ"
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    base = None
    for x in (tags.get(rid) or {}).get("tags", []):
        if x.startswith("base_model:"):
            p = x.split(":")
            if len(p) >= 3: base = ":".join(p[2:]); break
    print(f"{rid}\n  base: {base}")
    if not base: print("  no declared base"); return
    d = api(f"{H}/api/models/{rid}")
    st = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    h, off = header(rid, st[0])
    f8 = [(k, v) for k, v in h.items() if isinstance(v, dict) and v.get("dtype") == "F8_E4M3"]
    name, meta = max(f8, key=lambda c: c[1]["data_offsets"][1] - c[1]["data_offsets"][0])
    skey = next((k for k in (name + "_scale", name + "_scale_inv",
                             name.replace(".weight", ".weight_scale")) if k in h), None)
    if not skey: print("  no scale tensor"); return
    smeta = h[skey]
    rows, cols = meta["shape"]; srows, scols = smeta["shape"]
    bh, bw = rows // srows, cols // scols
    print(f"  tensor {name}\n  shape {meta['shape']}, scale {smeta['shape']} -> blocks of {bh}x{bw}")

    db = api(f"{H}/api/models/{base}")
    stb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    hb = None
    for fn in stb[:6]:
        hb, offb = header(base, fn)
        if hb and name in hb: bfn = fn; break
        hb = None
    if not hb: print(f"  {name} not found in the base"); return
    mb = hb[name]
    if mb["shape"] != meta["shape"]: print(f"  shape differs: base {mb['shape']}"); return
    print(f"  base dtype {mb['dtype']}, same shape")

    nrow = min(bh * 4, rows)                       # a few block-rows is enough to decide
    q = grab(rid, st[0], off + meta["data_offsets"][0], nrow * cols)
    w = grab(base, bfn, offb + mb["data_offsets"][0], nrow * cols * 2)
    sc_all = grab(rid, st[0], off + smeta["data_offsets"][0],
                  (smeta["data_offsets"][1] - smeta["data_offsets"][0]))
    if q is None or w is None or sc_all is None: print("  fetch failed"); return
    q = q.reshape(nrow, cols)
    wf = bf16_to_f32(np.frombuffer(w.tobytes(), dtype=np.uint16)).reshape(nrow, cols).astype(np.float64)
    sdt = smeta["dtype"]
    sv = np.frombuffer(sc_all.tobytes(), dtype=np.uint16) if sdt == "BF16" else np.frombuffer(sc_all.tobytes(), dtype=np.float32)
    scale = (bf16_to_f32(sv) if sdt == "BF16" else sv).astype(np.float64).reshape(srows, scols)[:nrow // bh + 1]

    # broadcast the block scale over the slice and cast
    rep = np.repeat(np.repeat(scale, bh, axis=0), bw, axis=1)[:nrow, :cols]
    for label, xf in (("w / scale", wf / rep), ("w * scale", wf * rep)):
        pred = f32_to_e4m3(xf).reshape(nrow, cols)
        ex = float(np.mean(pred == q))
        near = float(np.mean(np.abs(pred.astype(np.int16) - q.astype(np.int16)) <= 1))
        print(f"  {label:12s} exact {100*ex:6.2f}%   within one code {100*near:6.2f}%")
    print("FP8B_DONE")

if __name__ == "__main__":
    main()
