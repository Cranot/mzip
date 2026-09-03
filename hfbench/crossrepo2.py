#!/usr/bin/env python3
"""Does the quantisation prediction cross a repo boundary? Second attempt, with a real name mapping.

The first attempt returned nothing, and the fault was the join, not the hypothesis: it matched GGUF
tensor names to safetensors names by substring, which almost never fires. llama.cpp's naming is a
fixed, documented mapping, so it can be written down exactly. It also scanned shards blindly instead
of reading model.safetensors.index.json, which says which shard holds which tensor.

The hypothesis under test is the cleanest in the format. Q8_0 stores, per block of 32 weights,
d = amax/127 as fp16 and q = round(x/d) as int8. There is no imatrix, no calibration and no search --
if llama.cpp made this GGUF from the declared base, the arithmetic reproduces it exactly. A miss means
a transform was applied to the weights first, which is what RESULT 59 found for the official Qwen
GGUFs and RESULT 83 found for AWQ."""
import json, os, re, sys
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32

SUF = {"attn_q": "self_attn.q_proj", "attn_k": "self_attn.k_proj", "attn_v": "self_attn.v_proj",
       "attn_output": "self_attn.o_proj", "ffn_gate": "mlp.gate_proj", "ffn_up": "mlp.up_proj",
       "ffn_down": "mlp.down_proj"}

def to_st(g):
    """llama.cpp GGUF tensor name -> HF safetensors name"""
    if g == "token_embd.weight": return "model.embed_tokens.weight"
    if g == "output.weight": return "lm_head.weight"
    m = re.match(r"blk\.(\d+)\.([a-z_]+)\.weight$", g)
    if not m: return None
    n, part = m.group(1), m.group(2)
    if part not in SUF: return None
    return f"model.layers.{n}.{SUF[part]}.weight"

def index_map(rid):
    """tensor name -> shard file, from the safetensors index when there is one"""
    b = api(f"{H}/{rid}/resolve/main/model.safetensors.index.json")
    if isinstance(b, dict) and "weight_map" in b: return b["weight_map"]
    return None

def q8_blocks(raw):
    n = len(raw) // 34
    a = raw[:n*34].reshape(n, 34)
    d = a[:, 0:2].copy().view(np.float16).astype(np.float32).ravel()
    q = a[:, 2:34].view(np.int8).astype(np.int32)
    return d, q

def try_pair(rid, base):
    d = api(f"{H}/api/models/{rid}")
    if not d: return None
    ggufs = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8 = next((f for f in ggufs if "q8_0" in f.lower()), None)
    if not q8: return None
    db = api(f"{H}/api/models/{base}")
    if not db: return None
    sts = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
    if not sts: return None
    hg = read_header(rid, q8)
    if not hg: return None
    wmap = index_map(base) or {}
    cands = [(n, m) for n, m in hg["tensors"].items() if m["ttype"] == 8 and to_st(n)]
    if not cands: return None
    cands.sort(key=lambda c: -int(np.prod(c[1]["dims"])))
    hdr_cache = {}
    for gname, gm in cands[:25]:
        sname = to_st(gname)
        shard = wmap.get(sname) or (sts[0] if len(sts) == 1 else None)
        if not shard: continue
        if shard not in hdr_cache:
            hdr_cache[shard] = st_header(base, shard)
        hb, offb = hdr_cache[shard]
        if not hb or sname not in hb: continue
        meta = hb[sname]
        if meta.get("dtype") not in ("BF16", "F16"): continue
        if sorted(meta["shape"]) != sorted(list(gm["dims"])): continue
        nel = int(np.prod(meta["shape"])); nb = min(20000, nel // 32)
        if nb < 200: continue
        rg = fetch(rid, q8, hg["data_start"] + gm["offset"], nb * 34)
        rw = grab(base, shard, offb + meta["data_offsets"][0], nb * 32 * 2)
        if rg is None or rw is None: continue
        dq, q = q8_blocks(rg)
        raw16 = np.frombuffer(rw.tobytes(), dtype=np.uint16)
        w = (bf16_to_f32(raw16) if meta["dtype"] == "BF16"
             else raw16.view(np.float16).astype(np.float32)).astype(np.float32)
        n = min(len(q), len(w) // 32)
        if n < 200: continue
        q = q[:n]; w = w[:n*32].reshape(n, 32)
        amax = np.abs(w).max(axis=1)
        dpred = (amax / 127.0).astype(np.float16).astype(np.float32)
        with np.errstate(divide="ignore", invalid="ignore"):
            pred = np.where(dpred[:, None] != 0, np.rint(w / dpred[:, None]), 0)
        pred = np.clip(pred, -127, 127).astype(np.int32)
        resid = (q - pred).ravel()
        alone = h0(q.ravel()) + 0.5; cost = h0(resid) + 0.5
        return dict(gname=gname, sname=sname, blocks=n,
                    scale=float(np.mean(dpred == dq[:n])), exact=float(np.mean(pred == q)),
                    near=float(np.mean(np.abs(resid) <= 1)), alone=alone, cost=cost)
    return None

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    gg = [p for p in pairs if "gguf" in p[0].lower()]
    order = gg + [p for p in pairs if p not in gg]
    print(f"declared quantized-of pairs: {len(pairs)} ({len(gg)} name themselves GGUF)\n")
    print(f"{'repo':30s} {'base':26s} {'tensor':16s} {'blk':>6s} {'scale':>7s} {'codes':>8s} "
          f"{'|r|<=1':>8s} {'resid':>7s}")
    ok = 0; tried = 0
    for rid, base in order:
        if ok >= 6 or tried >= 90: break
        tried += 1
        try: r = try_pair(rid, base)
        except Exception: r = None
        if not r: continue
        ok += 1
        print(f"{rid[:30]:30s} {base[:26]:26s} {r['gname'].split('.')[-2][:16]:16s} {r['blocks']:6d} "
              f"{100*r['scale']:6.1f}% {100*r['exact']:7.2f}% {100*r['near']:7.2f}% "
              f"{100*r['cost']/r['alone']:6.1f}%")
    print(f"\n  pairs attempted {tried}, measured {ok}")
    print("CROSSREPO2_DONE")

if __name__ == "__main__":
    main()
