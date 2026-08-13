#!/usr/bin/env bash
# install_hooks.sh — copy hooks/ into .git/hooks/ so they actually fire.
# .git/hooks is not version-controlled, so a tracked hook only protects anyone who installs it;
# build_evals.sh calls this, which means the first build in a fresh clone arms the gates.
set -u
cd "$(dirname "$0")"
root=$(git rev-parse --git-dir 2>/dev/null) || { echo "install_hooks: not a git repo, skipping"; exit 0; }
n=0
for h in hooks/*; do
  [ -f "$h" ] || continue
  cp "$h" "$root/hooks/$(basename "$h")" && chmod +x "$root/hooks/$(basename "$h")" && n=$((n+1))
done
echo "install_hooks: installed $n hook(s) into $root/hooks"
