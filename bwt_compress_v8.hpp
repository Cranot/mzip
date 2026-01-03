// BWT Compression v8 - Fixed Model Range Coding
//
// Key insight: BWT+MTF output has a KNOWN distribution:
// - ~55% zeros (MTF rank 0)
// - ~14% ones (MTF rank 1)
// - Exponential decay after that
//
// By using a FIXED pre-trained model, we eliminate model overhead!

#pragma once
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cstring>
#include <queue>
#include <functional>

namespace bwt8 {

constexpr int MAX_ALPHA = 258;

// Empirically-trained probability model for BWT+MTF+ZRLE output
// Trained on 200 samples of English text (512B-4KB)
// Key insight: Symbol 2 (MTF rank 1) is ~21%, not 9% as originally assumed
constexpr uint16_t FIXED_FREQ[MAX_ALPHA] = {
    // Symbols 0-7: RUNA, RUNB, and high-frequency MTF values
    12407, 5678, 13921, 7292, 4963, 3494, 2609, 2228,  // 0-7
     1991, 1771,  1568, 1394, 1292, 1074,  926,  434,  // 8-15
       88,   77,    29,   40,   37,   37,   52,   59,  // 16-23
       82,   68,    96,   55,   59,   53,   19,    1,  // 24-31
        1,    1,     1,    1,    1,    1,    1,    1,  // 32-39
        1,    1,     1,    1,    1,    1,    1,   55,  // 40-47
       10,   45,     1,    1,    1,    1,    1,    1,  // 48-55
        1,    1,     1,    1,    1,    1,    1,    1,  // 56-63
        1,    1,     1,    1,    1,    1,    1,    1,  // 64-71
        1,    2,     1,    2,    4,    4,    5,    8,  // 72-79
       31,    4,     7,    8,   11,   15,   24,   58,  // 80-87
        6,    5,     7,    8,   43,    7,   39,    3,  // 88-95
        6,   45,    37,    9,   45,   22,   65,   28,  // 96-103
       55,   21,    17,   56,   23,   29,   39,   61,  // 104-111
       65,   47,    25,   37,  104,  100,   69,   59,  // 112-119
       26,   84,    55,    1,    1,    1,    1,    1,  // 120-127
        1,    1,     1,    1,    1,    1,    1,    1,  // 128-135
        1,    1,     1,    1,    1,    1,    1,    1,  // 136-143
        1,    1,     1,    1,    1,    1,    1,    1,  // 144-151
        1,    1,     1,    1,    1,    1,    1,    1,  // 152-159
        1,    1,     1,    1,    1,    1,    1,    1,  // 160-167
        1,    1,     1,    1,    1,    1,    1,    1,  // 168-175
        1,    1,     1,    1,    1,    1,    1,    1,  // 176-183
        1,    1,     1,    1,    1,    1,    1,    1,  // 184-191
        1,    1,     1,    1,    1,    1,    1,    1,  // 192-199
        1,    1,     1,    1,    1,    1,    1,    1,  // 200-207
        1,    1,     1,    1,    1,    1,    1,    1,  // 208-215
        1,    1,     1,    1,    1,    1,    1,    1,  // 216-223
        1,    1,     1,    1,    1,    1,    1,    1,  // 224-231
        1,    1,     1,    1,    1,    1,    1,    1,  // 232-239
        1,    1,     1,    1,    1,    1,    1,    1,  // 240-247
        1,    1,     1,    1,    1,    1,    1,    1,  // 248-255
        1,    1,                                       // 256-257
};

// Precompute cumulative frequencies
inline void build_cumul(uint32_t* cumul, int alpha_size) {
    cumul[0] = 0;
    for (int i = 0; i < alpha_size; i++) {
        cumul[i + 1] = cumul[i] + FIXED_FREQ[i];
    }
}

// =============================================================================
// BWT Transform
// =============================================================================
inline std::vector<uint8_t> bwt_encode(const uint8_t* data, size_t n, uint32_t& primary_idx) {
    if (n == 0) return {};
    std::vector<uint32_t> sa(n);
    std::iota(sa.begin(), sa.end(), 0);
    std::sort(sa.begin(), sa.end(), [&](uint32_t a, uint32_t b) {
        for (size_t i = 0; i < n; i++) {
            uint8_t ca = data[(a + i) % n];
            uint8_t cb = data[(b + i) % n];
            if (ca != cb) return ca < cb;
        }
        return false;
    });
    primary_idx = 0;
    for (size_t i = 0; i < n; i++) {
        if (sa[i] == 0) { primary_idx = i; break; }
    }
    std::vector<uint8_t> bwt(n);
    for (size_t i = 0; i < n; i++) {
        bwt[i] = data[(sa[i] + n - 1) % n];
    }
    return bwt;
}

inline std::vector<uint8_t> bwt_decode(const uint8_t* bwt, size_t n, uint32_t primary_idx) {
    if (n == 0) return {};
    std::vector<uint32_t> count(256, 0);
    for (size_t i = 0; i < n; i++) count[bwt[i]]++;
    std::vector<uint32_t> cumul(256, 0);
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) { cumul[i] = sum; sum += count[i]; }
    std::vector<uint32_t> transform(n);
    std::vector<uint32_t> occ(256, 0);
    for (size_t i = 0; i < n; i++) {
        transform[i] = cumul[bwt[i]] + occ[bwt[i]];
        occ[bwt[i]]++;
    }
    std::vector<uint8_t> output(n);
    uint32_t idx = primary_idx;
    for (size_t i = n; i > 0; i--) {
        output[i - 1] = bwt[idx];
        idx = transform[idx];
    }
    return output;
}

// =============================================================================
// MTF Transform
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
// Zero-RLE
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
// Range Encoder/Decoder with Fixed Model
// =============================================================================
class RangeEncoder {
    std::vector<uint8_t>& output;
    uint32_t low = 0;
    uint32_t range = 0xFFFFFFFF;

    void normalize() {
        while (true) {
            if ((low ^ (low + range)) >= 0x1000000) {
                if (range >= 0x10000) break;
                range = -low & 0xFFFF;
            }
            output.push_back(low >> 24);
            low <<= 8;
            range <<= 8;
        }
    }

public:
    RangeEncoder(std::vector<uint8_t>& out) : output(out) {}

    void encode(int symbol, const uint32_t* cumul, uint32_t total) {
        uint32_t r = range / total;
        low += cumul[symbol] * r;
        range = (cumul[symbol + 1] - cumul[symbol]) * r;
        normalize();
    }

    void finish() {
        for (int i = 0; i < 4; i++) {
            output.push_back(low >> 24);
            low <<= 8;
        }
    }
};

class RangeDecoder {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    uint32_t low = 0;
    uint32_t code = 0;
    uint32_t range = 0xFFFFFFFF;

    uint8_t get_byte() {
        return pos < size ? data[pos++] : 0;
    }

    void normalize() {
        while (true) {
            if ((low ^ (low + range)) >= 0x1000000) {
                if (range >= 0x10000) break;
                range = -low & 0xFFFF;
            }
            code = (code << 8) | get_byte();
            low <<= 8;
            range <<= 8;
        }
    }

public:
    RangeDecoder(const uint8_t* d, size_t n) : data(d), size(n) {
        for (int i = 0; i < 4; i++) {
            code = (code << 8) | get_byte();
        }
    }

    int decode(const uint32_t* cumul, uint32_t total, int alpha_size) {
        uint32_t r = range / total;
        uint32_t target = (code - low) / r;

        // Binary search for symbol
        int lo = 0, hi = alpha_size;
        while (lo + 1 < hi) {
            int mid = (lo + hi) / 2;
            if (cumul[mid] <= target) lo = mid;
            else hi = mid;
        }
        int symbol = lo;

        low += cumul[symbol] * r;
        range = (cumul[symbol + 1] - cumul[symbol]) * r;
        normalize();

        return symbol;
    }
};

// =============================================================================
// Main Compress/Decompress
// =============================================================================
inline std::vector<uint8_t> compress(const uint8_t* data, size_t n) {
    if (n == 0) return {};

    uint32_t primary_idx;
    auto bwt = bwt_encode(data, n, primary_idx);
    auto mtf = mtf_encode(bwt.data(), bwt.size());
    auto rle = zrle_encode(mtf.data(), mtf.size());

    if (rle.empty()) return {};

    // Find max symbol to determine alpha size needed
    int max_sym = 0;
    for (auto s : rle) if (s > max_sym) max_sym = s;
    int alpha_size = max_sym + 1;

    // Build cumulative table from fixed model
    std::vector<uint32_t> cumul(alpha_size + 1);
    build_cumul(cumul.data(), alpha_size);
    uint32_t total = cumul[alpha_size];

    // Output
    std::vector<uint8_t> output;

    // Header - 'B8' for version 8
    output.push_back('B');
    output.push_back('8');

    // Original size (4 bytes)
    output.push_back(n & 0xFF);
    output.push_back((n >> 8) & 0xFF);
    output.push_back((n >> 16) & 0xFF);
    output.push_back((n >> 24) & 0xFF);

    // Primary index (4 bytes)
    output.push_back(primary_idx & 0xFF);
    output.push_back((primary_idx >> 8) & 0xFF);
    output.push_back((primary_idx >> 16) & 0xFF);
    output.push_back((primary_idx >> 24) & 0xFF);

    // RLE count (4 bytes)
    uint32_t rle_count = rle.size();
    output.push_back(rle_count & 0xFF);
    output.push_back((rle_count >> 8) & 0xFF);
    output.push_back((rle_count >> 16) & 0xFF);
    output.push_back((rle_count >> 24) & 0xFF);

    // Alpha size (2 bytes) - needed for decoding
    output.push_back(alpha_size & 0xFF);
    output.push_back((alpha_size >> 8) & 0xFF);

    // NO frequency table needed - using fixed model!

    // Range encode the data
    RangeEncoder encoder(output);
    for (auto sym : rle) {
        encoder.encode(sym, cumul.data(), total);
    }
    encoder.finish();

    return output;
}

inline std::vector<uint8_t> decompress(const uint8_t* data, size_t n) {
    if (n < 16) return {};
    if (data[0] != 'B' || data[1] != '8') return {};

    uint32_t orig_size = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
    uint32_t primary_idx = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
    uint32_t rle_count = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
    int alpha_size = data[14] | (data[15] << 8);

    // Build cumulative table from fixed model
    std::vector<uint32_t> cumul(alpha_size + 1);
    build_cumul(cumul.data(), alpha_size);
    uint32_t total = cumul[alpha_size];

    // Range decode
    size_t encoded_start = 16;
    RangeDecoder decoder(data + encoded_start, n - encoded_start);

    std::vector<uint16_t> rle(rle_count);
    for (size_t i = 0; i < rle_count; i++) {
        rle[i] = decoder.decode(cumul.data(), total, alpha_size);
    }

    // Reverse transforms
    auto mtf_data = zrle_decode(rle.data(), rle.size());
    auto bwt_data = mtf_decode(mtf_data.data(), mtf_data.size());
    auto output = bwt_decode(bwt_data.data(), bwt_data.size(), primary_idx);

    return output;
}

} // namespace bwt8
