// word_dict.hpp — per-file word-dictionary preprocessor for prose.
//
// Why it works: BWT compresses bytes by clustering same-byte right-contexts.
// On prose, the byte-level entropy of "the" (3 bytes) is well above its
// word-level entropy (one of ~250 high-frequency tokens). Replacing the top
// N most-bytes-saving words with 2-byte tokens before BWT shrinks the input
// by 5-15% on English prose, and the residual compresses similarly to before.
//
// Format on the wire (inside the BWT container):
//   - num_words: varint
//   - For each word: byte(length) + length bytes of word
//   - Then: encoded byte stream (passed to BWT as input)
//
// Encoding (in the byte stream):
//   - 0xFE 0x00         → literal 0xFE byte
//   - 0xFE 0xCC (CC≠0)  → dictionary entry (CC-1)  — 1..255 codes
//   - any other byte    → literal
//
// 0xFE is reserved as ESC. It's never a valid UTF-8 leading byte so it
// virtually never appears in real text; the 0x00 literal-escape costs ~zero.

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace bwt5 {

constexpr uint8_t WD_ESC      = 0xFE;
constexpr uint8_t WD_ESC2     = 0xFD;    // reserved (unused; kept for format stability)
// Tested direct single-byte codes for top words (bytes 0xF8..0xFD that are
// invalid UTF-8 leaders): broke roundtrip in some edge case and gave near-zero
// compressed gain anyway because the direct-code byte's right context is
// variable, polluting BWT clustering. Reverted.
constexpr uint8_t WD_DIRECT_BASE  = 0xF8;
constexpr int     WD_DIRECT_CODES = 0;   // disabled
constexpr int     WD_MIN_LEN  = 3;
constexpr int     WD_MAX_LEN  = 64;
constexpr int     WD_MAX_CODES = 255;
constexpr int     WD_MIN_FREQ = 2;

inline bool wd_is_word_char(uint8_t c) {
    // [a-zA-Z]. Letters only. Tested broader definitions (digits, apostrophe, all
    // non-whitespace): all regressed because they create unique long tokens that
    // displace recurring words from the 255 dict slots.
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

struct WordDict {
    std::vector<std::string> words;  // index → word (codes are index+1, 0 reserved for literal-ESC)
    std::unordered_map<std::string, int> lookup;

    void rebuild_lookup() {
        lookup.clear();
        for (size_t i = 0; i < words.size(); i++) lookup[words[i]] = (int)i;
    }
};

inline WordDict wd_build(const uint8_t* data, size_t n) {
    // Pass 1: count occurrences of each word ≥ WD_MIN_LEN.
    // Tested adding bigrams (word+space+word) here: regressed by ~3KB on 10MB enwik9.
    // Bigrams displaced higher-impact single words from the 255 dict slots.
    std::unordered_map<std::string, uint32_t> counts;
    counts.reserve(64 * 1024);
    size_t i = 0;
    while (i < n) {
        if (wd_is_word_char(data[i])) {
            size_t start = i;
            while (i < n && wd_is_word_char(data[i])) i++;
            size_t len = i - start;
            if (len >= (size_t)WD_MIN_LEN && len <= (size_t)WD_MAX_LEN) {
                counts[std::string((const char*)(data + start), len)]++;
            }
        } else {
            i++;
        }
    }
    // Score by raw bytes saved per occurrence (length - 2 for the ESC+code replacement)
    // minus dict-storage cost (length + 1 byte per entry).
    struct Scored { int64_t saving; std::string word; };
    std::vector<Scored> scored;
    scored.reserve(counts.size());
    for (auto& kv : counts) {
        if (kv.second < (uint32_t)WD_MIN_FREQ) continue;
        int64_t per_occ = (int64_t)kv.first.size() - 2;
        if (per_occ <= 0) continue;
        int64_t total = per_occ * (int64_t)kv.second - (int64_t)kv.first.size() - 1;
        if (total > 0) scored.push_back({total, kv.first});
    }
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.saving > b.saving; });
    if (scored.size() > (size_t)WD_MAX_CODES) scored.resize(WD_MAX_CODES);

    WordDict d;
    d.words.reserve(scored.size());
    for (auto& s : scored) d.words.push_back(std::move(s.word));
    d.rebuild_lookup();
    return d;
}

// Truncate an existing dict to its top N entries. Used to dynamically pick
// dict size per block (smaller dict = less header overhead but fewer encoded words).
inline WordDict wd_truncate(const WordDict& full, int n) {
    WordDict d;
    int take = std::min(n, (int)full.words.size());
    d.words.assign(full.words.begin(), full.words.begin() + take);
    d.rebuild_lookup();
    return d;
}

// Encode dictionary entries as varint(num) + (len + bytes)*num.
inline void wd_serialize(const WordDict& d, std::vector<uint8_t>& out) {
    // varint num_words
    uint32_t n = (uint32_t)d.words.size();
    while (n >= 128) { out.push_back((uint8_t)(n | 0x80)); n >>= 7; }
    out.push_back((uint8_t)n);
    for (auto& w : d.words) {
        out.push_back((uint8_t)w.size());  // 1..255
        out.insert(out.end(), w.begin(), w.end());
    }
}

inline WordDict wd_deserialize(const uint8_t* data, size_t n, size_t& pos) {
    WordDict d;
    uint32_t num = 0;
    int shift = 0;
    while (pos < n) {
        uint8_t b = data[pos++];
        num |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    d.words.reserve(num);
    for (uint32_t i = 0; i < num && pos < n; i++) {
        uint8_t len = data[pos++];
        if (pos + len > n) { d.words.clear(); return d; }
        d.words.emplace_back((const char*)(data + pos), len);
        pos += len;
    }
    d.rebuild_lookup();
    return d;
}

// Emit a literal byte; ESC/ESC2 need escape with trailing 0x00.
inline void wd_emit_literal(uint8_t b, std::vector<uint8_t>& out) {
    if (b == WD_ESC)       { out.push_back(WD_ESC);  out.push_back(0x00); }
    else if (b == WD_ESC2) { out.push_back(WD_ESC2); out.push_back(0x00); }
    else                   { out.push_back(b); }
}

// Emit a 2-byte ESC+code reference. Codes 0..254 → bytes 1..255.
inline void wd_emit_code(int code, std::vector<uint8_t>& out) {
    out.push_back(WD_ESC);
    out.push_back((uint8_t)(code + 1));
}

inline std::vector<uint8_t> wd_encode(const uint8_t* data, size_t n, const WordDict& d) {
    std::vector<uint8_t> out;
    out.reserve(n);
    size_t i = 0;
    std::string buf;
    buf.reserve(WD_MAX_LEN);
    while (i < n) {
        if (wd_is_word_char(data[i])) {
            size_t start = i;
            while (i < n && wd_is_word_char(data[i])) i++;
            size_t len = i - start;
            if (len >= (size_t)WD_MIN_LEN && len <= (size_t)WD_MAX_LEN) {
                buf.assign((const char*)(data + start), len);
                auto it = d.lookup.find(buf);
                if (it != d.lookup.end()) {
                    wd_emit_code(it->second, out);
                    continue;
                }
            }
            for (size_t k = start; k < i; k++) wd_emit_literal(data[k], out);
        } else {
            wd_emit_literal(data[i++], out);
        }
    }
    return out;
}

inline std::vector<uint8_t> wd_decode(const uint8_t* data, size_t n, const WordDict& d) {
    std::vector<uint8_t> out;
    out.reserve(n * 2);
    size_t i = 0;
    while (i < n) {
        uint8_t b = data[i++];
        if (b == WD_ESC) {
            if (i >= n) return {};
            uint8_t code = data[i++];
            if (code == 0) {
                out.push_back(WD_ESC);
            } else {
                int idx = (int)code - 1;
                if (idx >= (int)d.words.size()) return {};
                const std::string& w = d.words[idx];
                out.insert(out.end(), w.begin(), w.end());
            }
        } else if (b == WD_ESC2) {
            if (i >= n) return {};
            uint8_t code = data[i++];
            if (code == 0) out.push_back(WD_ESC2);
            else return {};  // ESC2 codes not used
        } else {
            out.push_back(b);
        }
    }
    return out;
}

}  // namespace bwt5
