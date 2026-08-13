#!/usr/bin/env bash
# ab_family.sh — prove the A0..A8 family gating is BEHAVIOUR-NEUTRAL at the default mask.
#
# The family masks and MZCAND telemetry are measurement scaffolding. The whole claim is that with
# MZIP_FAMILY_MASK unset the search is the one that always ran, so the archive must be IDENTICAL
# byte for byte -- not "the same size", identical. Sizes can coincide while a different candidate
# shipped, and that would be a silent change to which decoder path every future archive exercises.
#
#   usage: bash ab_family.sh <reference-exe> <candidate-exe>
set -u
cd "$(dirname "$0")"
REF="${1:-./mzip_cm.exe}"
NEW="${2:-./mzip_fam.exe}"
TMP="ab_fam_tmp_$$"; mkdir -p "$TMP"; trap 'rm -rf "$TMP"' EXIT

files=$(python3 -c "
import bench_corpus as bc
for _, f, _, _ in bc.entries(): print(f)
")

n=0; same=0; diff=0; rtok=0; rtbad=0; noran=0
for f in $files; do
  n=$((n+1))
  # Do NOT swallow the encoder's stderr. The first version of this script sent it to /dev/null,
  # so when the binary failed to run at all the script reported "RTFAIL" -- i.e. it labelled a
  # compressor that never executed as a LOSSLESSNESS failure. That is the same coerce-a-failure
  # -into-a-verdict defect this whole effort is about, reproduced in the tool checking for it.
  "$REF" c "$f" "$TMP/a.mz" >/dev/null 2>"$TMP/a.err"; rca=$?
  "$NEW" c "$f" "$TMP/b.mz" >/dev/null 2>"$TMP/b.err"; rcb=$?
  if [ $rca -ne 0 ] || [ $rcb -ne 0 ] || [ ! -s "$TMP/a.mz" ] || [ ! -s "$TMP/b.mz" ]; then
    noran=$((noran+1))
    echo "NORUN $f  ref_rc=$rca new_rc=$rcb  ref_err=$(head -c 160 "$TMP/a.err" | tr '\n' ' ')  new_err=$(head -c 160 "$TMP/b.err" | tr '\n' ' ')"
    rm -f "$TMP/a.mz" "$TMP/b.mz" "$TMP/a.err" "$TMP/b.err"
    continue
  fi
  ha=$(sha256sum < "$TMP/a.mz" | cut -d' ' -f1)
  hb=$(sha256sum < "$TMP/b.mz" | cut -d' ' -f1)
  if [ "$ha" = "$hb" ]; then
    same=$((same+1))
  else
    diff=$((diff+1))
    printf 'DIFF  %-52s ref=%s new=%s\n' "$f" \
      "$(stat -c%s "$TMP/a.mz")" "$(stat -c%s "$TMP/b.mz")"
  fi
  # losslessness of the NEW binary is checked here too: byte-identity to a reference that was
  # itself never decoded would only prove the two agree, not that either is correct.
  "$NEW" d "$TMP/b.mz" "$TMP/b.out" >/dev/null 2>&1
  if cmp -s "$f" "$TMP/b.out"; then rtok=$((rtok+1)); else rtbad=$((rtbad+1)); echo "RTFAIL $f"; fi
  rm -f "$TMP/a.mz" "$TMP/b.mz" "$TMP/b.out"
done

echo ""
echo "files=$n  byte-identical=$same  DIFFERENT=$diff   roundtrip ok=$rtok FAIL=$rtbad   never-ran=$noran"
# A run where nothing executed is not a pass. This repo has already shipped a gate that passed
# vacuously on zero inputs; refusing here is cheaper than discovering that later.
if [ "$n" -eq 0 ] || [ "$same" -eq 0 ]; then
  echo "AB_FAMILY: FAIL — no file was actually compared. Refusing to call that a pass."
elif [ "$diff" -eq 0 ] && [ "$rtbad" -eq 0 ] && [ "$noran" -eq 0 ]; then
  echo "AB_FAMILY: PASS"
else
  echo "AB_FAMILY: FAIL"
fi
