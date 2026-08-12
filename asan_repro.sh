#!/bin/bash
# asan_repro.sh — run one fuzzer and dump the FULL first sanitizer report, plus keep the input.
#   wsl -d Ubuntu-24.04 bash .../asan_repro.sh <fuzz_decode_asan|fuzz_mzip_asan> <iters> <seed>
set -u
cd "$(dirname "$0")"
BIN="${1:-fuzz_decode_asan}"; ITERS="${2:-6000}"; SEED="${3:-0x51}"
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0:allocator_may_return_null=1"
export UBSAN_OPTIONS="print_stacktrace=1"
./asan_build/"$BIN" "$ITERS" "$SEED" > asan_repro.out 2>&1
echo "exit=$?"
echo "=== first AddressSanitizer report ==="
awk '/ERROR: AddressSanitizer/{f=1} f{print} f&&/SUMMARY/{exit}' asan_repro.out | head -40
echo "=== crashing input metadata (written every iteration by the fuzzer) ==="
cat fuzzd_cur.txt 2>/dev/null || cat fuzz_cur.txt 2>/dev/null || echo "(none)"
