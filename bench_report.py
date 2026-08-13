#!/usr/bin/env python3
"""bench_report.py — REPORT. Reads ONLY an observation matrix. Runs no compressor.

Separating this from measurement is what makes the numbers checkable: every figure below is a pure
function of the matrix, so anyone can recompute it, and a statistic can be added or corrected
without re-running anything (a re-run is where a corpus quietly changes under a claim).

WHY MORE THAN ONE AGGREGATE
---------------------------
The legacy report quoted ONE number: total original bytes over total compressed bytes. That is a
MICRO average, and on this corpus it is close to a statement about the few largest files — one
17.7 MB SQL file was 43% of an earlier corpus's wall. It is a legitimate statistic (it answers "if I
compressed this whole pile, how much smaller?") but it is not the statistic most readers hear, and
it cannot reveal whether the headline rests on one family.

So this reports, side by side:
  * micro     — total/total. Size-weighted. The "whole pile" question.
  * macro     — mean of per-file ratios. One file, one vote.
  * macro-grp — mean over SOURCE GROUPS of each group's mean. One domain, one vote. Corrects for
                the fact that 'Other-langs' contributes 12 files and 'CSV' contributes 1.
  * geomean   — geometric mean of per-file ratios. The correct centre for a ratio; a file that
                compresses 40x cannot drag the mean the way it does an arithmetic average.
  * median + quantiles — the shape, not just a centre.
  * leave-one-domain-out — the headline recomputed with each domain deleted. If deleting one family
                moves it a lot, the headline was that family's result wearing a corpus-wide label.
  * bootstrap — a resampling interval on the geometric mean, so "8.05x" comes with a width.

VALIDITY
--------
An INVALID observation is never a number. A pairwise comparison uses only files where BOTH sides
are OK, and it always prints its denominator and what it dropped. A statistic over a silently
shrunken denominator is the failure this whole pipeline exists to prevent.

  usage: python3 bench_report.py [matrix.jsonl] [--out bench_report_v3.md] [--framing 32]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import statistics
import sys
from collections import defaultdict

MZIP = "mzip+CM"
DEFAULT_FRAMING = 32     # bytes; declared, not implicit — see FRAMING NOTE in the output


def load(path):
    meta, rows = None, []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            r = json.loads(line)
            if "_meta" in r:
                meta = r["_meta"]
            else:
                rows.append(r)
    if meta is None:
        raise SystemExit(f"{path}: no header row — this is not a bench_run matrix")
    return meta, rows


def geomean(xs):
    xs = [x for x in xs if x > 0]
    if not xs:
        return float("nan")
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def quantile(xs, q):
    if not xs:
        return float("nan")
    s = sorted(xs)
    if len(s) == 1:
        return s[0]
    pos = q * (len(s) - 1)
    lo = int(math.floor(pos))
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def bootstrap_geomean(xs, n=2000, seed=20260813):
    """Percentile bootstrap CI for the geometric mean. Deterministic seed: a confidence interval
    that changes every time you look at it is not a measurement."""
    if len(xs) < 2:
        return (float("nan"), float("nan"))
    rng = random.Random(seed)
    gs = []
    for _ in range(n):
        samp = [xs[rng.randrange(len(xs))] for _ in range(len(xs))]
        gs.append(geomean(samp))
    gs.sort()
    return (gs[int(0.025 * n)], gs[int(0.975 * n)])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("matrix", nargs="?", default="bench_matrix.jsonl")
    ap.add_argument("--out", default="bench_report_v3.md")
    ap.add_argument("--framing", type=int, default=DEFAULT_FRAMING)
    ap.add_argument("--real-only", action="store_true",
                    help="restrict every statistic to real (non-generated) files")
    a = ap.parse_args()

    meta, rows = load(a.matrix)
    if a.real_only:
        rows = [r for r in rows if r.get("real")]

    tools = [t["tool"] for t in meta["tools"]]
    others = [t for t in tools if t != MZIP]

    # obs[file][tool] = row ; plus per-file metadata
    obs = defaultdict(dict)
    finfo = {}
    for r in rows:
        obs[r["file"]][r["tool"]] = r
        finfo[r["file"]] = (r.get("type", "?"), r.get("domain", "?"), r.get("real", 0),
                            r["file_bytes"])
    files = sorted(obs)

    def size(f, t):
        r = obs[f].get(t)
        if r is None or r["status"] != "ok" or r["size"] is None:
            return None
        return r["size"]

    # ---- validity ledger -------------------------------------------------------------------
    invalid = defaultdict(list)
    for f in files:
        for t in tools:
            r = obs[f].get(t)
            if r is None:
                invalid[t].append((f, "not_measured"))
            elif r["status"] != "ok":
                invalid[t].append((f, r["status"]))

    L = []
    W = L.append
    W(f"# mzip benchmark — matrix report\n")
    W(f"Generated from `{os.path.basename(a.matrix)}` by `bench_report.py`. "
      f"**No compressor was run to produce this file.** Every number is a pure function of the "
      f"matrix and can be recomputed from it.\n")

    c = meta["corpus"]
    W(f"\n## Corpus\n")
    W(f"- **Label:** `{c['corpus_label']}`  ·  **blind:** `{c['is_blind']}`")
    W(f"- {c['note']}")
    n_real = sum(1 for f in files if finfo[f][2])
    n_ty = len({finfo[f][0] for f in files})
    n_dom = len({finfo[f][1] for f in files})
    W(f"- **This report covers {len(files)} file(s)** ({n_real} real, {len(files)-n_real} "
      f"generated) across {n_ty} content type(s) and {n_dom} source group(s).")
    if len(files) != c["files_present"]:
        # A filtered run must SAY it is filtered. A report that quietly describes a subset while
        # naming the whole corpus is the same class of defect as a silently shrunken denominator.
        W(f"- ⚠ **SUBSET.** The declared corpus has {c['files_present']} files across "
          f"{c['types_declared']} types; this run measured {len(files)}"
          + (f" (types filter: `{meta['types_filter']}`)" if meta.get("types_filter") else "")
          + (f" (--quick: files < 256 KB only)" if meta.get("quick") else "")
          + f". Nothing here describes the full corpus.")
    if meta.get("corpus_missing"):
        W(f"- ⚠ {len(meta['corpus_missing'])} declared file(s) ABSENT on the measuring box — "
          f"recorded as UNKNOWN, never as 0.")
    if a.real_only:
        W(f"- ⚠ **This report is restricted to REAL files** (`--real-only`).")
    g = meta.get("git", {})
    W(f"- Measured at git `{g.get('head','?')[:12]}`, {meta.get('started_utc','?')}.")
    mod = g.get("modified_tracked")
    if mod:
        W(f"- ⚠ **{len(mod)} tracked file(s) modified** at measurement time, so the binary may not "
          f"correspond exactly to that commit: {', '.join('`'+m+'`' for m in mod[:8])}"
          + (" …" if len(mod) > 8 else ""))
    elif mod is not None:
        W(f"- Tracked sources were clean at that commit (untracked scratch files ignored).")
    if g.get("mzip_hpp_sha256"):
        W(f"- `mzip.hpp` sha256 `{g['mzip_hpp_sha256'][:16]}`. Binary identity is pinned per tool "
          f"by sha256 in the table below, which is the authority regardless of git state.")

    W(f"\n## Tools, and what their numbers are worth\n")
    W(f"| tool | mode | size evidence | roundtrip verified | version | sha256 |")
    W(f"|---|---|---|---|---|---|")
    for t in meta["tools"]:
        ev = "reported (no artifact)" if t["mode"] == "sizer" else "artifact"
        W(f"| `{t['tool']}` | {t['mode']} | {ev} | {'yes' if t['verifiable'] else '**no**'} | "
          f"{t['version'][:52]} | `{(t['sha256'] or '?')[:12]}` |")
    W(f"\n`sizer` tools print a size and write no artifact, so their numbers can never be "
      f"roundtrip-checked. They are reported, and marked, rather than silently mixed with verified "
      f"sizes.\n")

    W(f"\n## Validity ledger\n")
    W(f"An invalid observation is UNKNOWN. It is never coerced to a size, and it never enters a "
      f"statistic — but it is always counted here, because a statistic over a silently shrunken "
      f"denominator is exactly the defect this pipeline was built to remove.\n")
    W(f"| tool | ok | invalid | statuses seen |")
    W(f"|---|---|---|---|")
    for t in tools:
        bad = invalid[t]
        kinds = sorted({s for _, s in bad})
        W(f"| `{t}` | {len(files)-len(bad)} | {len(bad)} | {', '.join(kinds) if kinds else '—'} |")

    rt_checked = sum(1 for f in files for t in tools
                     if obs[f].get(t, {}).get("verified_roundtrip") is True)
    rt_failed = sum(1 for f in files for t in tools
                    if obs[f].get(t, {}).get("verified_roundtrip") is False)
    W(f"\n**Roundtrip:** {rt_checked} observations independently decompressed and matched the input "
      f"byte for byte; {rt_failed} failed. (The legacy harness verified mzip only and trusted every "
      f"comparator.)\n")

    # ---- per-file ratios vs each tool -------------------------------------------------------
    W(f"\n## Head-to-head: mzip+CM vs each tool\n")
    W(f"`ratio` = other/mzip per file (>1 means mzip is smaller). `n` is the number of files where "
      f"BOTH sides produced a valid observation; `dropped` is how many were excluded and why.\n")
    W(f"| vs | n | dropped | micro | macro | macro-grp | geomean | 95% CI (geo) | median | p10 | p90 |")
    W(f"|---|---|---|---|---|---|---|---|---|---|---|")

    pair_ratios = {}
    for t in others:
        rs, by_dom, dropped = [], defaultdict(list), 0
        tot_o = tot_m = 0
        for f in files:
            so, sm = size(f, t), size(f, MZIP)
            if so is None or sm is None or sm == 0:
                dropped += 1
                continue
            r = so / sm
            rs.append(r)
            by_dom[finfo[f][1]].append(r)
            tot_o += so
            tot_m += sm
        pair_ratios[t] = rs
        if not rs:
            W(f"| `{t}` | 0 | {dropped} | — | — | — | — | — | — | — | — |")
            continue
        micro = tot_o / tot_m
        macro = statistics.fmean(rs)
        macro_g = statistics.fmean([statistics.fmean(v) for v in by_dom.values()])
        gm = geomean(rs)
        lo, hi = bootstrap_geomean(rs)
        W(f"| `{t}` | {len(rs)} | {dropped} | {micro:.3f}x | {macro:.3f}x | {macro_g:.3f}x | "
          f"**{gm:.3f}x** | {lo:.3f}–{hi:.3f} | {statistics.median(rs):.3f}x | "
          f"{quantile(rs,0.10):.3f}x | {quantile(rs,0.90):.3f}x |")

    # ---- compression ratio (orig/comp) per tool ---------------------------------------------
    W(f"\n## Compression ratio (original / compressed)\n")
    W(f"Computed only over files where the tool produced a valid observation, so the columns can "
      f"have different denominators — `n` is stated for each.\n")
    W(f"| tool | n | micro | macro | macro-grp | geomean | median |")
    W(f"|---|---|---|---|---|---|---|")
    for t in tools:
        rs, by_dom = [], defaultdict(list)
        to = tc = 0
        for f in files:
            s = size(f, t)
            if s is None or s == 0:
                continue
            orig = finfo[f][3]
            rs.append(orig / s)
            by_dom[finfo[f][1]].append(orig / s)
            to += orig
            tc += s
        if not rs:
            W(f"| `{t}` | 0 | — | — | — | — | — |")
            continue
        mg = statistics.fmean([statistics.fmean(v) for v in by_dom.values()])
        W(f"| `{t}` | {len(rs)} | {to/tc:.3f}x | {statistics.fmean(rs):.3f}x | {mg:.3f}x | "
          f"{geomean(rs):.3f}x | {statistics.median(rs):.3f}x |")

    # ---- best-of-standard, win/tie/loss ------------------------------------------------------
    W(f"\n## Per-file verdict vs the best standard tool\n")
    W(f"**FRAMING NOTE — declared, not implicit.** mzip emits an *archive* (magic + type + length); "
      f"the comparators emit a raw stream with no container. A per-file difference of "
      f"**≤ {a.framing} B** is therefore recorded as a TIE rather than a win or a loss, because at "
      f"that scale the container header, not the compression, decides the sign. The threshold is "
      f"stated here so a reader can recompute with a different one.\n")

    verdicts = {"win": [], "tie": [], "loss": []}
    best_from_sizer = 0
    # A file excluded from the verdict must be COUNTED and NAMED. The legacy harness's `if ext:`
    # skipped such files silently, so the win/tie/loss denominator shrank with no notice — the same
    # class of defect as coercing a failure to 0, and it would be hypocritical to repeat it here.
    excluded = []
    per_type = defaultdict(lambda: {"win": 0, "tie": 0, "loss": 0})
    for f in files:
        sm = size(f, MZIP)
        cands = [(size(f, t), t) for t in others if t != "mzip(noCM)"]
        cands = [(s, t) for s, t in cands if s is not None]
        if sm is None:
            excluded.append((f, f"mzip+CM invalid ({obs[f].get(MZIP,{}).get('status','not measured')})"))
            continue
        if not cands:
            excluded.append((f, "no valid comparator"))
            continue
        bs, bt = min(cands)
        if obs[f][bt].get("size_evidence") == "reported":
            best_from_sizer += 1
        d = bs - sm                       # >0 means mzip is smaller
        v = "tie" if abs(d) <= a.framing else ("win" if d > 0 else "loss")
        verdicts[v].append((f, finfo[f][0], sm, bs, bt, d))
        per_type[finfo[f][0]][v] += 1

    n_v = sum(len(v) for v in verdicts.values())
    W(f"Over {n_v} of {len(files)} files: **{len(verdicts['win'])} wins, "
      f"{len(verdicts['tie'])} ties, {len(verdicts['loss'])} losses** (threshold {a.framing} B).\n")
    if excluded:
        W(f"\n⚠ **{len(excluded)} file(s) EXCLUDED from that verdict**, named here rather than "
          f"silently dropped:\n")
        for f, why in excluded:
            W(f"  - `{f}` — {why}")
        W("")
    if best_from_sizer:
        W(f"⚠ On {best_from_sizer} of those files the best comparator was an unverified **sizer** "
          f"(`zstd-*`), so the bar mzip was measured against is a reported number with no artifact "
          f"behind it.\n")

    if verdicts["loss"]:
        W(f"\n### Losses — every one, in full\n")
        W(f"| file | type | mzip+CM | best std | which | mzip larger by |")
        W(f"|---|---|---|---|---|---|")
        for f, ty, sm, bs, bt, d in sorted(verdicts["loss"], key=lambda x: x[5]):
            W(f"| `{f}` | {ty} | {sm:,} | {bs:,} | {bt} | {-d:,} B ({-d/bs*100:+.2f}%) |")
    else:
        W(f"\nNo losses.\n")

    # ---- leave-one-domain-out ----------------------------------------------------------------
    W(f"\n## Leave-one-domain-out\n")
    W(f"The geometric-mean head-to-head recomputed with each source group DELETED. A large swing "
      f"means the headline was carried by that family and should not be quoted as corpus-wide.\n")
    doms = sorted({finfo[f][1] for f in files})
    hdr = "| removed | n files | " + " | ".join(f"vs {t}" for t in others) + " |"
    W(hdr)
    W("|" + "---|" * (2 + len(others)))
    base_row = []
    for t in others:
        rs = pair_ratios[t]
        base_row.append(f"{geomean(rs):.3f}x" if rs else "—")
    W(f"| *(none — full corpus)* | {len(files)} | " + " | ".join(base_row) + " |")
    for d in doms:
        keep = [f for f in files if finfo[f][1] != d]
        cells = []
        for t in others:
            rs = []
            for f in keep:
                so, sm = size(f, t), size(f, MZIP)
                if so is not None and sm:
                    rs.append(so / sm)
            if not rs:
                cells.append("—")
                continue
            full = geomean(pair_ratios[t]) if pair_ratios[t] else float("nan")
            gm = geomean(rs)
            delta = (gm - full) / full * 100 if full == full and full else 0.0
            mark = " ⚠" if abs(delta) >= 10 else ""
            cells.append(f"{gm:.3f}x ({delta:+.1f}%){mark}")
        W(f"| {d} | {len(keep)} | " + " | ".join(cells) + " |")

    # ---- per-domain detail --------------------------------------------------------------------
    W(f"\n## By source group\n")
    W(f"| group | files | mzip+CM ratio (geo) | vs best std (geo) | win | tie | loss |")
    W(f"|---|---|---|---|---|---|---|")
    for d in doms:
        fs = [f for f in files if finfo[f][1] == d]
        mr, vb = [], []
        w = ti = lo_ = 0
        for f in fs:
            sm = size(f, MZIP)
            if sm:
                mr.append(finfo[f][3] / sm)
                cands = [size(f, t) for t in others if t != "mzip(noCM)"]
                cands = [x for x in cands if x is not None]
                if cands:
                    bs = min(cands)
                    vb.append(bs / sm)
                    dd = bs - sm
                    if abs(dd) <= a.framing:
                        ti += 1
                    elif dd > 0:
                        w += 1
                    else:
                        lo_ += 1
        W(f"| {d} | {len(fs)} | {geomean(mr):.2f}x | {geomean(vb):.3f}x | {w} | {ti} | {lo_} |")

    # ---- per-type detail ----------------------------------------------------------------------
    W(f"\n## By content type\n")
    W(f"| type | group | files | orig | mzip+CM | ratio | best std | which | verdict |")
    W(f"|---|---|---|---|---|---|---|---|---|")
    types = sorted({finfo[f][0] for f in files})
    for ty in types:
        fs = [f for f in files if finfo[f][0] == ty]
        orig = sum(finfo[f][3] for f in fs)
        mz = [size(f, MZIP) for f in fs]
        if any(x is None for x in mz):
            W(f"| {ty} | {finfo[fs[0]][1]} | {len(fs)} | {orig:,} | **INVALID** | — | — | — | — |")
            continue
        mzs = sum(mz)
        bests = {}
        for t in others:
            if t == "mzip(noCM)":
                continue
            ss = [size(f, t) for f in fs]
            if all(x is not None for x in ss):
                bests[t] = sum(ss)
        if not bests:
            W(f"| {ty} | {finfo[fs[0]][1]} | {len(fs)} | {orig:,} | {mzs:,} | "
              f"{orig/mzs:.2f}x | — | — | no valid comparator |")
            continue
        bt = min(bests, key=bests.get)
        bs = bests[bt]
        d = bs - mzs
        v = "tie" if abs(d) <= a.framing * len(fs) else ("**win**" if d > 0 else "**LOSS**")
        W(f"| {ty} | {finfo[fs[0]][1]} | {len(fs)} | {orig:,} | {mzs:,} | {orig/mzs:.2f}x | "
          f"{bs:,} | {bt} | {v} {d/bs*100:+.2f}% |")

    W(f"\n---\n")
    W(f"*Statistics: micro = Σorig/Σcomp (size-weighted). macro = mean of per-file values (one file, "
      f"one vote). macro-grp = mean over source groups of each group's mean (one domain, one vote). "
      f"geomean = geometric mean of per-file ratios. CI = 2,000-resample percentile bootstrap, "
      f"fixed seed. Framing threshold {a.framing} B, declared above.*")

    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(L) + "\n")
    print(f"wrote {a.out} ({len(L)} lines) from {len(rows)} observations over {len(files)} files")
    return 0


# --------------------------------------------------------------------------------------------
# SELF-TEST. The measurement layer proves its fail-closed paths fire; the reporting layer must
# prove the same, because a report that quietly drops an invalid observation is indistinguishable
# from one that never had it. This builds a matrix with KNOWN invalid rows and checks that the
# report (a) counts them, (b) states the reduced denominator, and (c) never lets an invalid
# observation act as a 0-byte win — the exact bug the old harness had.
#   python3 bench_report.py --selftest
# --------------------------------------------------------------------------------------------
def _selftest() -> int:
    import subprocess
    import tempfile
    ok = True

    def check(name, cond):
        nonlocal ok
        print(f"  {'PASS' if cond else 'FAIL'}  {name}")
        ok = ok and cond

    d = tempfile.mkdtemp(prefix="benchrep_")
    mpath = os.path.join(d, "m.jsonl")
    tools = [{"tool": "rival", "path": "/x", "available": True, "version": "v1",
              "sha256": "aa", "mode": "stdout", "argv": [], "label": "", "verifiable": True},
             {"tool": MZIP, "path": "/y", "available": True, "version": "v2",
              "sha256": "bb", "mode": "file", "argv": [], "label": "", "verifiable": True}]
    meta = {"kind": "bench_run", "started_utc": "t", "git": {"head": "deadbeef", "dirty": False},
            "tools": tools, "corpus_missing": [],
            "corpus": {"corpus_label": "TEST", "is_blind": False, "note": "n",
                       "files_present": 3, "files_real": 3, "types_declared": 1, "domains": 1}}

    def row(tool, f, status, size, **kw):
        r = {"schema": "t", "tool": tool, "file": f, "file_bytes": 1000,
             "file_sha256": "z", "status": status, "size": size, "encode_s": 0.1,
             "decode_s": None, "verified_roundtrip": kw.get("rt"), "tool_version": "v",
             "tool_sha256": "s", "returncode": 0, "stderr_tail": "", "detail": "",
             "mode": "stdout", "size_evidence": "artifact",
             "type": "T", "real": 1, "domain": "D", "corpus": "TEST"}
        return r

    with open(mpath, "w", encoding="utf-8") as fh:
        fh.write(json.dumps({"schema": "t", "_meta": meta}) + "\n")
        for i, f in enumerate(["a.txt", "b.txt", "c.txt", "d.txt"]):
            if f == "d.txt":
                # mzip ITSELF failed here. It must be named as excluded, not silently dropped,
                # and it must NOT be scored a win (the legacy harness scored mz=0 as `mz<=best`).
                fh.write(json.dumps(row(MZIP, f, "invalid_nonzero_rc", None)) + "\n")
                fh.write(json.dumps(row("rival", f, "ok", 800)) + "\n")
                continue
            # mzip is always valid and always 500 B
            fh.write(json.dumps(row(MZIP, f, "ok", 500, rt=True)) + "\n")
            if f == "c.txt":
                # THE CASE THAT USED TO LIE: the rival did not run. Under the old harness this
                # became 0 bytes, i.e. a perfect win for the rival over mzip's 500.
                fh.write(json.dumps(row("rival", f, "invalid_tool_missing", None)) + "\n")
            else:
                fh.write(json.dumps(row("rival", f, "ok", 800)) + "\n")

    out = os.path.join(d, "r.md")
    r = subprocess.run([sys.executable, __file__, mpath, "--out", out],
                       capture_output=True, text=True)
    check("report generates", r.returncode == 0 and os.path.exists(out))
    txt = open(out, encoding="utf-8").read() if os.path.exists(out) else ""

    # Expected values DERIVED from the fixture (rival ok on a,b,d; mzip ok on a,b,c):
    #   ledger rival  -> ok 3, invalid 1
    #   head-to-head  -> n 2 (a,b both valid), dropped 2 (c rival-invalid, d mzip-invalid)
    #   verdict       -> over 2 of 4 files, 2 wins, 2 excluded
    check("validity ledger counts the invalid observation",
          "invalid_tool_missing" in txt and "| `rival` | 3 | 1 |" in txt)
    check("head-to-head states the reduced denominator (n=2, dropped=2)",
          "| `rival` | 2 | 2 |" in txt)
    # 800/500 = 1.6 on both valid files. If the invalid row had become 0, the ratio would collapse
    # toward 0 and mzip would show a LOSS on c.txt.
    check("invalid row did NOT become a 0-byte win (geomean stays 1.600x)", "1.600x" in txt)
    check("no loss was manufactured from the missing tool",
          "0 losses" in txt or "No losses" in txt)
    check("a file where MZIP failed is NAMED as excluded, not silently dropped",
          "EXCLUDED from that verdict" in txt and "`d.txt`" in txt and "mzip+CM invalid" in txt)
    check("mzip's own failure was NOT scored a win (verdict is over 2 of 4 files, 2 excluded)",
          "Over 2 of 4 files" in txt and "**2 wins" in txt
          and "2 file(s) EXCLUDED" in txt)

    import shutil as _sh
    _sh.rmtree(d, ignore_errors=True)
    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        raise SystemExit(_selftest())
    raise SystemExit(main())
