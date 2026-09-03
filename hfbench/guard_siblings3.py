#!/usr/bin/env python3
"""Third guard patch: REFERENCE (cross-block delta). encode_reference output was
adopted with no roundtrip check. Verify at the encode site with
decode_reference(encoded, size, ref_data, ref_size, original_size) -- the same
reference block the decoder will use -- and discard the candidate on mismatch so
the normal per-block path runs. Anchored on the unique MDL comment line to avoid
whitespace fragility. Exact-string replace, backup kept."""
import shutil, sys
P = "/root/mzip/mzip.hpp"
src = open(P, encoding="utf-8", errors="surrogateescape").read()
if "REFERENCE GUARD (2026-09-02)" in src: print("ALREADY APPLIED"); sys.exit(0)
OLD = """            if (!reference_encoded.empty()) {
                // Compare MDL: reference vs regular encoding
"""
NEW = """            // REFERENCE GUARD (2026-09-02): cross-block delta output was adopted unverified.
            // Decode it against the SAME reference block the decoder will use; on any
            // mismatch drop the candidate so the ordinary per-block path runs. Same
            // failure shape as NUM_EXTRACT (lossy specialist stream -> top-level verify
            // -> whole file to uRAW), and the hardest variant to reproduce because it
            // depends on a previous block.
            if (!reference_encoded.empty()) {
                auto ref_rt = decode_reference(reference_encoded.data(), reference_encoded.size(),
                                               ref.data.data(), ref.data.size(), this_block);
                if (ref_rt.size() != this_block || std::memcmp(ref_rt.data(), block_data, this_block) != 0)
                    reference_encoded.clear();
            }
            if (!reference_encoded.empty()) {
                // Compare MDL: reference vs regular encoding
"""
n = src.count(OLD)
if n != 1: print(f"REFUSING: anchor matched {n} times"); sys.exit(1)
shutil.copy2(P, P + ".bak-reference")
open(P, "w", encoding="utf-8", errors="surrogateescape").write(src.replace(OLD, NEW))
print("PATCHED REFERENCE; backup mzip.hpp.bak-reference")
print("ALLDONE_SIB3")
