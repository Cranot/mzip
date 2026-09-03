#!/usr/bin/env python3
"""v6: compute the cross-block SimHash fingerprint only when the stream has >1 block."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
old = "        BlockFingerprint current_fp = compute_fingerprint(block_data, this_block, block_count);\n"
new = ("        // v6: the fingerprint is consumed only by (a) the similarity scan against recent_blocks\n"
       "        // (empty for the first block) and (b) storage for LATER blocks. In a single-block stream\n"
       "        // neither happens, yet the SimHash (64K shingles x 64-bit inner loop, ~4 ms per 64 KiB)\n"
       "        // was 72% of the remaining FAST time on 64 KiB chunks (gdb sampling, 43/60 samples).\n"
       "        // Skipping it when num_blocks == 1 cannot change any output.\n"
       "        BlockFingerprint current_fp;\n"
       "        if (num_blocks > 1) current_fp = compute_fingerprint(block_data, this_block, block_count);\n"
       "        else { current_fp.hash = 0; }\n")
assert s.count(old) == 1, "fingerprint anchor"
s = s.replace(old, new)
shutil.copy(P, P + ".bak-v6"); open(P, "w").write(s); print("PATCHED v6", P)
