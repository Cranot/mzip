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
# brotli + liblzma have no import libs here — link the local DLL copies (found at runtime from the exe's dir)
[ -f libbrotlienc.dll ] || cp /mingw64/bin/libbrotlienc.dll /mingw64/bin/libbrotlidec.dll /mingw64/bin/libbrotlicommon.dll .
[ -f liblzma-5.dll ] || cp /mingw64/bin/liblzma-5.dll .
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"
[ -f libsais.o ] || { echo "libsais.o..."; gcc -O3 -c libsais.c -o libsais.o; }
echo "zc.exe...";         g++ -O3 -std=c++17 zc.cpp -I $INC $LIB -o zc.exe
echo "mzip_cm.exe...";    g++ -O3 -std=c++17 -march=native            -o mzip_cm.exe   mzip_cli.cpp libsais.c -I $INC $LIB $BRO
echo "mzip_base.exe...";  g++ -O3 -std=c++17 -march=native -DMZIP_NO_CM -o mzip_base.exe mzip_cli.cpp libsais.c -I $INC $LIB $BRO
echo "cmtest.exe...";     g++ -O3 -std=c++17 -DCM_BACKEND_TEST -DCM_BACKEND_USE_BWT -x c++ cm_backend.hpp -x none libsais.o -o cmtest.exe
echo "bwt9_probe.exe..."; g++ -O3 -std=c++17 bwt9_probe.cpp libsais.o -o bwt9_probe.exe
echo "mzip_ut.exe...";    g++ -O3 -std=c++17 -march=native -D_USE_MATH_DEFINES -o mzip_ut.exe mzip_unit_tests.cpp libsais.c -I $INC $LIB $BRO
echo "repro_dec.exe...";  g++ -O2 -std=c++17 -o repro_dec.exe repro_dec.cpp libsais.c -I $INC $LIB $BRO
# crash-corpus regression: every stream in fuzz_corpus/ must decompress without crashing (SIGSEGV/abort)
if [ -f test_crashers.sh ] && [ -d fuzz_corpus ]; then echo "crash-corpus regression..."; bash test_crashers.sh || echo "WARNING: crash-corpus regression FAILED"; fi
# amalgamated single-header: regenerate, then verify it compiles standalone (stb pattern) + roundtrips.
# Catches the header going stale vs the source (it had drifted months behind, missing every fix).
if [ -f amalgamate.py ] && [ -f amalg_test.cpp ]; then
  echo "mzip_amalgamated.hpp (regen + check)..."; python3 amalgamate.py > mzip_amalgamated.hpp 2>/dev/null
  g++ -O2 -std=c++17 -march=native amalg_test.cpp -I $INC $LIB $BRO -o amalg_test.exe 2>/dev/null \
    && ./amalg_test.exe $(ls real_bench/* 2>/dev/null | head -3) || echo "WARNING: amalgamated build/roundtrip FAILED"
fi
# prep extra real corpora for benchmark_types.py v2 (numeric time-series truncated to 4MB + repo shell scripts)
if [ -d benchmark_data ] && [ ! -f corpus_extra/citytemp_float.bin ]; then
  mkdir -p corpus_extra/shell
  head -c 4194304 benchmark_data/citytemp.bin   > corpus_extra/citytemp_float.bin    2>/dev/null || true
  head -c 4194304 benchmark_data/phone-gyro.bin > corpus_extra/phonegyro_sensor.bin  2>/dev/null || true
  head -c 4194304 benchmark_data/ts_gas.bin     > corpus_extra/tsgas_series.bin      2>/dev/null || true
  head -c 4194304 benchmark_data/nyc-taxi.bin   > corpus_extra/nyctaxi_cols.bin      2>/dev/null || true
  cp build_evals.sh run_final.sh run_mem.sh match3.sh dict.sh phase23.sh overnight.sh screen_run.sh corpus_extra/shell/ 2>/dev/null || true
  echo "prepped corpus_extra/ (numeric + shell)"
fi
# real representative TypeScript (type-def/interface-heavy — where mzip's structure encoders win); optional, needs net
if command -v curl >/dev/null 2>&1 && [ ! -f corpus_extra/ts/typescript_types.d.ts ]; then
  mkdir -p corpus_extra/ts
  curl -sL --max-time 30 "https://unpkg.com/typescript@5.4.5/lib/typescript.d.ts" -o corpus_extra/ts/typescript_types.d.ts 2>/dev/null || true
  curl -sL --max-time 30 "https://raw.githubusercontent.com/microsoft/TypeScript/main/src/compiler/types.ts" -o corpus_extra/ts/compiler_types.ts 2>/dev/null || true
  curl -sL --max-time 30 "https://unpkg.com/rxjs@7.8.1/dist/types/internal/Observable.d.ts" -o corpus_extra/ts/rxjs_observable.d.ts 2>/dev/null || true
  find corpus_extra/ts -size 0 -delete 2>/dev/null || true
  echo "fetched real TS corpus (if online)"
fi
# real new-type corpus (#13): proto/rst/svg fetched; tsv/ndjson/patch derived from local real data
if [ ! -f corpus_extra/misc/descriptor.proto ]; then
  mkdir -p corpus_extra/misc
  command -v curl >/dev/null 2>&1 && {
    curl -sL --max-time 30 "https://raw.githubusercontent.com/protocolbuffers/protobuf/main/src/google/protobuf/descriptor.proto" -o corpus_extra/misc/descriptor.proto 2>/dev/null || true
    curl -sL --max-time 30 "https://raw.githubusercontent.com/python/cpython/main/Doc/tutorial/introduction.rst" -o corpus_extra/misc/cpython_intro.rst 2>/dev/null || true
    curl -sL --max-time 40 "https://upload.wikimedia.org/wikipedia/commons/8/84/Example.svg" -o corpus_extra/misc/example.svg 2>/dev/null || true
  }
  [ -f real_bench/events.csv ] && python3 -c "import csv;rows=list(csv.reader(open('real_bench/events.csv')));open('corpus_extra/misc/events.tsv','w',newline='').write('\n'.join('\t'.join(r) for r in rows))" 2>/dev/null || true
  [ -f real_bench/users.json ] && python3 -c "import json;d=json.load(open('real_bench/users.json'));it=d if isinstance(d,list) else (d.get('users') or d.get('data') or list(d.values())[0]);open('corpus_extra/misc/users.ndjson','w').write('\n'.join(json.dumps(x) for x in it[:5000])) if isinstance(it,list) else None" 2>/dev/null || true
  git diff 4a9bd5f..HEAD -- mzip.hpp > corpus_extra/misc/changes.patch 2>/dev/null || true
  find corpus_extra/misc -size 0 -delete 2>/dev/null || true
  echo "prepped real new-type corpus"
fi
echo "OK — all eval binaries built."
