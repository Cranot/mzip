#!/usr/bin/env python3
"""Static audit: which encoder-adoption branches verify their output?

The NUM_EXTRACT defect was: a specialist encoder's output is adopted with
`use_generator = true` (meaning "already fully encoded, don't re-compress")
without checking that it decodes back. Two sibling branches were hardened after
identical incidents. This finds every branch of that shape and reports which
ones carry a guard.

Heuristic, deliberately conservative: for each `use_generator = true`, look back
up to LOOKBACK lines for evidence of a roundtrip check (a memcmp against the
source block, or a decode_* call feeding a comparison). Anything without such
evidence is REPORTED FOR REVIEW, not declared a bug -- a hand check follows.
"""
import re, sys

SRC = "/root/mzip/mzip.hpp"
LOOKBACK = 45
lines = open(SRC, encoding="utf-8", errors="replace").read().splitlines()

VERIFY = re.compile(r"memcmp\s*\(|\.size\(\)\s*(==|!=)\s*this_block|_rt\b|roundtrip", re.I)
ENCODE = re.compile(r"\bencode_([a-z0-9_]+)\s*\(")
BTYPE  = re.compile(r"BlockType::([A-Z0-9_]+)")

hits = []
for i, l in enumerate(lines):
    if "use_generator = true" not in l:
        continue
    lo = max(0, i - LOOKBACK)
    ctx = lines[lo:i+1]
    blob = "\n".join(ctx)

    enc = ENCODE.findall(blob)
    # the governing block type: nearest preceding "analysis.type == BlockType::X"
    bt = None
    for j in range(i, lo-1, -1):
        m = re.search(r"analysis\.type\s*==\s*BlockType::([A-Z0-9_]+)", lines[j])
        if m: bt = m.group(1); break
    if bt is None:
        m = BTYPE.findall(blob)
        bt = m[-1] if m else "?"

    guarded = bool(VERIFY.search(blob))
    hits.append((i+1, bt, sorted(set(enc)), guarded))

print(f"{'line':>7}  {'guard':<9} {'block type':<24} encoders in scope")
print("-"*88)
un = []
for ln, bt, enc, g in hits:
    tag = "VERIFY" if g else "** NONE **"
    print(f"{ln:>7}  {tag:<9} {bt:<24} {', '.join(enc) if enc else '-'}")
    if not g: un.append((ln, bt, enc))

print(f"\n{len(hits)} adoption sites; {len(un)} with no roundtrip evidence:")
for ln, bt, enc in un:
    print(f"  mzip.hpp:{ln}  {bt}  ({', '.join(enc) if enc else 'no encode_* in scope'})")
