#!/usr/bin/env python3
"""One row per quant TYPE, not per file.

The first run measured the largest tensor common to each pair, which is the embedding, and that is
Q6_K in every file of the repo -- so all three rows measured the same thing. This takes the largest
tensor of EACH distinct type present, so Q4_K, Q5_K and Q6_K are each measured on a tensor that
actually uses them."""
import json, sys
import numpy as np
B = "/root/mzip-hfbench"
sys.path.insert(0, B)
from kquant_given import read_header, fetch, dec_q8, h0, api, H
from alltypes import dec_target, TT, SZ
NSB = 15000

def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "bartowski/Qwen2.5-1.5B-Instruct-GGUF"
    d = api(f"{H}/api/models/{repo}")
    files = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf")]
    q8f = next((f for f in files if "q8_0" in f.lower()), None)
    if not q8f: print("no q8_0 in the repo"); return
    h8 = read_header(repo, q8f)
    print(f"{repo}\n  source {q8f}\n")
    print(f"{'file':12s} {'type':6s} {'tensor':24s} {'exact':>8s} {'alone':>8s} {'as residual':>12s} {'':>10s}")
    out = []
    done = set()
    for key in ("q4_k_m", "q5_k_m", "q6_k", "q3_k_m", "q2_k"):
        f = next((x for x in files if key in x.lower()), None)
        if not f: continue
        hk = read_header(repo, f)
        if not hk: continue
        best = {}
        for nm, m in hk["tensors"].items():
            tt = m["ttype"]
            if tt not in SZ: continue
            if nm not in h8["tensors"] or h8["tensors"][nm]["ttype"] != 8: continue
            sz = int(np.prod(m["dims"]))
            if tt not in best or sz > best[tt][1]: best[tt] = (nm, sz)
        for tt, (name, _s) in sorted(best.items()):
            if (tt, name) in done: continue
            done.add((tt, name))
            nsb = min(NSB, int(np.prod(h8["tensors"][name]["dims"])) // 256)
            if nsb < 100: continue
            r8 = fetch(repo, q8f, h8["data_start"] + h8["tensors"][name]["offset"], nsb*8*34)
            rk = fetch(repo, f, hk["data_start"] + hk["tensors"][name]["offset"], nsb*SZ[tt])
            if r8 is None or rk is None: continue
            x = dec_q8(r8)[:nsb*256].reshape(nsb, 256)
            try: q, D, M, off, nmax = dec_target(rk, tt)
            except Exception as e:
                print(f"{key:12s} {TT[tt]:6s} decoder not implemented: {e}"); continue
            n = min(len(q), len(x)); q, D, M, x = q[:n], D[:n], M[:n], x[:n]
            with np.errstate(divide="ignore", invalid="ignore"):
                pred = np.where(D != 0, np.rint((x + M) / D) + off, 0)
            pred = np.clip(pred, 0, nmax).astype(np.int32)
            resid = (q - pred).ravel()
            exact = float(np.mean(resid == 0))
            code_bits = {12: 4, 13: 5, 14: 6, 11: 3, 10: 2}[tt]
            scale_bits = (SZ[tt]*8 - code_bits*256) / 256.0
            alone = h0(q.ravel()) + scale_bits
            cost = h0(resid) + scale_bits
            print(f"{key:12s} {TT[tt]:6s} {name[:24]:24s} {100*exact:7.2f}% {alone:8.4f} {cost:12.4f} "
                  f"= {100*cost/alone:5.1f}% ({alone/cost:.1f}x)")
            out.append(dict(file=key, type=TT[tt], tensor=name, exact=exact, alone=alone, cost=cost))
    if out:
        w = sum(o["alone"] for o in out); c = sum(o["cost"] for o in out)
        print(f"\n  across {len(out)} type/tensor pairs: {100*c/w:.1f}% of standalone ({w/c:.1f}x)")
    json.dump(out, open(f"{B}/qgq/alltypes2.json", "w"), indent=1)
    print("ALLTYPES2_DONE")

if __name__ == "__main__":
    main()
