#!/bin/bash
# A/B gate for the MT delimiter widening (';' and '|').
# Compares committed mzip_cm.exe vs patched mzip_cm_new.exe over the FULL corpus and
# roundtrip-verifies every patched output. Sizes are deterministic, so this may run
# concurrently with other work (a TIMING run could not).
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
NEWBIN=${NEWBIN:-./mzip_cm_new.exe}
OUT=${SWEEP_OUT:-sweep_delim_ab.tsv}
# UNIQUE per invocation. A fixed temp dir means two concurrent instances silently overwrite each
# other's o.mz/n.mz/n.out and produce garbage that LOOKS like real results -- observed 2026-08-12:
# 17 duplicate rows, four phantom size increases, and a phantom roundtrip FAIL on a file that
# compresses and round-trips perfectly when run directly. Same hazard class as "never run two cmix
# in one cwd". Derived from the output name so a resume reuses nothing from a dead run.
TMP="sweep_tmp_$(basename "${SWEEP_OUT:-sweep_delim_ab.tsv}" .tsv)_$$"
trap 'rm -rf "$TMP"' EXIT
rm -rf "$TMP"; mkdir -p "$TMP"
# resumable: keep any rows already measured so a killed run costs nothing
[ -s "$OUT" ] || printf "file\told\tnew\tdelta\troundtrip\n" > "$OUT"
sed -i '/^=== SUMMARY/,$d' "$OUT" 2>/dev/null

n=0; grew=0; shrank=0; rtfail=0
for f in $(find real_bench corpus_extra -type f | sort); do
  [ -s "$f" ] || continue
  cut -f1 "$OUT" | grep -qxF "$f" && continue
  ./mzip_cm.exe     c "$f" "$TMP/o.mz" >/dev/null 2>&1 || { echo "OLDFAIL $f" >> "$OUT"; continue; }
  "$NEWBIN" c "$f" "$TMP/n.mz" >/dev/null 2>&1 || { echo "NEWFAIL $f" >> "$OUT"; continue; }
  o=$(stat -c%s "$TMP/o.mz"); w=$(stat -c%s "$TMP/n.mz")
  "$NEWBIN" d "$TMP/n.mz" "$TMP/n.out" >/dev/null 2>&1
  if cmp -s "$f" "$TMP/n.out"; then rt=OK; else rt=FAIL; rtfail=$((rtfail+1)); fi
  d=$((w-o))
  [ $d -gt 0 ] && grew=$((grew+1))
  [ $d -lt 0 ] && shrank=$((shrank+1))
  n=$((n+1))
  printf "%s\t%d\t%d\t%d\t%s\n" "$f" "$o" "$w" "$d" "$rt" >> "$OUT"
done

# NOTE: the summary MUST go to its own file. Writing it with `>> "$OUT"` while the awk
# inside reads "$OUT" makes awk consume the lines it is appending -- an unbounded loop that
# grew this file to 1.35 GB / 22.5M lines on the first run.
{
  echo "=== SUMMARY ==="
  echo "files=$n grew=$grew shrank=$shrank roundtrip_fail=$rtfail"
  awk -F'\t' 'NR>1 && $2 ~ /^[0-9]+$/ {o+=$2; w+=$3} END {printf "total_old=%d total_new=%d delta=%d (%+.4f%%)\n", o, w, w-o, (w-o)*100.0/o}' "$OUT"
  echo "--- files that GREW (must be empty) ---"
  awk -F'\t' 'NR>1 && $4+0 > 0 {print}' "$OUT"
  echo "--- files that SHRANK ---"
  awk -F'\t' 'NR>1 && $4+0 < 0 {print}' "$OUT"
  echo "--- roundtrip FAILURES (must be empty) ---"
  awk -F'\t' 'NR>1 && $5=="FAIL" {print}' "$OUT"
} > "${OUT%.tsv}.summary.txt"

rm -rf "$TMP"
echo DONE > "${OUT%.tsv}.done"
