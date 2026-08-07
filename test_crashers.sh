#!/usr/bin/env bash
# Regression guard: every stream in fuzz_corpus/ must decompress WITHOUT crashing (rc 0 or a clean
# non-crash error). A SIGSEGV (rc 139) or heap-abort (rc 3) is a FAIL. These are the hostile/edge
# streams the decode-fuzz campaign found; run after any decode-path change.
#   usage: ./test_crashers.sh            (uses ./repro_dec.exe)
cd "$(dirname "$0")"
BIN=${1:-./repro_dec.exe}
[ -x "$BIN" ] || { echo "need $BIN (build: g++ -O0 -std=c++17 repro_dec.cpp libsais.c <zstd> <brotli> <lzma>)"; exit 2; }
fail=0; n=0
for f in fuzz_corpus/*.bin; do
  [ -f "$f" ] || continue
  n=$((n+1))
  "$BIN" "$f" >/dev/null 2>&1; rc=$?
  # rc 0 = decoded; rc 1/2 = clean rejection; rc 139 (SIGSEGV) / 134 / 3 (abort) = crash
  if [ $rc -eq 139 ] || [ $rc -eq 134 ] || [ $rc -eq 3 ] || [ $rc -ge 128 ]; then
    echo "CRASH rc=$rc  $f"; fail=$((fail+1))
  fi
done
echo "=== crash-corpus: $n streams, $fail crashes ==="
[ $fail -eq 0 ] && echo "PASS" || { echo "FAIL"; exit 1; }
