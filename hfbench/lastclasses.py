#!/usr/bin/env python3
"""The last untested classes, against Xet.

Three principles have paid so far: FREE GROUND (Xet stores the class raw, so any compression is pure
gain -- found for F16, GGUF-F16 and the 8-bit dtypes), PRODUCTION-AWARE coding, and UNWRAPPING
containers. This applies the first to what the census still leaves untested:
  safetensors U32 / I32 / I16      56.8 TB/week -- integer tensors: indices, offsets, quantised scales
  other-weights (onnx, tflite)     27.2 TB/week
  torch-pickle (.bin)               0.144 PB/week -- the table currently carries an ASSUMED F32 floor
                                    for this class, which is the last assumption in it
Same arms as the main table, 8 MiB element-aligned slices from the heaviest repos of the upload sample."""
import json, os, struct, subprocess
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/lastclass"
os.makedirs(OUT, exist_ok=True)
SLICE = 8 * 1024 * 1024

def sh(a, t=900): return subprocess.run(a, capture_output=True, text=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout)
    except Exception: return None

def arms(path):
    r = {}
    x = sh(["taskset", "-c", "3", f"{B}/agents/hf-fairness/xetsim/target/release/xetsim", path, "65536", "3"])
    for l in x.stdout.splitlines():
        if l.startswith("auto "):
            f = l.split(); r["xet"] = (float(f[3]), float(f[4]), float(f[5])); break
    p = sh(["taskset", "-c", "3", f"{B}/final/plane-entropy/bench_pe", path, "65536", "3"])
    for l in p.stdout.splitlines():
        if "ratio" in l:
            f = l.split(); r["pe"] = (float(f[3]), float(f[5]), float(f[8])); break
    m = sh(["taskset", "-c", "3", f"{B}/final/tree/pe_bench2", path, "65536", "FAST", "3"])
    for l in m.stdout.splitlines():
        if l.startswith("FAST"):
            f = l.split(); r["mzip"] = (float(f[2]), float(f[3]), float(f[4])); break
    return r

def report(tag, loc):
    r = arms(loc)
    if "xet" not in r or "pe" not in r: print(f"{tag:46s} arms failed"); return None
    best = max((r[k][0], k) for k in ("pe", "mzip") if k in r)
    xr = r["xet"][0]
    v = f"WIN -{100*(1-xr/best[0]):.1f}%" if best[0] > xr * 1.005 else (
        f"LOSS +{100*(best[0]/xr-1):.1f}%" if best[0] < xr * 0.995 else "tie")
    bv = r[best[1]]
    print(f"{tag[:46]:46s} | {xr:7.4f} {r['xet'][1]:6.0f} {r['xet'][2]:7.0f} | {best[0]:7.4f} {bv[1]:6.0f} {bv[2]:7.0f} | {v} ({best[1]})", flush=True)
    return dict(tag=tag, xet=r["xet"], best=best[0], arm=best[1], speeds=bv, verdict=v)

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl")]
    rows = [r for r in rows if r.get("ok")]
    rows.sort(key=lambda r: -sum(r.get("bytes_in_window", {}).values()))
    print(f"{'slice':46s} | {'Xet':>7s} {'comp':>6s} {'decomp':>7s} | {'ours':>7s} {'comp':>6s} {'decomp':>7s} | verdict")
    res = []
    want = {"U32": 2, "I32": 2, "I16": 2}
    got = {k: 0 for k in want}
    for r in rows:
        if all(got[k] >= want[k] for k in want): break
        need = [d for d in want if got[d] < want[d] and f"safetensors:{d}" in r.get("bytes_in_window", {})]
        if not need: continue
        t = api(f"{H}/api/models/{r['id']}/tree/main?recursive=true&limit=1000")
        if not isinstance(t, list): continue
        st = [f["path"] for f in t if f.get("type") == "file" and f["path"].endswith(".safetensors")
              and int(f.get("size") or 0) > SLICE * 2]
        if not st: continue
        fn = sorted(st)[0]; tmp = f"{OUT}/h.tmp"
        sh(["curl", "-sSL", "--max-time", "300", "-r", "0-8388607", "-o", tmp, f"{H}/{r['id']}/resolve/main/{fn}"])
        try:
            b = open(tmp, "rb").read(); n = struct.unpack("<Q", b[:8])[0]
            hdr = json.loads(b[8:8 + n].decode()); off = 8 + n
        except Exception: continue
        for dt in need:
            c = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in hdr.items()
                 if k != "__metadata__" and isinstance(v, dict) and v.get("dtype") == dt]
            c = [x for x in c if x[0] > SLICE + 4096]
            if not c: continue
            sz, name, v = max(c)
            a = off + v["data_offsets"][0] + 4096
            loc = f"{OUT}/{dt}.{got[dt]}.bin"
            sh(["curl", "-sSL", "--max-time", "600", "-r", f"{a}-{a+SLICE-1}", "-o", loc, f"{H}/{r['id']}/resolve/main/{fn}"], t=660)
            if os.path.exists(loc) and os.path.getsize(loc) == SLICE:
                x = report(f"{dt} {r['id']}/{name}", loc)
                if x: res.append(x)
                got[dt] += 1; os.remove(loc)
    # torch pickle: replace the table's assumed floor with a measurement
    tp = 0
    for r in rows:
        if tp >= 3: break
        if "torch-pickle" not in r.get("bytes_in_window", {}): continue
        t = api(f"{H}/api/models/{r['id']}/tree/main?recursive=true&limit=1000")
        if not isinstance(t, list): continue
        for f in t:
            if tp >= 3: break
            p = f.get("path", "")
            if f.get("type") != "file" or not p.endswith((".bin", ".pt", ".pth")): continue
            sz = int(f.get("size") or 0)
            if not (4 * 1024 * 1024 <= sz <= 120 * 1024 * 1024): continue
            loc = f"{OUT}/tp.{tp}.bin"
            sh(["curl", "-sSL", "--max-time", "900", "-o", loc, f"{H}/{r['id']}/resolve/main/{p}"], t=960)
            if os.path.exists(loc) and os.path.getsize(loc) > 4 * 1024 * 1024:
                x = report(f"torch-pickle {r['id']}/{p}", loc)
                if x: res.append(x)
                tp += 1; os.remove(loc)
    json.dump(res, open(f"{OUT}/lastclass.json", "w"), indent=1, default=str)
    print("LASTCLASS_DONE")

if __name__ == "__main__":
    main()
