#!/usr/bin/env python3
"""Does the quantisation prediction cross a repo boundary?

Every quant result so far needs the reference INSIDE the same repo -- a sibling Q8_0. But the Hub's own
tags say base_model:quantized is 27.7% of weight bytes, and most of those parents live in a different
repo. If a GGUF can be predicted from its declared base's safetensors, the lever stops being a property
of multi-file repos and becomes a property of a quarter of all weight bytes.

Q8_0 is the cleanest test in the format: per block of 32, d = amax/127 and q = round(x/d). No imatrix,
no search, no calibration -- if the GGUF was made from the declared base by llama.cpp, this reproduces
it exactly. A miss means a transform was applied first (RESULT 59's per-channel rescale, or AWQ)."""
import json, os, struct, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32

def q8_blocks(raw):
    n = len(raw) // 34
    a = raw[:n*34].reshape(n, 34)
    d = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    q = a[:, 2:34].view(np.int8).astype(np.int32)
    return d, q

def main():
    pairs = json.load(open(sys.argv[1])) if len(sys.argv) > 1 else None
    if not pairs:
        tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
        pairs = []
        for rid, t in tags.items():
            for x in (t.get("tags") or []):
                p = x.split(":")
                if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                    pairs.append((rid, ":".join(p[2:])))
                    break
    print(f"declared quantized-of pairs available: {len(pairs)}")
    done = 0
    for rid, base in pairs:
        if done >= 6: break
        d = api(f"{H}/api/models/{rid}")
        if not d: continue
        ggufs = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
        q8 = next((f for f in ggufs if "q8_0" in f.lower()), None)
        if not q8: continue
        db = api(f"{H}/api/models/{base}")
        if not db: continue
        sts = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
        if not sts: continue
        hg = read_header(rid, q8)
        if not hg: continue
        cands = [n for n, m in hg["tensors"].items() if m["ttype"] == 8]
        if not cands: continue
        cands.sort(key=lambda n: -int(np.prod(hg["tensors"][n]["dims"])))
        hit = None
        for fn in sts[:8]:
            hb, offb = st_header(base, fn)
            if not hb: continue
            for gname in cands[:40]:
                stem = gname.replace("blk.", "model.layers.").replace(".weight", "")
                for k in hb:
                    if not isinstance(hb[k], dict) or "data_offsets" not in hb[k]: continue
                    if hb[k].get("dtype") not in ("BF16", "F16"): continue
                    kk = k.lower()
                    if gname.split(".")[-2] in kk and any(s in kk for s in stem.split(".")[:3]):
                        hit = (fn, offb, k, hb[k], gname); break
                if hit: break
            if hit: break
        if not hit:
            print(f"  {rid[:40]:40s} no tensor-name match against {base[:34]}"); continue
        fn, offb, kname, meta, gname = hit
        gm = hg["tensors"][gname]
        gdims = list(gm["dims"]); sdims = list(meta["shape"])
        if sorted(gdims) != sorted(sdims):
            print(f"  {rid[:40]:40s} shape {gdims} vs {sdims}"); continue
        nel = int(np.prod(sdims)); nb = min(20000, nel // 32)
        rg = fetch(rid, q8, hg["data_start"] + gm["offset"], nb * 34)
        rw = grab(base, fn, offb + meta["data_offsets"][0], nb * 32 * 2)
        if rg is None or rw is None:
            print(f"  {rid[:40]:40s} fetch failed"); continue
        dq, q = q8_blocks(rg)
        w = bf16_to_f32(np.frombuffer(rw.tobytes(), dtype=np.uint16)).astype(np.float32)
        n = min(len(q), len(w) // 32); q = q[:n]; w = w[:n*32].reshape(n, 32)
        amax = np.abs(w).max(axis=1)
        dpred = (amax / 127.0).astype(np.float16).astype(np.float32)
        with np.errstate(divide="ignore", invalid="ignore"):
            pred = np.where(dpred[:, None] != 0, np.rint(w / dpred[:, None]), 0)
        pred = np.clip(pred, -127, 127).astype(np.int32)
        exact = float(np.mean(pred == q))
        dmatch = float(np.mean(dpred == dq[:n]))
        resid = (q - pred).ravel()
        alone = h0(q.ravel()) + 16.0/32; cost = h0(resid) + 16.0/32
        print(f"  {rid[:38]:38s} <- {base[:30]:30s} {gname[:22]:22s} "
              f"scale {100*dmatch:5.1f}%  codes {100*exact:6.2f}%  {100*cost/alone:5.1f}% ({alone/cost:.1f}x)")
        done += 1
    print("CROSSREPO_DONE")

if __name__ == "__main__":
    main()
