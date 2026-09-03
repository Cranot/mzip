#!/usr/bin/env bash
set -u; export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"; cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include; LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"; PPMD="Ppmd7.o Ppmd7Enc.o Ppmd7Dec.o -I."
echo "build mzip_cm (ppmd)..."; g++ -O3 -std=c++17 -march=native -o mzip_cm.exe mzip_cli.cpp libsais.c $PPMD -I $INC $LIB $BRO || { echo BUILD_FAIL_CM; exit 1; }
echo "build mzip_cm_np (baseline -DMZIP_NO_PPMD)..."; g++ -O3 -std=c++17 -march=native -DMZIP_NO_PPMD -o mzip_cm_np.exe mzip_cli.cpp libsais.c $PPMD -I $INC $LIB $BRO || { echo BUILD_FAIL_NP; exit 1; }
echo "build mzip_ut (ppmd)..."; g++ -O3 -std=c++17 -march=native -D_USE_MATH_DEFINES -o mzip_ut.exe mzip_unit_tests.cpp libsais.c $PPMD -I $INC $LIB $BRO 2>/dev/null || { echo BUILD_FAIL_UT; exit 1; }
echo "UNITS:"; ./mzip_ut.exe 2>&1 | tail -2
echo "NEVER-LARGER + LOSSLESS + MEASURE:"; python3 gate_ppmd.py
echo "GATE_PPMD_DONE"
