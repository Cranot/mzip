#!/usr/bin/env python3
"""bench_run.py — MEASURE. Produce the immutable observation matrix. Nothing here reports.

The split is the point. Measurement writes an append-only JSONL matrix of (tool, file)
observations, each carrying its own evidence; reporting (`bench_report.py`) reads ONLY that matrix.
A report therefore cannot contain a number the matrix does not justify, and re-deriving a statistic
never requires re-running a compressor — which matters because a re-run is where a corpus quietly
changes under a claim.

WHAT IS DIFFERENT FROM THE LEGACY HARNESS
-----------------------------------------
  * Failure is INVALID, never 0. The old path used 0 for failure at seven sites and the report
    picks the smallest size as best, so a broken tool scored a perfect win.
  * Every artifact-producing tool is ROUNDTRIP-VERIFIED, not just mzip+CM. The old harness verified
    mzip and trusted the comparators; an unverified comparator size is an unverified claim about a
    rival, which is the direction of error nobody checks.
  * `zc.exe` is declared a SIZER: it prints a number and writes no artifact, so its sizes are
    marked `size_evidence="reported"` and can never masquerade as verified.
  * Tool identity is name + version + sha256, recorded per observation.

  usage:
    python3 bench_run.py                        # full corpus, all tools -> bench_matrix.jsonl
    python3 bench_run.py --quick                # small files only (smoke)
    python3 bench_run.py --types CSV,SQL        # subset
    python3 bench_run.py --tools gzip-9,mzip+CM
    python3 bench_run.py --out runs/m.jsonl
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
import time

import bench_corpus as bc
from bench_core import Matrix, Mode, Observation, Status, Tool, observe_compress, sha256_file

ROOT = bc.ROOT
os.chdir(ROOT)


# Which BINARY gets measured must be a tracked decision, not a property of whoever's PATH launched
# the run. Two concrete hazards on this box, both found by measurement rather than assumed:
#   * w64devkit's `gzip` and `bzip2` are BUSYBOX APPLETS, not GNU gzip / reference bzip2. (Measured
#     2026-08-13: BusyBox bzip2 -9 is byte-identical to libbz2 -9 on 6 files, and BusyBox gzip is
#     0.05% SMALLER than zlib-9 overall — so they were not biasing anything. But nobody knew that,
#     because the version probe was recording the error text "gzip: unknown option -- version" as
#     the version. Prefer the named reference builds so the column is attributable by construction.)
#   * w64devkit's `xz` is BROKEN — it returns 0 bytes and has no --version. Presence on PATH is
#     therefore NOT evidence a tool works, so every candidate is PROBED on real input before use.
PREFERRED = {
    "gzip":   ["C:/Program Files/Git/usr/bin/gzip.exe", "/usr/bin/gzip", "gzip"],
    "bzip2":  ["C:/Program Files/Git/mingw64/bin/bzip2.exe", "/mingw64/bin/bzip2.exe", "bzip2"],
    "brotli": ["C:/Program Files/Git/mingw64/bin/brotli.exe", "/mingw64/bin/brotli.exe", "brotli"],
    "xz":     ["C:/Program Files/Git/mingw64/bin/xz.exe", "/mingw64/bin/xz.exe", "xz"],
}


def resolve(kind: str, probe_args) -> str:
    """First candidate that EXISTS and actually produces output. Returns the bare name if none
    works, so the observation records TOOL_MISSING / NONZERO_RC rather than silently vanishing."""
    for c in PREFERRED.get(kind, [kind]):
        p = c if os.path.exists(c) else shutil.which(c)
        if not p:
            continue
        try:
            r = subprocess.run([p] + list(probe_args) + [__file__], capture_output=True, timeout=60)
            if r.returncode == 0 and len(r.stdout) > 0:
                return p
        except Exception:
            pass
    return kind


ZDICT = "train_corpus/code_dict.bin"   # mzip's OWN pre-trained zstd dict


def build_tools():
    """Declare the comparator set. Absence is a first-class state — a missing tool yields
    TOOL_MISSING observations, not a missing column that silently narrows the comparison."""
    gz = resolve("gzip", ["-9", "-c"])
    bz = resolve("bzip2", ["-9", "-c"])
    br = resolve("brotli", ["-q", "11", "-c"])
    xz = resolve("xz", ["-9", "-e", "-c"])
    T = []
    T.append(Tool("gzip-9", [gz, "-9", "-c"], version_argv=[gz, "--version"],
                  decompress_argv=[gz, "-d", "-c"], mode=Mode.STDOUT))
    T.append(Tool("bzip2-9", [bz, "-9", "-c"], version_argv=[bz, "--help"],
                  decompress_argv=[bz, "-d", "-c"], mode=Mode.STDOUT))
    T.append(Tool("brotli-11", [br, "-q", "11", "-c"], version_argv=[br, "--version"],
                  decompress_argv=[br, "-d", "-c"], mode=Mode.STDOUT))
    T.append(Tool("xz-9e", [xz, "-9", "-e", "-c"], version_argv=[xz, "--version"],
                  decompress_argv=[xz, "-d", "-c"], mode=Mode.STDOUT))
    # zc.exe REPORTS a size and produces no artifact. Marked, never verified.
    zcv = ["./zc.exe", "--version"]
    T.append(Tool("zstd-19", ["./zc.exe", "{in}", "19"], version_argv=zcv, mode=Mode.SIZER,
                  label="sizer: prints a size, writes no artifact"))
    T.append(Tool("zstd-22", ["./zc.exe", "{in}", "22"], version_argv=zcv, mode=Mode.SIZER,
                  label="sizer: prints a size, writes no artifact"))
    T.append(Tool("zstd-19+dict", ["./zc.exe", "{in}", "19", ZDICT], version_argv=zcv,
                  mode=Mode.SIZER,
                  label="sizer, handed mzip's OWN dictionary (fairest dict-vs-dict comparator)"))
    T.append(Tool("mzip(noCM)", ["./mzip_base.exe", "c", "{in}", "{out}"], mode=Mode.FILE,
                  decompress_argv=["./mzip_base.exe", "d", "{in}", "{out}"],
                  label="-DMZIP_NO_CM A/B arm"))
    T.append(Tool("mzip+CM", ["./mzip_cm.exe", "c", "{in}", "{out}"], mode=Mode.FILE,
                  decompress_argv=["./mzip_cm.exe", "d", "{in}", "{out}"],
                  label="the product"))
    return T


def git_head():
    try:
        r = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, timeout=15)
        if r.returncode == 0:
            head = r.stdout.strip()
            # A bare "dirty" flag is nearly useless: this tree always has untracked scratch files,
            # so it would read dirty forever and stop meaning anything. What matters is whether a
            # TRACKED source changed, i.e. whether the binary can still be attributed to the
            # commit. Name them.
            d = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"],
                               capture_output=True, text=True, timeout=30)
            mod = [l[3:] for l in d.stdout.splitlines() if l.strip()] if d.returncode == 0 else None
            return {"head": head, "dirty": bool(mod) if mod is not None else None,
                    "modified_tracked": mod,
                    "mzip_hpp_sha256": sha256_file(os.path.join(ROOT, "mzip.hpp"))}
    except Exception:
        pass
    return {"head": "?", "dirty": None}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="bench_matrix.jsonl")
    ap.add_argument("--types", default="", help="comma-separated subset of content types")
    ap.add_argument("--tools", default="", help="comma-separated subset of tools")
    ap.add_argument("--quick", action="store_true", help="only files < 256 KB (smoke run)")
    ap.add_argument("--timeout", type=int, default=3600)
    ap.add_argument("--note", default="", help="free text recorded in the matrix header")
    a = ap.parse_args()

    problems = bc.check_partition()
    if problems:
        print("CORPUS PARTITION IS INCONSISTENT — refusing to measure:", file=sys.stderr)
        for p in problems:
            print("  " + p, file=sys.stderr)
        return 2

    tools = build_tools()
    if a.tools:
        want = {s.strip() for s in a.tools.split(",") if s.strip()}
        unknown = want - {t.name for t in tools}
        if unknown:
            print(f"unknown tool(s): {sorted(unknown)}", file=sys.stderr)
            return 2
        tools = [t for t in tools if t.name in want]

    ents = list(bc.entries())
    if a.types:
        want = {s.strip() for s in a.types.split(",") if s.strip()}
        unknown = want - set(bc.TYPES)
        if unknown:
            print(f"unknown type(s): {sorted(unknown)}", file=sys.stderr)
            return 2
        ents = [e for e in ents if e[0] in want]
    if a.quick:
        ents = [e for e in ents if os.path.getsize(e[1]) < 256 * 1024]

    if not ents:
        # A gate that passes on zero inputs is a gate that passes. This project has shipped one.
        print("REFUSING: zero input files selected. An empty run is not a passing run.",
              file=sys.stderr)
        return 2

    if os.path.exists(a.out):
        print(f"REFUSING: {a.out} already exists. The matrix is append-only and immutable per run; "
              f"move it aside or pass --out.", file=sys.stderr)
        return 2

    m = Matrix(a.out)
    m.header({
        "kind": "bench_run",
        "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "corpus": bc.summary(),
        "corpus_missing": bc.missing(),
        "tools": [t.identity() for t in tools],
        "files_selected": len(ents),
        "quick": bool(a.quick),
        "types_filter": a.types or None,
        "tools_filter": a.tools or None,
        "note": a.note,
        "git": git_head(),
        "host": {"platform": platform.platform(), "python": sys.version.split()[0],
                 "cpu": platform.processor()},
    })

    unavailable = [t.name for t in tools if not t.available]
    if unavailable:
        print(f"NOTE: {len(unavailable)} tool(s) unavailable and will be recorded as "
              f"TOOL_MISSING (not as 0): {unavailable}", file=sys.stderr)

    n_obs = n_ok = n_invalid = 0
    t_start = time.time()
    for i, (typ, f, real, dom) in enumerate(ents, 1):
        print(f"[{i}/{len(ents)}] {typ:38s} {f}", file=sys.stderr, flush=True)
        for t in tools:
            o = observe_compress(t, f, timeout=a.timeout)
            m.add(o, type=typ, real=real, domain=dom, corpus=bc.CORPUS_LABEL)
            n_obs += 1
            if o.status == Status.OK:
                n_ok += 1
            else:
                n_invalid += 1
                print(f"      {t.name:14s} {o.status}  {o.detail or o.stderr_tail[:90]}",
                      file=sys.stderr, flush=True)
    m.close()

    dt = time.time() - t_start
    print(f"\nmatrix: {a.out}", file=sys.stderr)
    print(f"observations: {n_obs}   ok: {n_ok}   invalid: {n_invalid}   wall: {dt:.1f}s",
          file=sys.stderr)
    print(f"corpus: {bc.CORPUS_LABEL} (NOT blind — see bench_corpus.CORPUS_NOTE)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
