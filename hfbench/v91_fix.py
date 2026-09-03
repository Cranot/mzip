#!/usr/bin/env python3
"""v9.1: segment the PLANE_ENTROPY input so every plane fits huff0's 128 KiB block limit.
Layout: [nseg varint] then per segment [seg_n varint][encode_k payload]. Segment size 256 KiB
(k=2 -> 128 KiB planes, k=4 -> 64 KiB). Below 256 KiB nothing changes but the 1-2 byte prefix."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
# rename the existing whole-input encode/decode to *_seg and add segmenting wrappers
rep('''inline std::vector<uint8_t> encode(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    static const bool best2 = std::getenv("MZIP_PE_BEST2") != nullptr;''','''inline std::vector<uint8_t> encode_seg(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    static const bool best2 = std::getenv("MZIP_PE_BEST2") != nullptr;''', "encode rename")
rep('''inline std::vector<uint8_t> decode(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    if (sz < 3) return {};''','''inline std::vector<uint8_t> decode_seg(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    if (sz < 3) return {};''', "decode rename")
# find end of decode_seg: it ends with "return out;\n}\n" right after the tail memcpy; append wrappers after the function
anchor = '''    if ((size_t)(e - p) != tail) return {};
    memcpy(out.data() + L*k, p, tail);'''
assert s.count(anchor) == 1, "decode tail anchor"
i = s.index(anchor); j = s.index("\n}\n", i) + 3
wrappers = '''
// v9.1: segmenting wrappers. SEG = 256 KiB keeps every plane within HUF_BLOCKSIZE_MAX (128 KiB)
// for k=2 (k=4 planes are 64 KiB). Layout: [nseg varint] { [seg_n varint][segment payload] }*.
static const size_t PE_SEG = 262144;
inline std::vector<uint8_t> encode(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    size_t nseg = (n + PE_SEG - 1) / PE_SEG;
    std::vector<uint8_t> out; out.reserve(n + 16 + 8 * nseg);
    uint8_t vb[16]; size_t m = wv(vb, nseg); out.insert(out.end(), vb, vb + m);
    for (size_t off = 0; off < n; off += PE_SEG) {
        size_t sn = std::min(PE_SEG, n - off);
        auto seg = encode_seg(d + off, sn);
        if (seg.empty()) return {};
        m = wv(vb, sn); out.insert(out.end(), vb, vb + m);
        m = wv(vb, seg.size()); out.insert(out.end(), vb, vb + m);
        out.insert(out.end(), seg.begin(), seg.end());
    }
    return out;
}
inline std::vector<uint8_t> decode(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    uint64_t nseg = 0; if (!rv(p, e, nseg) || nseg == 0 || nseg > (n / 64) + 1) return {};
    std::vector<uint8_t> out; out.reserve(n);
    for (uint64_t g = 0; g < nseg; g++) {
        uint64_t sn = 0, cl = 0;
        if (!rv(p, e, sn) || !rv(p, e, cl)) return {};
        if (sn == 0 || sn > n - out.size() || cl > (uint64_t)(e - p)) return {};
        auto seg = decode_seg(p, (size_t)cl, (size_t)sn);
        if (seg.size() != sn) return {};
        out.insert(out.end(), seg.begin(), seg.end());
        p += cl;
    }
    if (out.size() != n || p != e) return {};
    return out;
}
'''
s = s[:j] + wrappers + s[j:]
shutil.copy(P, P + ".bak-v91"); open(P, "w").write(s); print("PATCHED v9.1", P)
