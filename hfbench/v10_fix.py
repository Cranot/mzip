#!/usr/bin/env python3
"""v10: accept huff0's single-symbol (RLE) result. HUF_compress returns 1 with the byte in dst[0]
when every byte of the plane is identical; HUF_decompress(cSrcSize==1) regenerates it by memset.
v9 required h > 1 and therefore stored every constant plane RAW -- on bge-small plane 0 is all
zeros in all 128 chunks (2,097,152 B = 25% of the input stored raw). Measured by the lz-friendly-f32
scout: per-plane huff0 with RLE 2.3516x vs FAST 1.4801x."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
old = '''            if (!HUF_isError(h) && h > 1 && h < L) { flags |= (uint8_t)(1u << j); hflags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + h); }'''
new = '''            // h == 1 is huff0's RLE result (single symbol, byte in cb[0]); HUF_decompress regenerates it.
            if (!HUF_isError(h) && h >= 1 && h < L) { flags |= (uint8_t)(1u << j); hflags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + h); }'''
assert s.count(old) == 1, "anchor count=%d" % s.count(old)
s = s.replace(old, new)
open(P, "w").write(s); print("PATCHED v10", P)
