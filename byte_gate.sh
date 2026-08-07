#!/usr/bin/env bash
# Byte-identical gate for the decode-hardening fixes (libsais.c + bwt_compress_v8.hpp + mzip.hpp).
# Proves the guard clauses do NOT change valid-data output: builds a baseline from the committed
# tree (git stash of exactly the 3 changed files), compresses the whole held-out corpus with both
# NEW and BASE binaries, and diffs. Also roundtrip-verifies NEW (c->d->cmp). Any file that differs
# is EXPECTED only if it previously crashed compress (none should, since base must compress it).
set -u
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd "$(dirname "$0")"
INC=zstd_release/zstd-v1.5.6-win64/include
LIB=zstd_release/zstd-v1.5.6-win64/static/libzstd_static.lib
BRO="./libbrotlienc.dll ./libbrotlidec.dll ./libbrotlicommon.dll ./liblzma-5.dll"

# --- build BASELINE from committed tree ---
git stash push -- bwt_compress_v8.hpp libsais.c mzip.hpp >/dev/null 2>&1 || { echo "stash failed"; exit 2; }
trap 'git stash pop >/dev/null 2>&1' EXIT
echo "building baseline (committed tree)..."
g++ -O3 -std=c++17 -march=native -o mzip_cm_base.exe mzip_cli.cpp libsais.c -I $INC $LIB $BRO 2>&1 | tail -2
git stash pop >/dev/null 2>&1; trap - EXIT
[ -x ./mzip_cm_base.exe ] || { echo "baseline build failed"; exit 2; }
echo "baseline built."

mkdir -p /tmp/bg_new /tmp/bg_base /tmp/bg_rt
FILES=$( { ls real_bench/* 2>/dev/null; find corpus_extra -type f 2>/dev/null; } | sort -u )
tot=0; ident=0; differ=0; lossy=0; newfail=0; basefail=0
for f in $FILES; do
  [ -f "$f" ] || continue
  tot=$((tot+1)); b=$(basename "$f")
  ./mzip_cm_new.exe  c "$f" "/tmp/bg_new/$b.mz"  19 >/dev/null 2>&1 || { newfail=$((newfail+1)); echo "NEW-COMPRESS-FAIL $f"; continue; }
  ./mzip_cm_base.exe c "$f" "/tmp/bg_base/$b.mz" 19 >/dev/null 2>&1 || { basefail=$((basefail+1)); echo "BASE-COMPRESS-FAIL $f (new ok)"; }
  # roundtrip verify NEW
  ./mzip_cm_new.exe d "/tmp/bg_new/$b.mz" "/tmp/bg_rt/$b" >/dev/null 2>&1
  if ! cmp -s "$f" "/tmp/bg_rt/$b"; then lossy=$((lossy+1)); echo "*** LOSSY(new) $f ***"; fi
  # byte-identical vs baseline
  if [ -f "/tmp/bg_base/$b.mz" ]; then
    if cmp -s "/tmp/bg_new/$b.mz" "/tmp/bg_base/$b.mz"; then ident=$((ident+1)); else differ=$((differ+1)); echo "DIFFER $f  new=$(stat -c%s /tmp/bg_new/$b.mz) base=$(stat -c%s /tmp/bg_base/$b.mz)"; fi
  fi
done
echo "================ BYTE GATE ================"
echo "files=$tot  byte-identical=$ident  differ=$differ  lossy(new)=$lossy  new-compress-fail=$newfail  base-compress-fail=$basefail"
echo "=========================================="
