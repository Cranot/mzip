#!/usr/bin/env python3
"""Every cost in this programme charges for the block scales. For Q8_0 that charge is wrong.

Q8_0 stores d = amax/127 as fp16 per 32 weights, and amax comes from the SAME base weights the codes
come from. Across every cross-repo pair measured, the predicted d matched the stored d exactly -- 100%
of blocks, reported but never acted on. A quantity reproduced exactly does not need storing, so the
scale bits should come out of the cost, not sit in it.

This measures both policies side by side and reports the scale-match rate that licenses the cheaper
one. The K-quants are the control: their scales come from make_qkx2_quants, an iterative weighted
least-squares search rather than a formula, so they should NOT be reproducible from a closed form --
and if that is right, they must keep paying for their scales while Q8_0 stops."""
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
from crossrepo6 import load_weights, USABLE
CODE_BITS = {12: 4, 13: 5, 14: 6}

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    why = collections.Counter(); by = collections.defaultdict(list); att = done = 0
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    print(f"{'repo':22s} {'type':6s} {'kind':11s} {'scales exact':>13s} {'codes exact':>12s} "
          f"{'scales STORED':>14s} {'scales PREDICTED':>17s}")
    for rid, base in order:
        if done >= 7 or att >= LIMIT: break
        att += 1
        try:
            d = api(f"{H}/api/models/{rid}")
            if not d: continue
            gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
            if not gg: continue
            db = api(f"{H}/api/models/{base}")
            if not db: continue
            sb = sorted(s["rfilename"] for s in db.get("siblings", []) if s["rfilename"].endswith(".safetensors"))
            if not sb: continue
            wmap, cache = shard_map(base, sb)
            if not wmap: continue
        except Exception:
            continue
        got = 0
        for gf in sorted(gg, key=len)[:3]:
            try: hg = read_header(rid, gf)
            except Exception: hg = None
            if not hg or not hg.get("tensors"):
                why["gguf header unreadable"] += 1; continue
            picks = []
            for gname, gm in hg["tensors"].items():
                tt = gm["ttype"]
                if tt != 8 and tt not in CODE_BITS: continue
                if gname.startswith("blk.") and gname.split(".")[2] in ("attn_q", "attn_k"): continue
                sn = next((c for c in candidates(gname) if c in wmap), None)
                if not sn: continue
                picks.append((gname, gm, sn, int(np.prod(gm["dims"]))))
            if not picks: continue
            gname, gm, sn, _ = max(picks, key=lambda p: p[3])
            shard = wmap[sn]
            if shard not in cache: cache[shard] = st_header(base, shard)
            hb, offb = cache[shard]
            if not hb or sn not in hb: why["base header"] += 1; continue
            meta = hb[sn]
            if meta.get("dtype") not in USABLE: why[f"dtype {meta.get('dtype')}"] += 1; continue
            gd = list(gm["dims"]); sd = list(meta["shape"])
            if expert_names(gname):
                per = int(np.prod(gd[:2])) if len(gd) >= 3 else int(np.prod(gd))
                if per != int(np.prod(sd)): why["expert mismatch"] += 1; continue
            elif sorted(gd) != sorted(sd):
                why["shape mismatch"] += 1; continue
            tt = gm["ttype"]
            unit = 32 if tt == 8 else 256
            nunits = min(int(np.prod(sd)) // unit, (2 << 20) // unit)
            if nunits < 60: why["too small"] += 1; continue
            rg = fetch(rid, gf, hg["data_start"] + gm["offset"], nunits * (34 if tt == 8 else SZ[tt]))
            w = load_weights(base, shard, offb, hb, sn, nunits * unit)
            if rg is None or w is None: why["fetch/decode"] += 1; continue
            kind = ("embedding" if gname == "token_embd.weight" else
                    "output" if gname == "output.weight" else
                    (gname.split(".")[2] if gname.startswith("blk.") else "other"))
            if tt == 8:
                dq, q = q8_blocks(rg)
                n = min(len(q), len(w) // 32)
                if n < 60: why["short"] += 1; continue
                q = q[:n]; x = w[:n*32].reshape(n, 32)
                dp = (np.abs(x).max(axis=1) / 127.0).astype(np.float16).astype(np.float32)
                with np.errstate(divide="ignore", invalid="ignore"):
                    pred = np.where(dp[:, None] != 0, np.rint(x / dp[:, None]), 0)
                pred = np.clip(pred, -127, 127).astype(np.int32)
                resid = (q - pred).ravel()
                sbb = 16.0 / 32.0                       # one fp16 scale per 32 weights
                smatch = float(np.mean(dp == dq[:n]))
                lbl = "Q8_0"
            else:
                try: q, D, M, off, nmax = dec_target(rg, tt)
                except Exception: why["no decoder"] += 1; continue
                n = min(len(q), len(w) // 256)
                if n < 60: why["short"] += 1; continue
                q = q[:n]; D = D[:n]; M = M[:n]; x = w[:n*256].reshape(n, 256)
                with np.errstate(divide="ignore", invalid="ignore"):
                    pred = np.where(D != 0, np.rint((x + M) / D) + off, 0)
                pred = np.clip(pred, 0, nmax).astype(np.int32)
                resid = (q - pred).ravel()
                sbb = (SZ[tt]*8 - CODE_BITS[tt]*256) / 256.0
                # control: can the K-quant superblock scale be recovered by the closed form Q8_0 uses?
                blk = x.reshape(n, 8, 32) if x.shape[1] == 256 else None
                if blk is not None:
                    amax = np.abs(blk).max(axis=2)
                    naive = (amax / max(nmax, 1)).astype(np.float16).astype(np.float32)
                    got_d = D[:, ::32][:, :8]
                    smatch = float(np.mean(np.isclose(naive, got_d, rtol=1e-2)))
                else:
                    smatch = float("nan")
                lbl = TT[tt]
            code_h = h0(q.ravel())
            alone = code_h + sbb
            stored = h0(resid) + sbb
            predicted = h0(resid)
            by[lbl].append(dict(alone=alone, stored=stored, predicted=predicted,
                                exact=float(np.mean(resid == 0)), smatch=smatch))
            got += 1
            print(f"{rid[:22]:22s} {lbl:6s} {kind[:11]:11s} {100*smatch:12.2f}% "
                  f"{100*np.mean(resid == 0):11.2f}% {100*stored/alone:13.1f}% {100*predicted/alone:16.2f}%")
        if got: done += 1
    print(f"\n{'type':8s} {'n':>3s} {'scales exact':>13s} {'codes exact':>12s} "
          f"{'scales STORED':>14s} {'scales PREDICTED':>17s}")
    for t in sorted(by):
        v = by[t]; a = sum(x["alone"] for x in v)
        s = sum(x["stored"] for x in v); p = sum(x["predicted"] for x in v)
        sm = np.nanmean([x["smatch"] for x in v])
        print(f"{t:8s} {len(v):3d} {100*sm:12.2f}% {100*np.mean([x['exact'] for x in v]):11.2f}% "
              f"{100*s/a:13.1f}% {100*p/a:16.2f}%"
              f"   ({a/max(s,1e-9):.1f}x / {a/max(p,1e-9):.0f}x)")
    print("\n  'scales STORED' is what every earlier result reported. 'scales PREDICTED' is legitimate")
    print("  only where the scale reproduces exactly -- read the scales-exact column before believing it.")
    json.dump({k: v for k, v in by.items()}, open(f"{B}/qgq/scalefree.json", "w"), indent=1, default=float)
    print("SCALEFREE_DONE")

if __name__ == "__main__":
    main()
