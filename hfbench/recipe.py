#!/usr/bin/env python3
"""The recipe test: does running llama.cpp on the parent reproduce a published K-quant byte for byte?

Every result since RESULT 84 approximates llama.cpp with arithmetic and codes what is left. Line 1573
of the record says the real thing has been seen to reproduce: f16 input + the publisher's imatrix ->
his K-quants, exactly. If that holds, a K-quant is not 9-13% of itself given its parent -- it is a
version pin, an imatrix pointer, and nothing else. The store stops approximating the program and runs it.

Two stages, because the build and the download are independent:
  --find      pick a small repo that ships an F16/BF16 GGUF, a Q4_K_M and an imatrix, and fetch them
  --quantize  run llama-quantize on the F16 (with the imatrix) and compare to the published Q4_K_M:
              identical -> the recipe is exact; else count differing 256-weight superblocks, which is
              the residual a recipe-plus-patch would store.
"""
import json, os, sys, subprocess, hashlib, urllib.request
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"; W = f"{B}/recipe"
sys.path.insert(0, B)
from kquant_given import api, read_header
TOK = os.environ.get("HF_TOKEN", "")
os.makedirs(W, exist_ok=True)

def dl(rid, fn, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0: return True
    u = f"{H}/{rid}/resolve/main/{fn}"
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    try:
        with urllib.request.urlopen(r, timeout=120) as f, open(dst + ".part", "wb") as o:
            while True:
                b = f.read(8 << 20)
                if not b: break
                o.write(b)
        os.rename(dst + ".part", dst); return True
    except Exception as e:
        print("  download failed:", e); return False

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(16 << 20), b""): h.update(b)
    return h.hexdigest()

def find():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    cands = [r for r in tags if "gguf" in r.lower()]
    # prefer publishers known to ship imatrix + f16: bartowski, mradermacher (i1), QuantFactory
    cands.sort(key=lambda r: (0 if r.startswith(("bartowski/", "mradermacher/")) else 1, r))
    # The model-info endpoint lists siblings WITHOUT sizes (every size read as 0, so every candidate
    # was rejected as "too small"). The tree endpoint carries size / lfs.size. And "f16" also matches
    # the mmproj-*-f16.gguf vision projector, which is not the model.
    for rid in cands[:400]:
        t = api(f"{H}/api/models/{rid}/tree/main?recursive=1&expand=1")
        if not isinstance(t, list): continue
        sib = {f["path"]: int(f.get("size") or (f.get("lfs") or {}).get("size") or 0)
               for f in t if f.get("type") == "file"}
        gg = [f for f in sib if f.lower().endswith(".gguf") and "mmproj" not in f.lower()]
        f16 = [f for f in gg if any(k in f.lower() for k in ("-f16.", "-bf16.", "_f16.", "-f16-", ".f16.", "f16.gguf", "bf16.gguf"))]
        q4 = [f for f in gg if "q4_k_m" in f.lower() and "-0000" not in f]     # skip split shards
        im = [f for f in sib if "imatrix" in f.lower() and f.lower().endswith(".dat")]
        if not (f16 and q4): continue
        f16 = [f for f in f16 if 200e6 < sib[f] < 4.5e9]
        if not f16: continue
        f16.sort(key=lambda f: sib[f])                                     # smallest usable
        sz = sib[f16[0]]
        print(f"candidate {rid}\n  f16 {f16[0]} ({sz/1e9:.2f} GB)\n  q4  {q4[0]} ({sib[q4[0]]/1e9:.2f} GB)\n  imatrix {im[0] if im else 'NONE'}")
        meta = dict(rid=rid, f16=f16[0], q4=q4[0], imatrix=im[0] if im else None, f16_bytes=sz, q4_bytes=sib[q4[0]])
        ok = dl(rid, f16[0], f"{W}/f16.gguf") and dl(rid, q4[0], f"{W}/pub_q4.gguf")
        if im: dl(rid, im[0], f"{W}/imatrix.dat")
        if not ok: continue
        # what version made the published file? GGUF records it in general.quantization_version /
        # and often general.file_type; the header reader gives us the KV map
        json.dump(meta, open(f"{W}/meta.json", "w"), indent=1)
        print("  downloaded. FIND_DONE"); return
    print("no candidate with f16 + q4_k_m in the first 400; FIND_FAILED")

def quantize():
    meta = json.load(open(f"{W}/meta.json"))
    q = "/root/llama.cpp/build/bin/llama-quantize"
    if not os.path.exists(q): print("llama-quantize not built"); return
    rev = open("/root/llama-rev.txt").read().strip() if os.path.exists("/root/llama-rev.txt") else "?"
    args = [q]
    if meta.get("imatrix") and os.path.exists(f"{W}/imatrix.dat"): args += ["--imatrix", f"{W}/imatrix.dat"]
    args += [f"{W}/f16.gguf", f"{W}/our_q4.gguf", "Q4_K_M"]
    print("running:", " ".join(args), f"\n  llama.cpp {rev}")
    r = subprocess.run(args, capture_output=True, text=True, timeout=3600)
    if r.returncode != 0: print(r.stderr[-1500:]); print("QUANTIZE_FAILED"); return
    a, b = f"{W}/pub_q4.gguf", f"{W}/our_q4.gguf"
    sa, sb = os.path.getsize(a), os.path.getsize(b)
    print(f"  published {sa:,} B   ours {sb:,} B   {'SAME SIZE' if sa == sb else 'SIZE DIFFERS'}")
    ha, hb = sha(a), sha(b)
    if ha == hb:
        print("  sha256 IDENTICAL -- the recipe reproduces the file exactly. RECIPE_EXACT"); return
    # not identical: compare tensor by tensor so the residual is quantified, not just declared
    HA, HB = read_header_file(a), read_header_file(b)
    print(f"  sha differs. tensors: published {len(HA['tensors'])}, ours {len(HB['tensors'])}")
    tot = same = 0; diff_bytes = 0; kinds = {}
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for name, ta in HA["tensors"].items():
            tb = HB["tensors"].get(name)
            if not tb or ta["ttype"] != tb["ttype"] or ta["dims"] != tb["dims"]:
                kinds.setdefault("type/shape differs", []).append(name); continue
            n = ta["nbytes"]; tot += n
            fa.seek(HA["data_start"] + ta["offset"]); fb.seek(HB["data_start"] + tb["offset"])
            xa = np.frombuffer(fa.read(n), dtype=np.uint8); xb = np.frombuffer(fb.read(n), dtype=np.uint8)
            if xa.tobytes() == xb.tobytes(): same += n
            else:
                d = int(np.count_nonzero(xa != xb)); diff_bytes += d
                kinds.setdefault(f"ttype {ta['ttype']}", []).append((name, d / n))
    print(f"  tensor bytes identical: {100*same/max(tot,1):.2f}%   differing bytes overall: {100*diff_bytes/max(tot,1):.3f}%")
    for k, v in kinds.items():
        if isinstance(v[0], tuple):
            fr = [x[1] for x in v]
            print(f"    {k}: {len(v)} tensors differ, byte-diff fraction median {100*np.median(fr):.2f}%")
        else:
            print(f"    {k}: {len(v)}  e.g. {v[:3]}")
    print("RECIPE_MEASURED")

def read_header_file(path):
    """minimal GGUF header walk on a local file -> tensors with offset/nbytes/ttype/dims"""
    import struct
    TS = {0: (4, 1), 1: (2, 1), 2: (18, 32), 3: (20, 32), 6: (22, 32), 7: (24, 32), 8: (34, 32), 9: (36, 32),
          10: (84, 256), 11: (110, 256), 12: (144, 256), 13: (176, 256), 14: (210, 256), 15: (292, 256),
          16: (66, 256), 17: (74, 256), 18: (56, 256), 19: (50, 256), 20: (26, 32), 21: (60, 256),
          22: (18, 256), 23: (34, 256), 24: (12, 32), 25: (36, 32), 26: (62, 256), 27: (74, 256),
          28: (72, 256), 29: (56, 256), 30: (2, 1), 34: (18, 32), 35: (34, 32), 36: (66, 32), 37: (12, 32), 38: (14, 32), 39: (16, 32)}
    with open(path, "rb") as f:
        b = f.read(64 << 20)
    p = 4; ver, = struct.unpack("<I", b[p:p+4]); p += 4
    nt, nkv = struct.unpack("<QQ", b[p:p+16]); p += 16
    def rstr():
        nonlocal p
        ln, = struct.unpack("<Q", b[p:p+8]); p += 8; s = b[p:p+ln]; p += ln; return s.decode("utf-8", "replace")
    SZ1 = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    def skip(t):
        nonlocal p
        if t == 8: rstr()
        elif t == 9:
            et, = struct.unpack("<I", b[p:p+4]); p += 4; n, = struct.unpack("<Q", b[p:p+8]); p += 8
            for _ in range(n): skip(et)
        else: p += SZ1[t]
    align = 32
    for _ in range(nkv):
        k = rstr(); t, = struct.unpack("<I", b[p:p+4]); p += 4
        if k == "general.alignment" and t == 4:
            align, = struct.unpack("<I", b[p:p+4])
        skip(t)
    tens = {}
    for _ in range(nt):
        name = rstr(); nd, = struct.unpack("<I", b[p:p+4]); p += 4
        dims = list(struct.unpack("<" + "Q" * nd, b[p:p+8*nd])); p += 8 * nd
        tt, = struct.unpack("<I", b[p:p+4]); p += 4; off, = struct.unpack("<Q", b[p:p+8]); p += 8
        nel = int(np.prod(dims)); bs, bl = TS.get(tt, (0, 1))
        tens[name] = dict(dims=dims, ttype=tt, offset=off, nbytes=(nel // bl) * bs if bs else 0)
    ds = (p + align - 1) // align * align
    return dict(tensors=tens, data_start=ds)

if __name__ == "__main__":
    if "--find" in sys.argv: find()
    elif "--quantize" in sys.argv: quantize()
    else: print("use --find or --quantize")
