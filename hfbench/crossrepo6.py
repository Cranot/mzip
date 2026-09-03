#!/usr/bin/env python3
"""Push past the dtype wall: F32 and FP8 bases are usable, and I was discarding them.

Across the prediction runs, 'base dtype F32' rejected 25 pairs and 'base dtype F8_E4M3' more. Neither
is a real obstacle. F32 is the most precise input there is. FP8 is a quantised checkpoint, but
llama.cpp's converter dequantises it before quantising, so reproducing what the converter saw means
doing the same: decode E4M3 and multiply by the stored per-block scale (RESULT 83 built and validated
that decoder at 239/240 patterns).

This measures the JOINT rate that actually matters for a deployment -- names resolve AND the base's
dtype is usable -- and the prediction quality on the dtypes that were previously thrown away."""
import json, os, re, sys, collections
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, h0, api
from fp8_given_base import header as st_header, grab, bf16_to_f32
from crossrepo2 import q8_blocks
from crossrepo4 import shard_map, permute
from hfget import get_json
from namemap import candidates, expert_names
from alltypes import dec_target, TT, SZ
from fp8b import e4m3_to_f32
CODE_BITS = {12: 4, 13: 5, 14: 6}
USABLE = ("BF16", "F16", "F32", "F8_E4M3")

def scale_key(h, name):
    for k in (name + "_scale", name + "_scale_inv", name.replace(".weight", ".weight_scale"),
              name.replace(".weight", ".weight_scale_inv")):
        if k in h and isinstance(h[k], dict): return k
    return None

def load_weights(rid, shard, off, h, name, nel):
    """return the first nel weights as float32, whatever dtype the base stores them in"""
    m = h[name]; dt = m["dtype"]
    if dt in ("BF16", "F16"):
        raw = grab(rid, shard, off + m["data_offsets"][0], nel * 2)
        if raw is None: return None
        r = np.frombuffer(raw.tobytes(), dtype=np.uint16)
        return (bf16_to_f32(r) if dt == "BF16" else r.view(np.float16).astype(np.float32)).astype(np.float32)
    if dt == "F32":
        raw = grab(rid, shard, off + m["data_offsets"][0], nel * 4)
        if raw is None: return None
        return np.frombuffer(raw.tobytes(), dtype=np.float32).astype(np.float32)
    if dt == "F8_E4M3":
        sk = scale_key(h, name)
        if not sk: return None
        rows, cols = m["shape"][0], int(np.prod(m["shape"][1:]))
        nrow = max(1, min(rows, -(-nel // max(cols, 1))))
        raw = grab(rid, shard, off + m["data_offsets"][0], nrow * cols)
        sm = h[sk]
        sraw = grab(rid, shard, off + sm["data_offsets"][0],
                    sm["data_offsets"][1] - sm["data_offsets"][0])
        if raw is None or sraw is None: return None
        srows, scols = (sm["shape"] + [1])[:2] if len(sm["shape"]) < 2 else sm["shape"][:2]
        bh = max(1, rows // max(srows, 1)); bw = max(1, cols // max(scols, 1))
        sv = np.frombuffer(sraw.tobytes(), dtype=np.uint16) if sm["dtype"] == "BF16" \
             else np.frombuffer(sraw.tobytes(), dtype=np.float32)
        sc = (bf16_to_f32(sv) if sm["dtype"] == "BF16" else sv).astype(np.float32)
        need = srows * scols
        if sc.size < need: return None
        sc = sc[:need].reshape(srows, scols)
        q = np.frombuffer(raw.tobytes(), dtype=np.uint8)[:nrow * cols].reshape(nrow, cols)
        rep = np.repeat(np.repeat(sc, bh, axis=0), bw, axis=1)
        if rep.shape[0] < nrow or rep.shape[1] < cols: return None
        w = e4m3_to_f32(q).astype(np.float32) * rep[:nrow, :cols]
        return w.ravel()[:nel]
    return None

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 130
    why = collections.Counter(); bydt = collections.defaultdict(list)
    att = 0; name_ok = 0; joint_ok = 0
    dtseen = collections.Counter()
    print(f"{'repo':24s} {'base dtype':11s} {'kind':12s} {'type':6s} {'exact':>8s} {'cost':>7s}")
    for rid, base in order:
        if att >= LIMIT: break
        att += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: why["repo api failed"] += 1; continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: why["no gguf file"] += 1; continue
            db = api(f"{H}/api/models/{base}")
            if not db: why["base gone or gated"] += 1; continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: why["base publishes no safetensors"] += 1; continue
            gf = min(gg, key=len)
            hg = read_header(rid, gf)
            if not hg or not hg.get("tensors"): why["gguf header unreadable"] += 1; continue
            wmap, cache = shard_map(base, sb)
            if not wmap: why["no shard map"] += 1; continue
        except Exception:
            why["exception"] += 1; continue
        tot = hit = usable = 0.0; picks = []
        for gname, gm in hg["tensors"].items():
            sz = int(np.prod(gm["dims"]))
            if sz < 4096: continue
            tot += sz
            sn = next((c for c in candidates(gname) if c in wmap), None)
            if not sn: continue
            hit += sz
            shard = wmap[sn]
            if shard not in cache: cache[shard] = st_header(base, shard)
            hb, offb = cache[shard]
            if not hb or sn not in hb: continue
            dt = hb[sn].get("dtype"); dtseen[dt] += 1
            if dt not in USABLE: continue
            usable += sz
            picks.append((gname, gm, sn, shard, dt, sz))
        if tot <= 0: why["no sizeable tensors"] += 1; continue
        if hit / tot >= 0.8: name_ok += 1
        if usable / tot >= 0.8: joint_ok += 1
        else: why[f"names ok but dtype unusable ({sorted(set(dtseen))[:1]})"] += 0
        # measure one previously-discarded dtype per repo
        odd = [p for p in picks if p[4] in ("F32", "F8_E4M3")]
        if not odd: continue
        gname, gm, sn, shard, dt, _sz = max(odd, key=lambda p: p[5])
        hb, offb = cache[shard]
        meta = hb[sn]
        gd = list(gm["dims"]); sd = list(meta["shape"])
        if expert_names(gname):
            per = int(np.prod(gd[:2])) if len(gd) >= 3 else int(np.prod(gd))
            if per != int(np.prod(sd)): why["expert slice mismatch"] += 1; continue
        elif sorted(gd) != sorted(sd):
            why["shape mismatch"] += 1; continue
        tt = gm["ttype"]
        if tt not in SZ and tt != 8: why[f"target type {TT.get(tt,tt)} not decodable"] += 1; continue
        unit = 256 if tt in SZ else 32
        nunits = min(int(np.prod(sd)) // unit, (2 << 20) // unit)
        if nunits < 60: why["too small"] += 1; continue
        rg = fetch(rid, gf, hg["data_start"] + gm["offset"], nunits * (SZ[tt] if tt in SZ else 34))
        w = load_weights(base, shard, offb, hb, sn, nunits * unit)
        if rg is None or w is None: why[f"fetch/decode failed ({dt})"] += 1; continue
        kind = ("embedding" if gname == "token_embd.weight" else
                "output" if gname == "output.weight" else
                (gname.split(".")[2] if gname.startswith("blk.") else "other"))
        if kind in ("attn_q", "attn_k"):
            why["skipped q/k (needs whole tensor)"] += 1; continue
        if tt == 8:
            dq, q = q8_blocks(rg)
            n = min(len(q), len(w) // 32)
            if n < 60: why["short overlap"] += 1; continue
            q = q[:n]; x = w[:n*32].reshape(n, 32)
            dp = (np.abs(x).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
            with np.errstate(divide="ignore", invalid="ignore"):
                pred = np.where(dp[:, None] != 0, np.rint(x / dp[:, None]), 0)
            pred = np.clip(pred, -127, 127).astype(np.int32)
            resid = (q - pred).ravel(); alone = h0(q.ravel()) + 0.5; cost = h0(resid) + 0.5; lbl = "Q8_0"
        else:
            try: q, D, M, off, nmax = dec_target(rg, tt)
            except Exception: why["no decoder"] += 1; continue
            n = min(len(q), len(w) // 256)
            if n < 60: why["short overlap"] += 1; continue
            q = q[:n]; D = D[:n]; M = M[:n]; x = w[:n*256].reshape(n, 256)
            with np.errstate(divide="ignore", invalid="ignore"):
                pred = np.where(D != 0, np.rint((x + M) / D) + off, 0)
            pred = np.clip(pred, 0, nmax).astype(np.int32)
            resid = (q - pred).ravel()
            sbb = (SZ[tt]*8 - CODE_BITS[tt]*256) / 256.0
            alone = h0(q.ravel()) + sbb; cost = h0(resid) + sbb; lbl = TT[tt]
        ex = float(np.mean(resid == 0))
        bydt[dt].append(dict(exact=ex, alone=alone, cost=cost))
        print(f"{rid[:24]:24s} {dt:11s} {kind[:12]:12s} {lbl:6s} {100*ex:7.2f}% {100*cost/alone:6.1f}%")
    print(f"\npairs attempted {att}")
    print(f"  names resolve (>=80%):                 {name_ok}  = {100*name_ok/max(att,1):.1f}%")
    print(f"  names resolve AND dtype usable:        {joint_ok}  = {100*joint_ok/max(att,1):.1f}%")
    print(f"  base dtypes seen across mapped tensors: {dict(dtseen.most_common(8))}")
    for dt, v in bydt.items():
        a = sum(x["alone"] for x in v); c = sum(x["cost"] for x in v)
        print(f"\n  base dtype {dt}: {len(v)} tensors, mean exact "
              f"{100*np.mean([x['exact'] for x in v]):.2f}%, {100*c/a:.1f}% of standalone ({a/c:.1f}x)")
    print("\nwhy the rest produced nothing:")
    for r, n in why.most_common(10):
        if n: print(f"  {n:5d}  {r}")
    json.dump({"att": att, "name_ok": name_ok, "joint_ok": joint_ok,
               "dtseen": dict(dtseen), "bydt": {k: v for k, v in bydt.items()}, "why": dict(why)},
              open(f"{B}/qgq/crossrepo6.json", "w"), indent=1, default=float)
    print("CROSSREPO6_DONE")

if __name__ == "__main__":
    main()
