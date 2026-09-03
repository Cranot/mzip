#!/usr/bin/env python3
"""Is an FP8 model a deterministic cast of its BF16 base?

Category 3 -- files that are the output of a program over something the Hub already holds -- has only
been mined for one program, llama.cpp's quantiser. The 8-bit and 32-bit integer safetensors are 87 TB of
the week and were treated as ordinary bytes, but they are quantisations too, and the simplest of them is
the simplest possible transformation:

    w_fp8 = round_to_fp8( w_bf16 / scale )

with the scale stored in the same file, usually as `<tensor>.weight_scale` or `.weight_scale_inv`. No
calibration data, no search, no iteration -- unlike GPTQ or AWQ, and unlike an imatrix K-quant. If it
holds, an FP8 model costs its scales plus a recipe rather than its weights, and the same argument covers
INT8 with per-channel scales.

This finds repos that declare a BF16 base and publish FP8 weights, fetches the SAME tensor from both as
aligned byte ranges, applies the cast, and counts exact matches."""
import json, os, struct, subprocess, sys, time
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/fp8"
os.makedirs(OUT, exist_ok=True)
SLICE = 4 * 1024 * 1024

def sh(a, t=600): return subprocess.run(a, capture_output=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout.decode())
    except Exception: return None

def header(repo, fn, window=16*1024*1024):
    tmp = f"{OUT}/h.tmp"
    sh(["curl", "-sSL", "--max-time", "240", "-r", f"0-{window-1}", "-o", tmp, f"{H}/{repo}/resolve/main/{fn}"])
    try:
        b = open(tmp, "rb").read()
        n = struct.unpack("<Q", b[:8])[0]
        if n + 8 > len(b): return None, None
        return json.loads(b[8:8+n].decode()), 8 + n
    except Exception:
        return None, None

def grab(repo, fn, start, length):
    tmp = f"{OUT}/s.bin"
    sh(["curl", "-sSL", "--max-time", "600", "-r", f"{start}-{start+length-1}", "-o", tmp,
        f"{H}/{repo}/resolve/main/{fn}"], t=660)
    if not os.path.exists(tmp) or os.path.getsize(tmp) != length: return None
    return np.fromfile(tmp, dtype=np.uint8)

def bf16_to_f32(u16): return (u16.astype(np.uint32) << 16).view(np.float32)

def f32_to_e4m3(x):
    """IEEE-ish E4M3 as used by safetensors F8_E4M3: 1 sign, 4 exponent (bias 7), 3 mantissa,
    round-to-nearest-even, saturating at 448. Implemented from the bits so no framework is needed."""
    x = np.asarray(x, dtype=np.float32)
    sign = (np.signbit(x)).astype(np.uint8) << 7
    a = np.abs(x)
    out = np.zeros(a.shape, dtype=np.uint8)
    big = a >= 448.0
    small = a < 2.0**-9                       # below half the smallest subnormal
    normal = (~big) & (~small) & (a > 0)
    e = np.zeros(a.shape, dtype=np.int32)
    m = np.zeros(a.shape, dtype=np.float64)
    with np.errstate(divide="ignore", invalid="ignore"):
        e = np.where(normal, np.floor(np.log2(np.where(a > 0, a, 1))).astype(np.int32), 0)
    e = np.clip(e, -6, 8)
    scale = np.power(2.0, e.astype(np.float64) - 3)          # 3 mantissa bits
    q = np.rint(a.astype(np.float64) / scale)                 # round to nearest even
    # renormalise if rounding carried
    carry = q >= 16
    q = np.where(carry, q / 2, q); e = np.where(carry, e + 1, e)
    mant = np.clip(q - 8, 0, 7).astype(np.uint8)
    expo = np.clip(e + 7, 0, 15).astype(np.uint8)
    out = np.where(normal, sign | (expo << 3) | mant, out)
    out = np.where(big, sign | 0x7E, out)                     # saturate to max finite
    out = np.where(small, sign, out)
    return out.astype(np.uint8)

def find_pairs(limit=6):
    """repos whose weights are F8_E4M3 and which declare a base_model we can fetch"""
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    out = []
    for rid, v in tags.items():
        if len(out) >= limit: break
        t = v.get("tags") or []
        base = None
        for x in t:
            if x.startswith("base_model:"):
                p = x.split(":")
                if len(p) >= 3: base = ":".join(p[2:]); break
        if not base or base == rid: continue
        low = rid.lower()
        if not any(k in low for k in ("fp8", "f8", "-8bit", "w8a8")): continue
        out.append((rid, base))
    return out

def main():
    pairs = find_pairs()
    print(f"{len(pairs)} candidate FP8 repos with a declared base\n")
    print(f"{'repo':44s} {'tensor':22s} {'exact':>8s} {'within 1 code':>14s}")
    res = []
    for rid, base in pairs:
        d = api(f"{H}/api/models/{rid}")
        if not d: continue
        st = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors")]
        if not st: continue
        hq, offq = header(rid, sorted(st)[0])
        if not hq: continue
        cand = [(k, v) for k, v in hq.items()
                if isinstance(v, dict) and v.get("dtype") == "F8_E4M3"
                and (v["data_offsets"][1] - v["data_offsets"][0]) > SLICE + 4096]
        if not cand: print(f"{rid[:44]:44s} no large F8 tensor"); continue
        name, meta = max(cand, key=lambda c: c[1]["data_offsets"][1] - c[1]["data_offsets"][0])
        # the scale that goes with it
        skey = next((k for k in (name + "_scale", name + "_scale_inv",
                                 name.replace(".weight", ".weight_scale"),
                                 name.replace(".weight", ".weight_scale_inv")) if k in hq), None)
        db = api(f"{H}/api/models/{base}")
        if not db: print(f"{rid[:44]:44s} base unreachable"); continue
        stb = [s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors")]
        hb = offb = None
        for fn in sorted(stb)[:4]:
            hb, offb = header(base, fn)
            if hb and name in hb: bfn = fn; break
            hb = None
        if not hb: print(f"{rid[:44]:44s} tensor {name[:20]} not in the base"); continue
        mb = hb[name]
        if mb.get("dtype") not in ("BF16", "F16") or mb.get("shape") != meta.get("shape"):
            print(f"{rid[:44]:44s} base dtype/shape differs"); continue
        k = meta["shape"][-1]
        nq = (SLICE // k) * k
        aq = grab(rid, sorted(st)[0], offq + meta["data_offsets"][0], nq)
        ab = grab(base, bfn, offb + mb["data_offsets"][0], nq * 2)
        if aq is None or ab is None: print(f"{rid[:44]:44s} fetch failed"); continue
        w = bf16_to_f32(np.frombuffer(ab.tobytes(), dtype=np.uint16))
        # the per-channel scale is unknown without reading it; fit the single scale that maximises
        # agreement, which is the best case for the hypothesis
        best = (0.0, 1.0)
        amax = float(np.abs(w).max())
        for s in (amax/448.0, amax/240.0, 1.0):
            if s <= 0: continue
            pred = f32_to_e4m3(w / s)
            ex = float(np.mean(pred == aq[:len(pred)]))
            if ex > best[0]: best = (ex, s)
        pred = f32_to_e4m3(w / best[1])
        near = float(np.mean(np.abs(pred.astype(np.int16) - aq[:len(pred)].astype(np.int16)) <= 1))
        print(f"{rid[:44]:44s} {name[:22]:22s} {100*best[0]:7.2f}% {100*near:13.2f}%")
        res.append(dict(repo=rid, base=base, tensor=name, exact=best[0], near=near, scale_key=skey))
        json.dump(res, open(f"{OUT}/fp8.json", "w"), indent=1, default=str)
        time.sleep(0.2)
    print("FP8_DONE")

if __name__ == "__main__":
    main()
