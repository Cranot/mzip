#!/usr/bin/env bash
# build_evals.sh — build every eval helper binary once. Idempotent. Run before benchmarks.
#   zc.exe        : standalone zstd-19 size tool (no zstd CLI on PATH)
#   mzip_cm.exe   : mzip + CM backend (the product build)
#   mzip_base.exe : mzip with CM disabled (-DMZIP_NO_CM) — for A/B isolating CM's contribution
#   cmtest.exe    : cm_backend.hpp self-roundtrip (CM-alone + BWT+CM) on any file
#   bwt9_probe.exe: mzip's bwt9 size on any file (head-to-head vs CM)
#   mzip_ut.exe   : mzip unit tests (losslessness suite)
# See CLAUDE.md "Evals & scripts". Toolchain: w64devkit g++ (NOTE: its `xz` is broken — use /mingw64/bin/xz.exe).
set -e
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include
LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
[ -f libsais.o ] || { echo "libsais.o..."; gcc -O3 -c libsais.c -o libsais.o; }
echo "zc.exe...";         g++ -O3 -std=c++17 zc.cpp -I $INC $LIB -o zc.exe
echo "mzip_cm.exe...";    g++ -O3 -std=c++17 -march=native            -o mzip_cm.exe   mzip_cli.cpp libsais.c -I $INC $LIB
echo "mzip_base.exe...";  g++ -O3 -std=c++17 -march=native -DMZIP_NO_CM -o mzip_base.exe mzip_cli.cpp libsais.c -I $INC $LIB
echo "cmtest.exe...";     g++ -O3 -std=c++17 -DCM_BACKEND_TEST -DCM_BACKEND_USE_BWT -x c++ cm_backend.hpp -x none libsais.o -o cmtest.exe
echo "bwt9_probe.exe..."; g++ -O3 -std=c++17 bwt9_probe.cpp libsais.o -o bwt9_probe.exe
echo "mzip_ut.exe...";    g++ -O3 -std=c++17 -march=native -D_USE_MATH_DEFINES -o mzip_ut.exe mzip_unit_tests.cpp libsais.c -I $INC $LIB
echo "OK — all eval binaries built."
