// BWT Compression v9 - Smart Adaptive Dispatcher
//
// Intelligently dispatches to:
// - v8 (fixed model range coding) for data <5KB
// - v5 (multi-tree Huffman with symbol bitmap) for data >=5KB
//
// v8 now uses an empirically-trained model that's 7-9% better than before.
// Crossover point moved from ~2.5KB to ~5KB thanks to the improved model.

#pragma once
#include "bwt_compress_v5.hpp"
#include "bwt_compress_v8.hpp"
#ifndef CM_BACKEND_USE_BWT
#define CM_BACKEND_USE_BWT
#endif
#include "cm_backend.hpp"   // mode 2 = BWT + context-mixing (bzip3-class) entropy backend

namespace bwt9 {

// Crossover threshold: v8 wins below this, v4 wins above
// Empirically determined: v8 with optimal model beats v4 up to ~4.5-5KB
constexpr size_t CROSSOVER_SIZE = 5120;  // 5KB

// =============================================================================
// Mode Selection
// =============================================================================
enum class Mode : uint8_t {
    USE_V8 = 0,    // Fixed model range coding (v8)
    USE_V5 = 1,    // Multi-tree Huffman with bitmap (v5)
};

inline Mode choose_mode(size_t original_size) {
    // Simple size-based threshold
    // v8's trained model is efficient up to ~5KB
    // v4's local adaptation wins for larger data
    return (original_size < CROSSOVER_SIZE) ? Mode::USE_V8 : Mode::USE_V5;
}

// =============================================================================
// Main Compress/Decompress
// =============================================================================
inline std::vector<uint8_t> compress(const uint8_t* data, size_t n) {
    if (n == 0) return {};

    Mode mode = choose_mode(n);

    std::vector<uint8_t> output;
    output.push_back('B');
    output.push_back('9');
    output.push_back((uint8_t)mode);

    if (mode == Mode::USE_V8) {
        auto result = bwt8::compress(data, n);
        output.insert(output.end(), result.begin(), result.end());
    } else {
        auto result = bwt5::compress(data, n);
        output.insert(output.end(), result.begin(), result.end());
    }

    // mode 2: BWT + context-mixing (bzip3-class) — trial vs v5/v8, keep if smaller. Covers EVERY bwt9 call-site.
#ifndef MZIP_NO_CM
    auto cm = cmbk::compress_bwt(data, n);
    if (!cm.empty() && cm.size() + 3 < output.size()) {
        output.clear();
        output.push_back('B'); output.push_back('9'); output.push_back(2);
        output.insert(output.end(), cm.begin(), cm.end());
    }
#endif

    return output;
}

inline std::vector<uint8_t> decompress(const uint8_t* data, size_t n) {
    if (n < 4) return {};
    if (data[0] != 'B' || data[1] != '9') return {};

    uint8_t m = data[2];

    if (m == (uint8_t)Mode::USE_V8) return bwt8::decompress(data + 3, n - 3);
    if (m == 2)                     return cmbk::decompress_bwt(data + 3, n - 3);
    return bwt5::decompress(data + 3, n - 3);
}

// =============================================================================
// Utility
// =============================================================================
inline const char* mode_name(Mode m) {
    switch (m) {
        case Mode::USE_V8: return "USE_V8 (fixed model)";
        case Mode::USE_V5: return "USE_V5 (bitmap)";
        default: return "UNKNOWN";
    }
}

// Re-export transform functions for analysis
using bwt5::bwt_encode;
using bwt5::mtf_encode;
using bwt5::zrle_encode;

} // namespace bwt9
