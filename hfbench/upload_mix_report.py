#!/usr/bin/env python3
"""Assemble UPLOAD-MIX.md from the sampler's output.

Estimator: each sampled repo's in-window bytes are scaled by N/n. Repos whose per-file commit dates
could not be fetched cheaply (more than DATE_FILE_CAP files, or a capped listing) contribute nothing to
the LOWER bound and their whole current size to the UPPER bound, so the week's total is reported as an
interval rather than a point. Bootstrap over repos gives the sampling interval separately.

Savings per class are the measured per-chunk numbers (FINAL-TABLE-v2.md) and the beyond-chunk levers,
each applied only under its stated condition. Upload bytes are pre-dedup, so the PB figures are upper
bounds on stored-byte savings and the percentages are the honest form."""
import collections, json, os
import numpy as np

OUT = "/root/mzip-hfbench/upload-mix"; TB = 1e12; PB = 1e15

def jl(p): return [json.loads(l) for l in open(p)] if os.path.exists(p) else []
def fmt(b):
    if b >= PB / 10: return f"{b/PB:.3f} PB"
    if b >= TB: return f"{b/TB:.1f} TB"
    return f"{b/1e9:.1f} GB"

# fraction of the bytes Xet stores today that the per-chunk coder removes (floors of the measured range)
SAVE = {"safetensors:F32": (0.114, "11-36% over 7 F32 classes, floor used"),
        "safetensors:BF16": (0.227, "22.7-23.1%"),
        "safetensors:F16": (0.120, "12-24% over 3 F16 classes, floor used"),
        "gguf:F16": (0.333, "GGUF-F16, one file"), "gguf:BF16": (0.333, "assumed as GGUF-F16"),
        "gguf:F32": (0.333, "assumed as GGUF-F16"), "gguf:Q": (0.062, "7 quant types"),
        "parquet": (0.116, "3 files"), "text": (0.310, "8 corpora"),
        "torch-pickle": (0.114, "ASSUMED: same floats, dtype unknown; F32 floor")}
LEVER = {"parquet": (0.425, "page transcode at zstd-19, byte-identical restore; whole-file at rest"),
         "gguf:Q": (1.000, "recipe regeneration, bit-exact for all 7 types of one model; needs base revision + quantizer build"),
         "gguf:F16": (1.000, "the F16 GGUF is a deterministic conversion of the safetensors (measured byte-identical)")}
FT_LEVER = (0.611, "coding a fine-tune against its base: harmonic 2.94x vs Xet 1.1447 on 28 random real fine-tunes")

def klass(c):
    if c.startswith("gguf:"):
        q = c[5:]
        return "gguf:" + q if q in ("F16", "BF16", "F32") else "gguf:Q"
    return c

def popcat(tags, rid):
    t = (tags.get(rid) or {}).get("tags") or []
    if any(x.startswith("base_model:quantized:") for x in t): return "quantized"
    if any(x.startswith("base_model:adapter:") for x in t): return "adapter"
    if any(x.startswith("base_model:merge:") for x in t): return "merge"
    if any(x.startswith("base_model:finetune:") for x in t): return "finetune"
    return "untagged/base"

def main():
    tags = json.load(open(f"{OUT}/population_models_tags.json")) if os.path.exists(f"{OUT}/population_models_tags.json") else {}
    rng = np.random.default_rng(1)
    L = []; R = {}
    for kind in ("models", "datasets"):
        pop = json.load(open(f"{OUT}/population_{kind}.json")); N = pop["n"]
        samp = jl(f"{OUT}/sample_{kind}.jsonl"); n = len(samp)
        ok = [r for r in samp if r.get("ok")]
        fails = collections.Counter(str(r.get("status")) for r in samp if not r.get("ok"))
        undated = [r for r in ok if r.get("undated_large")]
        capped = [r for r in ok if r.get("tree_capped")]
        new = [r for r in ok if r.get("created_in_window")]
        scale = N / n
        per = np.zeros(n); perk = collections.defaultdict(lambda: np.zeros(n)); byk = collections.Counter()
        upper_extra = 0.0
        for i, r in enumerate(samp):
            if not r.get("ok"): continue
            for c, b in r.get("bytes_in_window", {}).items():
                k = klass(c); byk[k] += b; per[i] += b; perk[k][i] += b
            if r.get("undated_large"):
                upper_extra += float(r.get("used_storage") or sum(r.get("bytes_total", {}).values()))
        lower = per.sum() * scale; upper = lower + upper_extra * scale
        boot = rng.choice(per, size=(2000, n)).sum(axis=1) * scale
        lo, hi = np.percentile(boot, [5, 95])
        R[kind] = dict(N=N, n=n, ok=len(ok), byk=byk, perk=perk, scale=scale, lower=lower, upper=upper, samp=samp)
        L.append(f"### {kind}\n")
        L.append(f"Population **{N:,}** public {kind} repos with lastModified inside the window ({pop['pages']} listing pages). "
                 f"Sampled **{n:,}** uniformly: {len(ok):,} read, {n-len(ok)} failed (HTTP {dict(fails)}), {len(new)} created inside the window, "
                 f"{len(undated)} too large to date per file, {len(capped)} listings capped.\n")
        L.append(f"Bytes uploaded in the window: **{fmt(lower)} to {fmt(upper)}** "
                 f"(the interval is the {len(undated)} repos whose per-file dates were too expensive to fetch; "
                 f"sampling interval on the lower bound, 5-95%: {fmt(lo)} - {fmt(hi)}).\n")
        L.append("| class | sampled bytes | share | estimate for the week |\n|---|---:|---:|---:|")
        tot_s = max(per.sum(), 1)
        for k, b in sorted(byk.items(), key=lambda x: -x[1]):
            L.append(f"| {k} | {fmt(b)} | {100*b/tot_s:.1f}% | {fmt(b*scale)} |")
        L.append("")
        top = sorted(ok, key=lambda r: -sum(r.get("bytes_in_window", {}).values()))[:8]
        L.append("Heaviest sampled repos:\n")
        for r in top:
            bw = r.get("bytes_in_window", {})
            main3 = sorted(bw.items(), key=lambda x: -x[1])[:3]
            L.append(f"- `{r['id']}` {fmt(sum(bw.values()))} ({', '.join(f'{k} {fmt(v)}' for k, v in main3)})"
                     f"{' [new repo]' if r.get('created_in_window') else ''}")
        L.append("")
    # model bytes by derivative population
    M = R["models"]; cat = collections.defaultdict(collections.Counter)
    for r in M["samp"]:
        if not r.get("ok"): continue
        pc = popcat(tags, r["id"])
        for c, b in r.get("bytes_in_window", {}).items(): cat[pc][klass(c)] += b
    mtot = max(sum(sum(v.values()) for v in cat.values()), 1)
    L.append("### model bytes by declared relation to a base\n")
    L.append("| population | safetensors | gguf | other | total for the week | share |\n|---|---:|---:|---:|---:|---:|")
    for pc in ("untagged/base", "finetune", "quantized", "adapter", "merge"):
        v = cat.get(pc, collections.Counter())
        st = sum(b for k, b in v.items() if k.startswith("safetensors"))
        gg = sum(b for k, b in v.items() if k.startswith("gguf"))
        ot = sum(v.values()) - st - gg
        L.append(f"| {pc} | {fmt(st*M['scale'])} | {fmt(gg*M['scale'])} | {fmt(ot*M['scale'])} | {fmt(sum(v.values())*M['scale'])} | {100*sum(v.values())/mtot:.1f}% |")
    L.append("")
    # savings
    allk = collections.Counter()
    for kind in ("models", "datasets"):
        for k, b in R[kind]["byk"].items(): allk[k] += b * R[kind]["scale"]
    # safetensors whose dtype the API did not report are still weights: attribute them across the mix
    # of the safetensors that WERE typed, and say so in the table.
    unk = allk.pop("safetensors:?", 0.0) + allk.pop("safetensors", 0.0)
    typed = {k: v for k, v in allk.items() if k.startswith("safetensors:")}
    tsum = sum(typed.values())
    attributed = {}
    if unk > 0 and tsum > 0:
        for k, v in typed.items():
            share = v / tsum
            attributed[k] = unk * share
            allk[k] += unk * share
    L.append(f"Safetensors bytes whose dtype the API did not report ({fmt(unk)}) are attributed across the "
             f"observed dtype mix: " + ", ".join(f"{k.split(':')[1]} {fmt(v)}" for k, v in sorted(attributed.items(), key=lambda x: -x[1])[:5]) + ".\n")
    ft_st = sum(b for k, b in cat.get("finetune", collections.Counter()).items() if k.startswith("safetensors")) * M["scale"]
    L.append("## What the measured levers take off it\n")
    L.append("Per-chunk savings are fractions of what Xet stores for that class today; beyond-chunk levers apply only under their condition. "
             "Upload bytes are pre-dedup, so treat the byte figures as upper bounds and the percentages as the result.\n")
    L.append("| class | uploaded this week | per-chunk saving | | beyond the chunk | | condition |\n|---|---:|---:|---:|---:|---:|---|")
    sum_pc = 0.0; sum_lv = 0.0; covered = 0.0
    for k, b in sorted(allk.items(), key=lambda x: -x[1]):
        s, note = SAVE.get(k, (0.0, "not measured, counted as 0"))
        pcb = b * s; sum_pc += pcb; covered += b if s > 0 else 0
        lv, lvnote, lvb = 0.0, "-", 0.0
        if k in LEVER: lv, lvnote = LEVER[k]; lvb = b * lv
        elif k == "safetensors:BF16" and ft_st > 0:
            share = ft_st / max(sum(v for kk, v in allk.items() if kk.startswith("safetensors")), 1)
            lv, lvnote = FT_LEVER; lvb = b * share * lv
        sum_lv += lvb
        L.append(f"| {k} | {fmt(b)} | {100*s:.1f}% | {fmt(pcb)} | {f'{100*lv:.0f}%' if lv else '-'} | {fmt(lvb) if lvb else '-'} | {note if not lv else lvnote} |")
    total = sum(allk.values())
    L.append(f"| **total** | **{fmt(total)}** | **{100*sum_pc/max(total,1):.1f}%** | **{fmt(sum_pc)}** | | **{fmt(sum_lv)}** | measured classes cover {100*covered/max(total,1):.0f}% of the bytes |")
    head = [f"# What the public Hub uploaded in one week, and what our measurements take off it\n",
            f"Window {json.load(open(f'{OUT}/population_models.json'))['window_start']} to "
            f"{json.load(open(f'{OUT}/population_models.json'))['window_end']} (UTC), public repos only.\n",
            "Method: every public model and dataset repo whose lastModified falls in the window is enumerated by cursor pagination; "
            "a uniform random sample of repos is then read file by file, taking each file's size and its last commit date, and a file "
            "counts as uploaded in the window when that date is inside it. A repo created inside the window counts in full. "
            "Not visible and named here: private repos, spaces, files deleted or rewritten more than once in the window, Xet's dedup "
            "at rest, and the per-file dates of repos with very many files (reported as an interval, not guessed).\n",
            f"**Headline.** Public models and datasets uploaded in the window: "
            f"**{fmt(sum(R[k]['lower'] for k in R))} to {fmt(sum(R[k]['upper'] for k in R))}** as uploaded. "
            f"Of that, **{100*covered/max(total,1):.0f}%** falls in classes this programme measured. "
            f"Within those classes per-chunk coding removes **{100*sum_pc/max(covered,1):.1f}%** at the measured floors "
            f"(**{fmt(sum_pc)}**), and the production-aware levers remove **{fmt(sum_lv)}** more under their conditions, "
            f"so the two together are **{100*(sum_pc+sum_lv)/max(covered,1):.0f}% of the measured classes** and "
            f"**{100*(sum_pc+sum_lv)/max(total,1):.0f}% of everything uploaded**. "
            "This measures the public, single-count, post-listing part of any headline PB figure, so the percentages transfer and the byte totals do not.\n",
            "## Samples\n"]
    open(f"{OUT}/UPLOAD-MIX.md", "w").write("\n".join(head + L) + "\n")
    json.dump(dict(total=total, per_chunk=sum_pc, levers=sum_lv,
                   models={k: R["models"][k] for k in ("N", "n", "lower", "upper")},
                   datasets={k: R["datasets"][k] for k in ("N", "n", "lower", "upper")},
                   classes=dict(allk)), open(f"{OUT}/UPLOAD-MIX.json", "w"), indent=1, default=float)
    print(open(f"{OUT}/UPLOAD-MIX.md").read())

if __name__ == "__main__":
    main()
