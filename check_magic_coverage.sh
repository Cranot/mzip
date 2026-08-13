#!/usr/bin/env bash
# check_magic_coverage.sh — every top-level format the ENCODER can emit must be in the decode
# fuzzer's magic list.
#
# WHY: this exact drift has already happened. fuzz_decode.cpp's list is hand-maintained, and when
# MM/MY/MF/MI were added the newest dispatcher sat UNFUZZED — the list looked full, the fuzzer ran
# clean, and nothing indicated a gap. That is the third instance this week of a hand-maintained
# enumeration silently missing a member (the others: the MZSTATS name table written from memory,
# and a grep for UINT64_MAX that missed the site spelling the same value `~0ULL`). Reviewing a list
# cannot find what is absent from it; only comparing it against a generated one can.
#
# The authority is the mz_candidate() call sites in mzip.hpp. Those enumerate, by construction,
# every candidate the top-level search considers — adding a format without adding a call there
# would leave it out of the ablation telemetry too, which is separately visible.
#
#   usage: bash check_magic_coverage.sh
set -u
cd "$(dirname "$0")"

# Authoritative: 2-letter magics from mz_candidate(). BLOCKS/ZSTD/uRAW are not magic-dispatched
# formats (they are the block fallback, a raw zstd stream, and a store), so they are excluded.
authoritative=$(grep -oE 'mz_candidate\("[A-Za-z]+"' mzip.hpp \
                | sed 's/mz_candidate("//; s/"//' \
                | grep -E '^[A-Z][A-Za-z]$' | sort -u)

fuzzlist=$(grep -oE 'static const char\* M\[\]=\{[^}]*\}' fuzz_decode.cpp \
           | grep -oE '"[A-Za-z]{2}"' | tr -d '"' | sort -u)

if [ -z "$authoritative" ]; then
  echo "check_magic_coverage: FAIL — found no mz_candidate() magics in mzip.hpp."
  echo "  The extraction pattern has drifted from the source. A gate that finds nothing must"
  echo "  refuse, not pass: an empty authoritative set would make every list look complete."
  exit 1
fi
if [ -z "$fuzzlist" ]; then
  echo "check_magic_coverage: FAIL — could not parse the magic list out of fuzz_decode.cpp."
  exit 1
fi

missing=""
for m in $authoritative; do
  printf '%s\n' "$fuzzlist" | grep -qxF "$m" || missing="$missing $m"
done

n_auth=$(printf '%s\n' "$authoritative" | wc -l | tr -d ' ')
if [ -n "$missing" ]; then
  echo "check_magic_coverage: FAIL — format(s) the encoder can emit are NOT fuzzed:$missing"
  echo "  Add them to fuzz_decode.cpp's M[] so the decode fuzzer actually reaches those dispatchers."
  exit 1
fi
echo "check_magic_coverage: OK — all $n_auth encoder magics appear in the decode fuzzer's list"
exit 0
