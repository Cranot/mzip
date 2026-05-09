// cap_fold.hpp — capital letter folding for prose preprocessing.
//
// Lowercases all A-Z letters. Each capital letter L becomes [CAP_MARKER, lowercase_L].
// Net effect on input size: ~+3-10% on prose (depending on capital density).
// Net effect on alphabet: halved letter alphabet (~26 vs ~52). BWT clusters tighter,
// word dict sees more shared patterns ("the" and "The" become same word + 1-bit hint).
//
// Cap marker: 0x06 (ASCII ACK). Never appears in normal printable text.
// Literal 0x06: encoded as 0x06 0x00.
//
// Decoder: scan output. 0x06 followed by lowercase = original uppercase.
//          0x06 followed by 0x00 = literal 0x06.
//          other bytes pass through.

#pragma once
#include <cstdint>
#include <vector>

namespace bwt5 {

constexpr uint8_t CF_MARKER = 0x06;

inline std::vector<uint8_t> cf_encode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> out;
    out.reserve(n + (n >> 4));  // assume ~6% expansion for prose
    for (size_t i = 0; i < n; i++) {
        uint8_t b = data[i];
        if (b >= 'A' && b <= 'Z') {
            out.push_back(CF_MARKER);
            out.push_back(b + ('a' - 'A'));
        } else if (b == CF_MARKER) {
            out.push_back(CF_MARKER);
            out.push_back(0x00);
        } else {
            out.push_back(b);
        }
    }
    return out;
}

inline std::vector<uint8_t> cf_decode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ) {
        uint8_t b = data[i++];
        if (b == CF_MARKER) {
            if (i >= n) return {};
            uint8_t c = data[i++];
            if (c == 0x00) {
                out.push_back(CF_MARKER);
            } else if (c >= 'a' && c <= 'z') {
                out.push_back(c - ('a' - 'A'));
            } else {
                return {};  // invalid encoding
            }
        } else {
            out.push_back(b);
        }
    }
    return out;
}

}  // namespace bwt5
