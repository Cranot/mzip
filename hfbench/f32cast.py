#!/usr/bin/env python3
"""F32 uploads that are really bf16: how many of the week's F32 tensors have all-zero low mantissa bits?

A model trained in bf16 and saved as fp32 has 16 zero bits per weight; Fano codes a zero plane to
nothing, so those files compress 2x trivially -- the page uses 1.59x for the F32 class. This counts
how much of the F32 class is an upcast, from 64 KiB slices."""
import json, sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from fp8_given_base import header as st_header, grab
from crossrepo4 import shard_map
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("safetensors:F32", 0) or 0) > 100e6]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["safetensors:F32"]))
    tot = up = 0.0; n = 0; kinds = []
    for r in cand[:40]:
        rid = r["id"]; b = float(r["bytes_in_window"]["safetensors:F32"])
        try:
            d = api(f"{H}/api/models/{rid}")
            sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sts: continue
            wmap, cache = shard_map(rid, sts)
            ks = [k for k in wmap if k.endswith(".weight") and "norm" not in k and "bias" not in k]
            if not ks: continue
            k = sorted(ks)[len(ks) // 2]; fn = wmap[k]
            if fn not in cache: cache[fn] = st_header(rid, fn)
            hb, off = cache[fn]
            if not hb or k not in hb or hb[k]["dtype"] != "F32": continue
            m = hb[k]; raw = grab(rid, fn, off + m["data_offsets"][0], min(1 << 18, m["data_offsets"][1] - m["data_offsets"][0]))
            if raw is None: continue
            u = np.frombuffer(raw.tobytes(), dtype=np.uint32)
            low16_zero = float(np.mean((u & 0xFFFF) == 0)); low13_zero = float(np.mean((u & 0x1FFF) == 0))
            n += 1; tot += b
            kind = "bf16 upcast" if low16_zero > 0.99 else ("f16 upcast" if low13_zero > 0.99 else "true f32")
            if kind != "true f32": up += b
            kinds.append((kind, rid))
        except Exception:
            continue
    from collections import Counter
    c = Counter(k for k, _ in kinds)
    print(f"F32 repos probed {n} ({tot/1e12:.3f} TB in sample): {dict(c)}")
    print(f"  upcast (bf16 or f16 with zero low bits): {100*up/max(tot,1):.1f}% of F32 bytes -> compress 2x for free; page uses 1.59x on the class")
    print("F32CAST_DONE")

if __name__ == "__main__":
    main()
