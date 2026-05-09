// bigram_dict.hpp — second-pass dictionary that compresses common
// (WD_ESC X) (space) (WD_ESC Y) patterns into a single 2-byte token (BG_ESC K).
//
// After single-word dict encoding, English text contains tons of
// "ESC code_of SPACE ESC code_the" sequences (5 bytes each). Replacing the
// top ~255 such bigrams with 2-byte codes saves 3 raw bytes per occurrence,
// and the new BG_ESC byte clusters tightly post-BWT (just like WD_ESC does).
//
// Layered after word_dict's WD_ESC=0xFE. Uses 0xFD as the bigram escape.
// Word_dict output already escapes literal 0xFD (as 0xFD 0x00) so the
// bigram pass sees that pattern unambiguously.

#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#include <unordered_map>
#include <algorithm>

namespace bwt5 {

constexpr uint8_t BG_ESC          = 0xFD;
constexpr int     BG_MAX_BIGRAMS  = 255;
constexpr int     BG_MIN_FREQ     = 30;   // bigram needs to be very common to pay back its dict cost

struct BigramDict {
    std::vector<std::pair<uint8_t, uint8_t>> bigrams;  // (X, Y) word-dict codes
    std::unordered_map<uint16_t, int> lookup;          // (X<<8 | Y) → bigram index

    void rebuild_lookup() {
        lookup.clear();
        for (size_t i = 0; i < bigrams.size(); i++) {
            uint16_t key = ((uint16_t)bigrams[i].first << 8) | bigrams[i].second;
            lookup[key] = (int)i;
        }
    }
};

// Build bigram dict from word-encoded byte stream by scanning for
// (WD_ESC X) ' ' (WD_ESC Y) patterns where X,Y > 0 (real codes, not escapes).
inline BigramDict bg_build(const uint8_t* data, size_t n) {
    std::unordered_map<uint16_t, uint32_t> counts;
    counts.reserve(16 * 1024);
    for (size_t i = 0; i + 4 < n; i++) {
        if (data[i] == WD_ESC && data[i+2] == ' ' && data[i+3] == WD_ESC
            && data[i+1] != 0 && data[i+4] != 0) {
            uint16_t key = ((uint16_t)data[i+1] << 8) | data[i+4];
            counts[key]++;
        }
    }
    struct Scored { int64_t saving; uint16_t key; };
    std::vector<Scored> scored;
    scored.reserve(counts.size());
    for (auto& kv : counts) {
        if (kv.second < (uint32_t)BG_MIN_FREQ) continue;
        int64_t saving = (int64_t)3 * kv.second - 2;
        if (saving > 0) {
            Scored s;
            s.saving = saving;
            s.key = kv.first;
            scored.push_back(s);
        }
    }
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.saving > b.saving; });
    if (scored.size() > (size_t)BG_MAX_BIGRAMS) scored.resize(BG_MAX_BIGRAMS);

    BigramDict d;
    d.bigrams.reserve(scored.size());
    for (auto& s : scored) d.bigrams.push_back({(uint8_t)(s.key >> 8), (uint8_t)(s.key & 0xFF)});
    d.rebuild_lookup();
    return d;
}

inline void bg_serialize(const BigramDict& d, std::vector<uint8_t>& out) {
    uint32_t num = (uint32_t)d.bigrams.size();
    while (num >= 128) { out.push_back((uint8_t)(num | 0x80)); num >>= 7; }
    out.push_back((uint8_t)num);
    for (auto& p : d.bigrams) { out.push_back(p.first); out.push_back(p.second); }
}

inline BigramDict bg_deserialize(const uint8_t* data, size_t n, size_t& pos) {
    BigramDict d;
    uint32_t num = 0;
    int shift = 0;
    while (pos < n) {
        uint8_t b = data[pos++];
        num |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    d.bigrams.reserve(num);
    for (uint32_t i = 0; i < num && pos + 1 < n; i++) {
        d.bigrams.push_back({data[pos], data[pos + 1]});
        pos += 2;
    }
    d.rebuild_lookup();
    return d;
}

inline std::vector<uint8_t> bg_encode(const uint8_t* data, size_t n, const BigramDict& d) {
    std::vector<uint8_t> out;
    out.reserve(n);
    size_t i = 0;
    while (i < n) {
        if (i + 4 < n && data[i] == WD_ESC && data[i+2] == ' ' && data[i+3] == WD_ESC
            && data[i+1] != 0 && data[i+4] != 0) {
            uint16_t key = ((uint16_t)data[i+1] << 8) | data[i+4];
            auto it = d.lookup.find(key);
            if (it != d.lookup.end()) {
                out.push_back(BG_ESC);
                out.push_back((uint8_t)(it->second + 1));
                i += 5;
                continue;
            }
        }
        // Pass through; escape literal BG_ESC bytes
        if (data[i] == BG_ESC) {
            out.push_back(BG_ESC);
            out.push_back(0x00);
        } else {
            out.push_back(data[i]);
        }
        i++;
    }
    return out;
}

inline std::vector<uint8_t> bg_decode(const uint8_t* data, size_t n, const BigramDict& d) {
    std::vector<uint8_t> out;
    out.reserve(n + (n >> 1));
    for (size_t i = 0; i < n; ) {
        if (data[i] == BG_ESC) {
            if (i + 1 >= n) return {};
            uint8_t c = data[i + 1];
            if (c == 0) {
                out.push_back(BG_ESC);
            } else {
                int idx = c - 1;
                if (idx >= (int)d.bigrams.size()) return {};
                auto p = d.bigrams[idx];
                out.push_back(WD_ESC); out.push_back(p.first);
                out.push_back(' ');
                out.push_back(WD_ESC); out.push_back(p.second);
            }
            i += 2;
        } else {
            out.push_back(data[i++]);
        }
    }
    return out;
}

}  // namespace bwt5
