#!/usr/bin/env python3
"""One quantisation predicts another, because they share a block maximum.

A GGUF repo publishes many quant types of ONE model, and this programme has been treating each as an
independent file. They are not. For a block of 32 weights, Q8_0 stores d8 = amax/127 and Q4_0 stores
d4 = amax/8, both derived from the SAME amax -- so given one scale the other follows exactly, up to the
fp16 rounding of the stored value. And the integer codes follow too: with x ~ q8*d8,

    q4_predicted = round(q8 * d8 / d4)

is wrong only when x sits within d8/2 of a Q4 rounding boundary, which is a band of width
(1/2)(8/127) = 3.1% of a step. So most codes should be exactly right and the residual should be a
sparse +/-1 signal.

If that holds, a repo needs to keep ONE quantisation and code the rest as residuals -- no base model, no
quantizer run, and it covers the 18% of GGUF bytes that name no base at all. This measures it on real
published files: same repo, same tensor, aligned block ranges, no downloads of whole models."""
import json, os, struct, subprocess, sys
import numpy as np
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/qgq"
os.makedirs(OUT, exist_ok=True)
NBLOCK = 200000          # blocks of 32 weights to compare

def sh(a, t=600): return subprocess.run(a, capture_output=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout.decode())
    except Exception: return None

GG_T = {0: ("F32", 4, 1), 1: ("F16", 2, 1), 2: ("Q4_0", 18, 32), 3: ("Q4_1", 20, 32),
        8: ("Q8_0", 34, 32), 9: ("Q8_1", 36, 32)}

def read_header(repo, path, window=24 * 1024 * 1024):
    tmp = f"{OUT}/h.tmp"
    sh(["curl", "-sSL", "--max-time", "300", "-r", f"0-{window-1}", "-o", tmp,
        f"{H}/{repo}/resolve/main/{path}"])
    b = open(tmp, "rb").read()
    if b[:4] != b"GGUF": return None
    ver = struct.unpack("<I", b[4:8])[0]
    n_tensor, n_kv = struct.unpack("<QQ", b[8:24])
    p = 24
    def rstr():
        nonlocal p
        (ln,) = struct.unpack("<Q", b[p:p+8]); p += 8
        s = b[p:p+ln]; p += ln; return s.decode("utf-8", "replace")
    T = {0:1,1:1,2:2,3:2,4:4,5:4,6:4,7:1,10:8,11:8,12:8}
    def skip(t):
        nonlocal p
        if t == 8: rstr(); return
        if t == 9:
            (et,) = struct.unpack("<I", b[p:p+4]); p += 4
            (cnt,) = struct.unpack("<Q", b[p:p+8]); p += 8
            for _ in range(cnt):
                if et == 8: rstr()
                elif et == 9: return
                else: p += T.get(et, 4)
            return
        p += T.get(t, 4)
    for _ in range(n_kv):
        rstr(); (t,) = struct.unpack("<I", b[p:p+4]); p += 4; skip(t)
    tensors = {}
    for _ in range(n_tensor):
        name = rstr()
        (nd,) = struct.unpack("<I", b[p:p+4]); p += 4
        dims = struct.unpack("<" + "Q" * nd, b[p:p+8*nd]); p += 8 * nd
        (tt,) = struct.unpack("<I", b[p:p+4]); p += 4
        (off,) = struct.unpack("<Q", b[p:p+8]); p += 8
        tensors[name] = dict(dims=dims, ttype=tt, offset=off)
    align = 32
    data_start = (p + align - 1) // align * align
    return dict(tensors=tensors, data_start=data_start)

def fetch(repo, path, start, length):
    tmp = f"{OUT}/s.bin"
    sh(["curl", "-sSL", "--max-time", "600", "-r", f"{start}-{start+length-1}", "-o", tmp,
        f"{H}/{repo}/resolve/main/{path}"])
    if not os.path.exists(tmp) or os.path.getsize(tmp) != length: return None
    return np.fromfile(tmp, dtype=np.uint8)

def dec_q8(raw):
    n = len(raw) // 34
    a = raw[:n*34].reshape(n, 34)
    d = a[:, :2].copy().view(np.float16).astype(np.float32).ravel()
    q = a[:, 2:].view(np.int8).astype(np.int32)
    return d, q

def dec_q4(raw):
    n = len(raw) // 18
    a = raw[:n*18].reshape(n, 18)
    d = a[:, :2].copy().view(np.float16).astype(np.float32).ravel()
    lo = (a[:, 2:] & 0x0F).astype(np.int32)
    hi = (a[:, 2:] >> 4).astype(np.int32)
    q = np.concatenate([lo, hi], axis=1) - 8      # ggml packs first half in low nibbles
    return d, q

def h0(x):
    x = np.asarray(x).ravel()
    if x.size == 0: return 0.0
    v, c = np.unique(x, return_counts=True)
    p = c / c.sum()
    return float(-(p * np.log2(p)).sum())

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    if not d: print("repo not found"); return
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8 = [f for f in files if "q8_0" in f.lower()]
    q4 = [f for f in files if "q4_0" in f.lower()]
    print(f"{repo}\n  Q8_0: {q8}\n  Q4_0: {q4}")
    if not q8 or not q4: print("need both a Q8_0 and a Q4_0 in the repo"); return
    h8 = read_header(repo, q8[0]); h4 = read_header(repo, q4[0])
    if not h8 or not h4: print("header read failed"); return
    common = [n for n in h8["tensors"] if n in h4["tensors"]
              and h8["tensors"][n]["ttype"] == 8 and h4["tensors"][n]["ttype"] == 2]
    if not common: print("no tensor is Q8_0 in one and Q4_0 in the other"); return
    common.sort(key=lambda n: -np.prod(h8["tensors"][n]["dims"]))
    name = common[0]
    t8, t4 = h8["tensors"][name], h4["tensors"][name]
    nel = int(np.prod(t8["dims"]))
    nb = min(NBLOCK, nel // 32)
    print(f"  tensor {name}, {nel:,} elements, comparing {nb:,} blocks")
    r8 = fetch(repo, q8[0], h8["data_start"] + t8["offset"], nb * 34)
    r4 = fetch(repo, q4[0], h4["data_start"] + t4["offset"], nb * 18)
    if r8 is None or r4 is None: print("slice fetch failed"); return
    d8, qq8 = dec_q8(r8); d4, qq4 = dec_q4(r4)
    n = min(len(d8), len(d4)); d8, qq8, d4, qq4 = d8[:n], qq8[:n], d4[:n], qq4[:n]

    # 1. are the scales derivable? ggml takes Q4_0's scale from the SIGNED extreme of the block
    #    (d4 = max/-8, max being the value of largest magnitude), while Q8_0 takes it from the absolute
    #    one (d8 = amax/127). The sign is recoverable: in Q8_0 the extreme element codes to +/-127.
    ext = np.take_along_axis(qq8, np.argmax(np.abs(qq8), axis=1)[:, None], axis=1).ravel()
    sgn = np.where(ext < 0, -1.0, 1.0)
    amax = d8 * 127.0
    pred_d4 = (sgn * amax / -8.0).astype(np.float16).astype(np.float32)
    same_scale = float(np.mean(pred_d4 == d4))
    # where it is not exact, the difference is a whisker in fp16 units, so code that instead
    u_pred = pred_d4.astype(np.float16).view(np.int16).astype(np.int64)
    u_true = d4.astype(np.float16).view(np.int16).astype(np.int64)
    ulp = u_true - u_pred
    scale_bits = 0.0 if same_scale > 0.9999 else h0(np.clip(ulp, -8, 8)) / 32.0
    # 2. are the codes predictable?
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(d4 != 0, d8 / d4, 0.0)[:, None]
    pred_q4 = np.rint(qq8 * ratio).astype(np.int32)
    pred_q4 = np.clip(pred_q4, -8, 7)
    exact = float(np.mean(pred_q4 == qq4))
    resid = (qq4 - pred_q4).ravel()
    # 3. what it would cost
    bits_now = h0(qq4.ravel()) + 16.0 / 32          # codes plus the fp16 scale, per weight
    bits_res = h0(resid) + scale_bits
    print("")
    print(f"  scales derivable from Q8_0s: {100*same_scale:.2f}% exact; "
          f"the rest cost {scale_bits:.4f} bits/weight as an fp16 offset "
          f"({100*float(np.mean(np.abs(ulp)<=1)):.1f}% within one ulp)")
    print(f"  codes predicted exactly:      {100*exact:.2f}%")
    print(f"  residual: {100*float(np.mean(resid==0)):.2f}% zero, "
          f"{100*float(np.mean(np.abs(resid)<=1)):.2f}% within +/-1, range [{resid.min()},{resid.max()}]")
    print(f"  bits per weight: Q4_0 coded alone {bits_now:.4f}  |  as a residual on Q8_0 {bits_res:.4f}")
    print(f"  => Q4_0 costs {100*bits_res/bits_now:.1f}% of what it costs on its own "
          f"({bits_now/max(bits_res,1e-9):.2f}x)")
    json.dump(dict(repo=repo, tensor=name, blocks=int(n), scale_exact=same_scale, code_exact=exact,
                   bits_alone=bits_now, bits_residual=bits_res), open(f"{OUT}/qgq.json", "w"), indent=1)
    print("QGQ_DONE")

if __name__ == "__main__":
    main()
