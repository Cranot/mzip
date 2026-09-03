#!/usr/bin/env python3
"""Two classes in the top four have never been opened. This asks what they are made of.

torch-pickle (144 TB/wk): a pytorch_model.bin next to a model.safetensors is the SAME tensors in a
different wrapper -- category 2, an exact copy -- and Xet cannot dedup it because the framing and
tensor order differ, so the chunk hashes differ. Question: how many pickle bytes sit in a repo that
also carries safetensors, and are the sizes consistent with the same tensors?

archive (380 TB/wk, the largest class of the week): a container. Question: what extensions, and how
many bytes sit in repos whose OTHER files say what the archive holds.
"""
import json, collections, os
B = "/root/mzip-hfbench"
rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
try:
    rows += [json.loads(l) for l in open(f"{B}/upload-mix/sample_datasets.jsonl") if l.strip()]
except OSError:
    pass

def cls(r):
    return r.get("bytes_in_window") or {}

pick_tot = pick_with_st = 0.0
arch_tot = 0.0
nrep_p = nrep_ps = 0
ratios = []
arch_co = collections.Counter()
for r in rows:
    c = cls(r)
    p = float(c.get("torch-pickle", 0) or 0)
    s = sum(float(v or 0) for k, v in c.items() if k.startswith("safetensors"))
    a = float(c.get("archive", 0) or 0)
    if p > 0:
        pick_tot += p; nrep_p += 1
        if s > 0:
            pick_with_st += p; nrep_ps += 1
            ratios.append(p / s)
    if a > 0:
        arch_tot += a
        others = {k: v for k, v in c.items() if k != "archive" and float(v or 0) > 0}
        arch_co[",".join(sorted(others)) or "(archive only)"] += a

print(f"sampled repos: {len(rows)}\n")
print("TORCH-PICKLE")
print(f"  repos with pickle bytes            {nrep_p}")
print(f"  ... that also carry safetensors    {nrep_ps} ({100*nrep_ps/max(nrep_p,1):.1f}% of repos)")
print(f"  pickle bytes co-resident with safetensors: {100*pick_with_st/max(pick_tot,1):.1f}% of pickle bytes")
if ratios:
    ratios.sort()
    q = lambda f: ratios[min(len(ratios)-1, int(f*len(ratios)))]
    near = sum(1 for x in ratios if 0.9 <= x <= 1.1)
    print(f"  pickle/safetensors byte ratio: median {q(.5):.3f}  p10 {q(.1):.3f}  p90 {q(.9):.3f}")
    print(f"  within 10% of 1.0 (consistent with the same tensors): {near}/{len(ratios)}"
          f" ({100*near/len(ratios):.0f}%)")
print(f"\nARCHIVE   total in sample {arch_tot/1e12:.3f} TB")
print("  what else is in the repo (by archive bytes):")
for k, v in arch_co.most_common(10):
    print(f"    {100*v/max(arch_tot,1):5.1f}%  {k[:70]}")
json.dump({"pick_tot": pick_tot, "pick_with_st": pick_with_st, "nrep_p": nrep_p,
           "nrep_ps": nrep_ps, "ratios_n": len(ratios), "arch_tot": arch_tot,
           "arch_co": dict(arch_co.most_common(50))},
          open(f"{B}/upload-mix/coresident.json", "w"), indent=1)
print("CORESIDENT_DONE")
