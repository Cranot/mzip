#!/usr/bin/env python3
"""Sub-byte plane separation for the 8-bit weight formats the coder cannot reach.

87 TB of the week's model uploads are 8-bit: F8_E4M3 35.1, U8 37.1, I8 14.6. This programme scored all
of it zero, and there is an architectural reason. The coder splits data into BYTE planes, and in an
8-bit format the element IS one byte, so there is nothing to split -- it falls back to coding raw bytes
order-0. But an FP8 byte is not atomic: E4M3 packs sign(1) + exponent(4) + mantissa(3), and the
exponent field of a weight tensor is as skewed as it is in BF16, while the mantissa is near-uniform.
Order-0 over whole bytes mixes the two and sees neither.

Measured here on real Hub slices, per 64 KiB chunk:
  bytes        H0 of the raw byte stream                       -- what the coder does today
  nibble       H0(high nibble) + H0(low nibble), packed 2/byte -- a nibble-plane split
  fields       H0(sign+exp) + H0(mantissa) for the real E4M3/E5M2 field boundary
  bitshuffle   H0 of each of the 8 bit-planes after transposing bits across 8 elements
  cond         H(mantissa | exponent), the ceiling of a context model
Each is reported as an achievable ratio so it can be read against Xet's own number for the file."""
import json, os, struct, subprocess, sys
import numpy as np

H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/subbyte"
os.makedirs(OUT, exist_ok=True)
SLICE = 8 * 1024 * 1024

def sh(a, t=900): return subprocess.run(a, capture_output=True, text=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout)
    except Exception: return None

def h0(a):
    a = np.ascontiguousarray(a).ravel()
    if a.size == 0: return 0.0
    h = np.bincount(a, minlength=int(a.max()) + 1).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum())

def hc(x, c):
    x = np.asarray(x).ravel().astype(np.int64); c = np.asarray(c).ravel().astype(np.int64)
    nx = int(x.max()) + 1; nc = int(c.max()) + 1
    j = np.bincount(c * nx + x, minlength=nc * nx).astype(np.float64).reshape(nc, nx)
    tot = j.sum(); pc = j.sum(axis=1); out = 0.0
    for k in np.nonzero(pc)[0]:
        p = j[k][j[k] > 0] / pc[k]
        out += (pc[k] / tot) * float(-(p * np.log2(p)).sum())
    return out

def analyse(a, dtype):
    """a: uint8 array of one chunk. Returns bits-per-element under each model."""
    r = {}
    r["bytes"] = h0(a)
    hi = (a >> 4).astype(np.uint8); lo = (a & 15).astype(np.uint8)
    r["nibble"] = h0(hi) + h0(lo)
    if dtype == "F8_E4M3":            # sign 1 | exp 4 | mant 3
        f1 = (a >> 3).astype(np.uint8); f2 = (a & 7).astype(np.uint8)
    elif dtype == "F8_E5M2":          # sign 1 | exp 5 | mant 2
        f1 = (a >> 2).astype(np.uint8); f2 = (a & 3).astype(np.uint8)
    else:                             # int8 / uint8: split at the nibble, the only natural boundary
        f1, f2 = hi, lo
    r["fields"] = h0(f1) + h0(f2)
    r["cond"] = h0(f1) + hc(f2, f1)
    bits = np.unpackbits(a.reshape(-1, 1), axis=1)          # [n, 8], bit 0 = MSB
    n8 = (len(a) // 8) * 8
    planes = bits[:n8].reshape(-1, 8, 8).transpose(0, 2, 1).reshape(-1, 8)
    packed = np.packbits(planes, axis=1).ravel()            # 8 bit-planes, 1 byte per 8 elements
    per_plane = [h0(np.ascontiguousarray(packed[i::8])) for i in range(8)]
    r["bitshuffle"] = sum(per_plane) / 8.0                  # bits per original element
    r["bitplanes"] = [round(x, 3) for x in per_plane]
    return r

def find(dtypes, want=2):
    got = {d: [] for d in dtypes}
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl")]
    rows = [r for r in rows if r.get("ok")]
    rows.sort(key=lambda r: -sum(r.get("bytes_in_window", {}).values()))
    for r in rows:
        if all(len(got[d]) >= want for d in dtypes): break
        want_d = [d for d in dtypes if len(got[d]) < want and f"safetensors:{d}" in r.get("bytes_in_window", {})]
        if not want_d: continue
        d = api(f"{H}/api/models/{r['id']}/tree/main?recursive=true&limit=1000")
        if not isinstance(d, list): continue
        st = [f["path"] for f in d if f.get("type") == "file" and f["path"].endswith(".safetensors")
              and int(f.get("size") or 0) > SLICE * 3]
        if not st: continue
        fn = sorted(st)[0]
        tmp = f"{OUT}/h.tmp"
        sh(["curl", "-sSL", "--max-time", "300", "-r", "0-8388607", "-o", tmp, f"{H}/{r['id']}/resolve/main/{fn}"])
        try:
            b = open(tmp, "rb").read(); n = struct.unpack("<Q", b[:8])[0]
            hdr = json.loads(b[8:8 + n].decode()); off = 8 + n
        except Exception: continue
        for dt in want_d:
            cands = [(v["data_offsets"][1] - v["data_offsets"][0], k, v) for k, v in hdr.items()
                     if k != "__metadata__" and isinstance(v, dict) and v.get("dtype") == dt]
            cands = [c for c in cands if c[0] > SLICE + 4096]
            if not cands: continue
            sz, name, v = max(cands)
            a = off + v["data_offsets"][0] + 4096
            loc = f"{OUT}/{dt}.{len(got[dt])}.bin"
            sh(["curl", "-sSL", "--max-time", "600", "-r", f"{a}-{a+SLICE-1}", "-o", loc,
                f"{H}/{r['id']}/resolve/main/{fn}"], t=660)
            if os.path.exists(loc) and os.path.getsize(loc) == SLICE:
                got[dt].append((r["id"], name, loc))
    return got

def main():
    dtypes = ["F8_E4M3", "U8", "I8", "F8_E5M2"]
    got = find(dtypes)
    print(f"{'dtype':9s} {'repo/tensor':44s} | {'bytes':>7s} {'nibble':>7s} {'fields':>7s} {'cond':>7s} {'bitshuf':>8s} | best gain")
    res = []
    for dt, items in got.items():
        for rid, name, loc in items:
            a = np.fromfile(loc, dtype=np.uint8)
            # per 64 KiB chunk, then averaged, so it matches how a chunk store would code it
            acc = {}
            nch = len(a) // 65536
            for c in range(nch):
                r = analyse(a[c*65536:(c+1)*65536], dt)
                for k, v in r.items():
                    if k == "bitplanes": continue
                    acc[k] = acc.get(k, 0.0) + v / nch
            best = min(k for k in ("nibble", "fields", "cond", "bitshuffle"))
            bk = min(("nibble", "fields", "cond", "bitshuffle"), key=lambda k: acc[k])
            gain = 1 - acc[bk] / acc["bytes"]
            print(f"{dt:9s} {(rid+'/'+name)[:44]:44s} | {acc['bytes']:7.4f} {acc['nibble']:7.4f} {acc['fields']:7.4f} "
                  f"{acc['cond']:7.4f} {acc['bitshuffle']:8.4f} | {100*gain:5.1f}% ({bk})", flush=True)
            res.append(dict(dtype=dt, repo=rid, tensor=name, **{k: round(v, 4) for k, v in acc.items()},
                            best=bk, gain=gain, ratio_today=8/acc["bytes"], ratio_best=8/acc[bk]))
            json.dump(res, open(f"{OUT}/subbyte.json", "w"), indent=1)
    if res:
        print()
        for r in res:
            print(f"  {r['dtype']:9s} ratio today {r['ratio_today']:.4f} -> with {r['best']:10s} {r['ratio_best']:.4f}")
    print("SUBBYTE_DONE")

if __name__ == "__main__":
    main()
