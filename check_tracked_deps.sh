#!/usr/bin/env bash
# check_tracked_deps.sh — fail if a TRACKED script references a file that exists on disk but is NOT
# tracked, i.e. a fresh clone would get the script and not the thing it needs.
#
# WHY: this happened THREE times on 2026-08-12/13 alone.
#   - amalgamate.py  generates the tracked mzip_amalgamated.hpp, but was swallowed by .gitignore's
#     blanket `*.py`, so the shipped single header was unreproducible from a clone.
#   - decode_one.cpp is built by the committed build_asan.sh, and was simply never added.
#   - bench_core.py  is used by the benchmark, swallowed by the same `*.py` rule.
# Each failed SILENTLY, because nothing exercises a clean clone during normal work. The blanket
# `*.py` ignore is itself correct (33 scratch scripts vs 9 real ones) — the defect is that forgetting
# a negation is invisible. This makes it visible.
#
#   usage: bash check_tracked_deps.sh
set -u
cd "$(dirname "$0")"

TRACKED=$(git ls-files)
missing=0

# Vendored third-party trees are deliberately untracked (fetched by build_evals.sh), so a reference
# into them is not a defect. Keep this list SHORT and explicit: a broad skip list would hide the very
# thing the gate exists to find, and a gate that cries wolf stops being read.
is_vendored() {
  case "$1" in
    zstd_release/*|ppmd/*) return 0 ;;
    *) return 1 ;;
  esac
}

for f in $(git ls-files '*.sh' '*.py' '*.cpp' '*.hpp'); do
  # Strip comment-only lines first: a filename MENTIONED in prose is not a dependency.
  code=$(sed -E 's,^[[:space:]]*(//|#).*$,,' "$f" 2>/dev/null)
  refs=$(printf '%s\n' "$code" | grep -ohE '[A-Za-z0-9_./-]+\.(py|sh|cpp|hpp|inc)' | sort -u)
  for r in $refs; do
    r="${r#./}"
    [ -e "$r" ] || continue                # only files that really exist here
    [ "$r" = "$f" ] && continue            # self-reference
    is_vendored "$r" && continue
    if ! printf '%s\n' "$TRACKED" | grep -qxF "$r"; then
      echo "  UNTRACKED DEP: $f  ->  $r"
      missing=$((missing+1))
    fi
  done
done

if [ "$missing" -eq 0 ]; then
  echo "check_tracked_deps: OK — every file referenced by a tracked script is itself tracked"
  exit 0
fi
echo "check_tracked_deps: FAIL — $missing untracked dependency reference(s)."
echo "  A fresh clone would get the script but not the file it needs."
echo "  Fix with 'git add -f <file>' (plus a '!<file>' negation in .gitignore if it is ignored),"
echo "  or drop the reference if it is genuinely scratch."
exit 1
