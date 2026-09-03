#!/usr/bin/env python3
"""Progressive weights: does delivering a BF16 tensor in layers cost bytes?

A downloader of a 16-bit model waits for every byte before the first token. Store the tensor as layers
instead -- a 4-bit quant, then the residual that lifts it to 8-bit, then the residual that restores exact
bf16 -- and inference can start at the first layer and upgrade in place as the rest arrives. Progressive
JPEG, for weights. It is worth doing only if the layers do not cost much more than the flat file.

Layers, all integer, all measured as order-0 entropy plus stored scales:
  L1  Q4_0: per 32 weights d = max/-8, q in [-8,7]; cost = h0(q) + 16/32
  L2  Q8_0 codes given L1: predict q8 = round(dequant4 / d8) and code the difference; cost = h0(r2) + 16/32
  L3  exact bf16 given L2: code the bf16 integer difference to dequant8; cost = h0(r3)
Compared against the flat costs: raw bf16 16.0, Fano's BF16 ~10.7 (1.49x), Xet ~14.0 (1.14x).
The client at L1 holds a Q4_0 model, at L1+L2 a Q8_0 model, at L1+L2+L3 the exact bf16 bytes."""
import sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo4 import shard_map
H = "https://huggingface.co"

def bf16_bits(x):
    u = np.asarray(x, dtype=np.float32).view(np.uint32)
    lsb = (u >> 16) & 1
    return ((u + 0x7FFF + lsb) >> 16).astype(np.int32)

def q4_0(x):
    b = x.reshape(-1, 32)
    amax_i = np.argmax(np.abs(b), axis=1); mx = b[np.arange(len(b)), amax_i]
    d = (mx / -8.0).astype(np.float16).astype(np.float32)
    with np.errstate(divide="ignore", invalid="ignore"):
        q = np.where(d[:, None] != 0, np.rint(b / d[:, None]), 0)
    q = np.clip(q, -8, 7).astype(np.int32)
    return q, d, (q * d[:, None]).astype(np.float32)

def q8_0(x):
    b = x.reshape(-1, 32)
    d = (np.abs(b).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
    with np.errstate(divide="ignore", invalid="ignore"):
        q = np.where(d[:, None] != 0, np.rint(b / d[:, None]), 0)
    q = np.clip(q, -127, 127).astype(np.int32)
    return q, d, (q * d[:, None]).astype(np.float32)

def main():
    repos = ["Qwen/Qwen2.5-0.5B", "HuggingFaceTB/SmolLM2-360M"]
    print(f"{'model':28s} {'tensor':12s} {'L1 Q4':>6s} {'L2 ->Q8':>8s} {'L3 ->bf16':>9s} {'SUM':>6s} {'flat Q8':>8s} {'Fano':>6s} {'raw':>5s}")
    for rid in repos:
        d = api(f"{H}/api/models/{rid}")
        sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        wmap, cache = shard_map(rid, sts)
        k = next(k for k in sorted(wmap) if k.endswith(".weight") and "mlp" in k and "layers.0." in k)
        fn = wmap[k]
        if fn not in cache: cache[fn] = st_header(rid, fn)
        hb, off = cache[fn]; m = hb[k]
        rows, cols = m["shape"]; nrow = min(rows, max(64, (4 << 20) // (cols * 2)))
        raw = grab(rid, fn, off + m["data_offsets"][0], nrow * cols * 2)
        u16 = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        x = bf16_to_f32(u16).astype(np.float32)
        n = (x.size // 32) * 32; x = x[:n]; xb = u16[:n].astype(np.int32)
        # L1
        q4, d4, x4 = q4_0(x)
        L1 = h0(q4) + 16 / 32
        # L2: the Q8_0 of the ORIGINAL, predicted from the L1 reconstruction using Q8's own scales
        q8, d8, x8 = q8_0(x)
        with np.errstate(divide="ignore", invalid="ignore"):
            p8 = np.where(d8[:, None] != 0, np.rint(x4 / d8[:, None]), 0)
        r2 = (q8 - np.clip(p8, -127, 127).astype(np.int32)).ravel()
        L2 = h0(r2) + 16 / 32
        # L3: exact bf16 bits given the Q8_0 reconstruction
        r3 = xb - bf16_bits(x8.ravel())
        L3 = h0(r3)
        flat8 = h0(q8) + 16 / 32
        fano = h0((xb >> 8).astype(np.uint8)) + 8.0          # high plane coded, low plane raw
        print(f"{rid[:28]:28s} {k.split('.')[-2][:12]:12s} {L1:6.2f} {L2:8.2f} {L3:9.2f} {L1+L2+L3:6.2f} {flat8:8.2f} {fano:6.2f} {16.0:5.1f}")
    print("\n  bits per weight. A client holds a usable Q4 model after L1 (~28% of the flat bf16 bytes), a Q8 model")
    print("  after L1+L2, exact bf16 after all three. SUM vs Fano/raw is the price of progressiveness.")
    print("PROGRESSIVE_DONE")

if __name__ == "__main__":
    main()
