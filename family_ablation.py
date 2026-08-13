#!/usr/bin/env python3
"""family_ablation.py — what does each candidate FAMILY actually contribute to the archive?

THE QUESTION. mzip's top level is a search: it builds candidate encodings of the whole input and
ships the smallest. Until Phase 1b nothing recorded the LOSERS, so the only answerable question was
"what shipped". That cannot distinguish a family that is load-bearing from one that merely wins
occasionally by a byte over a candidate that would have been almost as good — nor from one whose
detector never fires at all.

THE METHOD. With MZIP_CANDIDATES=1 the encoder emits one MZCAND line per candidate considered,
winner or loser. Given every candidate's size, leave-one-family-out is ARITHMETIC, not N re-runs:

    ship(all)      = min over all available candidates
    ship(without F)= min over available candidates whose family != F
    cost(F)        = ship(without F) - ship(all)      >= 0 always

cost(F) is the bytes that family is actually worth on that file: what the archive would grow by if
the family did not exist. A family can win a file and still have cost 0 — that means the runner-up
was equally good and the win was cosmetic.

  ⚠ PREDICTION IS NOT MEASUREMENT. The numbers above are computed from logged candidate sizes, so
  they inherit any error in the logging. `--verify F` therefore RE-RUNS the encoder with
  MZIP_FAMILY_MASK actually excluding F and checks the real archive size equals the predicted one.
  An ablation that is never confronted with a real run is arithmetic wearing a measurement's name.

RECURSION. compress() calls itself for wrapper payloads, so MZCAND blocks nest. Each block begins
with the BLOCKS line; the OUTERMOST call emits last, because its comparison runs only after every
inner call has returned. This takes the final block and cross-checks its candidate count.

  usage:
    python3 family_ablation.py --exe ./mzip_fam.exe            # measure + analyse -> ablation.md
    python3 family_ablation.py --verify A4_NUMERIC             # confirm a prediction by re-running
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from collections import defaultdict

import bench_corpus as bc

FAMILIES = ["A0_STORE", "A1_GENERAL", "A2_CONTAINER", "A3_BWT", "A4_NUMERIC",
            "A5_GRID", "A6_RECORD", "A7_TEMPORAL", "A8_FILTER"]
FAM_BIT = {f: i for i, f in enumerate(FAMILIES)}
ALL_MASK = (1 << len(FAMILIES)) - 1


def candidates_for(exe: str, path: str, env_extra: dict | None = None):
    """Return (candidates, archive_size). candidates = [(magic, family, size, status)] for the
    OUTERMOST compress call. Raises on a failed run — a failed encode is never a data point."""
    env = dict(os.environ, MZIP_CANDIDATES="1")
    if env_extra:
        env.update(env_extra)
    out = os.path.join(os.environ.get("TEMP", "."), f"_fa_{os.getpid()}.mz")
    try:
        r = subprocess.run([exe, "c", path, out], capture_output=True, env=env, timeout=1800)
        if r.returncode != 0:
            raise RuntimeError(f"{exe} rc={r.returncode}: {r.stderr[-200:]!r}")
        if not os.path.exists(out) or os.path.getsize(out) == 0:
            raise RuntimeError(f"{exe} produced no output for {path}")
        arch = os.path.getsize(out)
    finally:
        if os.path.exists(out):
            os.remove(out)

    blocks, cur = [], None
    for line in r.stderr.decode("utf-8", "replace").splitlines():
        if not line.startswith("MZCAND\t"):
            continue
        _, magic, fam, status, sz = line.split("\t")
        if magic == "BLOCKS":                 # each call's block starts here
            cur = []
            blocks.append(cur)
        if cur is None:                       # defensive: telemetry without a header
            continue
        cur.append((magic, fam, int(sz), status))
    if not blocks:
        raise RuntimeError(f"no MZCAND telemetry from {exe} on {path}")
    return blocks[-1], arch                   # outermost call emits last


def ship(cands, exclude: str | None = None) -> int:
    """Smallest available candidate, optionally excluding one family. The BLOCKS fallback is
    always in play — the invariant is that a raw fallback always exists."""
    best = None
    for magic, fam, sz, status in cands:
        if status != "avail":
            continue
        if exclude is not None and fam == exclude:
            continue
        if best is None or sz < best:
            best = sz
    return best


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default="./mzip_fam.exe")
    ap.add_argument("--out", default="ablation.md")
    ap.add_argument("--verify", default="", help="family name; re-run with it masked off")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--receipts", default="", help="write per-file CompressionReceipt JSONL here")
    a = ap.parse_args()

    ents = [(t, f, real, dom) for t, f, real, dom in bc.entries()]
    if a.limit:
        ents = ents[:a.limit]

    if a.verify:
        if a.verify not in FAM_BIT:
            print(f"unknown family {a.verify!r}; known: {FAMILIES}", file=sys.stderr)
            return 2
        mask = ALL_MASK & ~(1 << FAM_BIT[a.verify])
        print(f"VERIFY {a.verify}: predicted-vs-actual with MZIP_FAMILY_MASK=0x{mask:03X}\n")
        agree = disagree = 0
        for t, f, _real, _dom in ents:
            cands, base = candidates_for(a.exe, f)
            predicted = ship(cands, exclude=a.verify)
            _c2, actual = candidates_for(a.exe, f, {"MZIP_FAMILY_MASK": f"0x{mask:X}"})
            ok = (predicted == actual)
            agree += ok
            disagree += (not ok)
            if not ok:
                print(f"  MISMATCH {f}: predicted {predicted} actual {actual} (base {base})")
        print(f"\nagree={agree} mismatch={disagree}")
        print("VERIFY: PASS" if disagree == 0 and agree > 0 else "VERIFY: FAIL")
        return 0 if (disagree == 0 and agree > 0) else 1

    rows = []
    failures = []
    for i, (t, f, real, dom) in enumerate(ents, 1):
        print(f"[{i}/{len(ents)}] {f}", file=sys.stderr, flush=True)
        try:
            cands, arch = candidates_for(a.exe, f)
        except Exception as e:
            failures.append((f, str(e)))       # UNKNOWN, never a zero
            continue
        base = ship(cands)
        winner = min(((sz, magic, fam) for magic, fam, sz, status in cands if status == "avail"))
        costs = {fam: ship(cands, exclude=fam) - base for fam in FAMILIES}
        rows.append(dict(type=t, file=f, real=real, domain=dom, orig=os.path.getsize(f),
                         archive=arch, base=base, winner_magic=winner[1], winner_fam=winner[2],
                         costs=costs, _cands=cands,
                         avail={fam for _m, fam, _s, st in cands if st == "avail"}))

    # ------------------------------------------------------------------------------------------
    # CompressionReceipt — the encoder's account of ONE archive: what it considered, what it chose,
    # what the alternatives would have cost. Written as a SIDECAR, never into the archive: Phase 1
    # is measurement, and embedding a receipt would change archive bytes, which this phase forbids.
    # It is the encoder-side half of the core law -- the archive carries the reconstruction program,
    # the receipt carries the evidence for why THAT program was chosen over the others.
    # ------------------------------------------------------------------------------------------
    if a.receipts:
        import hashlib
        import json as _json
        with open(a.receipts, "w", encoding="utf-8") as rf:
            for r in rows:
                h = hashlib.sha256()
                with open(r["file"], "rb") as fh:
                    for b in iter(lambda: fh.read(1 << 20), b""):
                        h.update(b)
                rf.write(_json.dumps({
                    "schema": "mzip/receipt/1",
                    "corpus": bc.CORPUS_LABEL,
                    "input": {"path": r["file"], "bytes": r["orig"], "sha256": h.hexdigest(),
                              "type": r["type"], "domain": r["domain"], "real": bool(r["real"])},
                    "archive": {"bytes": r["archive"], "ships_as": r["winner_magic"],
                                "family": r["winner_fam"]},
                    "candidates": [
                        {"magic": m, "family": fam, "status": st,
                         "bytes": (sz if st == "avail" else None)}
                        for m, fam, sz, st in r["_cands"]],
                    # What the archive would be WITHOUT each family. Derived from the candidate
                    # sizes above, so it is a PREDICTION; --verify confronts it with a real run.
                    "counterfactual_without_family": {
                        fam: r["base"] + r["costs"][fam] for fam in FAMILIES},
                    "evidence": "candidate sizes observed via MZIP_CANDIDATES=1; "
                                "counterfactuals derived, not measured",
                }, sort_keys=True) + "\n")
        print(f"wrote {a.receipts} ({len(rows)} receipts)")

    L = []
    W = L.append
    W("# Candidate-family ablation (A0–A8)\n")
    W(f"Measured with `{a.exe}` over {len(rows)} file(s) of corpus `{bc.CORPUS_LABEL}`. "
      f"Every candidate the encoder considered was logged, winner or loser.\n")
    if failures:
        W(f"\n⚠ **{len(failures)} file(s) produced no usable telemetry** and are excluded — listed, "
          f"not silently dropped:\n")
        for f, e in failures:
            W(f"  - `{f}` — {e}")
        W("")

    W("\n## What each family is worth\n")
    W("`cost` = bytes the archive would GROW by if the family did not exist "
      "(`ship(without F) − ship(all)`). A family can win files and still cost 0: that means the "
      "runner-up was just as good, so the win was cosmetic and the family is not load-bearing "
      "there.\n")
    W("| family | fires on | wins | LOAD-BEARING (cost>0) | total cost (B) | worst single file |")
    W("|---|---|---|---|---|---|")
    for fam in FAMILIES:
        fires = sum(1 for r in rows if fam in r["avail"])
        wins = sum(1 for r in rows if r["winner_fam"] == fam)
        lb = [r for r in rows if r["costs"][fam] > 0]
        tot = sum(r["costs"][fam] for r in rows)
        worst = max(rows, key=lambda r: r["costs"][fam]) if rows else None
        wtxt = (f"`{os.path.basename(worst['file'])}` +{worst['costs'][fam]:,}"
                if worst and worst["costs"][fam] > 0 else "—")
        W(f"| {fam} | {fires} | {wins} | {len(lb)} | {tot:,} | {wtxt} |")

    W("\n## Families that never fire, and families that fire but never matter\n")
    dead = [f for f in FAMILIES if not any(f in r["avail"] for r in rows)]
    inert = [f for f in FAMILIES
             if any(f in r["avail"] for r in rows) and all(r["costs"][f] == 0 for r in rows)]
    W(f"- **Never fired on this corpus:** {', '.join(dead) if dead else 'none'}")
    W(f"- **Fired but cost 0 everywhere** (removing it would not change one archive byte): "
      f"{', '.join(inert) if inert else 'none'}")
    W("\nBoth are honest negatives about this corpus, NOT about the format: a family that never "
      "fires here may be the only thing that works on data this corpus does not contain.\n")

    W("\n## Per-domain: which family carries which kind of data\n")
    doms = sorted({r["domain"] for r in rows})
    W("| domain | files | dominant winner | total cost of the top family |")
    W("|---|---|---|---|")
    for d in doms:
        rs = [r for r in rows if r["domain"] == d]
        byfam = defaultdict(int)
        for r in rs:
            byfam[r["winner_fam"]] += 1
        top = max(byfam.items(), key=lambda kv: kv[1])
        cost = sum(r["costs"].get(top[0], 0) for r in rs)
        W(f"| {d} | {len(rs)} | {top[0]} ({top[1]}/{len(rs)}) | {cost:,} B |")

    # The winner may be --FALLBACK (the multi-block path), which is not a family and cannot be
    # ablated — asking "what would this cost to remove" is meaningless for the thing that exists
    # so there is always something to fall back TO.
    def wcost(r):
        return r["costs"].get(r["winner_fam"], 0)

    W("\n## Files where a specialized family is load-bearing\n")
    W("| file | domain | ships as | family | archive | without that family | cost |")
    W("|---|---|---|---|---|---|---|")
    lb_rows = sorted((r for r in rows if wcost(r) > 0), key=lambda r: -wcost(r))
    for r in lb_rows[:40]:
        c = wcost(r)
        W(f"| `{r['file']}` | {r['domain']} | {r['winner_magic']} | {r['winner_fam']} | "
          f"{r['base']:,} | {r['base']+c:,} | **+{c:,}** ({c/r['base']*100:.1f}%) |")
    if not lb_rows:
        W("| — | | | | | | no family is load-bearing anywhere |")

    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(L) + "\n")
    print(f"\nwrote {a.out} ({len(rows)} files analysed, {len(failures)} excluded)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
