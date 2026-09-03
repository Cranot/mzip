#!/usr/bin/env python3
"""Second publisher, and the imatrix path: does the recipe stay exact when an importance matrix is in it?

RESULT 99 reproduced a mradermacher static quant byte for byte from the f32 source. n=1, no imatrix.
bartowski publishes imatrix.dat beside every quant and converts differently (bf16 is his usual
outtype). If convert(source) at some outtype + llama-quantize --imatrix reproduces his Q4_K_M exactly,
the recipe covers the dominant publisher AND the imatrix path.

    python3 recipe4.py <quant-repo> [target-quant-type]
Fetches the parent from the GGUF's general.source.url / base_model tag, converts at f32, bf16 and f16,
quantises each (with the imatrix when present), and compares tensor by tensor."""
import json, os, sys, subprocess, urllib.request
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import api, read_header
from recipe import dl, read_header_file
from recipe3 import BS
TOK = os.environ.get("HF_TOKEN", "")
Q = "/root/llama.cpp/build/bin/llama-quantize"; PY = "/root/llama.cpp/cvenv/bin/python"
CV = "/root/llama.cpp/convert_hf_to_gguf.py"

def tree(rid):
    t = api(f"{H}/api/models/{rid}/tree/main?recursive=1&expand=1")
    return {f["path"]: int(f.get("size") or (f.get("lfs") or {}).get("size") or 0)
            for f in (t or []) if isinstance(f, dict) and f.get("type") == "file"}

def compare(a, b):
    HA, HB = read_header_file(a), read_header_file(b)
    ta, tb = HA["tensors"], HB["tensors"]; tot = same = bt = bsame = 0; ident = 0; n = 0
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for k in ta:
            if k not in tb or ta[k]["ttype"] != tb[k]["ttype"] or ta[k]["dims"] != tb[k]["dims"]: continue
            nb = ta[k]["nbytes"]
            if not nb: continue
            fa.seek(HA["data_start"] + ta[k]["offset"]); fb.seek(HB["data_start"] + tb[k]["offset"])
            xa = np.frombuffer(fa.read(nb), dtype=np.uint8); xb = np.frombuffer(fb.read(nb), dtype=np.uint8)
            if xa.size != xb.size: continue
            n += 1; eq = xa == xb; tot += nb; same += int(eq.sum()); ident += int(eq.all())
            bs = BS.get(ta[k]["ttype"])
            if bs and nb % bs == 0:
                blk = ~(~eq).reshape(-1, bs).any(axis=1); bt += blk.size; bsame += int(blk.sum())
    return dict(tensors=n, ident=ident, bytes_same=100 * same / max(tot, 1), blocks_same=100 * bsame / max(bt, 1), blocks=bt)

def main():
    rid = sys.argv[1]; want = (sys.argv[2] if len(sys.argv) > 2 else "Q4_K_M")
    W = f"{B}/recipe4/{rid.replace('/', '__')}"; os.makedirs(f"{W}/src", exist_ok=True)
    files = tree(rid)
    q = [f for f in files if f.lower().endswith(".gguf") and want.lower() in f.lower() and "-0000" not in f and "mmproj" not in f.lower()]
    # bartowski names it <model>.imatrix, mradermacher imatrix.dat; requiring .dat silently dropped
    # bartowski's and would have made the recipe fail for the wrong reason
    im = [f for f in files if "imatrix" in f.lower() and not f.lower().endswith((".md", ".json", ".txt"))]
    im.sort(key=lambda f: files[f], reverse=True)
    if not q: print("no target quant file"); return
    q = min(q, key=lambda f: files[f])
    print(f"{rid}\n  target {q} ({files[q]/1e9:.2f} GB)\n  imatrix {im[0] if im else 'NONE'}")
    dl(rid, q, f"{W}/pub.gguf")
    if im: dl(rid, im[0], f"{W}/imatrix.dat")
    # parent: GGUF KV general.source.url or base_model tag
    hg = read_header(rid, q)
    src = None
    for k in ("general.source.url", "general.base_model.0.repo_url", "general.source.repo_url"):
        v = (hg.get("kv") or {}).get(k) if hg else None
        if isinstance(v, str) and "huggingface.co/" in v: src = v.split("huggingface.co/")[1].strip("/"); break
    if not src:
        tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
        for x in ((tags.get(rid) or {}).get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3: src = ":".join(p[2:]); break
    if not src: print("  no parent found"); return
    print(f"  parent {src}")
    sf = tree(src)
    need = [f for f in sf if not f.lower().endswith((".gguf", ".md", ".gitattributes", ".bin", ".pt", ".pth", ".msgpack", ".h5"))
            and not f.startswith(".")]
    total = sum(sf[f] for f in need)
    print(f"  fetching {len(need)} files, {total/1e9:.2f} GB")
    if total > 40e9: print("  too large for this box; skipping"); return
    for f in need:
        os.makedirs(os.path.dirname(f"{W}/src/{f}") or f"{W}/src", exist_ok=True)
        if not dl(src, f, f"{W}/src/{f}"): print("  fetch failed", f); return
    print("  parent downloaded")
    results = {}
    for ot in ("f32", "bf16", "f16"):
        conv = f"{W}/conv_{ot}.gguf"; out = f"{W}/q_{ot}.gguf"
        r = subprocess.run([PY, CV, f"{W}/src", "--outtype", ot, "--outfile", conv], capture_output=True, text=True, timeout=3600)
        if r.returncode != 0: print(f"  convert {ot} failed: {r.stderr[-300:]}"); continue
        args = [Q] + (["--imatrix", f"{W}/imatrix.dat"] if im else []) + [conv, out, want]
        r = subprocess.run(args, capture_output=True, text=True, timeout=3600)
        if r.returncode != 0: print(f"  quantize from {ot} failed: {r.stderr[-300:]}"); continue
        c = compare(out, f"{W}/pub.gguf"); results[ot] = c
        print(f"  from {ot:4s}: tensors {c['tensors']}, identical {c['ident']}, bytes same {c['bytes_same']:.3f}%, "
              f"superblocks same {c['blocks_same']:.2f}% of {c['blocks']:,}")
        os.remove(conv)
    exact = [ot for ot, c in results.items() if c["bytes_same"] >= 99.9999 and c["ident"] == c["tensors"]]
    print(f"\n  EXACT from: {exact if exact else 'NONE'}   (imatrix {'used' if im else 'absent'})")
    json.dump(dict(rid=rid, target=q, imatrix=bool(im), parent=src, results=results, exact=exact),
              open(f"{W}/result.json", "w"), indent=1)
    print("RECIPE4_DONE")

if __name__ == "__main__":
    main()
