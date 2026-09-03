#!/usr/bin/env python3
"""Stop guessing what happens to Q, and find the row correspondence directly.

attn_v reproduces at 99.27% and attn_q at 3.68% in the same repos, same code path -- so the arithmetic
is right and the row ORDER is wrong. Two guesses have already failed (llama.cpp's LLaMA permutation,
applied whole and applied to a prefix). Rather than guess a third time, this recovers the mapping from
the data.

Each Q8_0 block of 32 stores d = amax/127, so 127*d recovers that block's maximum magnitude exactly.
The same statistic is computable from the base's rows. Matching GGUF blocks to base blocks on that
signature shows what the converter actually did: identity, a head-halves interleave, a transpose, or
nothing recognisable."""
import json, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import to_st, q8_blocks
from crossrepo4 import shard_map, permute

def main():
    rid = sys.argv[1] if len(sys.argv) > 1 else None
    base = sys.argv[2] if len(sys.argv) > 2 else None
    if not rid:
        tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
        for r, t in tags.items():
            for x in (t.get("tags") or []):
                p = x.split(":")
                if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized" and "gguf" in r.lower():
                    rid, base = r, ":".join(p[2:]); break
            if rid: break
    print(f"{rid}  <-  {base}")
    d = api(f"{H}/api/models/{rid}")
    q8 = next((s["rfilename"] for s in d.get("siblings", [])
               if "q8_0" in s["rfilename"].lower() and s["rfilename"].lower().endswith(".gguf")), None)
    hg = read_header(rid, q8)
    db = api(f"{H}/api/models/{base}")
    sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    wmap, cache = shard_map(base, sb)
    cfg = api(f"{H}/{base}/resolve/main/config.json") or {}
    nh = int(cfg.get("num_attention_heads") or 0); nkv = int(cfg.get("num_key_value_heads") or nh)
    print(f"  arch {cfg.get('model_type')}  heads {nh}  kv heads {nkv}  hidden {cfg.get('hidden_size')}")
    for kind in ("attn_q", "attn_v"):
        cands = [(n, m) for n, m in hg["tensors"].items()
                 if m["ttype"] == 8 and to_st(n) and f".{kind}." in n]
        if not cands: print(f"  {kind}: none"); continue
        gname, gm = min(cands, key=lambda c: int(np.prod(c[1]["dims"])))
        sname = to_st(gname); shard = wmap.get(sname)
        if not shard: print(f"  {kind}: {sname} absent"); continue
        if shard not in cache: cache[shard] = st_header(base, shard)
        hb, offb = cache[shard]
        meta = hb[sname]
        rows, cols = meta["shape"][0], int(np.prod(meta["shape"][1:]))
        nel = rows * cols
        if nel * 2 > (64 << 20): print(f"  {kind}: too large"); continue
        rg = fetch(rid, q8, hg["data_start"] + gm["offset"], (nel // 32) * 34)
        rw = grab(base, shard, offb + meta["data_offsets"][0], nel * 2)
        if rg is None or rw is None: print(f"  {kind}: fetch failed"); continue
        dq, _q = q8_blocks(rg)
        r16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
        w = (bf16_to_f32(r16) if meta["dtype"] == "BF16" else r16.view(np.float16).astype(np.float32)).astype(np.float32)
        nb = min(len(dq), nel // 32)
        gmax = (dq[:nb].astype(np.float64) * 127.0)                     # block maxima, from the GGUF
        bmax = np.abs(w[:nb*32].reshape(nb, 32)).max(axis=1).astype(np.float64)
        print(f"\n  {kind}: {sname}  shape {meta['shape']}  blocks {nb}  ({cols//32} blocks per row)")
        def agree(x, y):
            n = min(len(x), len(y))
            return 100.0 * np.mean(np.isclose(x[:n], y[:n], rtol=1e-3))
        print(f"    identity order                 {agree(gmax, bmax):6.2f}%")
        for heads, lbl in ((nh, f"permute n_head={nh}"), (nkv, f"permute n_kv={nkv}")):
            if not heads: continue
            wm = w[:rows*cols].reshape(rows, cols)
            pw = permute(wm, heads)
            if pw is None: print(f"    {lbl:30s} shape refuses"); continue
            pmax = np.abs(pw.ravel()[:nb*32].reshape(nb, 32)).max(axis=1).astype(np.float64)
            print(f"    {lbl:30s} {agree(gmax, pmax):6.2f}%")
        wt = w[:rows*cols].reshape(rows, cols).T.copy()
        tmax = np.abs(wt.ravel()[:nb*32].reshape(nb, 32)).max(axis=1).astype(np.float64)
        print(f"    transposed                     {agree(gmax, tmax):6.2f}%")
        # where does GGUF block 0 actually live in the base?
        bpr = max(cols // 32, 1)
        allb = np.abs(w[:(len(w)//32)*32].reshape(-1, 32)).max(axis=1).astype(np.float64)
        for gi in (0, 1, bpr, 2*bpr):
            if gi >= nb: continue
            hits = np.nonzero(np.isclose(allb, gmax[gi], rtol=1e-6))[0]
            print(f"    gguf block {gi:5d} (row {gi//bpr:4d}) matches base blocks "
                  f"{hits[:6].tolist()}{' ...' if len(hits) > 6 else ''}"
                  f"  -> base rows {sorted(set((hits[:6]//bpr).tolist()))}")
    print("QDIAG_DONE")

if __name__ == "__main__":
    main()
