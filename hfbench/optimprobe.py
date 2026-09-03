#!/usr/bin/env python3
"""144 TB/week of the Hub is optimizer state, and nobody has ever compressed it.

The torch-pickle class turned out not to be model weights at all. Its largest filenames are
bf16_zero_pp_rank_0_mp_rank_00_optim_states.pt, optim_shard_rank0.pt, training_state.pt -- DeepSpeed
ZeRO shards, which hold Adam's two moments. Those have structure that weights do not:

  * exp_avg_sq is a running mean of SQUARED gradients, so it is strictly positive -- every sign bit is
    the same value, and a sign plane of constant bits costs nothing to code and cannot be found by LZ4
    at all, because the bits are 32 apart in the byte stream.
  * both moments are smooth in magnitude across a tensor, so the exponent plane is narrow.
  * and unlike quantised codes, nothing has spent the codebook: no quantiser has already squeezed this.

A torch .pt file is a ZIP whose members are STORED, uncompressed, so the tensor bytes are readable with
two range requests and no download. This measures what Xet gets today (lz4, and lz4 over byte-grouped
planes, which is its ByteGrouping4LZ4 scheme) against what plane-entropy coding could get."""
import json, os, struct, subprocess, sys, collections, tempfile
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
TOK = os.environ.get("HF_TOKEN", "")
import urllib.request

def req(u, rng=None):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    if rng: r.add_header("Range", f"bytes={rng}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=90) as f: return f.read()
        except Exception: pass
    return None

def api(u):
    b = req(u); return json.loads(b) if b else None

def zip_members(url, size):
    """(name, offset, comp_size, uncomp_size, method) for each member, from the central directory"""
    tail = req(url, f"{max(0,size-65600)}-{size-1}")
    if not tail: return []
    i = tail.rfind(b"PK\x05\x06")
    if i < 0: return []
    try: n_ent, cd_size, cd_off = struct.unpack("<HII", tail[i+10:i+20])
    except Exception: return []
    if cd_off == 0xFFFFFFFF or n_ent == 0xFFFF:
        z = tail.rfind(b"PK\x06\x06")
        if z < 0: return []
        try: n_ent, cd_size, cd_off = struct.unpack("<QQQ", tail[z+32:z+56])[0:3]
        except Exception: return []
    if cd_size > 40_000_000: return []
    cd = req(url, f"{cd_off}-{cd_off+cd_size-1}")
    if not cd: return []
    out = []; p = 0
    while p + 46 <= len(cd) and cd[p:p+4] == b"PK\x01\x02":
        meth, = struct.unpack("<H", cd[p+10:p+12])
        csz, usz = struct.unpack("<II", cd[p+20:p+28])
        nl, el, cl = struct.unpack("<HHH", cd[p+28:p+34])
        lho, = struct.unpack("<I", cd[p+42:p+46])
        nm = cd[p+46:p+46+nl].decode("utf-8", "replace")
        out.append((nm, lho, csz, usz, meth))
        p += 46 + nl + el + cl
    return out

def codec(buf, prog, args):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(buf); path = f.name
    try:
        r = subprocess.run([prog] + args + [path, "-c"], capture_output=True, timeout=180)
        return len(r.stdout) if r.returncode == 0 and r.stdout else None
    except Exception:
        return None
    finally:
        os.unlink(path)

def h0(a):
    a = np.asarray(a).ravel()
    if a.size == 0: return 0.0
    _, c = np.unique(a, return_counts=True)
    p = c / c.sum()
    return float(-(p * np.log2(p)).sum())

def planes(b, w):
    n = (b.size // w) * w
    return b[:n].reshape(-1, w)

def main():
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("torch-pickle", 0) or 0) > 0]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["torch-pickle"]))
    KEY = ("optim", "training_state", "model_states", "zero_pp")
    print(f"{'repo':24s} {'member':22s} {'MB':>6s} {'sign=0':>7s} {'lz4':>7s} {'BG4+lz4':>8s} "
          f"{'zstd-3':>7s} {'zstd-19':>8s} {'planes':>7s}")
    agg = collections.defaultdict(float); n = 0
    for r in cand[:40]:
        if n >= 7: break
        rid = r["id"]
        t = api(f"{H}/api/models/{rid}/tree/main?recursive=1&expand=1")
        if not isinstance(t, list): continue
        files = [(f["path"], int(f.get("size") or (f.get("lfs") or {}).get("size") or 0))
                 for f in t if f.get("type") == "file"
                 and f.get("path", "").lower().endswith((".pt", ".pth", ".ckpt", ".bin"))]
        files = [x for x in files if x[1] > (8 << 20) and any(k in x[0].lower() for k in KEY)]
        if not files: continue
        files.sort(key=lambda x: -x[1])
        path, size = files[0]
        url = f"{H}/{rid}/resolve/main/{path}"
        mem = [m for m in zip_members(url, size) if m[4] == 0 and m[3] > (2 << 20)]
        if not mem: continue
        mem.sort(key=lambda m: -m[3])
        # the first run took only the largest member and every one turned out to be a SIGNED tensor
        # (sign=0 near 50%) -- the same one-tensor-called-the-model fault as three earlier runs. Adam's
        # squared moment is a different member of the same zip, so take up to four large members and
        # keep the one whose sign plane is most constant: that is exp_avg_sq if it is present at all.
        best = None
        for name, lho, _csz, usz, _ in mem[:4]:
            hdr = req(url, f"{lho}-{lho+127}")
            if not hdr or hdr[:4] != b"PK\x03\x04": continue
            nl, el = struct.unpack("<HH", hdr[26:30])
            data_off = lho + 30 + nl + el
            want = min(usz, 8 << 20)
            raw = req(url, f"{data_off}-{data_off+want-1}")
            if not raw or len(raw) < (1 << 20): continue
            b = np.frombuffer(raw, dtype=np.uint8)
            p4 = planes(b, 4)
            s0 = float(np.mean((p4[:, 3] & 0x80) == 0))
            if best is None or abs(s0 - 0.5) > abs(best[3] - 0.5): best = (name, raw, b, s0)
        if not best: continue
        name, raw, b, sign0 = best
        nby = b.size
        p4 = planes(b, 4)
        lz4 = codec(raw, "lz4", ["-1", "-f", "--no-frame-crc"])
        z3 = codec(raw, "zstd", ["-3", "-f", "-q"])
        z19 = codec(raw, "zstd", ["-19", "-f", "-q", "--long=27"])
        grouped = np.concatenate([p4[:, i] for i in range(4)]).tobytes()
        bg4 = codec(grouped, "lz4", ["-1", "-f", "--no-frame-crc"])
        plane_bits = sum(h0(p4[:, i]) for i in range(4)) / 4.0
        plane_bytes = plane_bits / 8.0 * nby
        f = lambda x: (nby / x) if x else float("nan")
        print(f"{rid[:24]:24s} {name.split('/')[-1][:22]:22s} {nby/1e6:6.1f} {100*sign0:6.1f}% "
              f"{f(lz4):6.2f}x {f(bg4):7.2f}x {f(z3):6.2f}x {f(z19):7.2f}x {nby/plane_bytes:6.2f}x")
        agg["raw"] += nby
        for k, v in (("lz4", lz4), ("bg4", bg4), ("z3", z3), ("z19", z19), ("planes", plane_bytes)):
            if v: agg[k] += v
        n += 1
    if n:
        print(f"\n  across {n} optimizer shards, {agg['raw']/1e6:.0f} MB sampled:")
        for k, lbl in (("lz4", "lz4 -1  (what Xet stores today)"),
                       ("bg4", "byte-grouped + lz4  (Xet's BG4 scheme)"),
                       ("z3", "zstd -3"), ("z19", "zstd -19"),
                       ("planes", "order-0 per byte plane  (plane-entropy bound)")):
            if agg[k]:
                print(f"    {lbl:44s} {agg['raw']/agg[k]:5.2f}x   "
                      f"{100*(1-agg[k]/agg['raw']):5.1f}% smaller")
    print("OPTIMPROBE_DONE")

if __name__ == "__main__":
    main()
