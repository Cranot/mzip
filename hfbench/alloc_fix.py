#!/usr/bin/env python3
"""Size compress_impl's per-call work buffers to the input, not to the 16 MiB block
default. block_size itself is untouched (recursive arms keep identical behaviour)."""
import sys, shutil
P = sys.argv[1]
s = open(P).read()
old_nb = "    // Calculate number of blocks\n    size_t num_blocks = (size + block_size - 1) / block_size;\n"
assert s.count(old_nb) == 1, "num_blocks anchor"
new_nb = old_nb + (
"\n    // Work-buffer sizing: a block is never larger than the input, so size the per-call\n"
"    // buffers to min(block_size, input) (64 KiB floor). Before this, every call below\n"
"    // 16 MiB zero-filled ~112 MB (output + 3x preprocess + compressBound(3x)) -- measured\n"
"    // 443k minor page faults / 279 MB RSS to compress one 64 KiB chunk 12 times, a fixed\n"
"    // ~40-80 ms per call that capped FAST at <1 MB/s regardless of encoder speed.\n"
"    // block_size itself is NOT changed: recursive compress_impl arms still receive it.\n"
"    const size_t alloc_block = std::min(block_size, std::max(size, (size_t)65536));\n")
s = s.replace(old_nb, new_nb)
reps = [
 ("    size_t max_output = 17 + num_blocks * (14 + block_size + block_size / 100 + 100);\n",
  "    size_t max_output = 17 + num_blocks * (14 + alloc_block + alloc_block / 100 + 100);\n"),
 ("    std::vector<uint8_t> preprocess_buf(block_size * 3);\n",
  "    std::vector<uint8_t> preprocess_buf(alloc_block * 3);\n"),
 ("    std::vector<uint8_t> compress_buf(ZSTD_compressBound(block_size * 3));\n",
  "    std::vector<uint8_t> compress_buf(ZSTD_compressBound(alloc_block * 3));\n"),
]
for a, b in reps:
    assert s.count(a) == 1, a
    s = s.replace(a, b)
shutil.copy(P, P + ".bak-allocfix")
open(P, "w").write(s)
print("PATCHED", P)
