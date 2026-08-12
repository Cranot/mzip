#!/usr/bin/env bash
# check_amalgam.sh — fail if the tracked single header is stale versus mzip.hpp.
#
# WHY THIS EXISTS: mzip_amalgamated.hpp is a TRACKED, README-documented distribution artifact that
# must be regenerated whenever mzip.hpp changes. It has now gone stale TWICE. The first time it was
# months behind; the second time (2026-08-12) it missed FOUR consecutive commits, so the published
# single-header build shipped without any of that day's security fixes — the allocation bounds, the
# heap-overflow fix, the UB fixes, the 45 decode-write bounds. Regenerating it by hand is clearly
# not a control; noticing is the part that fails. This is the control.
#
# It does NOT trust timestamps. It regenerates to a temp file and compares content, so it is exact.
#   usage: bash check_amalgam.sh            # verify only, non-zero exit if stale
#          bash check_amalgam.sh --fix      # regenerate in place, then verify
set -u
cd "$(dirname "$0")"

if [ ! -f amalgamate.py ] || [ ! -f mzip_amalgamated.hpp ]; then
  echo "check_amalgam: generator or artifact missing — cannot verify"; exit 2
fi

TMP="amalg_check_$$.hpp"
trap 'rm -f "$TMP"' EXIT

if ! python3 amalgamate.py > "$TMP" 2>amalg_check.err || [ ! -s "$TMP" ]; then
  echo "check_amalgam: FAIL — amalgamate.py produced nothing. See amalg_check.err"; exit 2
fi

if cmp -s "$TMP" mzip_amalgamated.hpp; then
  echo "check_amalgam: OK — mzip_amalgamated.hpp is current with mzip.hpp"
  rm -f amalg_check.err
  exit 0
fi

if [ "${1:-}" = "--fix" ]; then
  cp -f "$TMP" mzip_amalgamated.hpp
  echo "check_amalgam: REGENERATED mzip_amalgamated.hpp (it was stale) — commit it"
  exit 0
fi

echo "check_amalgam: FAIL — mzip_amalgamated.hpp is STALE versus mzip.hpp."
echo "  tracked : $(wc -c < mzip_amalgamated.hpp) bytes"
echo "  current : $(wc -c < "$TMP") bytes"
echo "  The single header is a shipped artifact; a stale one distributes code without the fixes"
echo "  that are in mzip.hpp. Run: bash check_amalgam.sh --fix   then commit the result."
exit 1
