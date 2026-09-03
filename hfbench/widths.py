#!/usr/bin/env python3
"""Test the packing hypothesis at every code width instead of letting a selector choose.

The first run's width selector always picked 8 bits, where 'unpacking' an int32 into four bytes IS the
raw byte view -- so packed and unpacked were identical by construction and the 4-bit hypothesis was
never tested. This forces each width and reports all of them in the SAME unit, bits per original byte,
so the columns are directly comparable and the raw column is the thing to beat."""
import json, sys
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from fp8_given_base import header as st_header, grab, api
from packed import h0, order1
H = "https://huggingface.co"

rows = [json.loads(l) for l in open("/root/mzip-hfbench/upload-mix/sample_models.jsonl") if l.strip()]
cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("safetensors:U32", 0) or 0) > 0]
cand.sort(key=lambda r: -float(r["bytes_in_window"]["safetensors:U32"]))

print("all columns are bits per ORIGINAL byte -- 8.000 means incompressible; raw is the thing to beat")
print("%-30s %-14s %7s %7s %7s %7s %7s" % ("repo", "tensor", "raw", "w4", "w2", "w16", "ord1"))
n = 0
for r in cand[:14]:
    if n >= 6: break
    rid = r["id"]
    d = api(f"{H}/api/models/{rid}")
    if not d: continue
    sts = sorted(s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    if not sts: continue
    h, off = st_header(rid, sts[0])
    if not h: continue
    u = [(k, v) for k, v in h.items() if isinstance(v, dict) and v.get("dtype") in ("U32", "I32")
         and (v["data_offsets"][1] - v["data_offsets"][0]) > (1 << 20)]
    if not u: continue
    nm, m = max(u, key=lambda x: x[1]["data_offsets"][1] - x[1]["data_offsets"][0])
    nb = min(4 << 20, m["data_offsets"][1] - m["data_offsets"][0])
    raw = grab(rid, sts[0], off + m["data_offsets"][0], nb)
    if raw is None: continue
    b = np.frombuffer(raw.tobytes()[:(nb // 4) * 4], dtype=np.uint8)
    w = b.view(np.uint32)
    cols = []
    for bits in (8, 4, 2, 16):
        per = 32 // bits
        cs = np.empty((w.size, per), dtype=np.uint16)
        for i in range(per):
            cs[:, i] = (w >> (i * bits)) & ((1 << bits) - 1)
        cols.append(h0(cs.ravel()) * per / 4.0)
    o1 = order1(b, 256)
    print("%-30s %-14s %7.4f %7.4f %7.4f %7.4f %7.4f"
          % (rid[:30], nm.split(".")[-2][:14], cols[0], cols[1], cols[2], cols[3], o1))
    n += 1
print("WIDTHS_DONE")
