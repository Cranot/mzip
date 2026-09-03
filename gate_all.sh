#!/usr/bin/env bash
set -u
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include
LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"
echo "building mzip_ut..."
g++ -O2 -std=c++17 -D_USE_MATH_DEFINES -o mzip_ut.exe mzip_unit_tests.cpp libsais.c -I $INC $LIB $BRO 2>&1 | grep -v "_USE_MATH\|previous def\|In file inc\|\^~" | tail -2
echo "UNITS:"; ./mzip_ut.exe 2>&1 | tail -2
echo "CRASHCORPUS:"; ./test_crashers.sh 2>&1 | tail -1
echo "--- byte gate ---"
bash run_gate.sh
echo "GATE_ALL_DONE"
