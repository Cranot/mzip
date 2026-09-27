#!/usr/bin/env python3
"""bench_core.py — fail-closed measurement primitives for the mzip benchmark.

WHY THIS EXISTS
---------------
The previous measurement layer returned a bare integer and used 0 to mean failure, at seven sites.
Because the report selects the SMALLEST size as best, that made a missing or crashed tool look like a
PERFECT ZERO-BYTE WIN. `_stdout` did not check the return code at all, so `len(b"") == 0` from a tool
that never ran was indistinguishable from a real measurement. The direction of the resulting lie
depended on which tool broke: a broken comparator flattered mzip's rivals to perfection, a broken mzip
runner flattered mzip. Nothing in the pipeline would have said so.

THE RULE HERE: a measurement is either a number that was OBSERVED, or it is INVALID. There is no third
option and INVALID is never coerced to a number. An observation carries its own evidence — return
code, stderr, tool version, binary hash, and whether the output file was newly created.

Everything is written to an append-only raw matrix (JSONL). Reports are regenerated ONLY from that
matrix, so a report can never contain a number the matrix does not justify.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass, asdict, field
from typing import Optional, Sequence

SCHEMA_VERSION = "bench_core/1"


class Mode:
    """How a tool delivers its result. This is not cosmetic: it decides what EVIDENCE exists.

    STDOUT / FILE produce a compressed ARTIFACT, so a decompressor can be run against it and the
    size can be tied to bytes that really existed.

    SIZER covers `zc.exe`, which PRINTS a number and writes nothing. Its size is therefore
    UNVERIFIABLE BY CONSTRUCTION — no artifact ever exists to decompress. The old harness mixed
    those numbers into the same column as verified ones with nothing marking the difference, so a
    reader could not tell which of the report's sizes had been checked. Every observation now
    records `size_evidence` so that distinction survives into the matrix.
    """
    STDOUT = "stdout"
    FILE = "file"
    SIZER = "sizer"


class Status:
    """An observation is OK or it is one of these. INVALID never becomes a number."""
    OK = "ok"
    TOOL_MISSING = "invalid_tool_missing"
    NONZERO_RC = "invalid_nonzero_rc"
    NO_OUTPUT = "invalid_no_output"
    STALE_OUTPUT = "invalid_stale_output"      # output existed before the run
    EMPTY_OUTPUT = "invalid_empty_output"      # 0 bytes is never a legitimate compressed size
    PARSE_FAILED = "invalid_parse_failed"
    ROUNDTRIP_FAILED = "invalid_roundtrip_failed"
    TIMEOUT = "invalid_timeout"
    EXCEPTION = "invalid_exception"

    INVALID = None  # sentinel for "anything not OK"

    @staticmethod
    def is_valid(s: str) -> bool:
        return s == Status.OK


@dataclass
class Observation:
    """One (tool, file) measurement. `size` is meaningful ONLY when status == ok."""
    schema: str
    tool: str
    file: str
    file_bytes: int
    file_sha256: str
    status: str
    size: Optional[int] = None            # None unless status == ok
    encode_s: Optional[float] = None
    decode_s: Optional[float] = None
    verified_roundtrip: Optional[bool] = None   # None = no decompressor available to check
    tool_version: str = "?"
    tool_sha256: str = "?"
    returncode: Optional[int] = None
    stderr_tail: str = ""
    detail: str = ""
    mode: str = "stdout"
    # "artifact" = a compressed file existed and was measured; "reported" = a sizer printed the
    # number and no artifact was ever produced, so it cannot be verified. Never conflate the two.
    size_evidence: str = "artifact"

    def as_row(self) -> dict:
        return asdict(self)


def sha256_file(path: str, limit: Optional[int] = None) -> str:
    h = hashlib.sha256()
    try:
        with open(path, "rb") as fh:
            while True:
                b = fh.read(1 << 20)
                if not b:
                    break
                h.update(b)
                if limit is not None and fh.tell() >= limit:
                    break
    except OSError:
        return "?"
    return h.hexdigest()


class Tool:
    """A comparator, with its identity pinned. Absence is a first-class state, not a zero."""

    def __init__(self, name: str, argv: Sequence[str], version_argv: Optional[Sequence[str]] = None,
                 decompress_argv: Optional[Sequence[str]] = None, mode: str = Mode.STDOUT,
                 label: str = ""):
        self.name = name
        self.argv = list(argv)
        self.version_argv = list(version_argv) if version_argv else None
        self.decompress_argv = list(decompress_argv) if decompress_argv else None
        self.mode = mode
        self.label = label            # free-text note carried into the matrix header
        self.path = self._resolve(self.argv[0])
        self.available = self.path is not None
        self.sha256 = sha256_file(self.path) if self.path else "?"
        self.version = self._probe_version() if self.available else "MISSING"

    @staticmethod
    def _resolve(exe: str) -> Optional[str]:
        if os.path.isabs(exe) or exe.startswith("./") or exe.startswith(".\\"):
            return exe if os.path.exists(exe) else None
        return shutil.which(exe)

    def _probe_version(self) -> str:
        # A FAILED probe must not be recorded as a version. The published report's own methodology
        # line reads "gzip: unknown option -- version": the old probe took the first non-empty line
        # of output whatever it was, so the ERROR TEXT became the version string and the shipped
        # column was attributed to a binary the harness could not name. Failure is now labelled as
        # failure; identity still holds because sha256 is recorded either way.
        if not self.version_argv:
            return "(no version probe)"
        try:
            r = subprocess.run(self.version_argv, capture_output=True, text=True, timeout=15)
        except Exception as e:
            return f"(version probe raised {type(e).__name__})"
        if r.returncode != 0:
            first = next((s.strip() for s in (r.stdout + "\n" + r.stderr).splitlines() if s.strip()), "")
            return f"(version probe failed rc={r.returncode}: {first[:80]})"
        for line in (r.stdout + "\n" + r.stderr).splitlines():
            if line.strip():
                return line.strip()
        return "(version probe produced no output)"

    def identity(self) -> dict:
        return {"tool": self.name, "path": self.path, "available": self.available,
                "version": self.version, "sha256": self.sha256, "mode": self.mode,
                "argv": self.argv, "label": self.label,
                "verifiable": self.mode != Mode.SIZER and bool(self.decompress_argv)}


def _subst(argv: Sequence[str], mapping: dict) -> list:
    out = []
    for a in argv:
        for k, v in mapping.items():
            a = a.replace(k, v)
        out.append(a)
    return out


def observe_compress(tool: Tool, src: str, timeout: int = 3600) -> Observation:
    """Compress `src` with `tool` into a UNIQUE temp dir. Fail closed at every step.

    The temp dir is per-observation: a SHARED temp path already produced a phantom roundtrip failure
    in this project's own A/B sweep on 2026-08-12, when two runs overwrote each other's artefacts.
    """
    src_bytes = os.path.getsize(src) if os.path.exists(src) else -1
    ev = "reported" if tool.mode == Mode.SIZER else "artifact"
    base = dict(schema=SCHEMA_VERSION, tool=tool.name, file=src,
                file_bytes=src_bytes, file_sha256=sha256_file(src),
                tool_version=tool.version, tool_sha256=tool.sha256,
                mode=tool.mode, size_evidence=ev)

    if not tool.available:
        return Observation(**base, status=Status.TOOL_MISSING,
                           detail=f"{tool.argv[0]} not found on PATH or at the given path")
    if src_bytes < 0:
        return Observation(**base, status=Status.NO_OUTPUT, detail="input file does not exist")

    workdir = tempfile.mkdtemp(prefix="benchobs_")
    out = os.path.join(workdir, "out.bin")
    try:
        if os.path.exists(out):                      # must not pre-exist
            return Observation(**base, status=Status.STALE_OUTPUT, detail=out)

        subs = {"{in}": src, "{out}": out}
        if tool.mode == Mode.STDOUT and "{in}" not in " ".join(tool.argv):
            argv = [tool.path] + tool.argv[1:] + [src]
        else:
            argv = [tool.path] + _subst(tool.argv[1:], subs)

        t0 = time.time()
        try:
            r = subprocess.run(argv, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return Observation(**base, status=Status.TIMEOUT, detail=f"timeout after {timeout}s")
        enc = time.time() - t0
        err_tail = (r.stderr or b"")[-400:].decode("utf-8", "replace")

        if r.returncode != 0:
            return Observation(**base, status=Status.NONZERO_RC, returncode=r.returncode,
                               stderr_tail=err_tail, encode_s=enc)

        if tool.mode == Mode.SIZER:
            # No artifact exists. The number is only as good as the tool's own reporting, and that
            # is recorded (size_evidence="reported") rather than papered over.
            txt = (r.stdout or b"").decode("utf-8", "replace").strip()
            try:
                size = int(txt.split()[0])
            except (ValueError, IndexError):
                return Observation(**base, status=Status.PARSE_FAILED, returncode=r.returncode,
                                   stderr_tail=err_tail, encode_s=enc,
                                   detail=f"stdout not an integer: {txt[:120]!r}")
            if size <= 0:
                return Observation(**base, status=Status.EMPTY_OUTPUT, returncode=r.returncode,
                                   stderr_tail=err_tail, encode_s=enc,
                                   detail=f"sizer reported {size}")
            return Observation(**base, status=Status.OK, size=size, encode_s=enc,
                               verified_roundtrip=None, returncode=0, stderr_tail=err_tail,
                               detail="size reported by a sizer; no artifact to verify")

        if tool.mode == Mode.STDOUT:
            with open(out, "wb") as fh:              # these tools write to stdout
                fh.write(r.stdout)

        if not os.path.exists(out):                  # FILE mode: the tool was asked for {out}
            return Observation(**base, status=Status.NO_OUTPUT, returncode=r.returncode,
                               stderr_tail=err_tail, encode_s=enc)
        size = os.path.getsize(out)
        if size == 0:
            # 0 is never a legitimate compressed size for a non-empty input, and treating it as one
            # is precisely the failure this module exists to prevent.
            return Observation(**base, status=Status.EMPTY_OUTPUT, returncode=r.returncode,
                               stderr_tail=err_tail, encode_s=enc)

        verified = None
        dec_s = None
        if tool.decompress_argv:
            dpath = os.path.join(workdir, "roundtrip.bin")
            dsubs = {"{in}": out, "{out}": dpath}
            if "{in}" in " ".join(tool.decompress_argv):
                dargv = _subst(tool.decompress_argv, dsubs)
            else:
                dargv = list(tool.decompress_argv) + [out]
            t1 = time.time()
            try:
                dr = subprocess.run(dargv, capture_output=True, timeout=timeout)
                dec_s = time.time() - t1
                if dr.returncode == 0:
                    if not os.path.exists(dpath):    # stdout decompressors
                        with open(dpath, "wb") as fh:
                            fh.write(dr.stdout)
                    verified = (sha256_file(dpath) == base["file_sha256"])
                else:
                    verified = False
            except Exception:
                verified = False
            if verified is False:
                return Observation(**base, status=Status.ROUNDTRIP_FAILED, size=size,
                                   returncode=r.returncode, stderr_tail=err_tail,
                                   encode_s=enc, decode_s=dec_s, verified_roundtrip=False)

        return Observation(**base, status=Status.OK, size=size, encode_s=enc, decode_s=dec_s,
                           verified_roundtrip=verified, returncode=0, stderr_tail=err_tail)
    except Exception as e:                            # never let an exception become a number
        return Observation(**base, status=Status.EXCEPTION, detail=f"{type(e).__name__}: {e}")
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


class Matrix:
    """Append-only raw observation matrix. Reports are regenerated ONLY from this."""

    def __init__(self, path: str):
        self.path = path
        self._fh = open(path, "a", encoding="utf-8")

    def add(self, obs: Observation, **extra) -> Observation:
        """Append one observation. `extra` carries the run's labelling (type, domain, real,
        corpus) — it may NOT overwrite a measured field, because a label silently replacing a
        measurement is exactly the class of defect this module exists to prevent."""
        row = obs.as_row()
        clash = set(extra) & set(row)
        if clash:
            raise ValueError(f"label(s) {sorted(clash)} would overwrite measured fields")
        row.update(extra)
        self._fh.write(json.dumps(row, sort_keys=True) + "\n")
        self._fh.flush()
        return obs

    def header(self, meta: dict) -> None:
        self._fh.write(json.dumps({"schema": SCHEMA_VERSION, "_meta": meta}, sort_keys=True) + "\n")
        self._fh.flush()

    def close(self) -> None:
        self._fh.close()

    @staticmethod
    def load(path: str):
        rows, meta = [], None
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                r = json.loads(line)
                if "_meta" in r:
                    meta = r["_meta"]
                else:
                    rows.append(r)
        return meta, rows


# --------------------------------------------------------------------------------------------
# SELF-TEST. An instrument must be SHOWN to move: this project has shipped a gate that passed
# vacuously on zero input files, and a "sampled peak" that was not a maximum. So the fail-closed
# behaviour is exercised here rather than asserted in a comment.
#   python3 bench_core.py --selftest
# --------------------------------------------------------------------------------------------
def _selftest() -> int:
    import sys
    ok = True

    def check(name, cond):
        nonlocal ok
        print(f"  {'PASS' if cond else 'FAIL'}  {name}")
        ok = ok and cond

    d = tempfile.mkdtemp(prefix="benchselftest_")
    src = os.path.join(d, "input.txt")
    with open(src, "wb") as fh:
        fh.write(b"the quick brown fox jumps over the lazy dog\n" * 500)

    missing = Tool("nope", ["definitely_not_a_real_tool_xyz", "-c"])
    o = observe_compress(missing, src)
    check("missing tool -> TOOL_MISSING, size is None (NOT 0)",
          o.status == Status.TOOL_MISSING and o.size is None)

    # a tool that exists but exits non-zero must not yield a size
    bad = Tool("bad-args", [sys.executable, "-c", "import sys; sys.exit(3)"])
    o = observe_compress(bad, src)
    check("nonzero rc -> NONZERO_RC, size is None",
          o.status == Status.NONZERO_RC and o.size is None and o.returncode == 3)

    # a tool that succeeds but emits nothing must be INVALID, not a 0-byte win
    empty = Tool("empty-out", [sys.executable, "-c", "pass"])
    o = observe_compress(empty, src)
    check("empty output -> EMPTY_OUTPUT, size is None (the 0-byte-win bug)",
          o.status == Status.EMPTY_OUTPUT and o.size is None)

    # a real measurement must carry a size and its evidence
    good = Tool("cat-passthrough", [sys.executable, "-c",
                "import sys;sys.stdout.buffer.write(open(sys.argv[1],'rb').read())"])
    o = observe_compress(good, src)
    check("working tool -> OK with a real size", o.status == Status.OK and o.size == os.path.getsize(src))
    check("observation records its own evidence", o.tool_sha256 != "?" and o.returncode == 0)

    # roundtrip mismatch must be INVALID even though compression 'succeeded'
    liar = Tool("liar", [sys.executable, "-c",
                "import sys;sys.stdout.buffer.write(b'garbage')"],
                decompress_argv=[sys.executable, "-c",
                "import sys;sys.stdout.buffer.write(b'not the input')"])
    o = observe_compress(liar, src)
    check("roundtrip mismatch -> ROUNDTRIP_FAILED", o.status == Status.ROUNDTRIP_FAILED)

    # FILE mode: the shape mzip uses (in-path + out-path, separate decode invocation).
    PY = sys.executable
    copy_io = "import sys,shutil;shutil.copyfile(sys.argv[1],sys.argv[2])"
    fmode = Tool("file-mode", [PY, "-c", copy_io, "{in}", "{out}"], mode=Mode.FILE,
                 decompress_argv=[PY, "-c", copy_io, "{in}", "{out}"])
    o = observe_compress(fmode, src)
    check("file mode -> OK, artifact evidence, roundtrip VERIFIED",
          o.status == Status.OK and o.size == os.path.getsize(src)
          and o.size_evidence == "artifact" and o.verified_roundtrip is True)

    # FILE mode where decode gives back the wrong bytes must still fail closed.
    fbad = Tool("file-mode-liar", [PY, "-c", copy_io, "{in}", "{out}"], mode=Mode.FILE,
                decompress_argv=[PY, "-c",
                                 "import sys;open(sys.argv[2],'wb').write(b'wrong')", "{in}", "{out}"])
    o = observe_compress(fbad, src)
    check("file mode + wrong decode -> ROUNDTRIP_FAILED", o.status == Status.ROUNDTRIP_FAILED)

    # SIZER mode: zc.exe's shape. A number with NO artifact behind it. It must be accepted but
    # MARKED, because nothing can ever verify it.
    sizer = Tool("sizer", [PY, "-c", "print(1234)"], mode=Mode.SIZER)
    o = observe_compress(sizer, src)
    check("sizer -> OK but size_evidence='reported' and roundtrip UNKNOWN (None, not True)",
          o.status == Status.OK and o.size == 1234 and o.size_evidence == "reported"
          and o.verified_roundtrip is None)

    junk = Tool("sizer-junk", [PY, "-c", "print('not a number')"], mode=Mode.SIZER)
    o = observe_compress(junk, src)
    check("sizer printing junk -> PARSE_FAILED, size is None",
          o.status == Status.PARSE_FAILED and o.size is None)

    zero = Tool("sizer-zero", [PY, "-c", "print(0)"], mode=Mode.SIZER)
    o = observe_compress(zero, src)
    check("sizer printing 0 -> EMPTY_OUTPUT, size is None (the 0-byte-win bug, sizer variant)",
          o.status == Status.EMPTY_OUTPUT and o.size is None)

    # matrix round-trips
    mpath = os.path.join(d, "matrix.jsonl")
    m = Matrix(mpath); m.header({"note": "selftest"}); m.add(o); m.close()
    meta, rows = Matrix.load(mpath)
    check("matrix persists meta + rows", meta is not None and len(rows) == 1)

    shutil.rmtree(d, ignore_errors=True)
    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    import sys
    if "--selftest" in sys.argv:
        raise SystemExit(_selftest())
    print(__doc__)
