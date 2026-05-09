// BWT Compression v5 - Multi-tree Huffman with Symbol Bitmap
//
// Key optimization over v4: Symbol bitmap (bzip2-style)
// - Only store tree lengths for actually used symbols
// - Saves ~2.5 bits per gap symbol per tree
// - Gap symbols = unused values between 0 and max_sym
//
// Format:
// - 16-bit range header: which of 16 ranges (0-15, 16-31, ..., 240-255) are used
// - Per used range: 16-bit sub-bitmap for specific symbols in that range
// - Then: tree lengths ONLY for symbols marked in bitmap
//
// v5.1: Integrated libsais for O(n) BWT construction (was O(n²) naive sort)

#pragma once
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <queue>
#include <functional>

// libsais - O(n) suffix array / BWT construction
// Source: https://github.com/IlyaGrebnov/libsais
extern "C" {
#include "libsais.h"
}

#include "range_coder.hpp"
#include "word_dict.hpp"
#include "bigram_dict.hpp"
#include "cap_fold.hpp"
#include "xml_entity.hpp"

namespace bwt5 {

// =============================================================================
// LZP (Lempel-Ziv-Predictive) preprocessing.
// One-position LZ77 with implicit offset (hash uniquely identifies position) and
// a high min-match (40 bytes) so only long literal runs are removed. Used by
// bzip3 to strip long repeats before BWT, which (a) shrinks the suffix-array
// sort and (b) leaves a cleaner residual for entropy coding.
//
// Format: byte stream with escape byte 0xFC.
//   - Literal byte b (b != 0xFC): emitted as-is
//   - Literal byte 0xFC: emitted as 0xFC 0xFF
//   - Match of length L ≥ 40: emitted as 0xFC L_minus_40 (single byte length)
// Max match length = 40 + 254 = 294 (since 0xFF is reserved for ESC literal).
// Hash: order-4 context, 18-bit (256K slots).
// =============================================================================
constexpr int    LZP_MIN_MATCH = 40;
constexpr int    LZP_CTX       = 4;
constexpr int    LZP_HASH_BITS = 18;
constexpr int    LZP_HASH_SIZE = 1 << LZP_HASH_BITS;
constexpr uint8_t LZP_ESC      = 0xFC;

inline uint32_t lzp_hash(const uint8_t* p) {
    // Mix 4 bytes; constants are standard golden-ratio / Bernstein-style.
    uint32_t h = (uint32_t)p[0] * 2654435761u
               ^ (uint32_t)p[1] * 40503u
               ^ (uint32_t)p[2] * 5381u
               ^ (uint32_t)p[3] * 31u;
    return h & (LZP_HASH_SIZE - 1);
}

inline std::vector<uint8_t> lzp_encode_min(const uint8_t* data, size_t n, int min_match) {
    if (n < (size_t)(min_match + LZP_CTX)) {
        return std::vector<uint8_t>(data, data + n);
    }
    std::vector<int32_t> hash(LZP_HASH_SIZE, -1);
    std::vector<uint8_t> out;
    out.reserve(n);
    for (int i = 0; i < LZP_CTX; i++) {
        if (data[i] == LZP_ESC) { out.push_back(LZP_ESC); out.push_back(0xFF); }
        else                     { out.push_back(data[i]); }
    }
    size_t i = LZP_CTX;
    while (i < n) {
        uint32_t h = lzp_hash(data + i - LZP_CTX);
        int32_t prev = hash[h];
        hash[h] = (int32_t)i;
        if (prev >= 0 && (size_t)prev + min_match <= i) {
            size_t maxlen = std::min(n - i, (size_t)(min_match + 254));
            size_t L = 0;
            while (L < maxlen && data[prev + L] == data[i + L]) L++;
            if (L >= (size_t)min_match) {
                out.push_back(LZP_ESC);
                out.push_back((uint8_t)(L - min_match));
                i += L;
                continue;
            }
        }
        if (data[i] == LZP_ESC) { out.push_back(LZP_ESC); out.push_back(0xFF); }
        else                    { out.push_back(data[i]); }
        i++;
    }
    return out;
}
inline std::vector<uint8_t> lzp_encode(const uint8_t* data, size_t n) {
    return lzp_encode_min(data, n, LZP_MIN_MATCH);
}

inline std::vector<uint8_t> lzp_decode_min(const uint8_t* data, size_t n, int min_match) {
    std::vector<int32_t> hash(LZP_HASH_SIZE, -1);
    std::vector<uint8_t> out;
    out.reserve(n * 2);
    size_t pos = 0;
    int copied = 0;
    while (copied < LZP_CTX && pos < n) {
        uint8_t b = data[pos++];
        if (b == LZP_ESC) {
            if (pos >= n) return out;
            uint8_t code = data[pos++];
            if (code != 0xFF) return {};
            out.push_back(LZP_ESC);
        } else {
            out.push_back(b);
        }
        copied++;
    }
    while (pos < n) {
        if (out.size() < (size_t)LZP_CTX) { out.push_back(data[pos++]); continue; }
        uint32_t h = lzp_hash(&out[out.size() - LZP_CTX]);
        int32_t prev = hash[h];
        hash[h] = (int32_t)out.size();
        uint8_t b = data[pos++];
        if (b == LZP_ESC) {
            if (pos >= n) return {};
            uint8_t code = data[pos++];
            if (code == 0xFF) {
                out.push_back(LZP_ESC);
            } else {
                size_t L = (size_t)code + min_match;
                if (prev < 0 || (size_t)prev + L > out.size()) return {};
                for (size_t i = 0; i < L; i++) out.push_back(out[prev + i]);
            }
        } else {
            out.push_back(b);
        }
    }
    return out;
}
inline std::vector<uint8_t> lzp_decode(const uint8_t* data, size_t n) {
    return lzp_decode_min(data, n, LZP_MIN_MATCH);
}

// =============================================================================
// Range-coder-based symbol encoder for post-ZRLE stream.
// Uses 9-bit decomposition with adaptive bit-context. Replacement for the
// MultiTreeEncoder Huffman backend; expected ~3-5% smaller on prose because
// arithmetic-class entropy coding doesn't pay Huffman's integer-bit penalty.
//
// This is the P1 phase. P3 will extend to per-context models (one bit tree
// per previous byte) for a further ~5-8% on top.
// =============================================================================
inline std::vector<uint8_t> rc_encode_symbols(const std::vector<uint16_t>& symbols,
                                                int /*alpha_size*/) {
    // 9-bit bit tree (probs[1..511]). All symbols 0..511 fit. probs[0] unused.
    // Adaptive: every encode_bit() updates the relevant Prob.
    rc::Prob probs[512];
    for (auto& p : probs) p = rc::Prob{};

    std::vector<uint8_t> out;
    out.reserve(symbols.size());
    rc::Encoder enc(out);
    for (uint16_t s : symbols) {
        // Bit tree: traverse 9 bits MSB-first; at each node use probs[m].
        uint32_t m = 1;
        for (int i = 8; i >= 0; i--) {
            uint32_t bit = (s >> i) & 1;
            enc.encode_bit(probs[m], bit);
            m = (m << 1) | bit;
        }
    }
    enc.finish();
    return out;
}

inline std::vector<uint16_t> rc_decode_symbols(const uint8_t* data, size_t size,
                                                int /*alpha_size*/, size_t expected_count) {
    rc::Prob probs[512];
    for (auto& p : probs) p = rc::Prob{};

    std::vector<uint16_t> out;
    out.reserve(expected_count);
    rc::Decoder dec(data, size);
    for (size_t i = 0; i < expected_count; i++) {
        uint32_t m = 1;
        for (int b = 0; b < 9; b++) {
            m = (m << 1) | dec.decode_bit(probs[m]);
        }
        out.push_back((uint16_t)(m - 512));  // strip leading 1-bit sentinel
    }
    return out;
}

// =============================================================================
// Order-1 range coder: separate bit tree per previous-symbol context.
// 256 contexts × 512 probs each = 256 KB working set. Captures the strong
// local correlation in BWT output (next byte often predictable from previous).
//
// Context = previous symbol mod 256 (so the rare high symbols share a context).
// =============================================================================
inline std::vector<uint8_t> rc_encode_symbols_o1(const std::vector<uint16_t>& symbols) {
    std::vector<rc::Prob> probs(256 * 512);
    // All probs init to 50% via Prob default ctor — already done.

    std::vector<uint8_t> out;
    out.reserve(symbols.size());
    rc::Encoder enc(out);
    int ctx = 0;
    for (uint16_t s : symbols) {
        rc::Prob* p = probs.data() + ctx * 512;
        uint32_t m = 1;
        for (int i = 8; i >= 0; i--) {
            uint32_t bit = (s >> i) & 1;
            enc.encode_bit(p[m], bit);
            m = (m << 1) | bit;
        }
        ctx = s & 0xFF;
    }
    enc.finish();
    return out;
}

inline std::vector<uint16_t> rc_decode_symbols_o1(const uint8_t* data, size_t size,
                                                   size_t expected_count) {
    std::vector<rc::Prob> probs(256 * 512);
    std::vector<uint16_t> out;
    out.reserve(expected_count);
    rc::Decoder dec(data, size);
    int ctx = 0;
    for (size_t i = 0; i < expected_count; i++) {
        rc::Prob* p = probs.data() + ctx * 512;
        uint32_t m = 1;
        for (int b = 0; b < 9; b++) {
            m = (m << 1) | dec.decode_bit(p[m]);
        }
        uint16_t s = (uint16_t)(m - 512);
        out.push_back(s);
        ctx = s & 0xFF;
    }
    return out;
}

// Helper: Check if size causes libsais_unbwt crash on MinGW
// Pattern: crashes when (n mod 4 == 2) AND (n >= 30)
// Some edge cases at idx transitions (54, 108, etc.) are safe but we pad anyway
inline bool is_libsais_problematic_size(size_t n) {
    return (n % 4 == 2) && (n >= 30);
}

// Helper: Pad size to next safe value (add bytes until n mod 4 != 2)
inline size_t get_safe_padded_size(size_t n) {
    while (is_libsais_problematic_size(n)) {
        n++;
    }
    return n;
}

constexpr int MAX_ALPHA = 258;  // 256 + RUNA + RUNB
// Defaults tuned for prose (enwik9 prefixes). Two sweep generations:
//   - 2026-05 P1 (GS=50, NT=4, NI=4) — first pass, picked NT=4 because it was the
//     best at GS=44.
//   - 2026-05 P0b (GS=45, NT=5, NI=4) — second pass after caching compact_of[]:
//     the GS×NT interaction surfaced. NT=5 at GS=45 saves another ~9 KB on 10MB
//     Wikipedia. NI saturates at 4; bumping to 8 or 16 changes nothing.
#ifndef BWT5_GROUP_SIZE
#define BWT5_GROUP_SIZE 45
#endif
#ifndef BWT5_MAX_TREES
#define BWT5_MAX_TREES 5
#endif
#ifndef BWT5_N_ITERS
#define BWT5_N_ITERS 4
#endif
constexpr int GROUP_SIZE = BWT5_GROUP_SIZE;  // Optimized: smaller groups = better local adaptation
constexpr int MAX_TREES = BWT5_MAX_TREES;
constexpr int N_ITERS = BWT5_N_ITERS;     // bzip2-style: 4 iterations sufficient

// =============================================================================
// Symbol Bitmap - bzip2-style compact symbol set representation
// Uses 3-byte header to support symbols 0-271 (ZRLE can produce 256+)
// =============================================================================
struct SymbolBitmap {
    uint32_t range_header = 0;  // Which of 17 ranges are used
    uint16_t sub_bitmaps[17] = {0};  // Per-range: which symbols are used
    std::vector<uint16_t> used_symbols;  // Ordered list of used symbols

    void build(const std::vector<uint16_t>& data) {
        range_header = 0;
        memset(sub_bitmaps, 0, sizeof(sub_bitmaps));
        used_symbols.clear();

        // Mark used symbols
        bool used[MAX_ALPHA] = {false};
        for (auto s : data) {
            if (s < MAX_ALPHA) used[s] = true;
        }

        // Build bitmap (17 ranges for symbols 0-271)
        for (int s = 0; s < MAX_ALPHA; s++) {
            if (used[s]) {
                int range = s / 16;
                int bit = s % 16;
                range_header |= (1u << range);
                sub_bitmaps[range] |= (1 << bit);
                used_symbols.push_back(s);
            }
        }
    }

    void write(std::vector<uint8_t>& out) const {
        // Variable-length header:
        // - Byte 0: ranges 0-7
        // - Byte 1: ranges 8-14 (bits 0-6), extension flag (bit 7)
        // - Byte 2 (if extension): ranges 15-16 (bits 0-1)
        // Extension is needed if ranges 15 or 16 are used
        bool need_extension = (range_header & 0x18000) != 0;  // Range 15 or 16 used?

        out.push_back(range_header & 0xFF);  // Ranges 0-7
        uint8_t byte1 = (range_header >> 8) & 0x7F;  // Ranges 8-14 (only 7 bits!)
        if (need_extension) {
            byte1 |= 0x80;  // Set extension flag
        }
        out.push_back(byte1);

        if (need_extension) {
            // Store ranges 15 and 16 in extension byte
            uint8_t ext = ((range_header >> 15) & 0x03);  // Bits 15-16
            out.push_back(ext);
        }

        for (int r = 0; r < 17; r++) {
            if (range_header & (1u << r)) {
                out.push_back(sub_bitmaps[r] & 0xFF);
                out.push_back((sub_bitmaps[r] >> 8) & 0xFF);
            }
        }
    }

    size_t read(const uint8_t* data, size_t max_size) {
        if (max_size < 2) return 0;
        used_symbols.clear();
        memset(sub_bitmaps, 0, sizeof(sub_bitmaps));

        // Variable-length header:
        // - Byte 0: ranges 0-7
        // - Byte 1: ranges 8-14 (bits 0-6), extension flag (bit 7)
        // - Byte 2 (if extension): ranges 15-16 (bits 0-1)
        uint8_t byte0 = data[0];
        uint8_t byte1 = data[1];
        bool need_extension = (byte1 & 0x80) != 0;
        byte1 &= 0x7F;  // Clear extension flag, keep ranges 8-14

        size_t pos = 2;
        if (need_extension) {
            if (max_size < 3) return 0;
            // Ranges 0-7 | ranges 8-14 | ranges 15-16
            range_header = byte0 | ((uint32_t)byte1 << 8) | ((uint32_t)(data[2] & 0x03) << 15);
            pos = 3;
        } else {
            // Ranges 0-7 | ranges 8-14 (no ranges 15-16)
            range_header = byte0 | ((uint32_t)byte1 << 8);
        }

        for (int r = 0; r < 17; r++) {
            if (range_header & (1u << r)) {
                if (pos + 2 > max_size) return 0;
                sub_bitmaps[r] = data[pos] | (data[pos + 1] << 8);
                pos += 2;
                for (int b = 0; b < 16; b++) {
                    if (sub_bitmaps[r] & (1 << b)) {
                        used_symbols.push_back(r * 16 + b);
                    }
                }
            }
        }
        return pos;
    }

    int to_compact(uint16_t sym) const {
        for (size_t i = 0; i < used_symbols.size(); i++) {
            if (used_symbols[i] == sym) return i;
        }
        return -1;
    }

    uint16_t from_compact(int idx) const {
        return (idx >= 0 && idx < (int)used_symbols.size()) ? used_symbols[idx] : 0;
    }

    // Build O(1) reverse-lookup table after build()/read(). Caller responsibility.
    // Symbol → compact index, or -1 if symbol not used. Sized for full alphabet.
    void build_compact_index(int16_t* idx_out) const {
        for (int s = 0; s < MAX_ALPHA; s++) idx_out[s] = -1;
        for (size_t i = 0; i < used_symbols.size(); i++) {
            idx_out[used_symbols[i]] = (int16_t)i;
        }
    }
};

// =============================================================================
// BWT Transform - using libsais for O(n) construction
// =============================================================================
inline std::vector<uint8_t> bwt_encode(const uint8_t* data, size_t n, uint32_t& primary_idx) {
    if (n == 0) return {};
    if (n > INT32_MAX) return {};  // libsais uses int32_t for size

    std::vector<uint8_t> bwt(n);
    // Note: Using malloc for A to avoid heap corruption issues with std::vector on MinGW
    int32_t* A = (int32_t*)malloc(n * sizeof(int32_t));
    if (!A) return {};

    // libsais_bwt returns the primary index (position of original string end)
    int32_t idx = libsais_bwt(data, bwt.data(), A, (int32_t)n, 0, nullptr);
    free(A);

    if (idx < 0) {
        // Error - fall back to empty (caller should handle)
        return {};
    }

    primary_idx = (uint32_t)idx;
    return bwt;
}

inline std::vector<uint8_t> bwt_decode(const uint8_t* bwt, size_t n, uint32_t primary_idx, bool debug = false) {
    if (n == 0) return {};
    if (primary_idx >= n) {
        if (debug) printf("ERROR: primary_idx %u >= n %zu\n", primary_idx, n);
        return {};
    }

    if (debug) { printf("bwt_decode: n=%zu, primary_idx=%u\n", n, primary_idx); fflush(stdout); }

    // Use libsais_unbwt for O(n) inverse BWT
    // Note: Using malloc for A to avoid heap corruption issues with std::vector on MinGW
    // Note: Caller must ensure n is a safe size (use is_libsais_problematic_size to check)
    std::vector<uint8_t> output(n);
    int32_t* A = (int32_t*)malloc(n * sizeof(int32_t));
    if (!A) {
        if (debug) printf("ERROR: malloc failed for A array\n");
        return {};
    }

    int32_t result = libsais_unbwt(bwt, output.data(), A, (int32_t)n, nullptr, (int32_t)primary_idx);
    free(A);

    if (result != 0) {
        if (debug) printf("ERROR: libsais_unbwt returned %d\n", result);
        return {};
    }

    return output;
}

// =============================================================================
// MTF Transform (same as v4)
// =============================================================================
inline std::vector<uint8_t> mtf_encode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> alphabet(256);
    std::iota(alphabet.begin(), alphabet.end(), 0);
    std::vector<uint8_t> output(n);
    for (size_t i = 0; i < n; i++) {
        uint8_t c = data[i];
        uint8_t pos = 0;
        while (alphabet[pos] != c) pos++;
        output[i] = pos;
        for (int j = pos; j > 0; j--) alphabet[j] = alphabet[j-1];
        alphabet[0] = c;
    }
    return output;
}

inline std::vector<uint8_t> mtf_decode(const uint8_t* data, size_t n) {
    std::vector<uint8_t> alphabet(256);
    std::iota(alphabet.begin(), alphabet.end(), 0);
    std::vector<uint8_t> output(n);
    for (size_t i = 0; i < n; i++) {
        uint8_t pos = data[i];
        uint8_t c = alphabet[pos];
        output[i] = c;
        for (int j = pos; j > 0; j--) alphabet[j] = alphabet[j-1];
        alphabet[0] = c;
    }
    return output;
}

// =============================================================================
// Zero-RLE (same as v4)
// =============================================================================
inline std::vector<uint16_t> zrle_encode(const uint8_t* data, size_t n) {
    std::vector<uint16_t> output;
    size_t i = 0;
    while (i < n) {
        if (data[i] == 0) {
            size_t run = 0;
            while (i < n && data[i] == 0) { run++; i++; }
            while (run > 0) {
                if (run & 1) {
                    output.push_back(0);
                    run = (run - 1) / 2;
                } else {
                    output.push_back(1);
                    run = (run - 2) / 2;
                }
            }
        } else {
            output.push_back(data[i] + 1);
            i++;
        }
    }
    return output;
}

inline std::vector<uint8_t> zrle_decode(const uint16_t* data, size_t n) {
    std::vector<uint8_t> output;
    size_t i = 0;
    while (i < n) {
        if (data[i] <= 1) {
            size_t run = 0;
            size_t power = 1;
            while (i < n && data[i] <= 1) {
                run += (data[i] + 1) * power;
                power *= 2;
                i++;
            }
            for (size_t j = 0; j < run; j++) output.push_back(0);
        } else {
            output.push_back(data[i] - 1);
            i++;
        }
    }
    return output;
}

// =============================================================================
// Pre-BWT Run-Length Encoding (bzip2-style)
// =============================================================================
// Encodes runs of 4+ identical bytes as: byte byte byte byte (count-4)
// where count-4 is 0-255, meaning runs of 4-259 bytes
inline std::vector<uint8_t> pre_rle_encode(const uint8_t* data, size_t n) {
    if (n == 0) return {};

    std::vector<uint8_t> output;
    output.reserve(n);

    size_t i = 0;
    while (i < n) {
        uint8_t byte = data[i];
        size_t run = 1;
        while (i + run < n && data[i + run] == byte && run < 259) {
            run++;
        }

        if (run >= 4) {
            // Encode as: byte byte byte byte (run-4)
            output.push_back(byte);
            output.push_back(byte);
            output.push_back(byte);
            output.push_back(byte);
            output.push_back(run - 4);
            i += run;
        } else {
            // Output bytes literally
            for (size_t j = 0; j < run; j++) {
                output.push_back(byte);
            }
            i += run;
        }
    }

    return output;
}

inline std::vector<uint8_t> pre_rle_decode(const uint8_t* data, size_t n) {
    if (n == 0) return {};

    std::vector<uint8_t> output;
    output.reserve(n * 2);

    size_t i = 0;
    while (i < n) {
        uint8_t byte = data[i];

        // Check for run encoding: 4 identical bytes followed by count
        if (i + 4 < n &&
            data[i+1] == byte &&
            data[i+2] == byte &&
            data[i+3] == byte) {
            size_t run = 4 + data[i+4];
            for (size_t j = 0; j < run; j++) {
                output.push_back(byte);
            }
            i += 5;
        } else {
            output.push_back(byte);
            i++;
        }
    }

    return output;
}

// =============================================================================
// Huffman Tree (same as v4)
// =============================================================================
struct HuffTree {
    int code_lengths[MAX_ALPHA] = {0};
    uint32_t codes[MAX_ALPHA] = {0};
    int max_length = 0;

    void build_from_freqs(const uint32_t* freqs, int alpha_size) {
        // bzip2-style: scale frequencies and rebuild if maxLen exceeded
        std::vector<uint32_t> scaled_freqs(freqs, freqs + alpha_size);

        for (int attempt = 0; attempt < 10; attempt++) {
            struct Node { int symbol; uint32_t freq; int left, right; };
            std::vector<Node> nodes;
            std::priority_queue<std::pair<uint32_t, int>, std::vector<std::pair<uint32_t, int>>, std::greater<>> pq;

            for (int i = 0; i < alpha_size; i++) {
                if (scaled_freqs[i] > 0) {
                    int idx = nodes.size();
                    nodes.push_back({i, scaled_freqs[i], -1, -1});
                    pq.push({scaled_freqs[i], idx});
                }
            }

            if (pq.empty()) return;
            if (pq.size() == 1) {
                code_lengths[nodes[pq.top().second].symbol] = 1;
                codes[nodes[pq.top().second].symbol] = 0;
                max_length = 1;
                break;
            }

            while (pq.size() > 1) {
                auto [f1, i1] = pq.top(); pq.pop();
                auto [f2, i2] = pq.top(); pq.pop();
                int idx = nodes.size();
                nodes.push_back({-1, f1 + f2, i1, i2});
                pq.push({f1 + f2, idx});
            }

            max_length = 0;
            memset(code_lengths, 0, sizeof(code_lengths));

            std::function<void(int, int)> assign_lengths = [&](int idx, int depth) {
                if (nodes[idx].symbol >= 0) {
                    code_lengths[nodes[idx].symbol] = depth;
                    max_length = std::max(max_length, depth);
                } else {
                    assign_lengths(nodes[idx].left, depth + 1);
                    assign_lengths(nodes[idx].right, depth + 1);
                }
            };
            assign_lengths(pq.top().second, 0);

            // Cap at 20 bits (decoder limit). Wider than bzip2's 17 lets skewed
            // distributions (long-tail symbol freqs) avoid frequency-flattening loss.
            if (max_length <= 20) break;

            // Scale all frequencies: freq = 1 + freq/2 (ensures min freq = 1)
            for (int i = 0; i < alpha_size; i++) {
                if (scaled_freqs[i] > 0) {
                    scaled_freqs[i] = 1 + scaled_freqs[i] / 2;
                }
            }
        }

        // Final clamp just in case (decoder hard limit is 20).
        for (int i = 0; i < alpha_size; i++) {
            if (code_lengths[i] > 20) code_lengths[i] = 20;
        }
        max_length = std::min(max_length, 20);

        std::vector<std::pair<int, int>> syms;
        for (int i = 0; i < alpha_size; i++) {
            if (code_lengths[i] > 0) syms.push_back({code_lengths[i], i});
        }
        std::sort(syms.begin(), syms.end());

        uint32_t code = 0;
        int prev_len = 0;
        for (auto [len, sym] : syms) {
            code <<= (len - prev_len);
            codes[sym] = code++;
            prev_len = len;
        }
    }

    void build_from_lengths(const int* lengths, int alpha_size) {
        if (alpha_size <= 0 || alpha_size > 258) return;  // Bounds check (max 256 + 2 special)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
        memcpy(code_lengths, lengths, static_cast<size_t>(alpha_size) * sizeof(int));
#pragma GCC diagnostic pop
        max_length = 0;
        for (int i = 0; i < alpha_size; i++) {
            max_length = std::max(max_length, code_lengths[i]);
        }

        std::vector<std::pair<int, int>> syms;
        for (int i = 0; i < alpha_size; i++) {
            if (code_lengths[i] > 0) syms.push_back({code_lengths[i], i});
        }
        std::sort(syms.begin(), syms.end());

        uint32_t code = 0;
        int prev_len = 0;
        for (auto [len, sym] : syms) {
            code <<= (len - prev_len);
            codes[sym] = code++;
            prev_len = len;
        }
    }
};

// =============================================================================
// Multi-Tree Encoder with Bitmap (key change from v4)
// =============================================================================
class MultiTreeEncoder {
    std::vector<uint8_t>& output;
    uint32_t buffer = 0;
    int bits_in_buffer = 0;

    void write_bits(uint32_t value, int n_bits) {
        buffer |= (value << bits_in_buffer);
        bits_in_buffer += n_bits;
        while (bits_in_buffer >= 8) {
            output.push_back(buffer & 0xFF);
            buffer >>= 8;
            bits_in_buffer -= 8;
        }
    }

    // Unary encoding: 0=0, 1=10, 2=110, 3=1110, etc.
    void write_unary(int value) {
        for (int i = 0; i < value; i++) {
            write_bits(1, 1);
        }
        write_bits(0, 1);
    }

public:
    MultiTreeEncoder(std::vector<uint8_t>& out) : output(out) {}

    // Inner encode with specific tree count - returns encoded size
    size_t encode_with_trees(const std::vector<uint16_t>& data, int alpha_size,
                             int n_trees, const SymbolBitmap& bitmap, int compact_size,
                             bool write_output) {
        size_t n = data.size();
        int n_groups = (n + GROUP_SIZE - 1) / GROUP_SIZE;
        std::vector<std::vector<uint32_t>> tree_freqs(n_trees, std::vector<uint32_t>(compact_size, 0));
        std::vector<int> group_tree(n_groups, 0);

        // Cache symbol → compact-index lookup. Replaces bitmap.to_compact() linear
        // scan in every hot loop below — was O(n × compact_size × N_ITERS) per encode.
        int16_t compact_idx[MAX_ALPHA];
        bitmap.build_compact_index(compact_idx);

        std::vector<uint32_t> sym_freq(compact_size, 0);
        for (size_t i = 0; i < n; i++) {
            int idx = compact_idx[data[i]];
            if (idx >= 0) sym_freq[idx]++;
        }

        std::vector<int> sym_tree_owner(compact_size, n_trees - 1);
        {
            int nPart = n_trees;
            uint64_t remF = n;
            int gs = 0;
            while (nPart > 0 && gs < compact_size) {
                uint64_t tFreq = remF / nPart;
                int ge = gs - 1;
                uint64_t aFreq = 0;
                while (aFreq < tFreq && ge < compact_size - 1) { ge++; aFreq += sym_freq[ge]; }
                for (int v = gs; v <= ge; v++) sym_tree_owner[v] = nPart - 1;
                nPart--; gs = ge + 1; remF -= aFreq;
            }
        }

        std::vector<std::vector<int>> initial_costs(n_trees, std::vector<int>(compact_size, 15));
        for (int s = 0; s < compact_size; s++) initial_costs[sym_tree_owner[s]][s] = 0;

        for (int g = 0; g < n_groups; g++) {
            size_t start = g * GROUP_SIZE;
            size_t end = std::min(start + GROUP_SIZE, n);
            int best_tree = 0, best_cost = INT_MAX;
            for (int t = 0; t < n_trees; t++) {
                int cost = 0;
                for (size_t i = start; i < end; i++) {
                    int idx = compact_idx[data[i]];
                    if (idx >= 0) cost += initial_costs[t][idx];
                }
                if (cost < best_cost) { best_cost = cost; best_tree = t; }
            }
            group_tree[g] = best_tree;
        }

        for (int iter = 0; iter < N_ITERS; iter++) {
            for (auto& tf : tree_freqs) std::fill(tf.begin(), tf.end(), 0);
            for (int g = 0; g < n_groups; g++) {
                int t = group_tree[g];
                size_t start = g * GROUP_SIZE;
                size_t end = std::min(start + GROUP_SIZE, n);
                for (size_t i = start; i < end; i++) {
                    int ci = compact_idx[data[i]];
                    if (ci >= 0) tree_freqs[t][ci]++;
                }
            }
            for (auto& tf : tree_freqs) for (auto& f : tf) if (f == 0) f = 1;

            std::vector<HuffTree> trees(n_trees);
            for (int t = 0; t < n_trees; t++) trees[t].build_from_freqs(tree_freqs[t].data(), compact_size);

            for (int g = 0; g < n_groups; g++) {
                size_t start = g * GROUP_SIZE;
                size_t end = std::min(start + GROUP_SIZE, n);
                int best_tree = 0; size_t best_cost = SIZE_MAX;
                for (int t = 0; t < n_trees; t++) {
                    size_t cost = 0;
                    for (size_t i = start; i < end; i++) {
                        int ci = compact_idx[data[i]];
                        if (ci >= 0) cost += trees[t].code_lengths[ci];
                    }
                    if (cost < best_cost) { best_cost = cost; best_tree = t; }
                }
                group_tree[g] = best_tree;
            }
        }

        // Final trees
        for (auto& tf : tree_freqs) std::fill(tf.begin(), tf.end(), 0);
        for (int g = 0; g < n_groups; g++) {
            int t = group_tree[g];
            size_t start = g * GROUP_SIZE;
            size_t end = std::min(start + GROUP_SIZE, n);
            for (size_t i = start; i < end; i++) {
                int ci = bitmap.to_compact(data[i]);
                if (ci >= 0) tree_freqs[t][ci]++;
            }
        }
        for (auto& tf : tree_freqs) for (auto& f : tf) if (f == 0) f = 1;
        std::vector<HuffTree> trees(n_trees);
        for (int t = 0; t < n_trees; t++) trees[t].build_from_freqs(tree_freqs[t].data(), compact_size);

        // Count total bits
        size_t total_bits = 3 + 24;  // n_trees + n_groups (24-bit, BC format)
        // Selectors
        {
            std::vector<int> mtf(n_trees);
            std::iota(mtf.begin(), mtf.end(), 0);
            for (int g = 0; g < n_groups; g++) {
                int tree = group_tree[g];
                int pos = 0;
                while (mtf[pos] != tree) pos++;
                total_bits += pos + 1;
                for (int j = pos; j > 0; j--) mtf[j] = mtf[j-1];
                mtf[0] = tree;
            }
        }
        // Tree lengths
        for (int t = 0; t < n_trees; t++) {
            total_bits += 5;
            int curr = trees[t].code_lengths[0];
            for (int s = 0; s < compact_size; s++) {
                int len = trees[t].code_lengths[s];
                total_bits += abs(len - curr) * 2 + 1;
                curr = len;
            }
        }
        // Data
        for (size_t i = 0; i < n; i++) {
            int g = i / GROUP_SIZE;
            int ci = compact_idx[data[i]];
            if (ci >= 0) total_bits += trees[group_tree[g]].code_lengths[ci];
        }

        size_t total_bytes = (total_bits + 7) / 8;

        if (!write_output) return total_bytes;

        // Actually write the output
        write_bits(n_trees - 1, 3);
        // n_groups stored in 24 bits (BC format) — supports up to 16M groups (~1GB raw input).
        // Old BB format used 16 bits, which overflows past ~3MB of post-BWT data.
        write_bits(n_groups & 0xFFFF, 16);
        write_bits((n_groups >> 16) & 0xFF, 8);

        std::vector<int> selector_mtf(n_trees);
        std::iota(selector_mtf.begin(), selector_mtf.end(), 0);
        for (int g = 0; g < n_groups; g++) {
            int tree = group_tree[g];
            int pos = 0;
            while (selector_mtf[pos] != tree) pos++;
            write_unary(pos);
            for (int j = pos; j > 0; j--) selector_mtf[j] = selector_mtf[j-1];
            selector_mtf[0] = tree;
        }

        for (int t = 0; t < n_trees; t++) {
            int curr_len = trees[t].code_lengths[0];
            write_bits(curr_len, 5);
            for (int s = 0; s < compact_size; s++) {
                int len = trees[t].code_lengths[s];
                while (curr_len < len) { write_bits(1, 2); curr_len++; }
                while (curr_len > len) { write_bits(3, 2); curr_len--; }
                write_bits(0, 1);
            }
        }

        int curr_group = -1, curr_tree = 0;
        for (size_t i = 0; i < n; i++) {
            int g = i / GROUP_SIZE;
            if (g != curr_group) { curr_group = g; curr_tree = group_tree[g]; }
            int ci = compact_idx[data[i]];
            if (ci >= 0) {
                uint32_t code = trees[curr_tree].codes[ci];
                int len = trees[curr_tree].code_lengths[ci];
                uint32_t rev = 0;
                for (int b = 0; b < len; b++) rev = (rev << 1) | ((code >> b) & 1);
                write_bits(rev, len);
            }
        }
        if (bits_in_buffer > 0) output.push_back(buffer & 0xFF);
        return total_bytes;
    }

    void encode(const std::vector<uint16_t>& data, int alpha_size) {
        if (data.empty()) return;

        SymbolBitmap bitmap;
        bitmap.build(data);
        int compact_size = bitmap.used_symbols.size();

        // Determine base tree count
        size_t n = data.size();
        int base_trees;
        if (n < 200) base_trees = 2;
        else if (n < 600) base_trees = 3;
        else if (n < 1200) base_trees = 4;
        else base_trees = MAX_TREES;

        // Per-block adaptive tree count: trial NT in {3,4,5,6} for any block ≥1200 symbols.
        // Different content in different blocks may prefer different NT — this catches
        // the variation that a globally-fixed NT misses. encode_with_trees in
        // measure-only mode (write_output=false) is cheap relative to the actual emit.
        int best_trees = base_trees;
        if (n >= 1200) {
            size_t best_size = SIZE_MAX;
            int max_try = std::min(MAX_TREES + 1, 7);
            for (int try_trees = 3; try_trees <= max_try; try_trees++) {
                size_t sz = encode_with_trees(data, alpha_size, try_trees, bitmap, compact_size, false);
                if (sz < best_size) { best_size = sz; best_trees = try_trees; }
            }
        }

        bitmap.write(output);
        encode_with_trees(data, alpha_size, best_trees, bitmap, compact_size, true);
    }
};

// =============================================================================
// Multi-Tree Decoder with Bitmap
// =============================================================================
class MultiTreeDecoder {
    const uint8_t* data;
    size_t size;
    size_t byte_pos = 0;
    uint32_t buffer = 0;
    int bits_in_buffer = 0;

    void ensure_bits(int n) {
        while (bits_in_buffer < n && byte_pos < size) {
            buffer |= ((uint32_t)data[byte_pos++] << bits_in_buffer);
            bits_in_buffer += 8;
        }
    }

    uint32_t read_bits(int n) {
        ensure_bits(n);
        uint32_t val = buffer & ((1u << n) - 1);
        buffer >>= n;
        bits_in_buffer -= n;
        return val;
    }

    // Unary decoding: count 1s until 0
    int read_unary() {
        int count = 0;
        while (read_bits(1) == 1) {
            count++;
        }
        return count;
    }

public:
    MultiTreeDecoder(const uint8_t* d, size_t n) : data(d), size(n) {}

    std::vector<uint16_t> decode(size_t expected_count, bool debug = false) {
        SymbolBitmap bitmap;
        size_t bitmap_size = bitmap.read(data, size);
        if (bitmap_size == 0) return {};

        byte_pos = bitmap_size;
        int compact_size = bitmap.used_symbols.size();

        int n_trees = read_bits(3) + 1;
        int n_groups = read_bits(16);

        if (debug) printf("Decoder: n_trees=%d, n_groups=%d, compact_size=%d\n", n_trees, n_groups, compact_size);

        // Decode selectors with MTF + unary
        std::vector<int> selector_mtf(n_trees);
        std::iota(selector_mtf.begin(), selector_mtf.end(), 0);

        std::vector<int> group_tree(n_groups);
        for (int g = 0; g < n_groups; g++) {
            int pos = read_unary();
            // Defensive bounds check — corrupt input could otherwise index past the array.
            if (pos < 0 || pos >= n_trees) return {};
            int tree = selector_mtf[pos];
            group_tree[g] = tree;

            // Move to front
            for (int j = pos; j > 0; j--) {
                selector_mtf[j] = selector_mtf[j - 1];
            }
            selector_mtf[0] = tree;
        }

        std::vector<HuffTree> trees(n_trees);
        for (int t = 0; t < n_trees; t++) {
            int curr_len = read_bits(5);
            std::vector<int> lengths(compact_size);

            for (int s = 0; s < compact_size; s++) {
                while (true) {
                    uint32_t bit = read_bits(1);
                    if (bit == 0) break;
                    bit = read_bits(1);
                    if (bit == 0) curr_len++;
                    else curr_len--;
                }
                lengths[s] = curr_len;
            }
            trees[t].build_from_lengths(lengths.data(), compact_size);
            if (debug) {
                printf("Tree %d: max_len=%d, lengths: ", t, trees[t].max_length);
                for (int s = 0; s < compact_size; s++) printf("%d ", trees[t].code_lengths[s]);
                printf("\n");
            }
        }

        struct DecodeEntry { int symbol; int length; };
        std::vector<std::vector<DecodeEntry>> decode_tables(n_trees);

        for (int t = 0; t < n_trees; t++) {
            int max_len = trees[t].max_length;
            if (max_len == 0) max_len = 1;
            decode_tables[t].resize(1 << max_len);

            if (debug) {
                printf("Building decode table for tree %d, max_len=%d\n", t, max_len);
                for (int s = 0; s < compact_size; s++) {
                    printf("  sym %d: len=%d code=%u\n", s, trees[t].code_lengths[s], trees[t].codes[s]);
                }
            }

            for (int s = 0; s < compact_size; s++) {
                int len = trees[t].code_lengths[s];
                if (len > 0) {
                    uint32_t code = trees[t].codes[s];
                    uint32_t rev_code = 0;
                    for (int b = 0; b < len; b++) {
                        rev_code = (rev_code << 1) | ((code >> b) & 1);
                    }

                    int fill = 1 << (max_len - len);
                    for (int f = 0; f < fill; f++) {
                        uint32_t idx = rev_code | (f << len);
                        if (debug && s < 3) {
                            printf("  table[%u] = {%d, %d} (code=%u->rev=%u, f=%d)\n", idx, s, len, code, rev_code, f);
                        }
                        decode_tables[t][idx] = {s, len};
                    }
                }
            }
        }

        std::vector<uint16_t> output;
        output.reserve(expected_count);

        int curr_group = -1;
        int curr_tree = 0;

        for (size_t i = 0; i < expected_count; i++) {
            int g = i / GROUP_SIZE;
            if (g != curr_group) {
                curr_group = g;
                curr_tree = group_tree[g];
            }

            int max_len = trees[curr_tree].max_length;
            if (max_len == 0) max_len = 1;
            ensure_bits(max_len);

            uint32_t peek = buffer & ((1u << max_len) - 1);
            auto& entry = decode_tables[curr_tree][peek];

            if (debug && i < 5) {
                printf("  decode[%zu]: peek=%u, sym=%d, len=%d, full_sym=%d\n",
                       i, peek, entry.symbol, entry.length, bitmap.from_compact(entry.symbol));
            }

            output.push_back(bitmap.from_compact(entry.symbol));
            buffer >>= entry.length;
            bits_in_buffer -= entry.length;
        }

        return output;
    }

    // Decode until EOB symbol (highest symbol in alphabet)
    // n_groups_bits: 16 for legacy 'BB', 24 for current 'BC' format
    std::vector<uint16_t> decode_until_eob(bool debug = false, int n_groups_bits = 24) {
        if (debug) { printf("decode_until_eob called: size=%zu n_groups_bits=%d\n", size, n_groups_bits); fflush(stdout); }
        SymbolBitmap bitmap;
        size_t bitmap_size = bitmap.read(data, size);
        if (debug) { printf("bitmap_size=%zu\n", bitmap_size); fflush(stdout); }
        if (bitmap_size == 0) return {};

        byte_pos = bitmap_size;
        int compact_size = bitmap.used_symbols.size();
        if (debug) { printf("compact_size=%d\n", compact_size); fflush(stdout); }

        // EOB is the highest symbol
        if (compact_size == 0) return {};
        uint16_t eob_sym = bitmap.used_symbols.back();
        if (debug) { printf("eob_sym=%d\n", eob_sym); fflush(stdout); }

        int n_trees = read_bits(3) + 1;
        int n_groups;
        if (n_groups_bits == 24) {
            int low = read_bits(16);
            int high = read_bits(8);
            n_groups = low | (high << 16);
        } else {
            n_groups = read_bits(16);
        }

        if (debug) { printf("Decoder EOB: n_trees=%d, n_groups=%d, compact_size=%d, eob=%d\n",
                          n_trees, n_groups, compact_size, eob_sym); fflush(stdout); }

        // Decode selectors
        if (debug) { printf("Decoding %d selectors...\n", n_groups); fflush(stdout); }
        std::vector<int> selector_mtf(n_trees);
        std::iota(selector_mtf.begin(), selector_mtf.end(), 0);

        std::vector<int> group_tree(n_groups);
        for (int g = 0; g < n_groups; g++) {
            int pos = read_unary();
            if (pos >= n_trees) {
                if (debug) { printf("ERROR: selector pos %d >= n_trees %d at group %d\n", pos, n_trees, g); fflush(stdout); }
                return {};
            }
            int tree = selector_mtf[pos];
            group_tree[g] = tree;
            for (int j = pos; j > 0; j--) selector_mtf[j] = selector_mtf[j - 1];
            selector_mtf[0] = tree;
        }
        if (debug) { printf("Selectors decoded\n"); fflush(stdout); }

        // Build trees
        if (debug) { printf("Building %d trees...\n", n_trees); fflush(stdout); }
        std::vector<HuffTree> trees(n_trees);
        for (int t = 0; t < n_trees; t++) {
            int curr_len = read_bits(5);
            std::vector<int> lengths(compact_size);
            for (int s = 0; s < compact_size; s++) {
                while (true) {
                    uint32_t bit = read_bits(1);
                    if (bit == 0) break;
                    bit = read_bits(1);
                    if (bit == 0) curr_len++;
                    else curr_len--;
                }
                lengths[s] = curr_len;
            }
            trees[t].build_from_lengths(lengths.data(), compact_size);
            if (debug) { printf("Tree %d: max_len=%d\n", t, trees[t].max_length); fflush(stdout); }
        }
        if (debug) { printf("Trees built\n"); fflush(stdout); }

        // Build decode tables
        if (debug) { printf("Building decode tables...\n"); fflush(stdout); }
        struct DecodeEntry { int symbol; int length; };
        std::vector<std::vector<DecodeEntry>> decode_tables(n_trees);

        for (int t = 0; t < n_trees; t++) {
            int max_len = trees[t].max_length;
            if (max_len == 0) max_len = 1;
            if (max_len > 20) {
                if (debug) { printf("ERROR: tree %d max_len %d too large!\n", t, max_len); fflush(stdout); }
                return {};
            }
            decode_tables[t].resize(1 << max_len);

            for (int s = 0; s < compact_size; s++) {
                int len = trees[t].code_lengths[s];
                if (len > 0) {
                    uint32_t code = trees[t].codes[s];
                    uint32_t rev_code = 0;
                    for (int b = 0; b < len; b++) {
                        rev_code = (rev_code << 1) | ((code >> b) & 1);
                    }
                    int fill = 1 << (max_len - len);
                    for (int f = 0; f < fill; f++) {
                        uint32_t idx = rev_code | (f << len);
                        decode_tables[t][idx] = {s, len};
                    }
                }
            }
            if (debug) { printf("Decode table %d: size=%zu\n", t, decode_tables[t].size()); fflush(stdout); }
        }
        if (debug) { printf("Decode tables built\n"); fflush(stdout); }

        // Decode until EOB
        if (debug) { printf("Starting decode loop (expected ~%d symbols)...\n", n_groups * GROUP_SIZE); fflush(stdout); }
        std::vector<uint16_t> output;
        output.reserve(n_groups * GROUP_SIZE);

        int curr_group = -1;
        int curr_tree = 0;
        size_t i = 0;

        while (true) {
            int g = i / GROUP_SIZE;
            if (g != curr_group && g < n_groups) {
                curr_group = g;
                curr_tree = group_tree[g];
            }

            int max_len = trees[curr_tree].max_length;
            if (max_len == 0) max_len = 1;
            ensure_bits(max_len);

            uint32_t peek = buffer & ((1u << max_len) - 1);
            auto& entry = decode_tables[curr_tree][peek];

            // Debug: check for invalid entry
            if (entry.length == 0) {
                if (debug) { printf("ERROR at i=%zu: entry.length=0, peek=%u, tree=%d\n", i, peek, curr_tree); fflush(stdout); }
                return {};
            }

            uint16_t sym = bitmap.from_compact(entry.symbol);

            if (sym == eob_sym) {
                if (debug) { printf("EOB found at i=%zu\n", i); fflush(stdout); }
                break;
            }

            output.push_back(sym);
            buffer >>= entry.length;
            bits_in_buffer -= entry.length;
            i++;

            // Debug: periodic progress
            if (debug && (i % 5000 == 0)) { printf("Decoded %zu symbols...\n", i); fflush(stdout); }

            if (i > 100000000) break;  // Safety limit
        }

        return output;
    }
};

// =============================================================================
// Main Compress/Decompress
// =============================================================================
// Varint encoding: 7 bits per byte, high bit = continuation
inline void write_varint(std::vector<uint8_t>& out, uint32_t val) {
    while (val >= 128) {
        out.push_back((val & 0x7F) | 0x80);
        val >>= 7;
    }
    out.push_back(val & 0x7F);
}

inline uint32_t read_varint(const uint8_t* data, size_t max_size, size_t& pos) {
    uint32_t val = 0;
    int shift = 0;
    while (pos < max_size && shift < 35) {
        uint8_t b = data[pos++];
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return val;
}

// Run BWT → MTF → ZRLE on a (possibly pre-RLE'd) buffer. Returns the post-ZRLE
// symbol stream + primary_idx, ready to feed into either the Huffman or range
// coder backend. Returns empty on failure.
struct BwtPipelineResult {
    std::vector<uint16_t> rle;       // post-ZRLE symbols (EOB appended)
    uint32_t primary_idx = 0;
    int alpha_size = 0;
    bool ok = false;
};
inline BwtPipelineResult run_bwt_pipeline(const uint8_t* bwt_input, size_t bwt_n) {
    BwtPipelineResult r;
    std::vector<uint8_t> padded;
    const uint8_t* bwt_data = bwt_input;
    size_t bwt_size = bwt_n;
    if (is_libsais_problematic_size(bwt_size)) {
        padded.assign(bwt_input, bwt_input + bwt_n);
        size_t safe_size = get_safe_padded_size(bwt_size);
        padded.resize(safe_size, 0);
        bwt_data = padded.data();
        bwt_size = padded.size();
    }
    auto bwt = bwt_encode(bwt_data, bwt_size, r.primary_idx);
    auto mtf = mtf_encode(bwt.data(), bwt.size());
    r.rle = zrle_encode(mtf.data(), mtf.size());
    if (r.rle.empty()) return r;

    int max_sym = 0;
    for (auto s : r.rle) if (s > max_sym) max_sym = s;
    uint16_t eob_sym = max_sym + 1;
    r.rle.push_back(eob_sym);
    r.alpha_size = eob_sym + 1;
    r.ok = true;
    return r;
}

// Huffman backend: header 'BC' (pre-RLE applied) or 'BD' (no pre-RLE).
inline std::vector<uint8_t> compress_huffman(const BwtPipelineResult& r,
                                              uint32_t original_size, char header_byte) {
    std::vector<uint8_t> output;
    output.push_back('B');
    output.push_back(header_byte);
    write_varint(output, original_size);
    write_varint(output, r.primary_idx);
    MultiTreeEncoder encoder(output);
    encoder.encode(r.rle, r.alpha_size);
    return output;
}

// Range coder backends:
//   header_byte 'E' = order-0, pre-RLE
//   header_byte 'F' = order-0, no pre-RLE
//   header_byte 'I' = order-1, pre-RLE  (BI)
//   header_byte 'J' = order-1, no pre-RLE (BJ)
// Format: header(2) + varint(original_size) + varint(primary_idx) + varint(rle_count) + rc_data
// rle_count is the number of symbols including the EOB sentinel.
inline std::vector<uint8_t> compress_rc(const BwtPipelineResult& r,
                                         uint32_t original_size, char header_byte) {
    std::vector<uint8_t> output;
    output.push_back('B');
    output.push_back(header_byte);
    write_varint(output, original_size);
    write_varint(output, r.primary_idx);
    write_varint(output, static_cast<uint32_t>(r.rle.size()));

    std::vector<uint8_t> rc_bytes;
    bool order1 = (header_byte == 'I' || header_byte == 'J');
    if (order1) {
        rc_bytes = rc_encode_symbols_o1(r.rle);
    } else {
        rc_bytes = rc_encode_symbols(r.rle, r.alpha_size);
    }
    output.insert(output.end(), rc_bytes.begin(), rc_bytes.end());
    return output;
}

inline std::vector<uint8_t> compress(const uint8_t* data, size_t n) {
    if (n == 0) return {};

    // Run BWT pipeline twice (with and without pre-RLE), then for each, try both
    // entropy backends (Huffman + range coder o0 + range coder o1). Plus LZP-then-BWT.
    // The BWT runs dominate cost; the entropy backends are cheap by comparison.
    auto rle1 = pre_rle_encode(data, n);
    auto pipe_with    = run_bwt_pipeline(rle1.data(), rle1.size());
    auto pipe_without = run_bwt_pipeline(data, n);

    // LZP variant: only worth trying when input is large enough to benefit and
    // LZP actually shrinks it (otherwise the trial cost wastes encode time).
    std::vector<uint8_t> lzp_buf;
    BwtPipelineResult pipe_lzp;
    bool lzp_ok = false;
    if (n >= 4096) {
        lzp_buf = lzp_encode(data, n);
        if (lzp_buf.size() < n) {  // LZP shrunk the input; worth running BWT on it
            pipe_lzp = run_bwt_pipeline(lzp_buf.data(), lzp_buf.size());
            lzp_ok = pipe_lzp.ok;
        }
    }

    std::vector<std::vector<uint8_t>> candidates;
    if (pipe_with.ok) {
        candidates.push_back(compress_huffman(pipe_with,
                              static_cast<uint32_t>(rle1.size()), 'C'));
        candidates.push_back(compress_rc(pipe_with,
                              static_cast<uint32_t>(rle1.size()), 'E'));
        candidates.push_back(compress_rc(pipe_with,
                              static_cast<uint32_t>(rle1.size()), 'I'));
    }
    if (pipe_without.ok) {
        candidates.push_back(compress_huffman(pipe_without,
                              static_cast<uint32_t>(n), 'D'));
        candidates.push_back(compress_rc(pipe_without,
                              static_cast<uint32_t>(n), 'F'));
        candidates.push_back(compress_rc(pipe_without,
                              static_cast<uint32_t>(n), 'J'));
    }
    if (lzp_ok) {
        // LZP path: header 'BK' = LZP + Huffman. The "original_size" here is the
        // LZP-encoded size (the size BWT inverse will produce); the decoder needs
        // BOTH the LZP-encoded size and the truly-original size.
        candidates.push_back(compress_huffman(pipe_lzp,
                              static_cast<uint32_t>(lzp_buf.size()), 'K'));
        candidates.push_back(compress_rc(pipe_lzp,
                              static_cast<uint32_t>(lzp_buf.size()), 'M'));
    }

    // Word dictionary preprocessing: build per-file dict of high-saving words,
    // emit ESC+code per occurrence, then BWT the encoded stream. Pays off when
    // a non-trivial fraction of the input is repeating multi-char words (prose,
    // markdown, code with common identifiers). Trial-pick gates it.
    auto try_dict_variant = [](const uint8_t* in, size_t in_n, char header_byte,
                                std::vector<std::vector<uint8_t>>& cands) {
        if (in_n < 4096) return;
        WordDict dict_full = wd_build(in, in_n);
        if (dict_full.words.empty()) return;
        // Trial multiple dict sizes — smaller has less header, larger has more
        // compression. Optimum varies with block size and content.
        int sizes[] = { 64, 128, 192, 255 };
        for (int sz : sizes) {
            WordDict dict = wd_truncate(dict_full, sz);
            if (dict.words.empty()) continue;
            auto enc = wd_encode(in, in_n, dict);
            if (enc.size() >= in_n) continue;
            auto pipe_dict = run_bwt_pipeline(enc.data(), enc.size());
            if (!pipe_dict.ok) continue;

            std::vector<uint8_t> payload = compress_huffman(pipe_dict,
                                              static_cast<uint32_t>(enc.size()), header_byte);
            std::vector<uint8_t> full;
            full.push_back('B');
            full.push_back(header_byte);
            wd_serialize(dict, full);
            full.insert(full.end(), payload.begin() + 2, payload.end());
            cands.push_back(std::move(full));
        }
    };

    // Variant 1: dict on raw input ('BR')
    try_dict_variant(data, n, 'R', candidates);

    // Variant 5: capfold + dict + LZP-AFTER-DICT ('BW'). Synergy: LZP on the
    // dict-encoded stream catches long repeats that emerged from word substitution.
    // Trial multiple LZP min_match values per block — dict-encoded patterns are
    // shorter than raw text patterns so smaller min_match captures more.
    if (n >= 4096) {
        auto cf_w = cf_encode(data, n);
        WordDict dict_w_full = wd_build(cf_w.data(), cf_w.size());
        if (!dict_w_full.words.empty()) {
            int sizes[] = { 128, 255 };
            int min_matches[] = { 10, 20, 40 };
            for (int sz : sizes) {
                WordDict dict_w = wd_truncate(dict_w_full, sz);
                if (dict_w.words.empty()) continue;
                auto wd_enc = wd_encode(cf_w.data(), cf_w.size(), dict_w);
                for (int mm : min_matches) {
                    auto lzp_enc = lzp_encode_min(wd_enc.data(), wd_enc.size(), mm);
                    if (lzp_enc.size() >= wd_enc.size()) continue;
                    auto pipe_w = run_bwt_pipeline(lzp_enc.data(), lzp_enc.size());
                    if (!pipe_w.ok) continue;
                    std::vector<uint8_t> payload = compress_huffman(pipe_w,
                                                       static_cast<uint32_t>(lzp_enc.size()), 'W');
                    // Header BW + 1-byte min_match + dict + (data after BW header)
                    std::vector<uint8_t> full;
                    full.push_back('B');
                    full.push_back('W');
                    full.push_back((uint8_t)mm);
                    wd_serialize(dict_w, full);
                    full.insert(full.end(), payload.begin() + 2, payload.end());
                    candidates.push_back(std::move(full));
                }
            }
        }
    }

    // Variant 2: capital-fold then dict ('BS'). Try multiple dict sizes per block —
    // smaller dict has less header overhead, larger dict has more compression.
    // The optimum varies with block size and content type, so trial and pick smallest.
    if (n >= 4096) {
        auto cf = cf_encode(data, n);
        WordDict dict_cf_full = wd_build(cf.data(), cf.size());
        if (!dict_cf_full.words.empty()) {
            int sizes[] = { 64, 128, 192, 255 };
            for (int sz : sizes) {
                WordDict dict_cf = wd_truncate(dict_cf_full, sz);
                if (dict_cf.words.empty()) continue;
                auto enc = wd_encode(cf.data(), cf.size(), dict_cf);
                if (enc.size() >= cf.size() + 256) continue;
                auto pipe_cf = run_bwt_pipeline(enc.data(), enc.size());
                if (!pipe_cf.ok) continue;
                std::vector<uint8_t> payload = compress_huffman(pipe_cf,
                                                   static_cast<uint32_t>(enc.size()), 'S');
                std::vector<uint8_t> full;
                full.push_back('B');
                full.push_back('S');
                wd_serialize(dict_cf, full);
                full.insert(full.end(), payload.begin() + 2, payload.end());
                candidates.push_back(std::move(full));
            }
        }
    }

    // Variant 4: capfold + dict + BIGRAM second pass ('BV'). After single-word dict
    // encoding, common (ESC X SPACE ESC Y) sequences are 5 bytes. A second-pass
    // bigram dict collapses them to 2 bytes. The new BG_ESC=0xFD byte clusters
    // tightly post-BWT just like WD_ESC=0xFE does.
    if (n >= 4096) {
        auto cf3 = cf_encode(data, n);
        WordDict dict_cf3 = wd_build(cf3.data(), cf3.size());
        if (!dict_cf3.words.empty()) {
            auto wd_enc = wd_encode(cf3.data(), cf3.size(), dict_cf3);
            BigramDict bg_dict = bg_build(wd_enc.data(), wd_enc.size());
            if (!bg_dict.bigrams.empty()) {
                auto bg_enc = bg_encode(wd_enc.data(), wd_enc.size(), bg_dict);
                if (bg_enc.size() < wd_enc.size()) {
                    auto pipe_bv = run_bwt_pipeline(bg_enc.data(), bg_enc.size());
                    if (pipe_bv.ok) {
                        std::vector<uint8_t> payload = compress_huffman(pipe_bv,
                                                          static_cast<uint32_t>(bg_enc.size()), 'V');
                        std::vector<uint8_t> full;
                        full.push_back('B');
                        full.push_back('V');
                        wd_serialize(dict_cf3, full);
                        bg_serialize(bg_dict, full);
                        full.insert(full.end(), payload.begin() + 2, payload.end());
                        candidates.push_back(std::move(full));
                    }
                }
            }
        }
    }

    // Variant 3: XML entity collapse + capfold + dict ('BU'). Wikipedia-targeted.
    // `&amp;` (5B) → 0x01, `&quot;` (6B) → 0x02, etc. Each entity occurrence saves 3-5
    // raw bytes; the substituted control byte clusters tightly post-BWT. Only fires when
    // entity collapse actually shrinks the input meaningfully.
    if (n >= 4096) {
        auto xe = xe_encode(data, n);
        if (xe.size() + 1024 < n) {  // require at least ~1KB shrinkage to bother
            auto cf2 = cf_encode(xe.data(), xe.size());
            WordDict dict_xe = wd_build(cf2.data(), cf2.size());
            if (!dict_xe.words.empty()) {
                auto enc = wd_encode(cf2.data(), cf2.size(), dict_xe);
                if (enc.size() < cf2.size() + 256) {
                    auto pipe_xe = run_bwt_pipeline(enc.data(), enc.size());
                    if (pipe_xe.ok) {
                        std::vector<uint8_t> payload = compress_huffman(pipe_xe,
                                                           static_cast<uint32_t>(enc.size()), 'U');
                        std::vector<uint8_t> full;
                        full.push_back('B');
                        full.push_back('U');
                        wd_serialize(dict_xe, full);
                        full.insert(full.end(), payload.begin() + 2, payload.end());
                        candidates.push_back(std::move(full));
                    }
                }
            }
        }
    }

    std::vector<uint8_t>* best = nullptr;
    for (auto& c : candidates) {
        if (!c.empty() && (best == nullptr || c.size() < best->size())) best = &c;
    }
    return best ? std::move(*best) : std::vector<uint8_t>{};
}

inline std::vector<uint8_t> decompress(const uint8_t* data, size_t n, bool debug = false) {
    if (debug) { printf("bwt5::decompress called: n=%zu, header=%c%c\n", n, data[0], data[1]); fflush(stdout); }
    if (n < 4) return {};

    // Check header format
    bool is_bd = (data[0] == 'B' && data[1] == 'D');  // Huffman, no pre-RLE
    bool is_bc = (data[0] == 'B' && data[1] == 'C');  // Huffman, pre-RLE, 24-bit n_groups
    bool is_bb = (data[0] == 'B' && data[1] == 'B');  // Huffman, pre-RLE, 16-bit n_groups (legacy)
    bool is_be = (data[0] == 'B' && data[1] == 'E');  // Range coder order-0, pre-RLE
    bool is_bf = (data[0] == 'B' && data[1] == 'F');  // Range coder order-0, no pre-RLE
    bool is_bi = (data[0] == 'B' && data[1] == 'I');  // Range coder order-1, pre-RLE
    bool is_bj = (data[0] == 'B' && data[1] == 'J');  // Range coder order-1, no pre-RLE
    bool is_bk = (data[0] == 'B' && data[1] == 'K');  // LZP + Huffman
    bool is_bm = (data[0] == 'B' && data[1] == 'M');  // LZP + RC order-0
    bool is_br = (data[0] == 'B' && data[1] == 'R');  // WordDict + Huffman
    bool is_bs = (data[0] == 'B' && data[1] == 'S');  // CapFold + WordDict + Huffman
    bool is_bu = (data[0] == 'B' && data[1] == 'U');  // XmlEntity + CapFold + WordDict + Huffman
    bool is_bv = (data[0] == 'B' && data[1] == 'V');  // CapFold + WordDict + Bigram + Huffman
    bool is_bw = (data[0] == 'B' && data[1] == 'W');  // CapFold + WordDict + LZP + Huffman
    bool is_huffman = is_bb || is_bc || is_bd || is_bk || is_br || is_bs || is_bu || is_bv || is_bw;
    bool is_rc_o0  = is_be || is_bf || is_bm;
    bool is_rc_o1  = is_bi || is_bj;
    bool is_rc     = is_rc_o0 || is_rc_o1;
    bool no_prerle = is_bd || is_bf || is_bj || is_bk || is_bm || is_br || is_bs || is_bu || is_bv || is_bw;
    bool is_lzp    = is_bk || is_bm;
    bool is_lzp_after_dict = is_bw;
    bool is_wd     = is_br || is_bs || is_bu || is_bv || is_bw;
    bool is_capfold = is_bs || is_bu || is_bv || is_bw;
    bool is_xmlentity = is_bu;
    bool is_bigram  = is_bv;
    if (is_huffman || is_rc) {
        if (debug) { printf("Matched %c%c header\n", data[0], data[1]); fflush(stdout); }
        size_t pos = 2;

        // BR/BS/BU/BV/BW carry a serialized word dict right after the header.
        // BV additionally carries a bigram dict; BW carries a 1-byte LZP min_match
        // value before the dict.
        WordDict wd_for_decode;
        BigramDict bg_for_decode;
        int lzp_min_match_for_decode = LZP_MIN_MATCH;
        if (is_lzp_after_dict) {
            if (pos >= n) return {};
            lzp_min_match_for_decode = data[pos++];
        }
        if (is_wd) {
            wd_for_decode = wd_deserialize(data, n, pos);
        }
        if (is_bigram) {
            bg_for_decode = bg_deserialize(data, n, pos);
        }

        uint32_t rle1_size = read_varint(data, n, pos);
        uint32_t primary_idx = read_varint(data, n, pos);

        if (debug) { printf("Compact format: rle1_size=%u, primary_idx=%u, data_start=%zu\n",
                          rle1_size, primary_idx, pos); fflush(stdout); }

        std::vector<uint16_t> rle;
        if (is_rc) {
            // Range-coded path: read symbol count, then decode that many symbols.
            uint32_t rle_count = read_varint(data, n, pos);
            if (is_rc_o1) {
                rle = rc_decode_symbols_o1(data + pos, n - pos, rle_count);
            } else {
                rle = rc_decode_symbols(data + pos, n - pos, /*alpha unused*/ 512, rle_count);
            }
            // Strip trailing EOB sentinel (compress_inner appended it for parity with Huffman path).
            if (!rle.empty()) rle.pop_back();
        } else {
            MultiTreeDecoder decoder(data + pos, n - pos);
            // BC and BD use 24-bit n_groups; legacy BB uses 16-bit.
            rle = decoder.decode_until_eob(debug, is_bb ? 16 : 24);
        }

        if (debug) {
            printf("Decoded RLE (%zu symbols, excluding EOB): ", rle.size());
            for (size_t i = 0; i < std::min(rle.size(), (size_t)20); i++)
                printf("%d ", rle[i]);
            if (rle.size() > 20) printf("...");
            printf("\n"); fflush(stdout);
        }

        // ZRLE decode
        if (debug) { printf("Starting ZRLE decode...\n"); fflush(stdout); }
        auto mtf_data = zrle_decode(rle.data(), rle.size());
        if (debug) { printf("ZRLE decoded: %zu bytes\n", mtf_data.size()); fflush(stdout); }

        // MTF decode
        if (debug) { printf("Starting MTF decode...\n"); fflush(stdout); }
        auto bwt_data = mtf_decode(mtf_data.data(), mtf_data.size());
        if (debug) { printf("MTF decoded: %zu bytes\n", bwt_data.size()); fflush(stdout); }

        // BWT decode
        if (debug) { printf("Starting BWT decode (primary_idx=%u)...\n", primary_idx); fflush(stdout); }
        auto rle1_data = bwt_decode(bwt_data.data(), bwt_data.size(), primary_idx, debug);
        if (debug) { printf("BWT decoded: %zu bytes (expected %u)\n", rle1_data.size(), rle1_size); fflush(stdout); }

        // Truncate to original size (remove padding added during compression)
        if (rle1_data.size() > rle1_size) {
            if (debug) { printf("Truncating from %zu to %u bytes (removing padding)\n", rle1_data.size(), rle1_size); fflush(stdout); }
            rle1_data.resize(rle1_size);
        }

        // Pre-RLE decode (only if pre-RLE was applied; BD/BF/BJ/BK/BM/BR skip this)
        if (no_prerle) {
            if (debug) { printf("Skipping pre-RLE decode (no-prerle format)\n"); fflush(stdout); }
            // LZP variants need a final LZP-decode pass
            if (is_lzp) return lzp_decode(rle1_data.data(), rle1_data.size());
            // Reverse layered preprocessing in correct order
            if (is_wd) {
                std::vector<uint8_t> stage = rle1_data;
                if (is_lzp_after_dict) stage = lzp_decode_min(stage.data(), stage.size(), lzp_min_match_for_decode);
                if (is_bigram) stage = bg_decode(stage.data(), stage.size(), bg_for_decode);
                stage = wd_decode(stage.data(), stage.size(), wd_for_decode);
                if (is_capfold) stage = cf_decode(stage.data(), stage.size());
                if (is_xmlentity) stage = xe_decode(stage.data(), stage.size());
                return stage;
            }
            return rle1_data;
        }
        if (debug) { printf("Starting Pre-RLE decode...\n"); fflush(stdout); }
        auto output = pre_rle_decode(rle1_data.data(), rle1_data.size());
        if (debug) { printf("Pre-RLE decoded: %zu bytes\n", output.size()); fflush(stdout); }

        return output;
    }
    else if (data[0] == 'B' && data[1] == 'A') {
        // Old format (for backwards compatibility)
        if (n < 20) return {};

        uint32_t orig_size = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
        uint32_t rle1_size = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
        uint32_t primary_idx = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
        uint32_t rle_count = data[14] | (data[15] << 8) | (data[16] << 16) | (data[17] << 24);

        size_t encoded_start = 20;
        MultiTreeDecoder decoder(data + encoded_start, n - encoded_start);
        auto rle = decoder.decode(rle_count, debug);

        if (rle.size() != rle_count) return {};

        auto mtf_data = zrle_decode(rle.data(), rle.size());
        auto bwt_data = mtf_decode(mtf_data.data(), mtf_data.size());
        auto rle1_data = bwt_decode(bwt_data.data(), bwt_data.size(), primary_idx);
        auto output = pre_rle_decode(rle1_data.data(), rle1_data.size());

        return output;
    }

    return {};
}

} // namespace bwt5
