#!/usr/bin/env bash
set -u; export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"; cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include; LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"; PPMD="Ppmd7.o Ppmd7Enc.o Ppmd7Dec.o -I."
echo "build repro_dec (ppmd)..."; g++ -O0 -g -std=c++17 -o repro_dec.exe repro_dec.cpp libsais.c $PPMD -I $INC $LIB $BRO || { echo BUILD_FAIL; exit 1; }
echo "=== hostile PPMD decode (rc 139/134/3 = CRASH = BAD) ==="
bad=0; for f in ppmd_hostile/h_*.bin; do ./repro_dec.exe "$f" >/dev/null 2>&1; rc=$?; if [ $rc -ge 128 ] || [ $rc -eq 3 ]; then echo "CRASH rc=$rc $f"; bad=$((bad+1)); fi; done
echo "hostile PPMD: $(ls ppmd_hostile/*.bin|wc -l) streams, $bad crashes"
echo "=== crash-corpus regression ==="; bash test_crashers.sh 2>&1 | tail -2
echo "DECODE_GATE_PPMD_DONE bad=$bad"
