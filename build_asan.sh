#!/bin/bash
# build_asan.sh — build the fuzzers under AddressSanitizer + UndefinedBehaviorSanitizer.
#
# WHY THIS EXISTS: of ~22 defects found on 2026-08-11/12, the existing automated gates found zero.
# The heap overflow at the fall-through memcpy survived 20,000 hostile decode streams, because a
# modest overflow into heap slack does not fault. ASan turns that silent class loud.
#
# w64devkit ships no sanitizer runtimes (`cannot find -lasan`), so this runs under WSL:
#   wsl -d Ubuntu-24.04 bash /mnt/d/Safe/Projects/tieredcompress/build_asan.sh
# Requires: libzstd-dev libbrotli-dev liblzma-dev (the repo vendors no sources for these).
set -u
cd "$(dirname "$0")"
OUT=asan_build
mkdir -p "$OUT"

# Abort on everything EXCEPT signed-integer-overflow, which we let RECOVER so that one run
# enumerates every site instead of stopping at the first. These detectors reinterpret raw input as
# int64 and the wraparound is intentional (encode and decode must agree), so the inventory matters
# more than the first hit.
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -fsanitize-recover=signed-integer-overflow"
INC="-I. -Izstd_release/zstd-v1.5.6-win64/include"
LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"

echo "=== C objects (libsais + PPMd) ==="
for c in libsais.c ppmd/Ppmd7.c ppmd/Ppmd7Enc.c ppmd/Ppmd7Dec.c; do
  o="$OUT/$(basename "${c%.c}").o"
  gcc -std=c11 -g -O1 $SAN $INC -c "$c" -o "$o" 2> "$OUT/$(basename "$c").err" \
    && echo "  ok   $c" || { echo "  FAIL $c"; head -5 "$OUT/$(basename "$c").err"; }
done

for target in fuzz_decode fuzz_mzip decode_one; do
  echo "=== $target (ASan+UBSan) ==="
  g++ -std=c++17 -g -O1 $SAN $INC -o "$OUT/${target}_asan" "$target.cpp" \
      "$OUT"/libsais.o "$OUT"/Ppmd7.o "$OUT"/Ppmd7Enc.o "$OUT"/Ppmd7Dec.o $LIBS \
      2> "$OUT/$target.err"
  if [ -x "$OUT/${target}_asan" ]; then
    echo "  BUILT $OUT/${target}_asan"
  else
    echo "  FAILED — first errors:"
    grep -E "error:|fatal error:" "$OUT/$target.err" | head -8
    echo "  (total errors: $(grep -c 'error:' "$OUT/$target.err"))"
  fi
done
