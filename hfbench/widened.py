#!/usr/bin/env python3
"""Widened-dtype probe over the upload-mix sample: of the F32 and F16 safetensors bytes uploaded this
week, what share is an exact widening of a narrower dtype? For each sampled model repo with in-window
safetensors of dtype F32 or F16: list the tree, take the first .safetensors file whose header has a large
tensor of that dtype, fetch a 1 MiB element-aligned slice from its middle, and test:
  F32: bf16-widened  = low 16 bits zero on every element     (chance rate 1/65536)
       f16-widened   = x == f32(f16(x)) on every element        (chance rate ~1/2^13)
  F16: bf16-widened  = low 3 mantissa bits zero on every element (chance rate 1/8; require 100%)
Weighted by the repo's in-window bytes of that dtype. Output: upload-mix/widened.jsonl + summary."""
import json, os, re, struct, subprocess, time, collections, numpy as np
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/upload-mix"; CAP = {"F32": 120, "F16": 60}
def get(url, out=None, rng=None):
    for t in range(5):
        a = ["curl", "-sS", "-g", "-L", "--max-time", "120", "-D", f"{OUT}/hw.tmp"]
        if rng: a += ["-r", rng]
        a += ["-o", out] if out else []
        r = subprocess.run(a + [url], capture_output=True, text=True)
        st = 0
        for l in open(f"{OUT}/hw.tmp"):
            if l.startswith("HTTP/"): st = int(l.split()[1])
        if st == 429: time.sleep(60 * (t + 1)); continue
        if st in (401, 403, 404): return None
        return r.stdout if not out else out
    return None
def header(repo, fn):
    tmp = f"{OUT}/hw.hdr"
    if not get(f"{H}/{repo}/resolve/main/{fn}", tmp, "0-8388607"): return None, None
    b = open(tmp, "rb").read()
    if len(b) < 8: return None, None
    n = struct.unpack("<Q", b[:8])[0]
    if n + 8 > len(b): return None, None
    try: return json.loads(b[8:8 + n].decode()), 8 + n
    except Exception: return None, None
rows = [json.loads(l) for l in open(f"{OUT}/sample_models.jsonl")]
done = collections.Counter(); out = open(f"{OUT}/widened.jsonl", "a")
seen = set(l.split('"id": "')[1].split('"')[0] for l in open(f"{OUT}/widened.jsonl")) if os.path.getsize(f"{OUT}/widened.jsonl") else set()
for r in rows:
    if not r.get("ok"): continue
    bw = r["bytes_in_window"]
    for dt in ("F32", "F16"):
        b = bw.get(f"safetensors:{dt}", 0)
        if b <= 0 or done[dt] >= CAP[dt] or r["id"] in seen: continue
        tree = get(f"{H}/api/models/{r['id']}/tree/main?recursive=true&limit=1000")
        try: files = [f["path"] for f in json.loads(tree) if f.get("type") == "file" and f["path"].endswith(".safetensors")]
        except Exception: files = []
        rec = dict(id=r["id"], dtype=dt, bytes_in_window=b, status="no-file"); files.sort()
        for fn in files[:3]:
            h, off = header(r["id"], fn)
            if not h: continue
            cand = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in h.items() if k != "__metadata__" and v.get("dtype") == dt]
            if not cand: continue
            sz, name, v = max(cand)
            esz = 4 if dt == "F32" else 2; L = min(1 << 20, (sz // esz) * esz)
            if L < 65536: continue
            a = off + v["data_offsets"][0] + ((sz - L) // 2 // esz) * esz
            tmp = f"{OUT}/hw.slice"
            if not get(f"{H}/{r['id']}/resolve/main/{fn}", tmp, f"{a}-{a+L-1}") or os.path.getsize(tmp) != L: continue
            raw = open(tmp, "rb").read()
            if dt == "F32":
                u = np.frombuffer(raw, dtype=np.uint32); x = u.view(np.float32)
                bf = float(np.mean((u & 0xFFFF) == 0)); f16 = float(np.mean(x.astype(np.float16).astype(np.float32) == x))
                rec.update(status="ok", file=fn, tensor=name, n=int(len(u)), bf16_widened=bf, f16_widened=f16, zero=float(np.mean(u == 0)))
            else:
                u = np.frombuffer(raw, dtype=np.uint16)
                rec.update(status="ok", file=fn, tensor=name, n=int(len(u)), bf16_widened=float(np.mean((u & 0x7) == 0)), zero=float(np.mean(u == 0)))
            break
        out.write(json.dumps(rec) + "\n"); out.flush(); done[dt] += 1; seen.add(r["id"])
        print(dt, r["id"], rec.get("status"), rec.get("bf16_widened"), rec.get("f16_widened"), flush=True); time.sleep(0.3)
out.close()
# summary weighted by in-window bytes
recs = [json.loads(l) for l in open(f"{OUT}/widened.jsonl")]
for dt in ("F32", "F16"):
    rs = [x for x in recs if x["dtype"] == dt and x.get("status") == "ok"]; tot = sum(x["bytes_in_window"] for x in rs) or 1
    wb = sum(x["bytes_in_window"] for x in rs if x["bf16_widened"] >= 0.9999); wf = sum(x["bytes_in_window"] for x in rs if x.get("f16_widened", 0) >= 0.9999)
    print(f"SUMMARY {dt}: {len(rs)} repos probed; bf16-widened {100*wb/tot:.1f}% of their in-window {dt} bytes ({sum(1 for x in rs if x['bf16_widened']>=0.9999)} repos); f16-widened {100*wf/tot:.1f}% ({sum(1 for x in rs if x.get('f16_widened',0)>=0.9999)} repos)")
print("WIDENED_DONE")
