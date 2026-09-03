#!/usr/bin/env python3
"""v9: PLANE_ENTROPY planes coded with huff0 (reference FSE lib) instead of zstd-1 frames.
Layout: [k:1][flags:1][hflags:1][clen varint x k][planes][tail]; flags bit j = plane j coded,
hflags bit j = huff0 (else zstd-1, used when the plane exceeds HUF_BLOCKSIZE_MAX or
MZIP_PE_ZSTD=1 is set for A/B). Raw planes pass through."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
rep('''#include <cmath>
namespace mzpe {''','''#include <cmath>
#include "huf.h"
namespace mzpe {''', "include")
rep('''inline std::vector<uint8_t> encode_k(const uint8_t* d, size_t n, unsigned k) {
    size_t L = n / k, tail = n - L*k;
    std::vector<uint8_t> plane(L), out; out.reserve(n + 64);
    std::vector<std::vector<uint8_t>> pay(k); uint8_t flags = 0;
    std::vector<uint8_t> cb(ZSTD_compressBound(L) + 16);
    for (unsigned j = 0; j < k; j++) {
        for (size_t i = 0; i < L; i++) plane[i] = d[i*k + j];
        size_t z = ZSTD_compress(cb.data(), cb.size(), plane.data(), L, 1);
        if (!ZSTD_isError(z) && z < L) { flags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + z); }
        else pay[j] = plane;
    }
    out.push_back((uint8_t)k); out.push_back(flags);
''','''inline std::vector<uint8_t> encode_k(const uint8_t* d, size_t n, unsigned k) {
    static const bool use_zstd = std::getenv("MZIP_PE_ZSTD") != nullptr;
    size_t L = n / k, tail = n - L*k;
    std::vector<uint8_t> plane(L), out; out.reserve(n + 64);
    std::vector<std::vector<uint8_t>> pay(k); uint8_t flags = 0, hflags = 0;
    std::vector<uint8_t> cb(std::max(ZSTD_compressBound(L), HUF_compressBound(L)) + 16);
    for (unsigned j = 0; j < k; j++) {
        for (size_t i = 0; i < L; i++) plane[i] = d[i*k + j];
        // v9: huff0 per plane (order-0 is the whole gain on weights; huff0 encodes ~1 GB/s and
        // decodes multi-GB/s). zstd-1 only where huff0 cannot (plane > 128 KiB) or for A/B.
        if (!use_zstd && L <= HUF_BLOCKSIZE_MAX && L >= 16) {
            size_t h = HUF_compress(cb.data(), cb.size(), plane.data(), L);
            if (!HUF_isError(h) && h > 1 && h < L) { flags |= (uint8_t)(1u << j); hflags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + h); }
            else pay[j] = plane;
        } else {
            size_t z = ZSTD_compress(cb.data(), cb.size(), plane.data(), L, 1);
            if (!ZSTD_isError(z) && z < L) { flags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + z); }
            else pay[j] = plane;
        }
    }
    out.push_back((uint8_t)k); out.push_back(flags); out.push_back(hflags);
''', "encode_k")
rep('''    if (sz < 2) return {};
    unsigned k = *p++; uint8_t flags = *p++;
    if (k != 2 && k != 4) return {};''','''    if (sz < 3) return {};
    unsigned k = *p++; uint8_t flags = *p++; uint8_t hflags = *p++;
    if (k != 2 && k != 4) return {};''', "decode header")
old_dec = '''        if (flags & (1u << j)) {
            size_t r = ZSTD_decompress(plane.data(), L, p, (size_t)cl[j]);'''
assert s.count(old_dec) == 1, "decode plane anchor"
s = s.replace(old_dec, '''        if (flags & (1u << j)) {
            size_t r = (hflags & (1u << j)) ? HUF_decompress(plane.data(), L, p, (size_t)cl[j])
                                            : ZSTD_decompress(plane.data(), L, p, (size_t)cl[j]);''')
# the error check after decompress must accept both libs' error conventions
n = s.index('''            size_t r = (hflags & (1u << j)) ? HUF_decompress''')
seg_end = s.index("\n", s.index("\n", n) + 1)
nxt = s[seg_end:seg_end+400]
print("NEXT LINES AFTER DECODE CALL:\n" + nxt[:300])
shutil.copy(P, P + ".bak-v9"); open(P, "w").write(s); print("PATCHED v9", P)
