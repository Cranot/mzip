#!/usr/bin/env python3
"""v10.1 (on petree_v10): (1) decoder: reject a segment shorter than the encoder's minimum (64 B) --
an attacker-chosen seg_n < k made L == 0 and memcpy(nullptr, p, 0) (UBSan, decoder-robustness scout,
tree/mzip.hpp:208); (2) encoder: segment size 262080 and fold a trailing remainder < 64 B into the
last segment, so odd block sizes (n mod segment in 1..63) no longer fail the whole PE encode; a folded
k=2 plane is at most (262080+63)/2 = 131071 <= HUF_BLOCKSIZE_MAX."""
import sys
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
rep('''inline std::vector<uint8_t> decode_seg(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    if (sz < 3) return {};''','''inline std::vector<uint8_t> decode_seg(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    if (sz < 3 || n < 64) return {};   // v10.1: encoder never emits a segment < 64 B; L == 0 was a null memcpy''', "decoder guard")
rep('''static const size_t PE_SEG = 262144;''', '''static const size_t PE_SEG = 262080;   // v10.1: 262144 - 64 so a folded tail keeps k=2 planes <= 128 KiB''', "seg size")
rep('''    for (size_t off = 0; off < n; off += PE_SEG) {
        size_t sn = std::min(PE_SEG, n - off);
        auto seg = encode_seg(d + off, sn);''','''    for (size_t off = 0; off < n; ) {
        size_t sn = std::min(PE_SEG, n - off);
        if (n - off - sn > 0 && n - off - sn < 64) sn = n - off;   // v10.1: fold a short tail into this segment
        auto seg = encode_seg(d + off, sn);''', "tail fold")
rep('''        out.insert(out.end(), seg.begin(), seg.end());
    }
    return out;
}
inline std::vector<uint8_t> decode(const uint8_t* src, size_t sz, size_t n) {''','''        out.insert(out.end(), seg.begin(), seg.end());
        off += sn;
    }
    return out;
}
inline std::vector<uint8_t> decode(const uint8_t* src, size_t sz, size_t n) {''', "loop advance")
# nseg upper bound in decode: segments are >= 64 B, so nseg <= n/64 + 1 already holds; keep.
open(P, "w").write(s); print("PATCHED v10.1", P)
