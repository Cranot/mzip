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
# PPMd var.H backstop (LZMA-SDK Ppmd7, public domain) — compiled-in like libsais
[ -f Ppmd7.o ] || { echo "ppmd objs..."; gcc -O3 -march=native -c ppmd/Ppmd7.c ppmd/Ppmd7Enc.c ppmd/Ppmd7Dec.c; }
PPMD="Ppmd7.o Ppmd7Enc.o Ppmd7Dec.o -I."
echo "zc.exe...";         g++ -O3 -std=c++17 zc.cpp -I $INC $LIB -o zc.exe
echo "mzip_cm.exe...";    g++ -O3 -std=c++17 -march=native            -o mzip_cm.exe   mzip_cli.cpp libsais.c $PPMD -I $INC $LIB $BRO
echo "mzip_base.exe...";  g++ -O3 -std=c++17 -march=native -DMZIP_NO_CM -o mzip_base.exe mzip_cli.cpp libsais.c $PPMD -I $INC $LIB $BRO
echo "cmtest.exe...";     g++ -O3 -std=c++17 -DCM_BACKEND_TEST -DCM_BACKEND_USE_BWT -x c++ cm_backend.hpp -x none libsais.o -o cmtest.exe
echo "bwt9_probe.exe..."; g++ -O3 -std=c++17 bwt9_probe.cpp libsais.o -o bwt9_probe.exe
echo "mzip_ut.exe...";    g++ -O3 -std=c++17 -march=native -D_USE_MATH_DEFINES -o mzip_ut.exe mzip_unit_tests.cpp libsais.c $PPMD -I $INC $LIB $BRO
echo "repro_dec.exe...";  g++ -O2 -std=c++17 -o repro_dec.exe repro_dec.cpp libsais.c $PPMD -I $INC $LIB $BRO
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
# NEW real-class benchmark corpora (2026-08-08) — PERMISSIVE licenses only, fetch-or-skip (categories in
# benchmark_types.py skip cleanly if absent). Fills the corpus blind spots the gap-analysis sweeps found;
# each showcases an encoder shipped this cycle (MY/MF/BCJ/PPMd/CHAR_TEMPLATE).
if command -v curl >/dev/null 2>&1; then
  mkdir -p corpus_extra/yaml corpus_extra/fastq corpus_extra/wasm corpus_extra/binarm corpus_extra/minified
  # large nested k8s/CRD YAML (Apache-2.0) -> 'MY'
  [ -s corpus_extra/yaml/prom_bundle.yaml ] || curl -sL --max-time 45 "https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/bundle.yaml" -o corpus_extra/yaml/prom_bundle.yaml 2>/dev/null || true
  [ -s corpus_extra/yaml/certmgr.yaml ]     || curl -sL --max-time 45 "https://github.com/cert-manager/cert-manager/releases/download/v1.15.3/cert-manager.yaml" -o corpus_extra/yaml/certmgr.yaml 2>/dev/null || true
  # FASTQ (public SRA via nf-core/test-datasets, MIT) -> 'MF'; 10k-read subset
  [ -s corpus_extra/fastq/reads_10k.fastq ] || { curl -sL --max-time 60 "https://raw.githubusercontent.com/nf-core/test-datasets/rnaseq/testdata/GSE110004/SRR6357070_1.fastq.gz" -o /tmp/rfq.gz 2>/dev/null && gzip -dc /tmp/rfq.gz 2>/dev/null | head -n 40000 > corpus_extra/fastq/reads_10k.fastq; } || true
  # WASM (sqlite = public domain) -> XZLIB/BWT
  [ -s corpus_extra/wasm/sql-wasm.wasm ]    || curl -sL --max-time 45 "https://unpkg.com/sql.js@1.8.0/dist/sql-wasm.wasm" -o corpus_extra/wasm/sql-wasm.wasm 2>/dev/null || true
  # non-x86 binary: ripgrep aarch64 (MIT/Unlicense — release-safe, NOT GPL busybox) -> arch BCJ
  [ -s corpus_extra/binarm/rg-aarch64 ]     || { curl -sL --max-time 60 "https://github.com/BurntSushi/ripgrep/releases/download/14.1.0/ripgrep-14.1.0-aarch64-unknown-linux-gnu.tar.gz" -o /tmp/rg.tgz 2>/dev/null && tar -xzf /tmp/rg.tgz -C /tmp 2>/dev/null && find /tmp -name rg -path '*aarch64*' -exec cp {} corpus_extra/binarm/rg-aarch64 \; 2>/dev/null; } || true
  # minified web (MIT) -> PPMd
  [ -s corpus_extra/minified/jquery.min.js ]         || curl -sL --max-time 30 "https://code.jquery.com/jquery-3.7.1.min.js" -o corpus_extra/minified/jquery.min.js 2>/dev/null || true
  [ -s corpus_extra/minified/bootstrap.min.css ]     || curl -sL --max-time 30 "https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" -o corpus_extra/minified/bootstrap.min.css 2>/dev/null || true
  [ -s corpus_extra/minified/bootstrap.min.css.map ] || curl -sL --max-time 30 "https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css.map" -o corpus_extra/minified/bootstrap.min.css.map 2>/dev/null || true
  # audio (WAV/PCM) -> BWT_TEXT. Real permissive test WAV (pydub, MIT). mzip beats general tools ~-30%;
  # the specialist FLAC still wins (mzip is a general compressor, not an audio codec -- see EVALS.md).
  mkdir -p corpus_extra/audio
  [ -s corpus_extra/audio/test1.wav ] || curl -sL --max-time 45 "https://raw.githubusercontent.com/jiaaro/pydub/master/test/data/test1.wav" -o corpus_extra/audio/test1.wav 2>/dev/null || true
  find corpus_extra/yaml corpus_extra/fastq corpus_extra/wasm corpus_extra/binarm corpus_extra/minified corpus_extra/audio -size 0 -delete 2>/dev/null || true
fi
# generated realistic samples (no license issue), labeled (syn) in the report: syslog + BIGINT UNSIGNED SQL
if command -v python3 >/dev/null 2>&1; then
  mkdir -p corpus_extra/syslog corpus_extra/sqlbig
  [ -s corpus_extra/syslog/rfc3164.log ] || python3 -c "import sys;
open('corpus_extra/syslog/rfc3164.log','w').write(''.join('Aug  8 %02d:%02d:%02d host sshd[%d]: Failed password for root from 10.0.0.%d port %d ssh2\n'%((i//3600)%24,(i//60)%60,i%60,1000+i,i%255,20000+i%40000) for i in range(6000)))" 2>/dev/null || true
  [ -s corpus_extra/sqlbig/bigint_unsigned.sql ] || python3 -c "import sys;b=18000000000000000000;
open('corpus_extra/sqlbig/bigint_unsigned.sql','w').write('INSERT INTO users (id,name,created) VALUES '+','.join('(%d,\'u%d\',\'2024-01-%02d\')'%(b+i,i,1+i%28) for i in range(6000))+';\n')" 2>/dev/null || true
  # Scientific sparse matrix (Matrix Market coordinate) -> 'MM' encoder. GENERATED (CC0/public-domain),
  # column-sorted with sign-alignment spacing like real .mtx writers. Random values make this a CONSERVATIVE
  # showcase: real matrices (whose values carry FEM-assembly structure) win far more (bcsstk16 -58.6% vs xz).
  mkdir -p corpus_extra/grids
  [ -s corpus_extra/grids/synthetic_fem.mtx ] || python3 -c "import random;random.seed(20260809);N=6000
f=open('corpus_extra/grids/synthetic_fem.mtx','w',newline='')
f.write('%%MatrixMarket matrix coordinate real symmetric\n% Synthetic column-sorted FEM-like sparse matrix (generated, CC0/public-domain)\n')
e=[]
for j in range(1,N+1):
 b=sorted(set(random.sample(range(j,min(N,j+60)+1),min(14,N-j+1))))
 e+=[(i,j,random.uniform(-3e8,3e8)) for i in b]
f.write('%d %d %d\n'%(N,N,len(e)))
for i,j,v in e: f.write(('%d %d %.13e\n' if v<0 else '%d %d  %.13e\n')%(i,j,v))
f.close()" 2>/dev/null || true
fi

# Semicolon/pipe-delimited tabular -> 'MT' delimiter sniff. REAL OHLCV rows (plotly/datasets, public),
# re-delimited in place. The source contains no ';', '|' or '"', so the substitution is a BIJECTION:
# the three variants carry byte-identical content and differ only in the delimiter, which is exactly
# the control that isolates the detector from the transform.
if command -v curl >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1 && [ ! -s corpus_extra/delim/stocks_semi.csv ]; then
  mkdir -p corpus_extra/delim
  curl -sL --max-time 90 "https://raw.githubusercontent.com/plotly/datasets/master/all_stocks_5yr.csv" -o corpus_extra/delim/_src.csv 2>/dev/null || true
  [ -s corpus_extra/delim/_src.csv ] && python3 -c "
src=open('corpus_extra/delim/_src.csv','rb').read()
cut=src[:2000000]; cut=cut[:cut.rfind(b'\n')+1]
assert not any(d in cut for d in (b';', b'|', b'\"')), 'source contains a target delimiter; re-delimit would NOT be lossless'
open('corpus_extra/delim/stocks_comma.csv','wb').write(cut)
open('corpus_extra/delim/stocks_semi.csv','wb').write(cut.replace(b',', b';'))
open('corpus_extra/delim/stocks_pipe.csv','wb').write(cut.replace(b',', b'|'))" 2>/dev/null || true
  rm -f corpus_extra/delim/_src.csv
  find corpus_extra/delim -size 0 -delete 2>/dev/null || true
fi

# Uncompressed rasters -> 'MI' scanline filter. REAL photographic images (OpenCV sample data, BSD)
# in genuine uncompressed containers; ImageMagick converts to the BMP/PPM/PGM/TGA variants if present.
if command -v curl >/dev/null 2>&1 && [ ! -s corpus_extra/raster/baboon.bmp ]; then
  mkdir -p corpus_extra/raster
  B="https://raw.githubusercontent.com/opencv/opencv/4.x/samples/data"
  curl -sL --max-time 60 "$B/baboon.jpg" -o corpus_extra/raster/_baboon.jpg 2>/dev/null || true
  curl -sL --max-time 60 "$B/board.jpg"  -o corpus_extra/raster/_board.jpg  2>/dev/null || true
  curl -sL --max-time 60 "$B/fruits.jpg" -o corpus_extra/raster/_fruits.jpg 2>/dev/null || true
  if command -v magick >/dev/null 2>&1 || command -v convert >/dev/null 2>&1; then
    CONV=$(command -v magick || command -v convert)
    for n in baboon board; do
      [ -s corpus_extra/raster/_$n.jpg ] && { "$CONV" corpus_extra/raster/_$n.jpg -compress none BMP3:corpus_extra/raster/$n.bmp 2>/dev/null
                                              "$CONV" corpus_extra/raster/_$n.jpg -compress none corpus_extra/raster/$n.tga 2>/dev/null; }
    done
    [ -s corpus_extra/raster/_baboon.jpg ] && "$CONV" corpus_extra/raster/_baboon.jpg -compress none corpus_extra/raster/baboon.ppm 2>/dev/null
    [ -s corpus_extra/raster/_fruits.jpg ] && "$CONV" corpus_extra/raster/_fruits.jpg -colorspace Gray -compress none corpus_extra/raster/fruits.pgm 2>/dev/null
  else
    echo "note: ImageMagick not found — corpus_extra/raster/ left empty, the Raster benchmark type will be skipped"
  fi
  rm -f corpus_extra/raster/_*.jpg
  find corpus_extra/raster -size 0 -delete 2>/dev/null || true
fi
echo "OK — all eval binaries built."
