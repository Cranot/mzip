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
// `cm_pre` (2026-08-02): an OPTIONAL, already-computed cmbk::compress_bwt(data, n) for this exact
// input. Purely an efficiency channel — mode 2 below is byte-for-byte what it always was, because
// cmbk::compress_bwt is deterministic, so a supplied result is indistinguishable from a recomputed
// one. It exists because mzip's TEXT path computes that same stream for its own CM_TEXT trial and
// then calls us, making the BWT+CM pass run TWICE on identical bytes. Defaults to nullptr, so every
// other call site (all the sub-stream callers) is untouched and still computes it itself.
// `cap` (2026-08-04): the incumbent size the caller will compare this result against.
// Forwarded to bwt5 purely so it can skip candidates that provably cannot come in
// under it (see the bound argued in bwt_compress_v5.hpp). SIZE_MAX = no pruning.
inline std::vector<uint8_t> compress(const uint8_t* data, size_t n,
                                     const std::vector<uint8_t>* cm_pre = nullptr,
                                     size_t cap = SIZE_MAX) {
    if (n == 0) return {};

    Mode mode = choose_mode(n);

    // LATENT HAZARD, FIXED HERE (2026-08-04). bwt5::compress CAN return {} -- its own
    // comment says the BR/BS/BU/BV/BW variants "have rare encoder/decoder asymmetries
    // on real-world inputs", and if no candidate round-trips it returns empty. The old
    // code pushed the 3-byte 'B','9',mode header FIRST and then appended the result
    // unchecked, so an empty legacy arm produced a 3-byte payload-less stream. Callers
    // test `b9.size() < cur`, and 3 < cur is true for essentially every block, so that
    // stub would be ACCEPTED AND SHIPPED. Not reachable on today's corpus, which is why
    // it has never fired -- but pruning makes an empty legacy arm far more likely, so
    // the header is now emitted only once there is a payload to attach to it.
    std::vector<uint8_t> output;
    bool have_legacy = false;
    {
        auto result = (mode == Mode::USE_V8)
                        ? bwt8::compress(data, n)
                        : bwt5::compress(data, n, cap > 3 ? cap - 3 : 1);
        if (!result.empty()) {
            output.push_back('B');
            output.push_back('9');
            output.push_back((uint8_t)mode);
            output.insert(output.end(), result.begin(), result.end());
            have_legacy = true;
        }
    }

    // mode 2: BWT + context-mixing (bzip3-class) — trial vs v5/v8, keep if smaller. Covers EVERY bwt9 call-site.
#ifndef MZIP_NO_CM
    std::vector<uint8_t> cm_local;
    if (!cm_pre) { cm_local = cmbk::compress_bwt(data, n); cm_pre = &cm_local; }
    const std::vector<uint8_t>& cm = *cm_pre;
    // `!have_legacy` matters now: with no legacy arm output is EMPTY, so the old
    // size comparison alone would reject a perfectly good CM stream.
    if (!cm.empty() && (!have_legacy || cm.size() + 3 < output.size())) {
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
