#!/usr/bin/env bash
# Rebuild the NEW product binary with all current fixes, then run the byte-identical gate.
set -u
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include
LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"
echo "rebuilding mzip_cm_new.exe (current tree)..."
g++ -O3 -std=c++17 -march=native -o mzip_cm_new.exe mzip_cli.cpp libsais.c -I $INC $LIB $BRO 2>&1 | tail -2
[ -x ./mzip_cm_new.exe ] || { echo "NEW build failed"; exit 2; }
echo "NEW built. Running byte_gate.sh..."
bash byte_gate.sh
echo "RUN_GATE_DONE"
