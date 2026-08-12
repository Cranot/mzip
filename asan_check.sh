#!/bin/bash
# asan_check.sh — run the saved reproducers and the fuzzers under ASan+UBSan.
#   wsl -d Ubuntu-24.04 bash /mnt/d/Safe/Projects/tieredcompress/asan_check.sh [iters]
set -u
cd "$(dirname "$0")"
ITERS="${1:-3000}"
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0:allocator_may_return_null=1"
export UBSAN_OPTIONS="print_stacktrace=0"

echo "=== saved reproducers ==="
for f in repro_oom_319.bin repro_uzip_199.bin mi_seeds/baboon.bmp.mz mi_seeds/fruits.pgm.mz; do
  [ -f "$f" ] || { echo "  (missing $f)"; continue; }
  echo "--- $f ---"
  out=$(./asan_build/decode_one_asan "$f" 2>&1)
  if echo "$out" | grep -qE "ERROR: AddressSanitizer|runtime error:"; then
    echo "$out" | grep -E "ERROR: AddressSanitizer|runtime error:|#[0-9]+ 0x" | head -8
  else
    echo "  clean: $(echo "$out" | head -1)"
  fi
done

echo
echo "=== hostile-decode fuzz under ASan+UBSan ($ITERS iters) ==="
out=$(./asan_build/fuzz_decode_asan "$ITERS" 0x51 2>&1)
echo "$out" | grep -oE "mzip[^ ]*\.hpp:[0-9]+:[0-9]+: runtime error: [a-z ]*" | sort | uniq -c | sort -rn | head -25
  echo "  ASan (must be empty): $(echo "$out" | grep -c "ERROR: AddressSanitizer")"
echo "  ---"
echo "$out" | tail -1

echo
echo "=== losslessness fuzz under ASan+UBSan ($ITERS iters) ==="
out=$(./asan_build/fuzz_mzip_asan "$ITERS" 0xBEEF 2>&1)
echo "$out" | grep -oE "mzip[^ ]*\.hpp:[0-9]+:[0-9]+: runtime error: [a-z ]*" | sort | uniq -c | sort -rn | head -25
  echo "  ASan (must be empty): $(echo "$out" | grep -c "ERROR: AddressSanitizer")"
echo "  ---"
echo "$out" | tail -1
