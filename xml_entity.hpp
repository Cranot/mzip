// xml_entity.hpp — collapse common XML/HTML escape entities to single bytes.
//
// Wikipedia XML has thousands of `&amp;`, `&quot;`, `&lt;`, `&gt;`, `&nbsp;` per MB.
// Each is 4-6 raw bytes that compress to ~2-3 bytes via BWT (highly clustered).
// Collapsing to a single byte saves 3-5 raw bytes per occurrence; even after
// BWT the compressed savings are real because the substituted byte clusters even
// tighter than the multi-byte entity.
//
// Mapping:
//   &amp;   (5 bytes) → 0x01
//   &quot;  (6 bytes) → 0x02
//   &lt;    (4 bytes) → 0x03
//   &gt;    (4 bytes) → 0x04
//   &nbsp;  (6 bytes) → 0x05
// Literal bytes 0x01–0x05 escape as 0x07 0x01..0x05; literal 0x07 as 0x07 0x00.

#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

namespace bwt5 {

constexpr uint8_t XE_ESC = 0x07;

inline std::vector<uint8_t> xe_encode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ) {
        if (data[i] == '&') {
            // Try entity matches in length order (shortest wins ties — but they're disjoint)
            if (i + 5 <= n && std::memcmp(data + i, "&amp;",  5) == 0) { out.push_back(0x01); i += 5; continue; }
            if (i + 6 <= n && std::memcmp(data + i, "&quot;", 6) == 0) { out.push_back(0x02); i += 6; continue; }
            if (i + 4 <= n && std::memcmp(data + i, "&lt;",   4) == 0) { out.push_back(0x03); i += 4; continue; }
            if (i + 4 <= n && std::memcmp(data + i, "&gt;",   4) == 0) { out.push_back(0x04); i += 4; continue; }
            if (i + 6 <= n && std::memcmp(data + i, "&nbsp;", 6) == 0) { out.push_back(0x05); i += 6; continue; }
        }
        uint8_t b = data[i++];
        if (b == XE_ESC) {
            out.push_back(XE_ESC); out.push_back(0x00);
        } else if (b >= 0x01 && b <= 0x05) {
            out.push_back(XE_ESC); out.push_back(b);
        } else {
            out.push_back(b);
        }
    }
    return out;
}

inline std::vector<uint8_t> xe_decode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> out;
    out.reserve(n + (n >> 3));
    for (size_t i = 0; i < n; ) {
        uint8_t b = data[i++];
        if (b == XE_ESC) {
            if (i >= n) return {};
            uint8_t c = data[i++];
            if (c == 0x00) out.push_back(XE_ESC);
            else if (c >= 0x01 && c <= 0x05) out.push_back(c);
            else return {};
        } else if (b >= 0x01 && b <= 0x05) {
            switch (b) {
                case 0x01: { static const char s[] = "&amp;";  out.insert(out.end(), s, s + 5); break; }
                case 0x02: { static const char s[] = "&quot;"; out.insert(out.end(), s, s + 6); break; }
                case 0x03: { static const char s[] = "&lt;";   out.insert(out.end(), s, s + 4); break; }
                case 0x04: { static const char s[] = "&gt;";   out.insert(out.end(), s, s + 4); break; }
                case 0x05: { static const char s[] = "&nbsp;"; out.insert(out.end(), s, s + 6); break; }
            }
        } else {
            out.push_back(b);
        }
    }
    return out;
}

}  // namespace bwt5
