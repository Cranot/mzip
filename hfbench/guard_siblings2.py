#!/usr/bin/env python3
"""Second guard patch: DBF_CONSTCOL. It adopts encode_dbf_constcol output on a
size comparison against bwt9 with no roundtrip check. Verify with
decode_dbf_constcol(data, n, original_size); on mismatch fall to the bwt result
(which is what the branch already does when CC is not smaller). Exact-string
replace on /root/mzip/mzip.hpp, backup kept, refuses on non-unique anchor.
Run after guard_siblings.py, on a quiet box; then rebuild + suite."""
import shutil, subprocess, sys
P = "/root/mzip/mzip.hpp"
src = open(P, encoding="utf-8", errors="surrogateescape").read()
if "DBF_CONSTCOL GUARD (2026-09-02)" in src: print("ALREADY APPLIED"); sys.exit(0)
OLD = """            if (!cc_compressed.empty() && cc_compressed.size() < bwt_compressed.size()) {
                // CC+zstd wins — store as DBF_CONSTCOL (already zstd-compressed)
                memcpy(preprocess_data, cc_compressed.data(), cc_compressed.size());"""
NEW = """            // DBF_CONSTCOL GUARD (2026-09-02): the CC stream was adopted on size alone. Same
            // failure shape as NUM_EXTRACT (lossy specialist output -> top-level verify ->
            // whole file to uRAW). Verify it decodes; if not, let the bwt result win below.
            bool cc_ok = false;
            if (!cc_compressed.empty()) {
                auto cc_rt = decode_dbf_constcol(cc_compressed.data(), cc_compressed.size(), this_block);
                cc_ok = (cc_rt.size() == this_block && std::memcmp(cc_rt.data(), block_data, this_block) == 0);
            }
            if (cc_ok && cc_compressed.size() < bwt_compressed.size()) {
                // CC+zstd wins — store as DBF_CONSTCOL (already zstd-compressed)
                memcpy(preprocess_data, cc_compressed.data(), cc_compressed.size());"""
n = src.count(OLD)
if n != 1: print(f"REFUSING: anchor matched {n} times"); sys.exit(1)
shutil.copy2(P, P + ".bak-dbf")
open(P, "w", encoding="utf-8", errors="surrogateescape").write(src.replace(OLD, NEW))
print("PATCHED DBF_CONSTCOL; backup mzip.hpp.bak-dbf")
print("ALLDONE_SIB2")
