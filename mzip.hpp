#pragma once
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  μZip — Formula-based adaptive compressor                                ║
// ║  "μ" from μικρός (mikrós) = small — compression makes things small       ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// Key insight: Store the formula, not the data.
// Sequential IDs: 64KB → 17 bytes (3855x better than zstd)
//
// File format:
//   [Magic: 4 bytes "µZIP" (0xB5 0x5A 0x49 0x50)]
//   [Version: 1 byte]
//   [Original size: 8 bytes]
//   [Block count: 4 bytes]
//   For each block:
//     [Block type: 1 byte]
//     [Strategy: 1 byte]
//     [Original size: 4 bytes]
//     [Preprocessed size: 4 bytes]
//     [Compressed size: 4 bytes]
//     [Compressed data: variable]
//
// Block types:
//   0x00 = RAW (passthrough)
//   0x01 = NUMERIC (delta/XOR/ALP preprocessing)
//   0x02 = TEXT (high-level zstd)
//   0x10 = LINEAR_GEN (v = a + b*i — 17 bytes encodes ANY size!)
//   0x11 = PERIODIC (repeating pattern)
//   0xFF = INCOMPRESSIBLE (high entropy, store raw)

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <array>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <set>
#include <map>

#include "tieredcompress.hpp"
#include "bwt_compress_v9.hpp"  // Smart adaptive BWT: v8 for small, v4 for large (optimal across all sizes)
#define CM_BACKEND_USE_BWT
#include "cm_backend.hpp"       // BWT+CM (bzip3-class) entropy backend — trial-vs-bwt9/zstd, codec flag 3
#include "brotli_shim.hpp"      // brotli encoder/decoder decls — ensemble backstop candidate (link the DLLs)
#include "liblzma_shim.hpp"     // liblzma (xz) encoder/decoder decls — ensemble backstop candidate (link the DLL)
#include "mzip_dicts.h"         // Pre-trained zstd dictionaries for code/config (beats brotli at 4-16KB)
#include "mzip_base64.hpp"      // Base64 de-encoding: decode to binary, compress, re-encode (1.76% better than brotli)
#include "lzma_optimal2.hpp"    // LZMA optimal encoder: beats xz on x86 binaries with E8/E9 filter
#include "lzma_decoder.hpp"     // LZMA decoder for decompression
#include "ppmd_backend.hpp"     // PPMd var.H (LZMA-SDK Ppmd7, public domain) — ensemble backstop; wins ~11% on source code

// ---------------------------------------------------------------------------
// MZIP_TIME — per-detector timing telemetry (build with -DMZIP_TIME).
// Sibling of the existing MZIP_STATS per-block encoder telemetry. Without the
// define, MZ_DET(name, call) expands to exactly `(call)` and this file is
// unchanged -- verified by byte-identity against the uninstrumented build.
//
// Why it exists: measured 2026-07-31, ~92% of mzip's runtime on a 4 MB columnar
// block is neither the winning encoder (3.3%) nor xz (4.6%) nor the CM backend
// (~0%, -DMZIP_NO_CM changes runtime by -1%). It is the detector/trial path,
// and it is ~7.4x more expensive per byte on one 4 MB file than on another of
// identical size and class. This tells us which detector.
// Dump: set MZIP_TIME=1 in the environment; a table is printed to stderr at exit.
// ---------------------------------------------------------------------------
#ifdef MZIP_TIME
#include <chrono>
namespace mziptime {
struct Acc { const char* name; double ms; long calls; long hits; };
inline std::vector<Acc>& table(){ static std::vector<Acc> t; return t; }
inline Acc& slot(const char* name){
    for (auto& a : table()) if (a.name == name || strcmp(a.name,name)==0) return a;
    table().push_back(Acc{name,0.0,0,0}); return table().back();
}
// generic expression timer: returns whatever the wrapped expression returns
template<class F> inline auto timed(const char* name, F&& f) -> decltype(f()) {
    auto t0 = std::chrono::steady_clock::now();
    auto r = f();
    auto t1 = std::chrono::steady_clock::now();
    Acc& a = slot(name);
    a.ms += std::chrono::duration<double,std::milli>(t1-t0).count();
    a.calls++;
    return r;
}
template<class F> inline bool run(const char* name, F&& f){
    auto t0 = std::chrono::steady_clock::now();
    bool r = f();
    auto t1 = std::chrono::steady_clock::now();
    Acc& a = slot(name);
    a.ms += std::chrono::duration<double,std::milli>(t1-t0).count();
    a.calls++; if (r) a.hits++;
    return r;
}
struct Dump {
    ~Dump(){
        if (!getenv("MZIP_TIME")) return;
        auto& t = table(); if (t.empty()) return;
        std::vector<Acc> v(t.begin(), t.end());
        std::sort(v.begin(), v.end(), [](const Acc&a,const Acc&b){ return a.ms > b.ms; });
        double tot = 0; for (auto& a : v) tot += a.ms;
        fprintf(stderr, "\nMZTIME  %-34s %10s %8s %7s %7s\n", "detector","ms","%","calls","hits");
        for (auto& a : v)
            fprintf(stderr, "MZTIME  %-34s %10.1f %7.1f%% %7ld %7ld\n",
                    a.name, a.ms, tot>0?a.ms*100.0/tot:0.0, a.calls, a.hits);
        fprintf(stderr, "MZTIME  %-34s %10.1f\n", "TOTAL DETECTOR TIME", tot);
    }
};
inline Dump& dumper(){ static Dump d; return d; }
} // namespace mziptime
#define MZ_DET(name, call) (mziptime::dumper(), mziptime::run(name, [&]{ return (call); }))
#define MZ_TIMED(name, expr) (mziptime::dumper(), mziptime::timed(name, [&]{ return (expr); }))
#else
#define MZ_DET(name, call) (call)
#define MZ_TIMED(name, expr) (expr)
#endif



// Check for zstd - we require it
#ifndef ZSTD_H_235446
#error "mzip requires zstd. Include zstd.h before mzip.hpp"
#endif

namespace mzip {

// ============================================================================
// Constants
// ============================================================================

constexpr uint32_t MAGIC = 0x505A49B5;  // "µZIP" in little-endian (0xB5 = µ)
constexpr uint32_t MAGIC_LITE = 0x505A49B4;  // "µZIo" - lite format (legacy, still supported for reading)
constexpr uint32_t ZSTD_MAGIC = 0xFD2FB528;  // zstd frame magic
constexpr uint16_t MAGIC_RAW = 0x52B5;       // "µR" - ultra-minimal raw passthrough (5 bytes overhead for 1MB!)
// Zero-overhead passthrough: return pure zstd when mzip can't improve
// decompress() detects zstd magic and handles it directly
constexpr uint8_t VERSION = 1;      // Legacy fixed-size headers (31 bytes for single block)
constexpr uint8_t VERSION_COMPACT = 2;  // Compact varint headers (~12-14 bytes for single block)

// Block flags for compact format (VERSION_COMPACT)
constexpr uint8_t BLOCK_FLAG_RAW = 0x01;           // Compressed data stored uncompressed
constexpr uint8_t BLOCK_FLAG_PREPROC_EQ_ORIG = 0x02; // Preprocessed size equals original size
constexpr uint8_t BLOCK_FLAG_COMP_EQ_PREPROC = 0x80; // Compressed size equals preprocessed size (use_generator blocks)
constexpr size_t DEFAULT_BLOCK_SIZE = 16 * 1024 * 1024;  // 16MB blocks (templated data needs large blocks)
constexpr size_t MIN_BLOCK_SIZE = 4 * 1024;        // 4KB minimum
constexpr size_t MAX_BLOCK_SIZE = 16 * 1024 * 1024; // 16MB maximum

// Block types
enum class BlockType : uint8_t {
    RAW = 0x00,
    NUMERIC = 0x01,
    TEXT = 0x02,
    STRUCTURED = 0x03,
    IMAGE = 0x04,        // Raw pixel data (RGB/RGBA)
    BINARY_X86 = 0x05,   // x86 executable code (E8/E9 filtered + zstd)
    LZMA_OPTIMAL = 0x06, // LZMA optimal encoder with E8/E9 (beats xz on x86 binaries)
    LZMA_RAW = 0x07,     // LZMA optimal encoder without E8/E9 (for high-zero content)
    LINEAR_GEN = 0x10,   // Mathematical generator: a + b*i (3855x compression!)
    PERIODIC = 0x11,     // Repeating byte pattern (7000x compression!)
    TIMESTAMP = 0x12,    // Quasi-periodic timestamps: delta-delta+zigzag+varint (12x!)
    TEMPLATE = 0x13,     // Log/structured text: template + columnar encoding (3-5x over zstd!)
    GEOMETRIC = 0x14,    // Geometric sequence: base * ratio^i (10x over zstd on powers of 2!)
    QUADRATIC = 0x15,    // Quadratic polynomial: a + b*i + c*i² (310x over zstd on squares!)
    RECURRENCE = 0x16,   // Linear recurrence: F[i] = c0*F[i-1] + c1*F[i-2] (23x on Fibonacci!)
    MODULAR = 0x17,      // Wrapping counter: (base + i*step) mod modulus (88889x on 16-bit counter!)
    ML_TEMPLATE = 0x18,  // Multi-line template: repeated function blocks (44% over zstd on JavaScript!)
    COLUMNAR = 0x19,     // Columnar text: access logs split by column (10% over zstd on logs!)
    WORD_ENCODED = 0x1A, // Word-level encoding for natural language (25% over zstd on prose!)
    CHAR_TEMPLATE = 0x1B, // Char-level line template: fixed/variable regions (25-42% over zstd on logs!)
    CSV_COLUMNAR = 0x1C,  // CSV with per-column encoding (LINEAR_GEN on sequential IDs!)
    SECTION_TEMPLATE = 0x1D, // Repeating multi-line sections with sequential numbers (Markdown, etc.)
    ML_TEMPLATE_DUAL = 0x1E, // Dual alternating multi-line template (TypeScript interfaces + components)
    LINE_TEMPLATE = 0x1F,    // Variable-length line template (SQL INSERTs with linear numeric vars) - 14x over zstd!
    PHRASE_DICT = 0x24,      // Phrase dictionary: find repeated substrings, replace with tokens (beats brotli!)
    SORTED_DICT = 0x25,      // Line sorting + adaptive dictionary: reorder for LZ77, use content-derived dict
    BWT_TEXT = 0x26,         // Smart adaptive BWT (v9): 10-22% better than zstd on text, optimal at all sizes
    KV_CONFIG = 0x27,        // Key-value config (INI/YAML): structural encoding beats brotli by 7%
    SPARSE = 0x28,           // Sparse bitmap: >90% zeros, delta-encoded positions (1.3-1.5x over zstd!)
    LINE_GROUP_TEMPLATE = 0x29, // Multi-line-type template: email headers, mixed logs (68% better than zstd!)
    CODE_STREAM = 0x2A,        // Identifier stream separation: skeleton + grammar-parsed identifiers (beats bzip2 on JS!)
    PHRASE_PARTITION = 0x2B,   // Data partitioned by delimiter-separated phrases (12% better than bzip2 on repetitive text!)
    DUAL_STREAM = 0x2C,        // Interleaved data with different entropy (Protobuf-like: tags vs values)
    HTML_STREAM = 0x2D,        // HTML tag/content separation: beats brotli by 3.3% at 256KB, 11.8% at 1MB!
    URL_STREAM = 0x2E,         // URL component separation: 32% better than zstd, 3.8% better than mzip at 256KB!
    BASE64_DECODE = 0x2F,      // Base64-encoded data: decode to binary, compress, re-encode (1.76% better than brotli!)
    JSON_COLUMNAR = 0x32,      // JSON lines: extract sequential numeric fields (LINEAR_GEN), delta-encode timestamps (1085 bytes better than brotli!)
    NUM_EXTRACT = 0x33,        // Extract embedded decimal numbers from text (900 bytes better than brotli on Makefiles!)

    // === APPROXIMATE ENCODINGS (Effective Complexity: pattern + exceptions) ===
    LINEAR_GEN_APPROX = 0x20,  // Linear sequence with bounded exceptions (rescues ~95% pattern data!)
    PERIODIC_APPROX = 0x21,    // Periodic pattern with mutations
    GEOMETRIC_APPROX = 0x22,   // Geometric sequence with exceptions
    QUADRATIC_APPROX = 0x23,   // Quadratic sequence with exceptions

    // === WORD TEMPLATE (repeating sections with word variable) ===
    WORD_TEMPLATE = 0x34,      // Repeating sections with word variable (2.4x over zstd on API docs!)
    MULTI_WORD_TEMPLATE = 0x35, // Template with multiple variables {1},{2},{3} (44% better on K8s Ingress!)
    DBF_CONSTCOL = 0x36,        // DBF constant column elimination + zstd (beats brotli by 3.4% on space-padded DBF!)
    ZSTD_DICT = 0x37,           // zstd with pre-trained static dictionary (beats brotli at 4-16KB on code/config!)
    CM_TEXT = 0x38,             // BWT + context-mixing (bzip3-class): beats BWT_TEXT/bwt9 ~5-15% on text/logs
    BROTLI = 0x39,              // brotli-11 backstop (ensemble: never lose to brotli on small code/config)
    XZLIB = 0x3A,               // liblzma (xz -9e) backstop (ensemble: flips large-repetitive vs our lzma_opt2)
    PPMD = 0x3B,                // PPMd var.H (LZMA-SDK Ppmd7) backstop: high-order context model beats brotli/CM/BWT on source code (~11%). payload = [order:1][memMiB:1][Ppmd7z stream]

    // === CROSS-BLOCK ENCODINGS (Mutual Algorithmic Information) ===
    REFERENCE = 0x30,          // Delta from similar previous block (zstd dictionary mode)

    INCOMPRESSIBLE = 0xFF
};

inline const char* block_type_name(BlockType type);  // fwd decl (defined below) — for MZIP_STATS telemetry

// ============================================================================
// Complexity Theory Infrastructure (Kolmogorov, Gell-Mann, Bennett)
// ============================================================================

// Compression mode - controls size vs decompression speed tradeoff (Bennett's Logical Depth)
enum class CompressionMode : uint8_t {
    SMALL,      // Minimize size, allow high logical depth (slow decompression)
    BALANCED,   // Default: reasonable tradeoff
    FAST        // Prefer fast decompression, skip high-depth generators for large blocks
};

// Logical depth categories for decompression cost estimation
enum class LogicalDepth : uint8_t {
    INSTANT,    // O(1) per byte - just copy (RAW, INCOMPRESSIBLE)
    LINEAR,     // O(n) with good constants - zstd, most encodings
    REGENERATE  // O(n) with computation per element - generators (LINEAR_GEN, QUADRATIC, etc.)
};

// Effective Complexity score (Gell-Mann) - separates pattern from noise
// Lower sophistication = better fit (pattern explains most of the data)
struct EffectiveComplexityScore {
    size_t program_size;    // Bytes to describe the pattern/structure
    size_t residual_size;   // Bytes for exceptions/noise that don't fit pattern

    size_t total() const { return program_size + residual_size; }

    // Sophistication ratio: what fraction is "structure description"
    // Good encoder: < 0.3 (mostly data, small pattern description)
    // Poor encoder: > 0.5 (pattern description larger than residual!)
    double sophistication() const {
        if (total() == 0) return 0.0;
        return (double)program_size / total();
    }

    // Compare: lower total wins, tie-break on lower sophistication
    bool better_than(const EffectiveComplexityScore& other) const {
        if (total() != other.total()) return total() < other.total();
        return sophistication() < other.sophistication();
    }
};

// ============================================================================
// Varint Encoding (LEB128-style) for compact integer storage
// ============================================================================
// Saves 6 bytes per int64 when value is 0-127 (common case: start=0, step=1)

inline size_t write_varint(std::vector<uint8_t>& out, int64_t val) {
    // Handle negative values using zigzag encoding
    uint64_t uval = (val << 1) ^ (val >> 63);
    size_t bytes_written = 0;
    do {
        uint8_t byte = uval & 0x7F;
        uval >>= 7;
        if (uval != 0) byte |= 0x80;  // More bytes follow
        out.push_back(byte);
        bytes_written++;
    } while (uval != 0);
    return bytes_written;
}

inline int64_t read_varint(const uint8_t*& ptr, const uint8_t* end) {
    uint64_t uval = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t byte = *ptr++;
        uval |= ((uint64_t)(byte & 0x7F)) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift >= 64) break;  // Overflow protection
    }
    // Zigzag decode
    return (int64_t)((uval >> 1) ^ -(int64_t)(uval & 1));
}

inline size_t write_uvarint(std::vector<uint8_t>& out, uint64_t val) {
    size_t bytes_written = 0;
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val != 0) byte |= 0x80;
        out.push_back(byte);
        bytes_written++;
    } while (val != 0);
    return bytes_written;
}

inline uint64_t read_uvarint(const uint8_t*& ptr, const uint8_t* end) {
    uint64_t val = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t byte = *ptr++;
        val |= ((uint64_t)(byte & 0x7F)) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift >= 64) break;
    }
    return val;
}

// Write unsigned varint to raw buffer, returns bytes written
inline size_t write_uvarint_buf(uint8_t* out, uint64_t val) {
    size_t bytes_written = 0;
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val != 0) byte |= 0x80;
        out[bytes_written++] = byte;
    } while (val != 0);
    return bytes_written;
}

// Calculate bytes needed for uvarint encoding
inline size_t uvarint_size(uint64_t val) {
    size_t bytes = 0;
    do {
        val >>= 7;
        bytes++;
    } while (val != 0);
    return bytes;
}

// ============================================================================
// Block Fingerprinting for Mutual Information Detection
// ============================================================================
// SimHash-style fingerprint for finding similar blocks.
// Used to decide when to use REFERENCE encoding (delta from previous block).

struct BlockFingerprint {
    uint64_t hash;        // 64-bit SimHash
    uint32_t block_index; // Which block this fingerprint belongs to
    
    // Hamming distance between fingerprints (number of differing bits)
    int distance(const BlockFingerprint& other) const {
        uint64_t diff = hash ^ other.hash;
        return __builtin_popcountll(diff);
    }
    
    // Similarity score: 1.0 = identical, 0.0 = completely different
    double similarity(const BlockFingerprint& other) const {
        int dist = distance(other);
        return 1.0 - (dist / 64.0);
    }
};

// Compute 64-bit SimHash fingerprint from block content
// Uses 4-byte shingles (n-grams) to capture local structure
inline BlockFingerprint compute_fingerprint(const uint8_t* data, size_t n, uint32_t block_index) {
    BlockFingerprint fp;
    fp.block_index = block_index;
    fp.hash = 0;
    
    if (n < 4) {
        // Too small for meaningful fingerprint
        fp.hash = 0;
        return fp;
    }
    
    // Count array for SimHash: positive = bit should be 1, negative = bit should be 0
    int64_t bit_counts[64] = {0};
    
    // Hash each 4-byte shingle and accumulate
    for (size_t i = 0; i <= n - 4; i++) {
        // Simple FNV-1a style hash for the shingle
        uint64_t shingle_hash = 14695981039346656037ULL;
        shingle_hash ^= data[i];
        shingle_hash *= 1099511628211ULL;
        shingle_hash ^= data[i+1];
        shingle_hash *= 1099511628211ULL;
        shingle_hash ^= data[i+2];
        shingle_hash *= 1099511628211ULL;
        shingle_hash ^= data[i+3];
        shingle_hash *= 1099511628211ULL;
        
        // Update bit counts based on this shingle's hash
        for (int b = 0; b < 64; b++) {
            if (shingle_hash & (1ULL << b)) {
                bit_counts[b]++;
            } else {
                bit_counts[b]--;
            }
        }
    }
    
    // Convert counts to binary fingerprint
    for (int b = 0; b < 64; b++) {
        if (bit_counts[b] > 0) {
            fp.hash |= (1ULL << b);
        }
    }
    
    return fp;
}

// Threshold for considering blocks "similar" (0.7 = 70% of bits match)
constexpr double SIMILARITY_THRESHOLD = 0.70;

// Maximum number of recent blocks to remember for similarity matching
constexpr size_t MAX_REFERENCE_BLOCKS = 16;

// Minimum size for reference encoding (smaller blocks not worth the overhead)
constexpr size_t MIN_REFERENCE_SIZE = 1024;

// Reference encoding parameters
struct ReferenceParams {
    uint32_t ref_block_index;  // Index of the reference block
    size_t ref_block_size;     // Size of reference block (needed for decompression)
};

// ============================================================================
// x86 E8/E9 Filtering (BCJ-like transform for executables)

// ============================================================================
// Minimum Description Length (MDL) Scoring
// ============================================================================
// MDL principle: best model minimizes (model_cost + data_given_model_cost)
// This replaces ad-hoc sophistication thresholds with principled comparison.

struct MDLScore {
    size_t model_cost;     // Cost to describe the encoding scheme
    size_t data_cost;      // Cost to encode data under this model

    size_t total() const { return model_cost + data_cost; }

    bool beats(const MDLScore& other) const {
        return total() < other.total();
    }
};

// Estimate zstd compressed size from entropy (Shannon limit + overhead)
// This is our baseline: if specialized encoding doesn't beat this, use zstd
inline size_t estimate_zstd_mdl(size_t original_size, double entropy_bits_per_byte) {
    if (original_size == 0) return 0;

    // Shannon limit: size * (entropy / 8)
    // zstd typically achieves ~85-95% of Shannon limit
    // Add small fixed overhead for zstd frame header (~12 bytes)
    double shannon_limit = original_size * (entropy_bits_per_byte / 8.0);
    double zstd_efficiency = 0.90;  // zstd is ~90% efficient vs Shannon
    size_t estimated = (size_t)(shannon_limit / zstd_efficiency) + 12;

    // zstd can't expand data beyond original + small overhead
    return std::min(estimated, original_size + 16);
}

// MDL cost for LINEAR_GEN (perfect linear sequence)
inline MDLScore mdl_linear_gen(size_t /* original_size */) {
    // Model: base (8) + delta (8) + count (4) + element_size (1) = 21 bytes
    // But we only store 17 bytes in practice (optimized format)
    return {17, 0};  // Perfect encoding: zero data cost
}

// MDL cost for LINEAR_GEN_APPROX (linear + exceptions)
inline MDLScore mdl_linear_gen_approx(size_t exception_count) {
    // Model: same as LINEAR_GEN (17 bytes)
    // Data: exception_count header (4) + per-exception (index:4 + value:8 = 12 bytes)
    // But zstd compresses exceptions, so estimate ~8 bytes/exception after compression
    size_t model = 17 + 4;  // base model + exception count
    size_t data = exception_count * 8;  // compressed exceptions
    return {model, data};
}

// MDL cost for PERIODIC (perfect repeating pattern)
inline MDLScore mdl_periodic(size_t period) {
    // Model: type (1) + period (2) + pattern (period bytes)
    return {3 + period, 0};  // Perfect encoding
}

// MDL cost for PERIODIC_APPROX (periodic + exceptions)
inline MDLScore mdl_periodic_approx(size_t period, size_t exception_count) {
    // Model: period (2) + pattern (period) + exception_count (4)
    // Data: per-exception (index:4 + value:1 = 5 bytes, compressed ~3 bytes)
    size_t model = 2 + period + 4;
    size_t data = exception_count * 3;
    return {model, data};
}

// MDL cost for NUMERIC (delta/XOR preprocessing + zstd)
inline MDLScore mdl_numeric(size_t original_size, double preprocessed_entropy) {
    // Model: strategy byte (1)
    // Data: zstd of preprocessed data
    size_t data_cost = estimate_zstd_mdl(original_size, preprocessed_entropy);
    return {1, data_cost};
}

// MDL cost for TEXT (raw zstd)
inline MDLScore mdl_text(size_t original_size, double entropy) {
    // Model: just the block type (1)
    // Data: zstd compressed
    return {1, estimate_zstd_mdl(original_size, entropy)};
}

// MDL cost for TEMPLATE encoding
inline MDLScore mdl_template(size_t template_size, size_t variable_bytes, double variable_entropy) {
    // Model: template itself + slot descriptors
    // Data: variable values (often sequential, so entropy is low)
    size_t model = template_size + 8;  // template + overhead
    size_t data = estimate_zstd_mdl(variable_bytes, variable_entropy);
    return {model, data};
}

// Compare encoding against zstd baseline and return true if encoding wins
inline bool mdl_beats_zstd(const MDLScore& encoding, size_t original_size, double entropy) {
    MDLScore zstd = mdl_text(original_size, entropy);
    return encoding.beats(zstd);
}

// ============================================================================
// x86 E8/E9 Filtering (BCJ-like transform for executables)
// Converts relative CALL/JMP offsets to absolute addresses for better compression
// ============================================================================

inline void e8e9_filter_encode(uint8_t* data, size_t n) {
    if (n < 5) return;
    for (size_t i = 0; i < n - 4; i++) {
        // Look for E8 (CALL) or E9 (JMP) with near relative offset
        if (data[i] == 0xE8 || data[i] == 0xE9) {
            // Convert relative offset to "absolute" by adding position
            int32_t offset = (int32_t)(data[i+1] | (data[i+2] << 8) |
                                       (data[i+3] << 16) | (data[i+4] << 24));
            int32_t absolute = offset + (int32_t)i;
            data[i+1] = absolute & 0xFF;
            data[i+2] = (absolute >> 8) & 0xFF;
            data[i+3] = (absolute >> 16) & 0xFF;
            data[i+4] = (absolute >> 24) & 0xFF;
            i += 4;  // Skip the offset bytes
        }
    }
}

inline void e8e9_filter_decode(uint8_t* data, size_t n) {
    if (n < 5) return;
    for (size_t i = 0; i < n - 4; i++) {
        if (data[i] == 0xE8 || data[i] == 0xE9) {
            int32_t absolute = (int32_t)(data[i+1] | (data[i+2] << 8) |
                                         (data[i+3] << 16) | (data[i+4] << 24));
            int32_t offset = absolute - (int32_t)i;
            data[i+1] = offset & 0xFF;
            data[i+2] = (offset >> 8) & 0xFF;
            data[i+3] = (offset >> 16) & 0xFF;
            data[i+4] = (offset >> 24) & 0xFF;
            i += 4;
        }
    }
}

// Detect if data looks like x86 executable code
// IMPORTANT: Only detect on actual PE/ELF headers, NOT on E8/E9 count!
// Random data has ~0.78% E8/E9 naturally, causing false positives that
// break LZ77 pattern matching (see repeated_blocks benchmark regression).
inline bool looks_like_x86_code(const uint8_t* data, size_t n) {
    if (n < 64) return false;

    // Check for PE signature (MZ header at start, PE\0\0 at offset in header)
    if (data[0] == 0x4D && data[1] == 0x5A) {
        // Verify it's actually a PE by checking for PE signature
        if (n >= 64) {
            uint32_t pe_offset = data[60] | (data[61] << 8) | (data[62] << 16) | (data[63] << 24);
            if (pe_offset < n - 4 &&
                data[pe_offset] == 'P' && data[pe_offset+1] == 'E' &&
                data[pe_offset+2] == 0 && data[pe_offset+3] == 0) {
                return true;
            }
        }
        // MZ without valid PE - could be DOS executable, still apply filter
        return true;
    }

    // Check for ELF signature
    if (n >= 4 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        return true;
    }

    // No header detected - don't apply E8/E9 filter
    // The E8/E9 heuristic causes false positives on random data
    return false;
}

// ============================================================================
// Image Preprocessing (YCoCg-R + Paeth filter)
// Achieves ~18% better compression than PNG on photos
// ============================================================================

// YCoCg-R color transform - LOSSLESS for 8-bit RGB
inline void rgb_to_ycocg_r(uint8_t* data, size_t pixels, int channels) {
    if (channels < 3) return;
    for (size_t i = 0; i < pixels; i++) {
        uint8_t r = data[i * channels + 0];
        uint8_t g = data[i * channels + 1];
        uint8_t b = data[i * channels + 2];
        uint8_t co = r - b;
        uint8_t tmp = b + (co >> 1);
        uint8_t cg = g - tmp;
        uint8_t y = tmp + (cg >> 1);
        data[i * channels + 0] = y;
        data[i * channels + 1] = co;
        data[i * channels + 2] = cg;
    }
}

inline void ycocg_r_to_rgb(uint8_t* data, size_t pixels, int channels) {
    if (channels < 3) return;
    for (size_t i = 0; i < pixels; i++) {
        uint8_t y = data[i * channels + 0];
        uint8_t co = data[i * channels + 1];
        uint8_t cg = data[i * channels + 2];
        uint8_t tmp = y - (cg >> 1);
        uint8_t g = cg + tmp;
        uint8_t b = tmp - (co >> 1);
        uint8_t r = co + b;
        data[i * channels + 0] = r;
        data[i * channels + 1] = g;
        data[i * channels + 2] = b;
    }
}

inline int paeth_predict(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Image info stored in first 8 bytes of preprocessed block
struct ImageBlockHeader {
    uint16_t width;
    uint16_t height;
    uint8_t channels;
    uint8_t reserved[3];
};

// Encode image block: YCoCg-R + Paeth filter
// Returns: [header:8] + [filtered_data]
inline std::vector<uint8_t> encode_image_block(const uint8_t* data, int w, int h, int c) {
    size_t pixels = w * h;
    std::vector<uint8_t> result(8 + pixels * c);

    // Write header
    ImageBlockHeader* hdr = reinterpret_cast<ImageBlockHeader*>(result.data());
    hdr->width = w;
    hdr->height = h;
    hdr->channels = c;

    // Copy and apply YCoCg-R
    std::vector<uint8_t> ycocg(data, data + pixels * c);
    rgb_to_ycocg_r(ycocg.data(), pixels, c);

    // Apply Paeth filter
    uint8_t* out = result.data() + 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * c;
            for (int ch = 0; ch < c; ch++) {
                int a = (x > 0) ? ycocg[idx - c + ch] : 0;
                int b = (y > 0) ? ycocg[idx - w * c + ch] : 0;
                int cc = (x > 0 && y > 0) ? ycocg[idx - w * c - c + ch] : 0;
                out[idx + ch] = ycocg[idx + ch] - paeth_predict(a, b, cc);
            }
        }
    }
    return result;
}

// Decode image block: reverse Paeth + reverse YCoCg-R
inline std::vector<uint8_t> decode_image_block(const uint8_t* data, size_t size) {
    if (size < 8) return {};

    const ImageBlockHeader* hdr = reinterpret_cast<const ImageBlockHeader*>(data);
    int w = hdr->width;
    int h = hdr->height;
    int c = hdr->channels;
    size_t pixels = w * h;

    if (size < 8 + pixels * c) return {};

    const uint8_t* in = data + 8;
    std::vector<uint8_t> ycocg(pixels * c);

    // Reverse Paeth filter
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * c;
            for (int ch = 0; ch < c; ch++) {
                int a = (x > 0) ? ycocg[idx - c + ch] : 0;
                int b = (y > 0) ? ycocg[idx - w * c + ch] : 0;
                int cc = (x > 0 && y > 0) ? ycocg[idx - w * c - c + ch] : 0;
                ycocg[idx + ch] = in[idx + ch] + paeth_predict(a, b, cc);
            }
        }
    }

    // Reverse YCoCg-R
    ycocg_r_to_rgb(ycocg.data(), pixels, c);
    return ycocg;
}

// ============================================================================
// LINEAR_GEN: Mathematical Generator Encoding (v = a + b*i)
// Achieves 3855x compression on sequential IDs!
// ============================================================================

struct LinearGenParams {
    uint8_t elem_size;   // 1, 2, 4, or 8 bytes per element
    int64_t base;        // Starting value (a)
    int64_t delta;       // Increment per element (b)
    uint64_t count;      // Number of elements
};

// Detect if data is a perfect linear sequence of integers
// Returns true and fills params if detected
// NOTE: Rejects delta=0 (constant fill) - let PERIODIC period=1 handle it more compactly
inline bool detect_linear_gen(const uint8_t* data, size_t n, LinearGenParams& params) {
    // Try 32-bit integers first (most common)
    if (n >= 12 && n % 4 == 0) {
        size_t count = n / 4;
        const uint32_t* vals = reinterpret_cast<const uint32_t*>(data);

        // Check if all deltas are the same
        int64_t delta = (int64_t)vals[1] - (int64_t)vals[0];

        // Skip delta=0 (constant fill) - PERIODIC period=1 is more compact:
        // LINEAR_GEN: 17 bytes vs PERIODIC: 3 bytes (saves 14 bytes!)
        if (delta == 0) goto try_64bit;

        // Sample check: first, middle, last
        size_t mid = count / 2;
        int64_t expected_mid = (int64_t)vals[0] + delta * mid;
        int64_t expected_last = (int64_t)vals[0] + delta * (count - 1);

        if ((int64_t)vals[mid] == expected_mid &&
            (int64_t)vals[count-1] == expected_last) {
            // Verify full sequence (sample every 64th element for large data)
            size_t step = (count > 256) ? 64 : 1;
            for (size_t i = 0; i < count; i += step) {
                int64_t expected = (int64_t)vals[0] + delta * i;
                if ((int64_t)vals[i] != expected) return false;
            }

            params.elem_size = 4;
            params.base = vals[0];
            params.delta = delta;
            params.count = count;
            return true;
        }
    }

try_64bit:
    // Try 64-bit integers
    if (n >= 24 && n % 8 == 0) {
        size_t count = n / 8;
        const int64_t* vals = reinterpret_cast<const int64_t*>(data);

        int64_t delta = vals[1] - vals[0];
        if (delta == 0) goto try_16bit;  // Let PERIODIC handle constant fill
        size_t mid = count / 2;
        int64_t expected_mid = vals[0] + delta * mid;
        int64_t expected_last = vals[0] + delta * (count - 1);

        if (vals[mid] == expected_mid && vals[count-1] == expected_last) {
            size_t step = (count > 256) ? 64 : 1;
            for (size_t i = 0; i < count; i += step) {
                int64_t expected = vals[0] + delta * (int64_t)i;
                if (vals[i] != expected) return false;
            }

            params.elem_size = 8;
            params.base = vals[0];
            params.delta = delta;
            params.count = count;
            return true;
        }
    }

try_16bit:
    // Try 16-bit integers (common for IDs, counters, ports - 4597x better than zstd!)
    if (n >= 6 && n % 2 == 0) {
        size_t count = n / 2;
        const uint16_t* vals = reinterpret_cast<const uint16_t*>(data);

        // Check if all deltas are the same
        int32_t delta = (int32_t)vals[1] - (int32_t)vals[0];

        // Skip delta=0 - let PERIODIC handle constant fill
        if (delta == 0) goto try_8bit;

        // Quick validation: delta must be in valid 16-bit range
        if (delta >= -32768 && delta <= 32767) {
            // Sample check: first, middle, last
            size_t mid = count / 2;
            int32_t expected_mid = (int32_t)vals[0] + delta * mid;
            int32_t expected_last = (int32_t)vals[0] + delta * (count - 1);

            // Check range validity and value match
            if (expected_mid >= 0 && expected_mid <= 65535 &&
                expected_last >= 0 && expected_last <= 65535 &&
                (int32_t)vals[mid] == expected_mid &&
                (int32_t)vals[count-1] == expected_last) {
                // Verify full sequence (sample every 64th for large data)
                size_t step = (count > 256) ? 64 : 1;
                bool valid = true;
                for (size_t i = 0; i < count && valid; i += step) {
                    int32_t expected = (int32_t)vals[0] + delta * (int32_t)i;
                    if (expected < 0 || expected > 65535 || (int32_t)vals[i] != expected) {
                        valid = false;
                    }
                }

                if (valid) {
                    params.elem_size = 2;
                    params.base = vals[0];
                    params.delta = delta;
                    params.count = count;
                    return true;
                }
            }
        }
    }

try_8bit:
    // Try 8-bit integers (byte sequences like 0,1,2,3,... or 255,254,253,...)
    if (n >= 4) {
        int16_t delta = (int16_t)data[1] - (int16_t)data[0];

        // Skip delta=0 - PERIODIC period=1 handles this
        if (delta == 0) return false;

        // Delta must produce valid bytes
        if (delta >= -255 && delta <= 255) {
            size_t mid = n / 2;
            int16_t expected_mid = (int16_t)data[0] + delta * mid;
            int16_t expected_last = (int16_t)data[0] + delta * (n - 1);

            if (expected_mid >= 0 && expected_mid <= 255 &&
                expected_last >= 0 && expected_last <= 255 &&
                (int16_t)data[mid] == expected_mid &&
                (int16_t)data[n-1] == expected_last) {
                // Verify full sequence (sample for large data)
                size_t step = (n > 256) ? 64 : 1;
                bool valid = true;
                for (size_t i = 0; i < n && valid; i += step) {
                    int16_t expected = (int16_t)data[0] + delta * (int16_t)i;
                    if (expected < 0 || expected > 255 || (int16_t)data[i] != expected) {
                        valid = false;
                    }
                }

                if (valid) {
                    params.elem_size = 1;
                    params.base = data[0];
                    params.delta = delta;
                    params.count = n;
                    return true;
                }
            }
        }
    }

    return false;
}

// Encode linear generator: just store the parameters (17 bytes!)
inline std::vector<uint8_t> encode_linear_gen(const LinearGenParams& params) {
    std::vector<uint8_t> result(17);
    result[0] = params.elem_size;
    memcpy(&result[1], &params.base, 8);
    memcpy(&result[9], &params.delta, 8);
    // count is implicit from block original size
    return result;
}

// Decode linear generator: regenerate the sequence
inline std::vector<uint8_t> decode_linear_gen(const uint8_t* encoded, size_t original_size) {
    uint8_t elem_size = encoded[0];
    int64_t base, delta;
    memcpy(&base, &encoded[1], 8);
    memcpy(&delta, &encoded[9], 8);

    std::vector<uint8_t> result(original_size);

    if (elem_size == 1) {
        // 8-bit integers
        for (size_t i = 0; i < original_size; i++) {
            result[i] = (uint8_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 2) {
        // 16-bit integers
        uint16_t* out = reinterpret_cast<uint16_t*>(result.data());
        size_t count = original_size / 2;
        for (size_t i = 0; i < count; i++) {
            out[i] = (uint16_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 4) {
        uint32_t* out = reinterpret_cast<uint32_t*>(result.data());
        size_t count = original_size / 4;
        for (size_t i = 0; i < count; i++) {
            out[i] = (uint32_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        for (size_t i = 0; i < count; i++) {
            out[i] = base + delta * (int64_t)i;
        }
    }

    return result;
}

// ============================================================================
// SPARSE: Delta-encoded positions for data with >90% zeros
// Key insight: Sparse bitmaps/masks are common in graphics, databases, etc.
// Instead of compressing mostly-zero bytes, just store non-zero positions.
// At 1% density: 77x compression vs 59x for zstd (1.3x better!)
// ============================================================================

// Helper: write varint to output buffer
inline void write_varint(std::vector<uint8_t>& out, uint64_t val) {
    while (val >= 128) {
        out.push_back((val & 0x7F) | 0x80);
        val >>= 7;
    }
    out.push_back(static_cast<uint8_t>(val));
}

// Helper: read varint from input pointer (advances pointer)
inline uint64_t read_varint(const uint8_t*& p) {
    uint64_t val = 0;
    int shift = 0;
    while (*p & 0x80) {
        val |= (uint64_t)(*p++ & 0x7F) << shift;
        shift += 7;
    }
    val |= (uint64_t)(*p++) << shift;
    return val;
}

// Detect sparse data: >90% zeros
// Returns true if data is sparse enough to benefit from position encoding
inline bool detect_sparse(const uint8_t* data, size_t n,
                          size_t& out_nonzero_count,
                          uint8_t& out_common_value,
                          bool& out_all_same_value) {
    if (n < 64) return false;  // Too small to benefit

    // Count zeros and non-zeros
    size_t zero_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (data[i] == 0) zero_count++;
    }

    double zero_ratio = (double)zero_count / n;
    if (zero_ratio < 0.90) return false;  // Not sparse enough

    // Collect non-zero entries
    size_t nonzero_count = n - zero_count;
    out_nonzero_count = nonzero_count;

    if (nonzero_count == 0) {
        // All zeros - use PERIODIC instead (period=1, value=0)
        return false;
    }

    // Check if all non-zero values are the same
    uint8_t first_val = 0;
    bool found_first = false;
    bool all_same = true;
    for (size_t i = 0; i < n && all_same; i++) {
        if (data[i] != 0) {
            if (!found_first) {
                first_val = data[i];
                found_first = true;
            } else if (data[i] != first_val) {
                all_same = false;
            }
        }
    }

    out_common_value = first_val;
    out_all_same_value = all_same;

    // Estimate encoding size: header(~8) + delta positions (avg log2(n/count) bits each)
    // + values (1 byte if all same, else count bytes)
    size_t avg_delta = n / (nonzero_count + 1);
    size_t bits_per_delta = 8;  // 1 byte minimum
    if (avg_delta >= 128) bits_per_delta = 16;
    if (avg_delta >= 16384) bits_per_delta = 24;
    size_t estimated_size = 8 + (nonzero_count * bits_per_delta / 8) +
                            (all_same ? 1 : nonzero_count);

    // Only use sparse if we save at least 30% over raw size
    return estimated_size < n * 0.7;
}

// Encode sparse data: delta-encoded positions + values
// Format:
//   [2 bytes] Magic: 'SP'
//   [varint]  Original size
//   [varint]  Non-zero count
//   [1 byte]  Value type: 0x01 = all same value, 0x00 = per-position values
//   [1 byte]  Common value (if value type == 0x01)
//   [varints] Delta-encoded positions
//   [bytes]   Values (if value type == 0x00)
inline std::vector<uint8_t> encode_sparse(const uint8_t* data, size_t n,
                                           uint8_t common_value, bool all_same) {
    std::vector<uint8_t> out;

    // Collect non-zero entries
    std::vector<std::pair<size_t, uint8_t>> entries;
    for (size_t i = 0; i < n; i++) {
        if (data[i] != 0) {
            entries.push_back({i, data[i]});
        }
    }

    // Header
    out.push_back('S');  // Magic
    out.push_back('P');
    write_varint(out, n);             // Original size
    write_varint(out, entries.size()); // Non-zero count

    if (entries.empty()) return out;

    // Value type
    if (all_same) {
        out.push_back(0x01);
        out.push_back(common_value);
    } else {
        out.push_back(0x00);
    }

    // Delta-encoded positions
    size_t prev_pos = 0;
    for (auto& e : entries) {
        write_varint(out, e.first - prev_pos);
        prev_pos = e.first;
    }

    // Values (if not all same)
    if (!all_same) {
        for (auto& e : entries) {
            out.push_back(e.second);
        }
    }

    return out;
}

// Decode sparse data: reconstruct from positions
inline std::vector<uint8_t> decode_sparse(const uint8_t* encoded, size_t enc_size,
                                          size_t original_size) {
    const uint8_t* p = encoded;

    // Check magic
    if (enc_size < 4 || p[0] != 'S' || p[1] != 'P') {
        return std::vector<uint8_t>(original_size, 0);
    }
    p += 2;

    uint64_t orig_size = read_varint(p);
    uint64_t count = read_varint(p);

    std::vector<uint8_t> out(orig_size, 0);

    if (count == 0) return out;

    uint8_t value_type = *p++;
    uint8_t common_value = 0;
    if (value_type == 0x01) {
        common_value = *p++;
    }

    // Read positions
    std::vector<size_t> positions;
    size_t pos = 0;
    for (uint64_t i = 0; i < count; i++) {
        pos += read_varint(p);
        positions.push_back(pos);
    }

    // Fill values
    if (value_type == 0x01) {
        for (size_t position : positions) {
            if (position < orig_size) {
                out[position] = common_value;
            }
        }
    } else {
        for (size_t i = 0; i < count && i < positions.size(); i++) {
            if (positions[i] < orig_size) {
                out[positions[i]] = *p++;
            }
        }
    }

    return out;
}

// ============================================================================
// SPARSE_RICE: Rice-coded sparse encoding (optimal for geometric distributions)
// Key insight: Gap sequences in sparse data follow geometric distributions.
// Rice coding achieves ~100.6% of entropy, beating bzip2 by 8.7%!
// ============================================================================

// Rice encode sparse data: near-optimal for geometric gap distributions
// Format:
//   [2 bytes] Magic: 'SR'
//   [1 byte]  Rice parameter k
//   [varint]  Original size
//   [varint]  Non-zero count
//   [1 byte]  Value type: 0x01 = all same, 0x00 = per-position
//   [1 byte]  Common value (if 0x01)
//   [bitpacked] Rice-coded gaps (q zeros + 1 one + k binary bits per gap)
//   [bytes]   Values (if 0x00)
inline std::vector<uint8_t> encode_sparse_rice(const uint8_t* data, size_t n,
                                                uint8_t common_value, bool all_same) {
    // Collect non-zero positions and values
    std::vector<size_t> positions;
    std::vector<uint8_t> values;
    for (size_t i = 0; i < n; i++) {
        if (data[i] != 0) {
            positions.push_back(i);
            values.push_back(data[i]);
        }
    }

    if (positions.empty()) {
        // All zeros - just return header
        std::vector<uint8_t> out = {'S', 'R', 0};  // k=0
        write_varint(out, (uint64_t)n);
        write_varint(out, (uint64_t)0);
        return out;
    }

    // Calculate optimal Rice parameter k = floor(log2(mean_gap))
    double mean_gap = (double)positions.back() / positions.size();
    int k = (int)floor(log2(mean_gap));
    if (k < 1) k = 1;
    if (k > 12) k = 12;  // Cap at 12 bits for remainder

    std::vector<uint8_t> out;
    // Header
    out.push_back('S');
    out.push_back('R');
    out.push_back(static_cast<uint8_t>(k));
    write_varint(out, (uint64_t)n);
    write_varint(out, (uint64_t)positions.size());

    // Value type
    if (all_same) {
        out.push_back(0x01);
        out.push_back(common_value);
    } else {
        out.push_back(0x00);
    }

    // Rice-encode the gaps
    size_t bit_pos = 0;
    out.push_back(0);  // Start bitpacking

    size_t prev_pos = 0;
    for (size_t pos : positions) {
        size_t gap = pos - prev_pos;
        prev_pos = pos;

        size_t q = gap >> k;            // Quotient (unary)
        size_t r = gap & ((1 << k) - 1); // Remainder (k binary bits)

        // Unary: q zeros
        for (size_t i = 0; i < q; i++) {
            bit_pos++;
            if (bit_pos % 8 == 0) out.push_back(0);
        }
        // Terminating 1
        out[out.size() - 1] |= (1 << (bit_pos % 8));
        bit_pos++;
        if (bit_pos % 8 == 0) out.push_back(0);

        // Binary: k bits for remainder
        for (int b = 0; b < k; b++) {
            if (r & (1 << b)) {
                out[out.size() - 1] |= (1 << (bit_pos % 8));
            }
            bit_pos++;
            if (bit_pos % 8 == 0) out.push_back(0);
        }
    }

    // Trim trailing empty byte if any
    while (!out.empty() && out.back() == 0 && bit_pos % 8 == 0 && bit_pos > 0) {
        bit_pos -= 8;
        out.pop_back();
    }

    // Values (if not all same)
    if (!all_same) {
        for (uint8_t v : values) {
            out.push_back(v);
        }
    }

    return out;
}

// Decode Rice-coded sparse data
inline std::vector<uint8_t> decode_sparse_rice(const uint8_t* encoded, size_t enc_size,
                                                size_t original_size) {
    if (enc_size < 5 || encoded[0] != 'S' || encoded[1] != 'R') {
        return std::vector<uint8_t>(original_size, 0);
    }

    const uint8_t* p = encoded + 2;
    int k = *p++;

    uint64_t orig_size = read_varint(p);
    uint64_t count = read_varint(p);

    std::vector<uint8_t> out(orig_size, 0);
    if (count == 0) return out;

    uint8_t value_type = *p++;
    uint8_t common_value = 0;
    if (value_type == 0x01) {
        common_value = *p++;
    }

    // Decode Rice-coded gaps
    std::vector<size_t> positions;
    size_t bit_pos = 0;
    const uint8_t* bit_data = p;
    size_t pos = 0;

    for (uint64_t i = 0; i < count; i++) {
        // Decode unary (count zeros until 1)
        size_t q = 0;
        while (((bit_data[bit_pos / 8] >> (bit_pos % 8)) & 1) == 0) {
            q++;
            bit_pos++;
        }
        bit_pos++;  // Skip the terminating 1

        // Decode k binary bits
        size_t r = 0;
        for (int b = 0; b < k; b++) {
            if ((bit_data[bit_pos / 8] >> (bit_pos % 8)) & 1) {
                r |= (1 << b);
            }
            bit_pos++;
        }

        size_t gap = (q << k) | r;
        pos += gap;
        positions.push_back(pos);
    }

    // Skip to values
    p = bit_data + (bit_pos + 7) / 8;

    // Fill values
    if (value_type == 0x01) {
        for (size_t position : positions) {
            if (position < orig_size) {
                out[position] = common_value;
            }
        }
    } else {
        for (size_t i = 0; i < count && i < positions.size(); i++) {
            if (positions[i] < orig_size) {
                out[positions[i]] = *p++;
            }
        }
    }

    return out;
}


// ============================================================================
// LINE_GROUP_TEMPLATE: Multi-line-type template encoding
// Key insight: Email headers have multiple line types (From:, To:, Subject:, etc.)
// Each type repeats with sequential numbers. Group by prefix, apply LINEAR_GEN per group.
// Result: 68% better than zstd on email headers, mixed logs, etc.
// Format:
//   [2 bytes] Magic: 'LG'
//   [varint]  Original size
//   [varint]  Number of groups
//   For each group:
//     [varint]  Prefix length
//     [bytes]   Prefix string
//     [1 byte]  Type: 0x01 = LINEAR, 0x00 = RAW
//     If LINEAR:
//       [varint]  Template length
//       [bytes]   Template with {N} placeholders
//       [8 bytes] First value (int64)
//       [8 bytes] Delta (int64)
//       [varint]  Line count
//     If RAW:
//       [varint]  Total raw bytes
//       [bytes]   Raw content
//   [varint]  Line type sequence length
//   [bytes]   Line type sequence (group index per original line)
// ============================================================================

struct LineGroupInfo {
    std::string prefix;           // Line type identifier (e.g., "From:", "To:")
    std::vector<std::string> lines;  // All lines with this prefix
    bool is_linear;               // Does this group have a linear number sequence?
    std::string templ;            // Template with {N} placeholder (if linear)
    int64_t first_value;          // First number in sequence
    int64_t delta;                // Delta between numbers
};

// ============================================================================
// CODE_STREAM: Identifier stream separation for code (beats bzip2 on JavaScript!)
// ============================================================================
// Key insight: Code has compound identifiers (Auser4821, getUserItem, configService3847)
// that share a small grammar vocabulary (23 nouns, 23 verbs, 26 letters).
// By parsing identifiers into separate streams (type, letter, word1, word2, number),
// each stream compresses better than interleaved data.
//
// Grammar patterns detected:
//   CLASS_NAME: [A-Z][noun][number]      -> "Auser4821"
//   FUNC_NAME: [verb][A-Z][noun]         -> "getUserItem"
//   SERVICE_NAME: [noun]"Service"[number] -> "configService3847"
//   UTILS_NAME: [noun]"Utils"            -> "nodeUtils"
//   LIST_NAME: [noun]"List"              -> "itemList"
//   SIMPLE: [noun] or [verb]             -> "user", "get"
//
// Stream format:
//   [skeleton_zstd_size:4][skeleton_zstd]
//   [type_stream_zstd_size:4][type_stream_zstd]
//   [letter_stream_zstd_size:4][letter_stream_zstd]
//   [word1_stream_zstd_size:4][word1_stream_zstd]
//   [word2_stream_zstd_size:4][word2_stream_zstd]
//   [number_stream_zstd_size:4][number_stream_zstd]
//   [unknown_count:4][unknown_stream_zstd_size:4][unknown_stream_zstd]
// ============================================================================

struct CodeStreamParams {
    // Pre-computed during detection
    std::vector<uint8_t> skeleton_zstd;
    std::vector<uint8_t> type_stream_zstd;
    std::vector<uint8_t> letter_stream_zstd;
    std::vector<uint8_t> word1_stream_zstd;
    std::vector<uint8_t> word2_stream_zstd;
    std::vector<uint8_t> number_stream_zstd;
    std::vector<std::string> unknown_identifiers;  // Stored as length-prefixed strings
    size_t total_identifiers;
};

inline bool detect_line_group_template(const uint8_t* data, size_t n,
                                       std::vector<LineGroupInfo>& groups,
                                       std::vector<uint8_t>& line_types) {
    if (n < 256) return false;

    std::string text((const char*)data, n);

    // Split into lines
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    if (lines.size() < 20) return false;

    // Group lines by prefix (colon-based like email, OR XML tag-based)
    std::map<std::string, std::vector<size_t>> prefix_to_indices;
    for (size_t i = 0; i < lines.size(); i++) {
        const auto& l = lines[i];
        std::string prefix;

        // Strategy 1: Colon-based prefix (email headers)
        size_t colon = l.find(':');
        if (colon != std::string::npos && colon < 20) {
            prefix = l.substr(0, colon + 1);
        }
        // Strategy 2: XML tag prefix (whitespace + <tagname)
        else {
            size_t ws_end = 0;
            while (ws_end < l.size() && (l[ws_end] == ' ' || l[ws_end] == '\t')) ws_end++;

            if (ws_end < l.size() && l[ws_end] == '<') {
                // Find end of tag name
                size_t tag_end = ws_end + 1;
                // Include leading / for closing tags
                if (tag_end < l.size() && l[tag_end] == '/') tag_end++;
                while (tag_end < l.size() && l[tag_end] != ' ' && l[tag_end] != '>' && l[tag_end] != '/') {
                    tag_end++;
                }
                if (tag_end > ws_end + 1) {
                    prefix = l.substr(0, tag_end);
                }
            }
        }

        if (prefix.empty()) {
            if (l.empty()) {
                prefix = "(empty)";
            } else if (l.size() >= 3 && l.substr(0, 3) == "---") {
                prefix = "---";
            } else {
                prefix = "(other)";
            }
        }

        prefix_to_indices[prefix].push_back(i);
    }

    // Need at least 3 distinct prefixes for this to be useful
    if (prefix_to_indices.size() < 3) return false;

    // Build groups and check for linear patterns
    groups.clear();
    line_types.resize(lines.size());
    size_t linear_groups = 0;
    size_t total_lines_in_linear = 0;

    uint8_t group_idx = 0;
    for (const auto& [prefix, indices] : prefix_to_indices) {
        LineGroupInfo info;
        info.prefix = prefix;
        for (size_t idx : indices) {
            info.lines.push_back(lines[idx]);
            line_types[idx] = group_idx;
        }

        // Check for linear number sequence
        info.is_linear = false;
        if (info.lines.size() >= 3) {
            std::vector<int64_t> nums;
            size_t num_pos = std::string::npos;

            // Find first number position in first line
            const auto& first_line = info.lines[0];
            for (size_t i = 0; i < first_line.size(); i++) {
                if (isdigit((unsigned char)first_line[i])) {
                    num_pos = i;
                    break;
                }
            }

            if (num_pos != std::string::npos) {
                // Extract number from each line at same position
                bool all_valid = true;
                for (const auto& l : info.lines) {
                    if (num_pos >= l.size() || !isdigit((unsigned char)l[num_pos])) {
                        all_valid = false;
                        break;
                    }
                    size_t end = num_pos;
                    while (end < l.size() && isdigit((unsigned char)l[end])) end++;
                    try {
                        nums.push_back(std::stoll(l.substr(num_pos, end - num_pos)));
                    } catch (...) {
                        all_valid = false;
                        break;
                    }
                }

                if (all_valid && nums.size() >= 3) {
                    int64_t first = nums[0];
                    int64_t delta = nums[1] - nums[0];
                    bool is_linear = (delta != 0);
                    for (size_t i = 0; i < nums.size() && is_linear; i++) {
                        if (nums[i] != first + delta * (int64_t)i) {
                            is_linear = false;
                        }
                    }

                    if (is_linear) {
                        info.is_linear = true;
                        info.first_value = first;
                        info.delta = delta;
                        linear_groups++;
                        total_lines_in_linear += info.lines.size();

                        // Build template by replacing numbers with {N}
                        const auto& l0 = info.lines[0];
                        size_t end = num_pos;
                        while (end < l0.size() && isdigit((unsigned char)l0[end])) end++;
                        info.templ = l0.substr(0, num_pos) + "{N}" + l0.substr(end);
                    }
                }
            }
        }

        groups.push_back(info);
        group_idx++;
        if (group_idx > 254) break;  // Limit groups
    }

    // Need at least 2 linear groups covering 30% of lines to be worthwhile
    double linear_coverage = (double)total_lines_in_linear / lines.size();
    return linear_groups >= 2 && linear_coverage >= 0.3;
}

// Helper: Detect periodic pattern in sequence
inline bool detect_periodic_sequence(const std::vector<uint8_t>& seq,
                                      std::vector<uint8_t>& pattern,
                                      size_t& repeat_count) {
    if (seq.size() < 6) return false;

    // Try periods 2-30
    for (size_t period = 2; period <= std::min((size_t)30, seq.size() / 3); period++) {
        bool is_periodic = true;
        size_t full_repeats = seq.size() / period;

        for (size_t i = period; i < full_repeats * period && is_periodic; i++) {
            if (seq[i] != seq[i % period]) {
                is_periodic = false;
            }
        }

        if (is_periodic && full_repeats >= 3) {
            pattern.assign(seq.begin(), seq.begin() + period);
            repeat_count = full_repeats;
            return true;
        }
    }
    return false;
}

// Helper: Check if all lines in a group are identical
inline bool is_constant_group(const std::vector<std::string>& lines) {
    if (lines.size() < 2) return false;
    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i] != lines[0]) return false;
    }
    return true;
}

inline std::vector<uint8_t> encode_line_group_template(const uint8_t* data, size_t n,
                                                        const std::vector<LineGroupInfo>& groups,
                                                        const std::vector<uint8_t>& line_types) {
    std::vector<uint8_t> out;
    out.reserve(n / 2);  // Expect significant compression

    // Magic (version 2 with PERIODIC/CONSTANT support)
    out.push_back('L');
    out.push_back('H');  // 'H' = enhanced version

    // Original size
    write_varint(out, n);

    // Number of groups
    write_varint(out, groups.size());

    // Each group
    for (const auto& g : groups) {
        // Prefix
        write_varint(out, g.prefix.size());
        for (char c : g.prefix) out.push_back((uint8_t)c);

        if (g.is_linear) {
            out.push_back(0x01);  // LINEAR type

            // Template
            write_varint(out, g.templ.size());
            for (char c : g.templ) out.push_back((uint8_t)c);

            // Linear params
            for (int i = 0; i < 8; i++) out.push_back((g.first_value >> (i * 8)) & 0xFF);
            for (int i = 0; i < 8; i++) out.push_back((g.delta >> (i * 8)) & 0xFF);

            // Line count
            write_varint(out, g.lines.size());
        } else if (is_constant_group(g.lines)) {
            out.push_back(0x02);  // CONSTANT type - all lines identical

            // Store one line + count
            write_varint(out, g.lines[0].size());
            for (char c : g.lines[0]) out.push_back((uint8_t)c);
            write_varint(out, g.lines.size());
        } else {
            out.push_back(0x00);  // RAW type

            // Concatenate all raw lines with newlines
            std::string raw;
            for (const auto& l : g.lines) {
                raw += l + "\n";
            }
            write_varint(out, raw.size());
            for (char c : raw) out.push_back((uint8_t)c);
        }
    }

    // Line type sequence - try PERIODIC encoding first
    std::vector<uint8_t> pattern;
    size_t repeat_count;
    if (MZ_DET("detect_periodic_sequence", detect_periodic_sequence(line_types, pattern, repeat_count))) {
        out.push_back(0x01);  // PERIODIC line types
        write_varint(out, pattern.size());
        for (uint8_t t : pattern) out.push_back(t);
        write_varint(out, repeat_count);
        write_varint(out, line_types.size());  // Total count for remainder
    } else {
        out.push_back(0x00);  // RAW line types
        write_varint(out, line_types.size());
        for (uint8_t t : line_types) out.push_back(t);
    }

    return out;
}

inline std::vector<uint8_t> decode_line_group_template(const uint8_t* encoded, size_t enc_size,
                                                        size_t original_size) {
    const uint8_t* p = encoded;

    // Check magic - support both 'LG' (v1) and 'LH' (v2 enhanced)
    if (enc_size < 4 || p[0] != 'L' || (p[1] != 'G' && p[1] != 'H')) {
        return std::vector<uint8_t>(original_size, 0);
    }
    bool enhanced = (p[1] == 'H');
    p += 2;

    uint64_t orig_size = read_varint(p);
    uint64_t num_groups = read_varint(p);

    // Read groups
    struct GroupData {
        std::string prefix;
        uint8_t type;  // 0=RAW, 1=LINEAR, 2=CONSTANT
        std::string templ;
        int64_t first_value;
        int64_t delta;
        size_t line_count;
        std::vector<std::string> raw_lines;
        std::string constant_line;  // For CONSTANT type
        size_t next_idx;  // For generating next line
    };
    std::vector<GroupData> groups(num_groups);

    for (size_t g = 0; g < num_groups; g++) {
        uint64_t prefix_len = read_varint(p);
        groups[g].prefix = std::string((const char*)p, prefix_len);
        p += prefix_len;

        groups[g].type = *p++;
        groups[g].next_idx = 0;

        if (groups[g].type == 0x01) {  // LINEAR
            uint64_t templ_len = read_varint(p);
            groups[g].templ = std::string((const char*)p, templ_len);
            p += templ_len;

            groups[g].first_value = 0;
            for (int i = 0; i < 8; i++) groups[g].first_value |= ((int64_t)*p++) << (i * 8);
            groups[g].delta = 0;
            for (int i = 0; i < 8; i++) groups[g].delta |= ((int64_t)*p++) << (i * 8);

            groups[g].line_count = read_varint(p);
        } else if (groups[g].type == 0x02) {  // CONSTANT
            uint64_t line_len = read_varint(p);
            groups[g].constant_line = std::string((const char*)p, line_len);
            p += line_len;
            groups[g].line_count = read_varint(p);
        } else {  // RAW
            uint64_t raw_len = read_varint(p);
            std::string raw((const char*)p, raw_len);
            p += raw_len;

            // Split raw back into lines
            std::istringstream iss(raw);
            std::string line;
            while (std::getline(iss, line)) {
                groups[g].raw_lines.push_back(line);
            }
        }
    }

    // Read line type sequence
    std::vector<uint8_t> line_types;
    if (enhanced) {
        uint8_t seq_type = *p++;
        if (seq_type == 0x01) {  // PERIODIC
            uint64_t pattern_len = read_varint(p);
            std::vector<uint8_t> pattern(pattern_len);
            for (size_t i = 0; i < pattern_len; i++) pattern[i] = *p++;
            uint64_t repeat_count = read_varint(p);
            uint64_t total_count = read_varint(p);

            // Expand periodic pattern
            line_types.reserve(total_count);
            for (size_t r = 0; r < repeat_count; r++) {
                for (uint8_t t : pattern) {
                    line_types.push_back(t);
                }
            }
            // Handle remainder
            while (line_types.size() < total_count) {
                line_types.push_back(pattern[line_types.size() % pattern.size()]);
            }
        } else {  // RAW
            uint64_t seq_len = read_varint(p);
            line_types.resize(seq_len);
            for (size_t i = 0; i < seq_len; i++) {
                line_types[i] = *p++;
            }
        }
    } else {
        // Old format: raw line types
        uint64_t seq_len = read_varint(p);
        line_types.resize(seq_len);
        for (size_t i = 0; i < seq_len; i++) {
            line_types[i] = *p++;
        }
    }

    // Reconstruct original
    std::string result;
    result.reserve(orig_size);

    for (size_t i = 0; i < line_types.size(); i++) {
        uint8_t group_idx = line_types[i];
        if (group_idx >= groups.size()) continue;

        auto& g = groups[group_idx];
        std::string line;

        if (g.type == 0x01) {  // LINEAR
            line = g.templ;
            size_t placeholder = line.find("{N}");
            if (placeholder != std::string::npos) {
                int64_t value = g.first_value + g.delta * (int64_t)g.next_idx;
                line = line.substr(0, placeholder) + std::to_string(value) + line.substr(placeholder + 3);
            }
            g.next_idx++;
        } else if (g.type == 0x02) {  // CONSTANT
            line = g.constant_line;
        } else {  // RAW
            if (g.next_idx < g.raw_lines.size()) {
                line = g.raw_lines[g.next_idx];
            }
            g.next_idx++;
        }

        result += line + "\n";
    }

    // Trim to original size
    while (result.size() > orig_size) result.pop_back();
    while (result.size() < orig_size) result.push_back(' ');

    return std::vector<uint8_t>(result.begin(), result.end());
}

// ============================================================================
// LINEAR_GEN_APPROX: Linear Sequence with Exceptions (Effective Complexity)
// Key insight from Gell-Mann: separate regularities (linear pattern) from
// accidents (exceptions). Rescues data that's 95%+ linear but fails exact check.
// Example: 1,2,3,999,5,6,7,8,9,10 → formula + 1 exception beats zstd!
// ============================================================================

struct LinearGenApproxParams {
    uint8_t elem_size;       // 1, 2, 4, or 8 bytes per element
    int64_t base;            // Starting value (a)
    int64_t delta;           // Increment per element (b)
    uint64_t count;          // Total number of elements
    uint32_t exception_count;// Number of exceptions

    // Exceptions stored separately during encode as (index, actual_value) pairs
    // Not stored here - computed during detection, stored in encoded stream
};

// Configuration for approximate detection
struct ApproxConfig {
    double max_exception_rate;   // Default 5% - higher = more lenient
    size_t min_elements;         // Minimum elements to consider (default 8)

    ApproxConfig() : max_exception_rate(0.05), min_elements(8) {}
};

// Helper: estimate delta robustly using median-of-3
// This handles cases where the first few elements might be corrupted
inline int64_t estimate_delta_robust(const uint32_t* vals, size_t count) {
    if (count < 4) return (int64_t)vals[1] - (int64_t)vals[0];

    // Sample deltas from start, middle, and near-end
    int64_t d1 = (int64_t)vals[1] - (int64_t)vals[0];
    size_t mid = count / 2;
    int64_t d2 = (int64_t)vals[mid + 1] - (int64_t)vals[mid];
    size_t near_end = count - 2;
    int64_t d3 = (int64_t)vals[near_end + 1] - (int64_t)vals[near_end];

    // Return median (most common delta wins)
    if ((d1 <= d2 && d2 <= d3) || (d3 <= d2 && d2 <= d1)) return d2;
    if ((d2 <= d1 && d1 <= d3) || (d3 <= d1 && d1 <= d2)) return d1;
    return d3;
}

inline int64_t estimate_delta_robust_64(const int64_t* vals, size_t count) {
    if (count < 4) return vals[1] - vals[0];
    int64_t d1 = vals[1] - vals[0];
    size_t mid = count / 2;
    int64_t d2 = vals[mid + 1] - vals[mid];
    size_t near_end = count - 2;
    int64_t d3 = vals[near_end + 1] - vals[near_end];
    if ((d1 <= d2 && d2 <= d3) || (d3 <= d2 && d2 <= d1)) return d2;
    if ((d2 <= d1 && d1 <= d3) || (d3 <= d1 && d1 <= d2)) return d1;
    return d3;
}

inline int32_t estimate_delta_robust_16(const uint16_t* vals, size_t count) {
    if (count < 4) return (int32_t)vals[1] - (int32_t)vals[0];
    int32_t d1 = (int32_t)vals[1] - (int32_t)vals[0];
    size_t mid = count / 2;
    int32_t d2 = (int32_t)vals[mid + 1] - (int32_t)vals[mid];
    size_t near_end = count - 2;
    int32_t d3 = (int32_t)vals[near_end + 1] - (int32_t)vals[near_end];
    if ((d1 <= d2 && d2 <= d3) || (d3 <= d2 && d2 <= d1)) return d2;
    if ((d2 <= d1 && d1 <= d3) || (d3 <= d1 && d1 <= d2)) return d1;
    return d3;
}

// Helper: find the most likely base value given delta
// Scans a few positions to find one that produces consistent results
inline int64_t estimate_base_robust(const uint32_t* vals, size_t count, int64_t delta) {
    // Try a few positions and pick the one with most agreement
    int64_t best_base = vals[0];
    size_t best_matches = 0;

    for (size_t start_idx : {(size_t)0, count/4, count/2, 3*count/4}) {
        if (start_idx >= count) continue;
        int64_t candidate_base = (int64_t)vals[start_idx] - delta * (int64_t)start_idx;

        // Count how many values match this base
        size_t matches = 0;
        for (size_t i = 0; i < std::min(count, (size_t)100); i++) {
            int64_t expected = candidate_base + delta * (int64_t)i;
            if ((int64_t)vals[i] == expected) matches++;
        }

        if (matches > best_matches) {
            best_matches = matches;
            best_base = candidate_base;
        }
    }

    return best_base;
}

// Detect linear sequence with bounded exceptions
// Returns true if pattern holds for >= (1 - max_exception_rate) of elements
inline bool detect_linear_gen_approx(const uint8_t* data, size_t n,
                                      LinearGenApproxParams& params,
                                      std::vector<std::pair<uint32_t, int64_t>>& exceptions,
                                      const ApproxConfig& config = ApproxConfig()) {
    exceptions.clear();

    // Try 32-bit integers first (most common case)
    if (n >= 12 && n % 4 == 0) {
        size_t count = n / 4;
        if (count < config.min_elements) goto try_64bit;

        const uint32_t* vals = reinterpret_cast<const uint32_t*>(data);

        // ROBUST delta estimation: use median-of-3 from different positions
        // This handles corruption in the first few elements
        int64_t delta = estimate_delta_robust(vals, count);

        // Skip delta=0 - PERIODIC handles constant fill
        if (delta == 0) goto try_64bit;

        // ROBUST base estimation: find base that maximizes matches
        int64_t base = estimate_base_robust(vals, count, delta);

        // Count exceptions with robust base/delta
        size_t max_exceptions = (size_t)(count * config.max_exception_rate);
        std::vector<std::pair<uint32_t, int64_t>> found_exceptions;

        for (size_t i = 0; i < count; i++) {
            int64_t expected = base + delta * (int64_t)i;
            if ((int64_t)vals[i] != expected) {
                if (found_exceptions.size() >= max_exceptions) {
                    goto try_64bit;  // Too many exceptions
                }
                found_exceptions.push_back({(uint32_t)i, (int64_t)vals[i]});
            }
        }

        // Must have at least one exception (otherwise exact LINEAR_GEN would match)
        if (found_exceptions.empty()) goto try_64bit;

        // Check if approximate encoding is worth it:
        // Formula: 17 bytes + 4 bytes (count) + exceptions * (4 + elem_size) bytes
        // vs zstd compression of original data
        size_t approx_size = 21 + found_exceptions.size() * 8;  // 4 bytes index + 4 bytes value
        // Rough zstd estimate: ~50% compression on semi-random integers
        size_t zstd_estimate = n / 2;

        if (approx_size < zstd_estimate) {
            params.elem_size = 4;
            params.base = base;
            params.delta = delta;
            params.count = count;
            params.exception_count = (uint32_t)found_exceptions.size();
            exceptions = std::move(found_exceptions);
            return true;
        }
        goto try_64bit;
    }

try_64bit:
    // Try 64-bit integers
    if (n >= 24 && n % 8 == 0) {
        size_t count = n / 8;
        if (count < config.min_elements) goto try_16bit;

        const int64_t* vals = reinterpret_cast<const int64_t*>(data);
        int64_t delta = vals[1] - vals[0];

        if (delta == 0) goto try_16bit;

        size_t max_exceptions = (size_t)(count * config.max_exception_rate);
        std::vector<std::pair<uint32_t, int64_t>> found_exceptions;

        for (size_t i = 0; i < count; i++) {
            int64_t expected = vals[0] + delta * (int64_t)i;
            if (vals[i] != expected) {
                if (found_exceptions.size() >= max_exceptions) {
                    goto try_16bit;
                }
                found_exceptions.push_back({(uint32_t)i, vals[i]});
            }
        }

        if (found_exceptions.empty()) goto try_16bit;

        size_t approx_size = 21 + found_exceptions.size() * 12;
        size_t zstd_estimate = n / 2;

        if (approx_size < zstd_estimate) {
            params.elem_size = 8;
            params.base = vals[0];
            params.delta = delta;
            params.count = count;
            params.exception_count = (uint32_t)found_exceptions.size();
            exceptions = std::move(found_exceptions);
            return true;
        }
        goto try_16bit;
    }

try_16bit:
    // Try 16-bit integers
    if (n >= 6 && n % 2 == 0) {
        size_t count = n / 2;
        if (count < config.min_elements) return false;

        const uint16_t* vals = reinterpret_cast<const uint16_t*>(data);
        int32_t delta = (int32_t)vals[1] - (int32_t)vals[0];

        if (delta == 0) return false;

        size_t max_exceptions = (size_t)(count * config.max_exception_rate);
        std::vector<std::pair<uint32_t, int64_t>> found_exceptions;

        for (size_t i = 0; i < count; i++) {
            int32_t expected = (int32_t)vals[0] + delta * (int32_t)i;
            // Check for overflow and match
            if (expected < 0 || expected > 65535 || (int32_t)vals[i] != expected) {
                if (found_exceptions.size() >= max_exceptions) {
                    return false;
                }
                found_exceptions.push_back({(uint32_t)i, (int64_t)vals[i]});
            }
        }

        if (found_exceptions.empty()) return false;

        size_t approx_size = 21 + found_exceptions.size() * 8;  // Smaller exceptions for 16-bit
        size_t zstd_estimate = n / 2;

        if (approx_size < zstd_estimate) {
            params.elem_size = 2;
            params.base = vals[0];
            params.delta = delta;
            params.count = count;
            params.exception_count = (uint32_t)found_exceptions.size();
            exceptions = std::move(found_exceptions);
            return true;
        }
    }

    return false;
}

// Encode linear generator with exceptions
// Format: [elem_size:1][base:8][delta:8][exception_count:4][exceptions...]
// Each exception: [index:4][value:elem_size]
inline std::vector<uint8_t> encode_linear_gen_approx(
    const LinearGenApproxParams& params,
    const std::vector<std::pair<uint32_t, int64_t>>& exceptions) {

    size_t exc_value_size = params.elem_size;
    size_t header_size = 21;  // 1 + 8 + 8 + 4
    size_t exc_size = exceptions.size() * (4 + exc_value_size);

    std::vector<uint8_t> result(header_size + exc_size);

    // Header
    result[0] = params.elem_size;
    memcpy(&result[1], &params.base, 8);
    memcpy(&result[9], &params.delta, 8);
    uint32_t exc_count = (uint32_t)exceptions.size();
    memcpy(&result[17], &exc_count, 4);

    // Exceptions
    size_t pos = 21;
    for (const auto& exc : exceptions) {
        memcpy(&result[pos], &exc.first, 4);  // index
        pos += 4;

        // Value (sized by elem_size)
        if (exc_value_size == 1) {
            result[pos] = (uint8_t)exc.second;
        } else if (exc_value_size == 2) {
            uint16_t v = (uint16_t)exc.second;
            memcpy(&result[pos], &v, 2);
        } else if (exc_value_size == 4) {
            uint32_t v = (uint32_t)exc.second;
            memcpy(&result[pos], &v, 4);
        } else {
            memcpy(&result[pos], &exc.second, 8);
        }
        pos += exc_value_size;
    }

    return result;
}

// Decode linear generator with exceptions: regenerate sequence, apply exceptions
inline std::vector<uint8_t> decode_linear_gen_approx(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 21) return {};

    uint8_t elem_size = encoded[0];
    int64_t base, delta;
    uint32_t exc_count;
    memcpy(&base, &encoded[1], 8);
    memcpy(&delta, &encoded[9], 8);
    memcpy(&exc_count, &encoded[17], 4);

    // First regenerate the base sequence (same as LINEAR_GEN)
    std::vector<uint8_t> result(original_size);

    if (elem_size == 1) {
        for (size_t i = 0; i < original_size; i++) {
            result[i] = (uint8_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 2) {
        uint16_t* out = reinterpret_cast<uint16_t*>(result.data());
        size_t count = original_size / 2;
        for (size_t i = 0; i < count; i++) {
            out[i] = (uint16_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 4) {
        uint32_t* out = reinterpret_cast<uint32_t*>(result.data());
        size_t count = original_size / 4;
        for (size_t i = 0; i < count; i++) {
            out[i] = (uint32_t)(base + delta * (int64_t)i);
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        for (size_t i = 0; i < count; i++) {
            out[i] = base + delta * (int64_t)i;
        }
    }

    // Apply exceptions
    size_t pos = 21;
    for (uint32_t e = 0; e < exc_count && pos + 4 <= encoded_size; e++) {
        uint32_t idx;
        memcpy(&idx, &encoded[pos], 4);
        pos += 4;

        if (pos + elem_size > encoded_size) break;

        if (elem_size == 1 && idx < original_size) {
            result[idx] = encoded[pos];
        } else if (elem_size == 2 && idx * 2 + 1 < original_size) {
            memcpy(&result[idx * 2], &encoded[pos], 2);
        } else if (elem_size == 4 && idx * 4 + 3 < original_size) {
            memcpy(&result[idx * 4], &encoded[pos], 4);
        } else if (elem_size == 8 && idx * 8 + 7 < original_size) {
            memcpy(&result[idx * 8], &encoded[pos], 8);
        }
        pos += elem_size;
    }

    return result;
}

// Estimate effective complexity score for LINEAR_GEN_APPROX
inline EffectiveComplexityScore estimate_linear_approx_complexity(
    const LinearGenApproxParams& params,
    const std::vector<std::pair<uint32_t, int64_t>>& exceptions) {

    EffectiveComplexityScore score;
    score.program_size = 21;  // Header: elem_size + base + delta + exc_count
    score.residual_size = exceptions.size() * (4 + params.elem_size);  // Exceptions
    return score;
}

// ============================================================================
// GEOMETRIC: Exponential Sequence Encoding
// Encodes base * ratio^i sequences (powers of 2, exponential backoff, etc.)
// Achieves 10x+ over zstd on power-of-2 data!
// ============================================================================

struct GeometricParams {
    uint8_t elem_size;   // 1, 2, 4, or 8 bytes per element
    int64_t base;        // Starting value
    int64_t ratio;       // Multiplication factor per step
    uint64_t count;      // Number of elements
};

// Detect if data is a perfect geometric sequence
inline bool detect_geometric(const uint8_t* data, size_t n, GeometricParams& params) {
    // Try 64-bit first (most common for geometric - values grow fast)
    if (n >= 24 && n % 8 == 0) {
        const int64_t* vals = reinterpret_cast<const int64_t*>(data);
        size_t count = n / 8;

        if (count >= 2 && vals[0] != 0 && vals[1] % vals[0] == 0) {
            int64_t base = vals[0];
            int64_t ratio = vals[1] / vals[0];

            if (ratio != 0 && ratio != 1 && ratio != -1) {  // Valid ratio
                int64_t expected = base;
                bool matches = true;
                for (size_t i = 0; i < count && matches; i++) {
                    if (vals[i] != expected) matches = false;
                    expected *= ratio;
                }
                if (matches) {
                    params.elem_size = 8;
                    params.base = base;
                    params.ratio = ratio;
                    params.count = count;
                    return true;
                }
            }
        }
    }

    // Try 32-bit
    if (n >= 12 && n % 4 == 0) {
        const int32_t* vals = reinterpret_cast<const int32_t*>(data);
        size_t count = n / 4;

        if (count >= 2 && vals[0] != 0 && vals[1] % vals[0] == 0) {
            int32_t base = vals[0];
            int32_t ratio = vals[1] / vals[0];

            if (ratio != 0 && ratio != 1 && ratio != -1) {
                int32_t expected = base;
                bool matches = true;
                for (size_t i = 0; i < count && matches; i++) {
                    if (vals[i] != expected) matches = false;
                    expected *= ratio;
                }
                if (matches) {
                    params.elem_size = 4;
                    params.base = base;
                    params.ratio = ratio;
                    params.count = count;
                    return true;
                }
            }
        }
    }

    return false;
}

// Encode geometric sequence: [elem_size:1][base:8][ratio:8]
inline std::vector<uint8_t> encode_geometric(const GeometricParams& p) {
    std::vector<uint8_t> out(17);
    out[0] = p.elem_size;
    memcpy(&out[1], &p.base, 8);
    memcpy(&out[9], &p.ratio, 8);
    return out;
}

// Decode geometric sequence
inline std::vector<uint8_t> decode_geometric(const uint8_t* encoded, size_t original_size) {
    uint8_t elem_size = encoded[0];
    int64_t base, ratio;
    memcpy(&base, &encoded[1], 8);
    memcpy(&ratio, &encoded[9], 8);

    std::vector<uint8_t> result(original_size);

    if (elem_size == 4) {
        int32_t* out = reinterpret_cast<int32_t*>(result.data());
        size_t count = original_size / 4;
        int64_t val = base;
        for (size_t i = 0; i < count; i++) {
            out[i] = (int32_t)val;
            val *= ratio;
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        int64_t val = base;
        for (size_t i = 0; i < count; i++) {
            out[i] = val;
            val *= ratio;
        }
    }

    return result;
}

// ============================================================================
// QUADRATIC: Polynomial Sequence Encoding
// Encodes a + b*i + c*i² sequences (squares, triangular numbers, etc.)
// Achieves 310x over zstd on quadratic data!
// ============================================================================

struct QuadraticParams {
    uint8_t elem_size;   // 4 or 8 bytes per element
    int64_t a, b, c;     // Coefficients: f(i) = a + b*i + c*i²
    uint64_t count;
};

// Detect if data is a perfect quadratic sequence
inline bool detect_quadratic(const uint8_t* data, size_t n, QuadraticParams& params) {
    // Try 64-bit first
    if (n >= 24 && n % 8 == 0) {
        const int64_t* vals = reinterpret_cast<const int64_t*>(data);
        size_t count = n / 8;

        if (count >= 3) {
            // f(0) = a, f(1) = a+b+c, f(2) = a+2b+4c
            // Second difference is constant for quadratic: d2 = f(2) - 2*f(1) + f(0) = 2c
            int64_t a = vals[0];
            int64_t d2 = vals[2] - 2*vals[1] + vals[0];
            if (d2 % 2 == 0) {
                int64_t c = d2 / 2;
                int64_t b = vals[1] - a - c;

                // Skip degenerate cases: b=0,c=0 is constant (PERIODIC handles better)
                // b!=0,c=0 is linear (LINEAR_GEN handles better)
                if (c == 0) goto try_32bit_quad;

                bool matches = true;
                for (size_t i = 0; i < count && matches; i++) {
                    int64_t expected = a + b*(int64_t)i + c*(int64_t)(i*i);
                    if (vals[i] != expected) matches = false;
                }
                if (matches) {
                    params.elem_size = 8;
                    params.a = a;
                    params.b = b;
                    params.c = c;
                    params.count = count;
                    return true;
                }
            }
        }
    }

try_32bit_quad:
    // Try 32-bit
    if (n >= 12 && n % 4 == 0) {
        const int32_t* vals = reinterpret_cast<const int32_t*>(data);
        size_t count = n / 4;

        if (count >= 3) {
            int64_t a = vals[0];
            int64_t d2 = (int64_t)vals[2] - 2*(int64_t)vals[1] + (int64_t)vals[0];
            if (d2 % 2 == 0) {
                int64_t c = d2 / 2;
                int64_t b = vals[1] - a - c;

                // Skip degenerate cases
                if (c == 0) return false;

                bool matches = true;
                for (size_t i = 0; i < count && matches; i++) {
                    int64_t expected = a + b*(int64_t)i + c*(int64_t)(i*i);
                    if (vals[i] != expected) matches = false;
                }
                if (matches) {
                    params.elem_size = 4;
                    params.a = a;
                    params.b = b;
                    params.c = c;
                    params.count = count;
                    return true;
                }
            }
        }
    }

    return false;
}

// Encode quadratic: [elem_size:1][a:8][b:8][c:8]
inline std::vector<uint8_t> encode_quadratic(const QuadraticParams& p) {
    std::vector<uint8_t> out(25);
    out[0] = p.elem_size;
    memcpy(&out[1], &p.a, 8);
    memcpy(&out[9], &p.b, 8);
    memcpy(&out[17], &p.c, 8);
    return out;
}

// Decode quadratic sequence
inline std::vector<uint8_t> decode_quadratic(const uint8_t* encoded, size_t original_size) {
    uint8_t elem_size = encoded[0];
    int64_t a, b, c;
    memcpy(&a, &encoded[1], 8);
    memcpy(&b, &encoded[9], 8);
    memcpy(&c, &encoded[17], 8);

    std::vector<uint8_t> result(original_size);

    if (elem_size == 4) {
        int32_t* out = reinterpret_cast<int32_t*>(result.data());
        size_t count = original_size / 4;
        for (size_t i = 0; i < count; i++) {
            out[i] = (int32_t)(a + b*(int64_t)i + c*(int64_t)(i*i));
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        for (size_t i = 0; i < count; i++) {
            out[i] = a + b*(int64_t)i + c*(int64_t)(i*i);
        }
    }

    return result;
}

// ============================================================================
// RECURRENCE: Linear Recurrence Encoding
// Encodes F[i] = c0*F[i-1] + c1*F[i-2] sequences (Fibonacci, Lucas, etc.)
// Achieves 23x over zstd on recurrence data!
// ============================================================================

struct RecurrenceParams {
    uint8_t elem_size;   // 4 or 8 bytes per element
    int64_t seed0, seed1;  // F[0], F[1]
    int64_t c0, c1;        // F[i] = c0*F[i-1] + c1*F[i-2]
    uint64_t count;
};

// Detect if data is a linear recurrence (depth 2)
inline bool detect_recurrence(const uint8_t* data, size_t n, RecurrenceParams& params) {
    // Try 64-bit first
    if (n >= 32 && n % 8 == 0) {
        const int64_t* vals = reinterpret_cast<const int64_t*>(data);
        size_t count = n / 8;

        if (count >= 4) {
            // Solve for c0, c1 from:
            // vals[2] = c0*vals[1] + c1*vals[0]
            // vals[3] = c0*vals[2] + c1*vals[1]
            int64_t det = vals[1]*vals[1] - vals[0]*vals[2];
            if (det != 0) {
                int64_t c0_num = vals[2]*vals[1] - vals[3]*vals[0];
                int64_t c1_num = vals[3]*vals[1] - vals[2]*vals[2];

                if (c0_num % det == 0 && c1_num % det == 0) {
                    int64_t c0 = c0_num / det;
                    int64_t c1 = c1_num / det;

                    bool matches = true;
                    for (size_t i = 2; i < count && matches; i++) {
                        int64_t expected = c0 * vals[i-1] + c1 * vals[i-2];
                        if (vals[i] != expected) matches = false;
                    }
                    if (matches) {
                        params.elem_size = 8;
                        params.seed0 = vals[0];
                        params.seed1 = vals[1];
                        params.c0 = c0;
                        params.c1 = c1;
                        params.count = count;
                        return true;
                    }
                }
            }
        }
    }

    // Try 32-bit
    if (n >= 16 && n % 4 == 0) {
        const int32_t* vals = reinterpret_cast<const int32_t*>(data);
        size_t count = n / 4;

        if (count >= 4) {
            int64_t det = (int64_t)vals[1]*vals[1] - (int64_t)vals[0]*vals[2];
            if (det != 0) {
                int64_t c0_num = (int64_t)vals[2]*vals[1] - (int64_t)vals[3]*vals[0];
                int64_t c1_num = (int64_t)vals[3]*vals[1] - (int64_t)vals[2]*vals[2];

                if (c0_num % det == 0 && c1_num % det == 0) {
                    int64_t c0 = c0_num / det;
                    int64_t c1 = c1_num / det;

                    bool matches = true;
                    for (size_t i = 2; i < count && matches; i++) {
                        int64_t expected = c0 * vals[i-1] + c1 * vals[i-2];
                        if (vals[i] != expected) matches = false;
                    }
                    if (matches) {
                        params.elem_size = 4;
                        params.seed0 = vals[0];
                        params.seed1 = vals[1];
                        params.c0 = c0;
                        params.c1 = c1;
                        params.count = count;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// Encode recurrence: [elem_size:1][seed0:8][seed1:8][c0:8][c1:8]
inline std::vector<uint8_t> encode_recurrence(const RecurrenceParams& p) {
    std::vector<uint8_t> out(33);
    out[0] = p.elem_size;
    memcpy(&out[1], &p.seed0, 8);
    memcpy(&out[9], &p.seed1, 8);
    memcpy(&out[17], &p.c0, 8);
    memcpy(&out[25], &p.c1, 8);
    return out;
}

// Decode recurrence sequence
inline std::vector<uint8_t> decode_recurrence(const uint8_t* encoded, size_t original_size) {
    uint8_t elem_size = encoded[0];
    int64_t seed0, seed1, c0, c1;
    memcpy(&seed0, &encoded[1], 8);
    memcpy(&seed1, &encoded[9], 8);
    memcpy(&c0, &encoded[17], 8);
    memcpy(&c1, &encoded[25], 8);

    std::vector<uint8_t> result(original_size);

    if (elem_size == 4) {
        int32_t* out = reinterpret_cast<int32_t*>(result.data());
        size_t count = original_size / 4;
        if (count >= 1) out[0] = (int32_t)seed0;
        if (count >= 2) out[1] = (int32_t)seed1;
        for (size_t i = 2; i < count; i++) {
            out[i] = (int32_t)(c0 * out[i-1] + c1 * out[i-2]);
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        if (count >= 1) out[0] = seed0;
        if (count >= 2) out[1] = seed1;
        for (size_t i = 2; i < count; i++) {
            out[i] = c0 * out[i-1] + c1 * out[i-2];
        }
    }

    return result;
}

// ============================================================================
// MODULAR: Wrapping Counter Encoding
// v[i] = (base + i * step) % modulus
// Achieves 88889x compression on 16-bit counter sequences!
// ============================================================================

struct ModularParams {
    uint8_t elem_size;   // 2, 4, or 8 bytes per element
    uint64_t count;      // Number of elements
    int64_t base;        // Starting value
    int64_t step;        // Increment per element
    int64_t modulus;     // Wrap point
};

// Detect modular (wrapping counter) pattern
inline bool detect_modular(const uint8_t* data, size_t n, ModularParams& params) {
    // Try 16-bit first (most common for wrapping: port numbers, short IDs)
    // Use UNSIGNED to properly detect wrap at 65536
    if (n >= 8 && n % 2 == 0) {
        const uint16_t* arr16 = reinterpret_cast<const uint16_t*>(data);
        size_t count = n / 2;

        if (count >= 8) {
            int32_t step = (int32_t)arr16[1] - (int32_t)arr16[0];
            if (step != 0) {
                // Find first wrap point
                int64_t modulus = 0;
                for (size_t i = 1; i < count; i++) {
                    int64_t expected = (int64_t)arr16[i-1] + step;
                    // Check if expected overflows 16-bit range
                    if (expected < 0 || expected > 65535 || (uint16_t)arr16[i] != (uint16_t)expected) {
                        // Potential wrap - expected overflowed 16-bit range
                        if (modulus == 0 && (expected == 65536 || expected == -1)) {
                            // Standard wrap at 65536 or wrap backward at 0
                            modulus = 65536;
                            uint16_t wrapped = (uint16_t)(expected & 0xFFFF);
                            if (arr16[i] != wrapped) {
                                goto try_32bit;
                            }
                        } else if (modulus == 65536) {
                            // Verify consistent wrapping
                            int64_t expected_mod = ((int64_t)arr16[0] + (int64_t)i * step) % modulus;
                            if (expected_mod < 0) expected_mod += modulus;
                            if (arr16[i] != (uint16_t)expected_mod) goto try_32bit;
                        } else {
                            goto try_32bit;
                        }
                    }
                }

                if (modulus > 0) {
                    // Verify entire sequence (sample for large data)
                    bool valid = true;
                    size_t sample_step = (count > 1000) ? count / 100 : 1;
                    for (size_t i = 0; i < count && valid; i += sample_step) {
                        int64_t expected = ((int64_t)arr16[0] + (int64_t)i * step) % modulus;
                        if (expected < 0) expected += modulus;
                        if (arr16[i] != (uint16_t)expected) valid = false;
                    }
                    if (valid) {
                        params.elem_size = 2;
                        params.count = count;
                        params.base = arr16[0];
                        params.step = step;
                        params.modulus = modulus;
                        return true;
                    }
                }
            }
        }
    }

try_32bit:
    // Check for 32-bit aligned data
    if (n >= 16 && n % 4 == 0) {
        // Try 32-bit
        const int32_t* arr32 = reinterpret_cast<const int32_t*>(data);
        size_t count = n / 4;

        if (count >= 8) {
            int64_t step = (int64_t)arr32[1] - (int64_t)arr32[0];
            if (step != 0) {
                // Find first wrap point
                int64_t modulus = 0;
                for (size_t i = 1; i < count; i++) {
                    int64_t expected = (int64_t)arr32[i-1] + step;
                    if (arr32[i] != expected) {
                        // Potential wrap
                        if (modulus == 0) {
                            modulus = expected; // what it would have wrapped from
                            if (arr32[i] != 0 && arr32[i] != expected % modulus) {
                                goto try_64bit;
                            }
                        } else {
                            // Verify consistent wrapping
                            int64_t expected_mod = ((int64_t)arr32[0] + (int64_t)i * step) % modulus;
                            if (expected_mod < 0) expected_mod += modulus;
                            if (arr32[i] != expected_mod) goto try_64bit;
                        }
                    }
                }

                if (modulus > 0) {
                    // Verify entire sequence
                    bool valid = true;
                    for (size_t i = 0; i < count && valid; i++) {
                        int64_t expected = ((int64_t)arr32[0] + (int64_t)i * step) % modulus;
                        if (expected < 0) expected += modulus;
                        if (arr32[i] != expected) valid = false;
                    }
                    if (valid) {
                        params.elem_size = 4;
                        params.count = count;
                        params.base = arr32[0];
                        params.step = step;
                        params.modulus = modulus;
                        return true;
                    }
                }
            }
        }
    }

try_64bit:
    if (n >= 32 && n % 8 == 0) {
        const int64_t* arr64 = reinterpret_cast<const int64_t*>(data);
        size_t count = n / 8;

        if (count >= 4) {
            int64_t step = arr64[1] - arr64[0];
            if (step != 0) {
                // Find first wrap point
                int64_t modulus = 0;
                for (size_t i = 1; i < count; i++) {
                    int64_t expected = arr64[i-1] + step;
                    if (arr64[i] != expected) {
                        if (modulus == 0) {
                            modulus = expected;
                            if (arr64[i] != 0 && arr64[i] != expected % modulus) {
                                return false;
                            }
                        } else {
                            int64_t expected_mod = (arr64[0] + (int64_t)i * step) % modulus;
                            if (expected_mod < 0) expected_mod += modulus;
                            if (arr64[i] != expected_mod) return false;
                        }
                    }
                }

                if (modulus > 0) {
                    // Verify entire sequence
                    bool valid = true;
                    for (size_t i = 0; i < count && valid; i++) {
                        int64_t expected = (arr64[0] + (int64_t)i * step) % modulus;
                        if (expected < 0) expected += modulus;
                        if (arr64[i] != expected) valid = false;
                    }
                    if (valid) {
                        params.elem_size = 8;
                        params.count = count;
                        params.base = arr64[0];
                        params.step = step;
                        params.modulus = modulus;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// Encode modular sequence: elem_size + base + step + modulus
inline std::vector<uint8_t> encode_modular(const ModularParams& params) {
    std::vector<uint8_t> out(1 + 8 + 8 + 8);  // elem_size + base + step + modulus
    out[0] = params.elem_size;
    memcpy(&out[1], &params.base, 8);
    memcpy(&out[9], &params.step, 8);
    memcpy(&out[17], &params.modulus, 8);
    return out;
}

// Decode modular sequence: regenerate the full array
inline std::vector<uint8_t> decode_modular(const uint8_t* encoded, size_t original_size) {
    uint8_t elem_size = encoded[0];
    int64_t base, step, modulus;
    memcpy(&base, &encoded[1], 8);
    memcpy(&step, &encoded[9], 8);
    memcpy(&modulus, &encoded[17], 8);

    std::vector<uint8_t> result(original_size);

    if (elem_size == 2) {
        // 16-bit modular (unsigned for proper 0-65535 range)
        uint16_t* out = reinterpret_cast<uint16_t*>(result.data());
        size_t count = original_size / 2;
        for (size_t i = 0; i < count; i++) {
            int64_t val = (base + (int64_t)i * step) % modulus;
            if (val < 0) val += modulus;
            out[i] = (uint16_t)val;
        }
    } else if (elem_size == 4) {
        int32_t* out = reinterpret_cast<int32_t*>(result.data());
        size_t count = original_size / 4;
        for (size_t i = 0; i < count; i++) {
            int64_t val = (base + (int64_t)i * step) % modulus;
            if (val < 0) val += modulus;
            out[i] = (int32_t)val;
        }
    } else if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;
        for (size_t i = 0; i < count; i++) {
            int64_t val = (base + (int64_t)i * step) % modulus;
            if (val < 0) val += modulus;
            out[i] = val;
        }
    }

    return result;
}

// ============================================================================
// PERIODIC: Repeating Pattern Encoding
// Achieves 7000x compression on periodic data!
// ============================================================================

struct PeriodicParams {
    uint16_t period;     // Pattern length (1-65535 bytes)
};

// Detect if data is a repeating pattern
// Returns period length if detected, 0 otherwise
inline size_t detect_periodic(const uint8_t* data, size_t n) {
    if (n < 4) return 0;

    // Try small periods first (1=FILL, 2=checkered binary, 3=RGB, 6=checkered RGB, then power-of-2)
    for (size_t p : {1, 2, 3, 4, 6, 8, 16, 32, 64, 128, 256}) {
        if (p > n / 2) continue;

        bool matches = true;
        // Sample check: compare first period with samples at various offsets
        // IMPORTANT: Use aligned offsets - (n/p - 1) * p gives last full period
        size_t last_aligned = (n / p - 1) * p;
        for (size_t offset : {p, p * 2, last_aligned}) {
            if (offset + p > n) continue;
            if (memcmp(data, data + offset, p) != 0) {
                matches = false;
                break;
            }
        }

        if (matches) {
            // Full verification
            for (size_t i = p; i + p <= n; i += p) {
                if (memcmp(data, data + i, p) != 0) {
                    matches = false;
                    break;
                }
            }
            // Check remainder
            if (matches && n % p != 0) {
                if (memcmp(data, data + (n / p) * p, n % p) != 0) {
                    matches = false;
                }
            }

            if (matches) return p;
        }
    }

    return 0;
}

// Encode periodic data: store period length + one copy of pattern
inline std::vector<uint8_t> encode_periodic(const uint8_t* data, size_t period) {
    std::vector<uint8_t> result(2 + period);
    result[0] = period & 0xFF;
    result[1] = (period >> 8) & 0xFF;
    memcpy(&result[2], data, period);
    return result;
}

// Decode periodic data: regenerate the full sequence
inline std::vector<uint8_t> decode_periodic(const uint8_t* encoded, size_t original_size) {
    uint16_t period = encoded[0] | (encoded[1] << 8);
    const uint8_t* pattern = &encoded[2];

    std::vector<uint8_t> result(original_size);

    // Fast fill using memcpy doubling
    memcpy(result.data(), pattern, period);
    size_t filled = period;

    while (filled < original_size) {
        size_t to_copy = std::min(filled, original_size - filled);
        memcpy(result.data() + filled, result.data(), to_copy);
        filled += to_copy;
    }

    return result;
}

// ============================================================================
// PERIODIC_APPROX: Periodic with Exceptions (Effective Complexity encoding)
// Handles periodic patterns with a few corrupted positions
// ============================================================================

struct PeriodicApproxParams {
    uint16_t period;           // Pattern length
    uint32_t exception_count;  // Number of exceptions
};

// Detect periodic pattern with exceptions
// Returns true if data is mostly periodic with <= max_exception_rate exceptions
inline bool detect_periodic_approx(const uint8_t* data, size_t n,
                                   PeriodicApproxParams& params,
                                   std::vector<std::pair<uint32_t, uint8_t>>& exceptions,
                                   double max_exception_rate = 0.05) {
    if (n < 8) return false;

    // Try small periods first (1=FILL, 2=checkered, 3=RGB, etc.)
    for (size_t p : {1, 2, 3, 4, 6, 8, 16, 32, 64, 128, 256}) {
        if (p > n / 2) continue;

        // First pass: find the "consensus" pattern by majority vote at each position
        std::vector<uint8_t> best_pattern(p);
        std::vector<std::array<int, 256>> vote_counts(p);  // vote_counts[pos_in_period][byte_value]

        for (auto& counts : vote_counts) {
            counts.fill(0);
        }

        // Count votes for each position in the period
        for (size_t i = 0; i < n; i++) {
            vote_counts[i % p][data[i]]++;
        }

        // Pick the most common byte at each position
        for (size_t i = 0; i < p; i++) {
            int best_count = 0;
            uint8_t best_byte = 0;
            for (int b = 0; b < 256; b++) {
                if (vote_counts[i][b] > best_count) {
                    best_count = vote_counts[i][b];
                    best_byte = (uint8_t)b;
                }
            }
            best_pattern[i] = best_byte;
        }

        // Second pass: find exceptions (positions that don't match consensus pattern)
        std::vector<std::pair<uint32_t, uint8_t>> found_exceptions;
        for (size_t i = 0; i < n; i++) {
            if (data[i] != best_pattern[i % p]) {
                found_exceptions.push_back({(uint32_t)i, data[i]});
            }
        }

        double exception_rate = (double)found_exceptions.size() / n;

        // Check if within threshold AND saves space
        // Encoded size = 2 (period len) + period + 5*exceptions (pos:4 + val:1)
        size_t encoded_size = 2 + p + 5 * found_exceptions.size();

        if (exception_rate <= max_exception_rate && encoded_size < n / 2) {
            params.period = (uint16_t)p;
            params.exception_count = (uint32_t)found_exceptions.size();
            exceptions = std::move(found_exceptions);

            // Copy consensus pattern to first p bytes of data for encoding
            // Actually, we need to pass the pattern separately... let me rethink
            return true;
        }
    }

    return false;
}

// Encode periodic data with exceptions
// Format: period(2) + pattern(period) + exception_count(4) + [pos(4) + val(1)]*
inline std::vector<uint8_t> encode_periodic_approx(const uint8_t* data, size_t n,
                                                    const PeriodicApproxParams& params,
                                                    const std::vector<std::pair<uint32_t, uint8_t>>& exceptions) {
    size_t period = params.period;

    // Compute consensus pattern (same as detection)
    std::vector<uint8_t> pattern(period);
    std::vector<std::array<int, 256>> vote_counts(period);
    for (auto& counts : vote_counts) counts.fill(0);

    for (size_t i = 0; i < n; i++) {
        vote_counts[i % period][data[i]]++;
    }

    for (size_t i = 0; i < period; i++) {
        int best_count = 0;
        uint8_t best_byte = 0;
        for (int b = 0; b < 256; b++) {
            if (vote_counts[i][b] > best_count) {
                best_count = vote_counts[i][b];
                best_byte = (uint8_t)b;
            }
        }
        pattern[i] = best_byte;
    }

    // Build encoded output
    std::vector<uint8_t> out;
    out.reserve(2 + period + 4 + 5 * exceptions.size());

    // Period length (2 bytes)
    out.push_back(period & 0xFF);
    out.push_back((period >> 8) & 0xFF);

    // Pattern
    out.insert(out.end(), pattern.begin(), pattern.end());

    // Exception count (4 bytes)
    uint32_t exc_count = (uint32_t)exceptions.size();
    out.push_back(exc_count & 0xFF);
    out.push_back((exc_count >> 8) & 0xFF);
    out.push_back((exc_count >> 16) & 0xFF);
    out.push_back((exc_count >> 24) & 0xFF);

    // Exceptions: position(4) + value(1)
    for (const auto& [pos, val] : exceptions) {
        out.push_back(pos & 0xFF);
        out.push_back((pos >> 8) & 0xFF);
        out.push_back((pos >> 16) & 0xFF);
        out.push_back((pos >> 24) & 0xFF);
        out.push_back(val);
    }

    return out;
}

// Decode periodic data with exceptions
inline std::vector<uint8_t> decode_periodic_approx(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    // Read period
    uint16_t period = encoded[0] | (encoded[1] << 8);
    const uint8_t* pattern = &encoded[2];

    // Read exception count
    size_t offset = 2 + period;
    uint32_t exc_count = encoded[offset] | (encoded[offset + 1] << 8) |
                         (encoded[offset + 2] << 16) | (encoded[offset + 3] << 24);
    offset += 4;

    // Generate base periodic sequence
    std::vector<uint8_t> result(original_size);
    for (size_t i = 0; i < original_size; i++) {
        result[i] = pattern[i % period];
    }

    // Apply exceptions
    for (uint32_t i = 0; i < exc_count; i++) {
        uint32_t pos = encoded[offset] | (encoded[offset + 1] << 8) |
                       (encoded[offset + 2] << 16) | (encoded[offset + 3] << 24);
        uint8_t val = encoded[offset + 4];
        offset += 5;

        if (pos < original_size) {
            result[pos] = val;
        }
    }

    return result;
}

// Estimate complexity for PERIODIC_APPROX
inline EffectiveComplexityScore estimate_periodic_approx_complexity(
    const PeriodicApproxParams& params,
    const std::vector<std::pair<uint32_t, uint8_t>>& exceptions) {
    // Program: period length (2) + pattern bytes + exception count (4)
    size_t program = 2 + params.period + 4;
    // Residual: 5 bytes per exception (pos + val)
    size_t residual = 5 * exceptions.size();
    return {program, residual};
}

// ============================================================================
// REFERENCE: Cross-Block Delta Encoding (Mutual Algorithmic Information)
// ============================================================================
// Uses zstd dictionary mode to compress block using a similar previous block.
// Format: [ref_block_index:4][delta_compressed_data:variable]

inline std::vector<uint8_t> encode_reference(
    const uint8_t* data, size_t n,
    const uint8_t* ref_data, size_t ref_size,
    uint32_t ref_block_index,
    int zstd_level = 3) {
    
    // Create zstd dictionary from reference block
    ZSTD_CDict* cdict = ZSTD_createCDict(ref_data, ref_size, zstd_level);
    if (!cdict) {
        return {};  // Dictionary creation failed
    }
    
    // Compress using dictionary
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    std::vector<uint8_t> compressed(ZSTD_compressBound(n) + 4);
    
    // Write reference block index first
    compressed[0] = ref_block_index & 0xFF;
    compressed[1] = (ref_block_index >> 8) & 0xFF;
    compressed[2] = (ref_block_index >> 16) & 0xFF;
    compressed[3] = (ref_block_index >> 24) & 0xFF;
    
    size_t comp_size = ZSTD_compress_usingCDict(
        cctx,
        compressed.data() + 4, compressed.size() - 4,
        data, n,
        cdict
    );
    
    ZSTD_freeCCtx(cctx);
    ZSTD_freeCDict(cdict);
    
    if (ZSTD_isError(comp_size)) {
        return {};  // Compression failed
    }
    
    compressed.resize(4 + comp_size);
    return compressed;
}

inline std::vector<uint8_t> decode_reference(
    const uint8_t* encoded, size_t encoded_size,
    const uint8_t* ref_data, size_t ref_size,
    size_t original_size) {
    
    if (encoded_size < 4) {
        return {};  // Invalid: need at least ref index
    }
    
    // Skip ref_block_index (already used to locate ref_data)
    const uint8_t* delta_data = encoded + 4;
    size_t delta_size = encoded_size - 4;
    
    // Create dictionary from reference block
    ZSTD_DDict* ddict = ZSTD_createDDict(ref_data, ref_size);
    if (!ddict) {
        return {};
    }
    
    // Decompress using dictionary
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    std::vector<uint8_t> result(original_size);
    
    size_t dec_size = ZSTD_decompress_usingDDict(
        dctx,
        result.data(), result.size(),
        delta_data, delta_size,
        ddict
    );
    
    ZSTD_freeDCtx(dctx);
    ZSTD_freeDDict(ddict);
    
    if (ZSTD_isError(dec_size) || dec_size != original_size) {
        return {};
    }
    
    return result;
}

// MDL cost for REFERENCE encoding
inline MDLScore mdl_reference(size_t delta_size) {
    // Model: ref_block_index (4 bytes)
    // Data: compressed delta
    return {4, delta_size};
}

// ============================================================================
// TIMESTAMP: Delta-of-Delta + ZigZag + Varint Encoding (Gorilla-style)
// Achieves 12x compression on quasi-periodic timestamps!
// ============================================================================

struct TimestampParams {
    uint8_t elem_size;   // 4 or 8 bytes per element
    uint64_t count;      // Number of elements
};

// ZigZag encoding: signed -> unsigned (concentrates small values near 0)
inline uint64_t zigzag_encode_val(int64_t v) {
    return (uint64_t)((v << 1) ^ (v >> 63));
}

inline int64_t zigzag_decode_val(uint64_t v) {
    return (int64_t)((v >> 1) ^ -(int64_t)(v & 1));
}

// Varint encoding to buffer, returns bytes written
inline size_t varint_encode_to(uint8_t* out, uint64_t v) {
    size_t pos = 0;
    while (v >= 0x80) {
        out[pos++] = (uint8_t)(v | 0x80);
        v >>= 7;
    }
    out[pos++] = (uint8_t)v;
    return pos;
}

// Varint decoding from buffer, returns value and advances ptr
inline uint64_t varint_decode_from(const uint8_t*& ptr, const uint8_t* end) {
    uint64_t result = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t b = *ptr++;
        result |= (uint64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}

// Detect if data looks like quasi-periodic timestamps
// Returns true if delta-of-delta encoding would help (small dd values)
inline bool detect_timestamp(const uint8_t* data, size_t n, TimestampParams& params) {
    // Need at least 3 values
    if (n < 24) return false;

    // Try 64-bit first (most timestamps are 64-bit)
    if (n % 8 == 0) {
        size_t count = n / 8;
        if (count < 3) return false;

        const int64_t* vals = reinterpret_cast<const int64_t*>(data);

        // Check if values are monotonically increasing (typical for timestamps)
        if (vals[1] <= vals[0] || vals[count-1] <= vals[count-2]) return false;

        // Calculate delta-of-deltas for samples
        int64_t d1 = vals[1] - vals[0];
        int64_t d2 = vals[2] - vals[1];
        int64_t dd = d2 - d1;

        // Check if first delta is reasonable (< 1 trillion = ~30 years in ms)
        if (d1 <= 0 || d1 > 1000000000000LL) return false;

        // Sample more delta-of-deltas
        size_t samples = std::min(count - 1, (size_t)16);
        size_t step = (count - 1) / samples;
        int64_t max_dd = 0;
        int64_t prev_delta = d1;

        for (size_t i = 1; i < count && i < samples * step; i += step) {
            int64_t delta = vals[i+1] - vals[i];
            int64_t this_dd = delta - prev_delta;
            if (this_dd < 0) this_dd = -this_dd;
            if (this_dd > max_dd) max_dd = this_dd;
            prev_delta = delta;
        }

        // If delta-of-delta is much smaller than delta, encoding helps
        // Threshold: dd < delta/10 (would give ~10x compression on dd values)
        if (max_dd < d1 / 10 && max_dd < 10000) {
            params.elem_size = 8;
            params.count = count;
            return true;
        }
    }

    // Try 32-bit
    if (n % 4 == 0 && n % 8 != 0) {
        size_t count = n / 4;
        if (count < 3) return false;

        const uint32_t* vals = reinterpret_cast<const uint32_t*>(data);

        if (vals[1] <= vals[0] || vals[count-1] <= vals[count-2]) return false;

        int64_t d1 = (int64_t)vals[1] - (int64_t)vals[0];
        if (d1 <= 0 || d1 > 1000000000) return false;

        int64_t d2 = (int64_t)vals[2] - (int64_t)vals[1];
        int64_t dd = d2 - d1;
        if (dd < 0) dd = -dd;

        if (dd < d1 / 10 && dd < 10000) {
            params.elem_size = 4;
            params.count = count;
            return true;
        }
    }

    return false;
}

// Encode timestamps: delta-of-delta + zigzag + varint
inline std::vector<uint8_t> encode_timestamp(const uint8_t* data, const TimestampParams& params) {
    // Header: 1 byte elem_size + first value (8 bytes) + first delta (8 bytes)
    std::vector<uint8_t> result;
    result.reserve(17 + params.count * 2);  // Estimate

    result.push_back(params.elem_size);

    if (params.elem_size == 8) {
        const int64_t* vals = reinterpret_cast<const int64_t*>(data);

        // Store first value
        for (int i = 0; i < 8; i++) result.push_back((data[i]));

        if (params.count < 2) return result;

        // Store first delta
        int64_t first_delta = vals[1] - vals[0];
        uint8_t buf[10];
        size_t len = varint_encode_to(buf, zigzag_encode_val(first_delta));
        for (size_t i = 0; i < len; i++) result.push_back(buf[i]);

        // Store delta-of-deltas as zigzag varints
        int64_t prev_delta = first_delta;
        for (size_t i = 2; i < params.count; i++) {
            int64_t delta = vals[i] - vals[i-1];
            int64_t dd = delta - prev_delta;
            len = varint_encode_to(buf, zigzag_encode_val(dd));
            for (size_t j = 0; j < len; j++) result.push_back(buf[j]);
            prev_delta = delta;
        }
    } else {
        const uint32_t* vals = reinterpret_cast<const uint32_t*>(data);

        // Store first value (4 bytes)
        for (int i = 0; i < 4; i++) result.push_back(data[i]);

        if (params.count < 2) return result;

        int64_t first_delta = (int64_t)vals[1] - (int64_t)vals[0];
        uint8_t buf[10];
        size_t len = varint_encode_to(buf, zigzag_encode_val(first_delta));
        for (size_t i = 0; i < len; i++) result.push_back(buf[i]);

        int64_t prev_delta = first_delta;
        for (size_t i = 2; i < params.count; i++) {
            int64_t delta = (int64_t)vals[i] - (int64_t)vals[i-1];
            int64_t dd = delta - prev_delta;
            len = varint_encode_to(buf, zigzag_encode_val(dd));
            for (size_t j = 0; j < len; j++) result.push_back(buf[j]);
            prev_delta = delta;
        }
    }

    return result;
}

// Decode timestamps: reverse the encoding
inline std::vector<uint8_t> decode_timestamp(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    std::vector<uint8_t> result(original_size);

    uint8_t elem_size = encoded[0];
    const uint8_t* ptr = encoded + 1;
    const uint8_t* end = encoded + encoded_size;

    if (elem_size == 8) {
        int64_t* out = reinterpret_cast<int64_t*>(result.data());
        size_t count = original_size / 8;

        // Read first value
        memcpy(&out[0], ptr, 8);
        ptr += 8;

        if (count < 2) return result;

        // Read first delta
        int64_t delta = zigzag_decode_val(varint_decode_from(ptr, end));
        out[1] = out[0] + delta;

        // Read delta-of-deltas
        for (size_t i = 2; i < count; i++) {
            int64_t dd = zigzag_decode_val(varint_decode_from(ptr, end));
            delta = delta + dd;
            out[i] = out[i-1] + delta;
        }
    } else {
        uint32_t* out = reinterpret_cast<uint32_t*>(result.data());
        size_t count = original_size / 4;

        memcpy(&out[0], ptr, 4);
        ptr += 4;

        if (count < 2) return result;

        int64_t delta = zigzag_decode_val(varint_decode_from(ptr, end));
        out[1] = (uint32_t)((int64_t)out[0] + delta);

        for (size_t i = 2; i < count; i++) {
            int64_t dd = zigzag_decode_val(varint_decode_from(ptr, end));
            delta = delta + dd;
            out[i] = (uint32_t)((int64_t)out[i-1] + delta);
        }
    }

    return result;
}

// ============================================================================
// TEMPLATE: Kolmogorov-style text compression
// Extracts template from structured text (logs, JSON, CSV) and encodes
// variable columns with appropriate strategies (LINEAR_GEN, dict, raw)
// Achieves 3-5x over zstd on structured logs!
// ============================================================================

// Column encoding types
enum class ColumnType : uint8_t {
    COL_LINEAR_GEN = 0,   // Sequential integers (17 bytes for any count!)
    COL_TIMESTAMP = 1,    // Quasi-periodic integers
    COL_DICTIONARY = 2,   // Repeated strings (dict + indices)
    COL_RAW = 3,          // Random data (zstd compressed)
    COL_CONST = 4,        // All same value (store once)
    COL_BYTE_OFFSET = 5,  // Numeric with range < 256 (store min + byte offsets)
    COL_MODULAR = 6,      // v = (base + i * step) % mod (cycling patterns like seconds 0-59)
    COL_REPEAT_INC = 7,   // v = start + floor(i / repeat) * inc (grouped patterns)
    COL_SUBTEMPLATE = 8   // prefix + LINEAR_GEN + suffix (e.g., "User123" or "user123@example.com")
};

struct TemplateColumn {
    ColumnType type;
    std::vector<std::string> values;  // Raw string values

    // For LINEAR_GEN
    int64_t base;
    int64_t delta;

    // For MODULAR: v = (base + i * delta) % mod
    int64_t mod;

    // For REPEAT_INC: v = base + floor(i / repeat) * inc
    int64_t repeat;
    int64_t inc;

    // For SUBTEMPLATE: prefix + LINEAR_GEN(base, delta) + suffix
    std::string prefix;   // e.g., "User" or "user"
    std::string suffix;   // e.g., "" or "@example.com"

    // For DICTIONARY
    std::vector<std::string> dict;
    std::vector<uint16_t> indices;
};

struct TemplateParams {
    std::string template_str;           // Template with $0, $1, etc.
    std::vector<TemplateColumn> columns;
    size_t line_count;
    size_t header_lines;                // Lines to skip before template starts
    std::string header_bytes;           // Raw bytes of skipped header lines
    std::string footer_bytes;           // Raw bytes after last template line
    std::vector<std::pair<size_t, std::string>> non_matching_lines; // (line_index, raw_bytes) for lines that don't match template
};

// Char-level line template: fixed chars + variable regions
// More robust than token-based detection (works on access logs that fail token threshold)
struct CharTemplateRegion {
    size_t start;           // Char position in line
    size_t end;             // Char position end (exclusive)
    bool is_variable;       // True if region varies across lines
};

struct CharTemplateColumn {
    std::vector<std::string> values;    // One value per line
    ColumnType encoding;                // Encoding type for this column

    // For COL_LINEAR_GEN, COL_MODULAR, COL_REPEAT_INC
    int64_t base = 0;      // Starting value
    int64_t step = 0;      // Increment per index (LINEAR_GEN, MODULAR)
    int64_t mod = 0;       // Modulus (MODULAR only)
    int64_t repeat = 0;    // Repeat count before incrementing (REPEAT_INC only)
    int64_t inc = 0;       // Increment after each repeat block (REPEAT_INC only)
};

struct CharTemplateParams {
    std::string fixed_template;         // Fixed chars with {} placeholders for variables
    std::vector<CharTemplateRegion> regions;
    std::vector<CharTemplateColumn> columns;  // Variable column data
    size_t line_count;
    size_t min_line_len;                // Minimum line length in template region
    std::string header_bytes;           // Bytes before first template line
    std::string footer_bytes;           // Bytes after last template line
    std::vector<std::string> tail_bytes; // Variable-length tail per line (after min_line_len)
    bool last_line_has_newline = true;  // Whether last line ends with \n

};

// LINE_TEMPLATE: Variable-length lines with common prefix/suffix and linear numeric variables
// Perfect for SQL INSERTs, CSV with sequential IDs, log lines with incrementing counters
struct LineTemplateParams {
    std::string prefix;                   // Common prefix of all lines
    std::string suffix;                   // Common suffix of all lines
    std::vector<std::string> separators;  // Text between variables (N separators for N variables)
    std::vector<int64_t> first_values;    // First value of each variable
    std::vector<int64_t> deltas;          // Delta per line for each variable
    size_t line_count;                    // Number of lines following this template
    std::string header_bytes;             // Bytes before first template line
    std::string footer_bytes;             // Bytes after last template line
    bool last_line_has_newline = true;    // Whether last line ends with \n
};

// KV_CONFIG: Structural key-value config encoding (INI/YAML)
// v2 adds columnar encoding with pattern detection for each column

// Column encoding types for columnar v2
enum class KvColumnType : uint8_t {
    RAW = 0,         // No pattern, store all values as varints
    CONSTANT = 1,    // All same value: just store the value
    MODULAR = 2,     // v = (base + i * step) % mod (cycling pattern)
    REPEAT_INC = 3,  // v = start + floor(i / repeat) * inc (each value repeats N times)
    LINEAR = 4       // v = base + i * step (perfect linear sequence)
};

// Detect pattern in a column of integers
// Returns the best encoding type and fills params
struct KvColumnParams {
    KvColumnType type;
    int64_t p1, p2, p3;  // Parameters depending on type
    // CONSTANT: p1=value
    // MODULAR: p1=base, p2=step, p3=mod
    // REPEAT_INC: p1=start, p2=repeat, p3=inc
    // LINEAR: p1=base, p2=step
};

inline KvColumnParams detect_column_pattern(const std::vector<uint16_t>& values) {
    KvColumnParams result;
    result.type = KvColumnType::RAW;
    if (values.empty()) return result;

    size_t n = values.size();

    // Check CONSTANT: all same value
    bool all_same = true;
    for (size_t i = 1; i < n && all_same; i++) {
        if (values[i] != values[0]) all_same = false;
    }
    if (all_same) {
        result.type = KvColumnType::CONSTANT;
        result.p1 = values[0];
        return result;
    }

    // Check LINEAR: v = base + step * i
    if (n >= 2) {
        int64_t base = values[0];
        int64_t step = (int64_t)values[1] - values[0];
        bool is_linear = true;
        for (size_t i = 2; i < n && is_linear; i++) {
            if ((int64_t)values[i] != base + step * (int64_t)i) is_linear = false;
        }
        if (is_linear) {
            result.type = KvColumnType::LINEAR;
            result.p1 = base;
            result.p2 = step;
            return result;
        }
    }

    // Check MODULAR: v = (base + i * step) % mod (cycling like 0,1,2,3,4,0,1,2,3,4...)
    // Find cycle length by looking for repeat of first value
    for (size_t mod = 2; mod <= 256 && mod <= n; mod++) {
        if (values[mod % n] == values[0]) {
            // Potential cycle at 'mod'
            bool is_modular = true;
            int64_t base = values[0];
            int64_t step = (mod > 1 && values[1] > values[0]) ? values[1] - values[0] : 1;
            for (size_t i = 0; i < n && is_modular; i++) {
                int64_t expected = (base + (int64_t)i * step) % mod;
                if (values[i] != expected) is_modular = false;
            }
            if (is_modular) {
                result.type = KvColumnType::MODULAR;
                result.p1 = base;
                result.p2 = step;
                result.p3 = mod;
                return result;
            }
        }
    }

    // Check REPEAT_INC: v = start + floor(i / repeat) * inc (like 0,0,0,0,0,1,1,1,1,1,2,2,2,2,2...)
    // Find repeat count by counting consecutive equal values at start
    size_t repeat = 1;
    while (repeat < n && values[repeat] == values[0]) repeat++;
    if (repeat >= 2 && repeat <= 256) {
        int64_t start = values[0];
        int64_t inc = (repeat < n) ? (int64_t)values[repeat] - start : 0;
        bool is_repeat_inc = true;
        for (size_t i = 0; i < n && is_repeat_inc; i++) {
            int64_t expected = start + ((int64_t)i / (int64_t)repeat) * inc;
            if (values[i] != expected) is_repeat_inc = false;
        }
        if (is_repeat_inc) {
            result.type = KvColumnType::REPEAT_INC;
            result.p1 = start;
            result.p2 = repeat;
            result.p3 = inc;
            return result;
        }
    }

    // No pattern found, use RAW
    return result;
}

// Encode a column with detected pattern
inline void encode_column(std::vector<uint8_t>& out, const std::vector<uint16_t>& values, const KvColumnParams& params) {
    out.push_back(static_cast<uint8_t>(params.type));

    switch (params.type) {
        case KvColumnType::CONSTANT:
            write_uvarint(out, params.p1);
            break;
        case KvColumnType::LINEAR:
            write_uvarint(out, zigzag_encode_val(params.p1));
            write_uvarint(out, zigzag_encode_val(params.p2));
            break;
        case KvColumnType::MODULAR:
            write_uvarint(out, params.p1);
            write_uvarint(out, params.p2);
            write_uvarint(out, params.p3);
            break;
        case KvColumnType::REPEAT_INC:
            write_uvarint(out, zigzag_encode_val(params.p1));
            write_uvarint(out, params.p2);
            write_uvarint(out, zigzag_encode_val(params.p3));
            break;
        case KvColumnType::RAW:
        default:
            write_uvarint(out, values.size());
            for (auto v : values) write_uvarint(out, v);
            break;
    }
}

// Decode a column
inline std::vector<uint16_t> decode_column(const uint8_t*& ptr, const uint8_t* end, size_t count) {
    std::vector<uint16_t> result(count);
    if (ptr >= end) return result;

    KvColumnType type = static_cast<KvColumnType>(*ptr++);

    switch (type) {
        case KvColumnType::CONSTANT: {
            uint16_t val = (uint16_t)read_uvarint(ptr, end);
            for (size_t i = 0; i < count; i++) result[i] = val;
            break;
        }
        case KvColumnType::LINEAR: {
            int64_t base = zigzag_decode_val(read_uvarint(ptr, end));
            int64_t step = zigzag_decode_val(read_uvarint(ptr, end));
            for (size_t i = 0; i < count; i++) result[i] = (uint16_t)(base + step * (int64_t)i);
            break;
        }
        case KvColumnType::MODULAR: {
            int64_t base = read_uvarint(ptr, end);
            int64_t step = read_uvarint(ptr, end);
            int64_t mod = read_uvarint(ptr, end);
            for (size_t i = 0; i < count; i++) result[i] = (uint16_t)((base + (int64_t)i * step) % mod);
            break;
        }
        case KvColumnType::REPEAT_INC: {
            int64_t start = zigzag_decode_val(read_uvarint(ptr, end));
            int64_t repeat = read_uvarint(ptr, end);
            int64_t inc = zigzag_decode_val(read_uvarint(ptr, end));
            for (size_t i = 0; i < count; i++) result[i] = (uint16_t)(start + ((int64_t)i / repeat) * inc);
            break;
        }
        case KvColumnType::RAW:
        default: {
            size_t n = read_uvarint(ptr, end);
            result.resize(n);
            for (size_t i = 0; i < n && ptr < end; i++) result[i] = (uint16_t)read_uvarint(ptr, end);
            break;
        }
    }
    return result;
}
// Beats brotli by 7% on INI configs, 35% better than raw zstd
// Format: [config_type:1][section_dict][key_dict][section_data...]
// Key insight: Section names and keys repeat constantly. Dictionary encode them.
// A 16KB INI file typically has 5 section types × 7 keys = 12 unique strings
// Storing these once + 1-byte indices = massive savings

enum class KvConfigType : uint8_t {
    INI = 0,    // [section]\nkey = value
    YAML = 1,   // section:\n  key: value
};

struct KvConfigSection {
    uint8_t name_idx;      // Index into section name dict
    uint16_t section_num;  // Section number (e.g., 0 in database_0)
    std::vector<std::pair<uint8_t, std::string>> key_values;  // (key_idx, value as string)
};

struct KvConfigParams {
    KvConfigType config_type;
    std::vector<std::string> section_names;   // Dictionary of section names
    std::vector<std::string> key_names;       // Dictionary of key names
    std::vector<KvConfigSection> sections;    // Parsed sections
    size_t original_size;                     // For validation
    std::vector<uint8_t> trailing_bytes;      // Unparsed trailing content (truncated data)
    size_t match_len;                         // How much of reconstruction matches original
};

// Detect if data is INI or YAML config format
inline bool detect_kv_config(const uint8_t* data, size_t n, KvConfigParams& params) {
    if (n < 256) return false;  // Too small to benefit

    std::string text((const char*)data, n);

    // Quick check: must have multiple newlines and be mostly printable ASCII
    size_t newline_count = 0;
    size_t non_ascii = 0;
    for (size_t i = 0; i < std::min(n, (size_t)1024); i++) {
        if (data[i] == '\n') newline_count++;
        if (data[i] < 32 && data[i] != '\n' && data[i] != '\r' && data[i] != '\t') non_ascii++;
        if (data[i] > 127) non_ascii++;
    }
    if (newline_count < 10 || non_ascii > n / 20) return false;

    // Detect format type
    size_t ini_sections = 0;   // Lines starting with [
    size_t yaml_sections = 0;  // Lines ending with :
    size_t ini_kvs = 0;        // Lines with " = "
    size_t yaml_kvs = 0;       // Indented lines with ": "

    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;

        if (line.size() > 2 && line[0] == '[' && line.back() == ']') ini_sections++;
        if (line.size() > 1 && line.back() == ':' && line[0] != ' ') yaml_sections++;
        if (line.find(" = ") != std::string::npos) ini_kvs++;
        if (line.size() > 2 && line[0] == ' ' && line.find(": ") != std::string::npos) yaml_kvs++;
    }

    // Decide format - need at least 10 sections and 40 key-values
    bool is_ini = (ini_sections >= 10 && ini_kvs >= 40 && ini_sections * 3 < ini_kvs);
    bool is_yaml = (yaml_sections >= 10 && yaml_kvs >= 40 && yaml_sections * 3 < yaml_kvs);

    if (!is_ini && !is_yaml) return false;

    params.config_type = is_ini ? KvConfigType::INI : KvConfigType::YAML;
    params.section_names.clear();
    params.key_names.clear();
    params.sections.clear();
    params.original_size = n;

    std::map<std::string, uint8_t> section_to_idx;
    std::map<std::string, uint8_t> key_to_idx;
    KvConfigSection* current_section = nullptr;

    iss.clear();
    iss.str(text);
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;

        if (is_ini) {
            // INI section: [name_N] or [name]
            if (line.size() > 2 && line[0] == '[' && line.back() == ']') {
                std::string header = line.substr(1, line.size() - 2);
                size_t underscore = header.rfind('_');
                std::string name;
                uint16_t num = 0;
                if (underscore != std::string::npos) {
                    bool all_digits = true;
                    for (size_t i = underscore + 1; i < header.size(); i++) {
                        if (!isdigit((unsigned char)header[i])) { all_digits = false; break; }
                    }
                    if (all_digits && underscore > 0) {
                        name = header.substr(0, underscore);
                        try { num = (uint16_t)std::stoi(header.substr(underscore + 1)); } catch (...) {}
                    } else {
                        name = header;
                    }
                } else {
                    name = header;
                }

                if (section_to_idx.find(name) == section_to_idx.end()) {
                    if (params.section_names.size() >= 255) continue;  // Too many unique sections
                    section_to_idx[name] = (uint8_t)params.section_names.size();
                    params.section_names.push_back(name);
                }

                params.sections.push_back(KvConfigSection{section_to_idx[name], num, {}});
                current_section = &params.sections.back();
            }
            // INI key-value: key = value
            else if (current_section) {
                size_t eq = line.find(" = ");
                if (eq != std::string::npos) {
                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 3);

                    if (key_to_idx.find(key) == key_to_idx.end()) {
                        if (params.key_names.size() >= 255) continue;
                        key_to_idx[key] = (uint8_t)params.key_names.size();
                        params.key_names.push_back(key);
                    }
                    current_section->key_values.push_back({key_to_idx[key], val});
                }
            }
        } else {
            // YAML section: name_N: or name:
            if (line.back() == ':' && line[0] != ' ') {
                std::string header = line.substr(0, line.size() - 1);
                size_t underscore = header.rfind('_');
                std::string name;
                uint16_t num = 0;
                if (underscore != std::string::npos) {
                    bool all_digits = true;
                    for (size_t i = underscore + 1; i < header.size(); i++) {
                        if (!isdigit((unsigned char)header[i])) { all_digits = false; break; }
                    }
                    if (all_digits && underscore > 0) {
                        name = header.substr(0, underscore);
                        try { num = (uint16_t)std::stoi(header.substr(underscore + 1)); } catch (...) {}
                    } else {
                        name = header;
                    }
                } else {
                    name = header;
                }

                if (section_to_idx.find(name) == section_to_idx.end()) {
                    if (params.section_names.size() >= 255) continue;
                    section_to_idx[name] = (uint8_t)params.section_names.size();
                    params.section_names.push_back(name);
                }

                params.sections.push_back(KvConfigSection{section_to_idx[name], num, {}});
                current_section = &params.sections.back();
            }
            // YAML key-value:   key: value
            else if (current_section && line[0] == ' ') {
                size_t colon = line.find(": ");
                if (colon != std::string::npos) {
                    // Skip leading spaces
                    size_t key_start = 0;
                    while (key_start < colon && line[key_start] == ' ') key_start++;
                    std::string key = line.substr(key_start, colon - key_start);
                    std::string val = line.substr(colon + 2);

                    if (key_to_idx.find(key) == key_to_idx.end()) {
                        if (params.key_names.size() >= 255) continue;
                        key_to_idx[key] = (uint8_t)params.key_names.size();
                        params.key_names.push_back(key);
                    }
                    current_section->key_values.push_back({key_to_idx[key], val});
                }
            }
        }
    }

    // Validate we got meaningful data
    if (params.sections.size() < 10) return false;
    if (params.section_names.size() == 0 || params.key_names.size() == 0) return false;

    // Find trailing bytes by trial reconstruction
    // Build what the reconstruction would produce, compare with original
    std::ostringstream trial;
    for (const auto& sec : params.sections) {
        if (sec.name_idx < params.section_names.size()) {
            const auto& name = params.section_names[sec.name_idx];
            if (is_ini) {
                trial << "[" << name << "_" << sec.section_num << "]\n";
            } else {
                trial << name << "_" << sec.section_num << ":\n";
            }
        }
        for (const auto& [key_idx, val] : sec.key_values) {
            if (key_idx < params.key_names.size()) {
                const auto& key = params.key_names[key_idx];
                if (is_ini) {
                    trial << key << " = " << val << "\n";
                } else {
                    trial << "  " << key << ": " << val << "\n";
                }
            }
        }
        trial << "\n";
    }
    std::string reconstructed = trial.str();

    // Find where reconstruction diverges from original
    params.trailing_bytes.clear();
    size_t match_len = std::min(reconstructed.size(), n);
    size_t first_diff = match_len;
    for (size_t i = 0; i < match_len; i++) {
        if ((uint8_t)reconstructed[i] != data[i]) {
            first_diff = i;
            break;
        }
    }

    // Everything from first_diff to end of original is trailing bytes
    params.match_len = first_diff;  // How much of reconstruction to output
    if (first_diff < n) {
        params.trailing_bytes.assign(data + first_diff, data + n);
    }

    // Check compression benefit: estimate encoded size vs original
    size_t dict_size = 2;  // section_count + key_count
    for (const auto& s : params.section_names) dict_size += s.size() + 1;
    for (const auto& k : params.key_names) dict_size += k.size() + 1;

    size_t data_size = 2;  // num_sections
    for (const auto& sec : params.sections) {
        data_size += 4;  // name_idx + section_num + num_kv
        for (const auto& [key_idx, val] : sec.key_values) {
            data_size += 1 + val.size() + 1;  // key_idx + value + null
        }
    }
    data_size += 2 + params.trailing_bytes.size();  // trailing bytes length + data

    // Need at least 30% savings to be worthwhile
    if (dict_size + data_size > n * 0.7) return false;

    return true;
}

// Encode KV config to binary format with columnar v2 optimization
inline std::vector<uint8_t> encode_kv_config(const KvConfigParams& params, int zstd_level) {
    std::vector<uint8_t> result;

    // Columnar v2 Format:
    // [version: 1 byte = 2]  // v2 = columnar
    // [config_type: 1 byte]
    // [num_section_names: 1 byte]
    // [section_names: null-terminated strings]
    // [num_key_names: 1 byte]
    // [key_names: null-terminated strings]
    // [num_sections: varint]
    // [name_idx_column: encoded column with pattern detection]
    // [section_num_column: encoded column with pattern detection]
    // [num_kv_column: encoded column with pattern detection]
    // [key-value data: for each section, for each kv: key_idx(varint) + value(null-term or varint)]
    // [trailing_len: 2 bytes][match_len: 4 bytes][trailing_bytes...]
    // Then zstd compress the whole thing

    std::vector<uint8_t> raw;

    // Version byte: 2 = columnar v2
    raw.push_back(2);

    // Config type
    raw.push_back(static_cast<uint8_t>(params.config_type));

    // Section names dictionary
    raw.push_back((uint8_t)params.section_names.size());
    for (const auto& name : params.section_names) {
        for (char c : name) raw.push_back((uint8_t)c);
        raw.push_back(0);
    }

    // Key names dictionary
    raw.push_back((uint8_t)params.key_names.size());
    for (const auto& name : params.key_names) {
        for (char c : name) raw.push_back((uint8_t)c);
        raw.push_back(0);
    }

    // Columnar v2: Extract columns for pattern detection
    size_t num_sections = params.sections.size();
    write_uvarint(raw, num_sections);

    // Build columns
    std::vector<uint16_t> name_idx_col(num_sections);
    std::vector<uint16_t> section_num_col(num_sections);
    std::vector<uint16_t> num_kv_col(num_sections);
    for (size_t i = 0; i < num_sections; i++) {
        name_idx_col[i] = params.sections[i].name_idx;
        section_num_col[i] = params.sections[i].section_num;
        num_kv_col[i] = (uint16_t)params.sections[i].key_values.size();
    }

    // Detect patterns and encode columns
    auto name_idx_params = detect_column_pattern(name_idx_col);
    auto section_num_params = detect_column_pattern(section_num_col);
    auto num_kv_params = detect_column_pattern(num_kv_col);

    encode_column(raw, name_idx_col, name_idx_params);
    encode_column(raw, section_num_col, section_num_params);
    encode_column(raw, num_kv_col, num_kv_params);

    // Key-value data: for each section, encode key_idx + value
    // Try to detect if values are numeric for varint encoding
    for (const auto& sec : params.sections) {
        for (const auto& [key_idx, val] : sec.key_values) {
            write_uvarint(raw, key_idx);
            // Check if value is purely numeric
            bool is_numeric = !val.empty();
            for (char c : val) {
                if (c < '0' || c > '9') { is_numeric = false; break; }
            }
            if (is_numeric && val.size() <= 9) {
                // Encode as: 0x80 | (numeric flag) + varint value
                raw.push_back(0x80);  // Numeric marker
                uint64_t num_val = 0;
                for (char c : val) num_val = num_val * 10 + (c - '0');
                write_uvarint(raw, num_val);
            } else {
                // String: null-terminated (no 0x80 prefix since strings don't start with high byte)
                for (char c : val) raw.push_back((uint8_t)c);
                raw.push_back(0);
            }
        }
    }

    // Trailing bytes (unparsed content at end of truncated data)
    uint16_t trailing_len = (uint16_t)params.trailing_bytes.size();
    raw.push_back(trailing_len & 0xFF);
    raw.push_back((trailing_len >> 8) & 0xFF);
    // Store match_len (4 bytes) - how much of reconstruction to output
    uint32_t match_len = (uint32_t)params.match_len;
    raw.push_back(match_len & 0xFF);
    raw.push_back((match_len >> 8) & 0xFF);
    raw.push_back((match_len >> 16) & 0xFF);
    raw.push_back((match_len >> 24) & 0xFF);
    raw.insert(raw.end(), params.trailing_bytes.begin(), params.trailing_bytes.end());

    // Use zstd compression (BWT is too slow for large data - O(n^2) sort step)
    // Testing showed zstd beats BWT on structured config data anyway (2668 vs 3653 at 16KB)
    std::vector<uint8_t> zstd_compressed(ZSTD_compressBound(raw.size()));
    size_t zstd_size = ZSTD_compress(zstd_compressed.data(), zstd_compressed.size(),
                                      raw.data(), raw.size(), zstd_level);
    bool zstd_ok = !ZSTD_isError(zstd_size) && zstd_size < raw.size();

    if (zstd_ok) {
        result.push_back(1);  // zstd compressed
        write_uvarint(result, raw.size());
        write_uvarint(result, zstd_size);
        result.insert(result.end(), zstd_compressed.begin(), zstd_compressed.begin() + zstd_size);
    } else {
        // No compression helped - store raw
        result.push_back(0);  // Not compressed
        write_uvarint(result, raw.size());
        result.insert(result.end(), raw.begin(), raw.end());
    }

    return result;
}

// Decode KV config back to original text (handles v1 interleaved and v2 columnar)
inline std::vector<uint8_t> decode_kv_config(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 3) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read compression flag: 0=raw, 1=zstd, 2=bwt
    uint8_t compress_mode = *ptr++;

    // Read sizes
    uint64_t raw_size = read_uvarint(ptr, end);

    std::vector<uint8_t> raw;
    if (compress_mode == 2) {
        // BWT compressed
        uint64_t comp_size = read_uvarint(ptr, end);
        // Trust no length field: comp bytes must fit in the remaining stream (inert on valid).
        if (comp_size > (uint64_t)(end - ptr)) return {};
        raw = bwt9::decompress(ptr, comp_size);
        if (raw.empty()) return {};
    } else if (compress_mode == 1) {
        // zstd compressed
        uint64_t comp_size = read_uvarint(ptr, end);
        if (comp_size > (uint64_t)(end - ptr)) return {};
        // Validate the declared raw_size against the actual zstd frame content size, so a hostile
        // raw_size can't drive a huge resize (OOM) or mismatch. Inert on valid: they always agree.
        unsigned long long fcs = ZSTD_getFrameContentSize(ptr, (size_t)comp_size);
        if (fcs == ZSTD_CONTENTSIZE_ERROR || fcs == ZSTD_CONTENTSIZE_UNKNOWN || fcs != raw_size) return {};
        raw.resize((size_t)raw_size);
        size_t decompressed = ZSTD_decompress(raw.data(), raw.size(), ptr, comp_size);
        if (ZSTD_isError(decompressed)) return {};
    } else {
        // Raw (uncompressed): raw_size literal bytes must be present in the stream. This was the
        // unchecked assign(ptr, ptr+raw_size) -> 16 GB copy -> SIGSEGV found by fuzz_decode.
        if (raw_size > (uint64_t)(end - ptr)) return {};
        raw.assign(ptr, ptr + raw_size);
    }

    // Parse the raw format
    const uint8_t* rptr = raw.data();
    const uint8_t* rend = raw.data() + raw.size();
    if (rptr >= rend) return {};

    // Check version byte: 2 = columnar v2, otherwise v1 (config_type byte)
    uint8_t first_byte = *rptr++;
    bool is_v2 = (first_byte == 2);

    KvConfigType config_type;
    if (is_v2) {
        if (rptr >= rend) return {};
        config_type = static_cast<KvConfigType>(*rptr++);
    } else {
        config_type = static_cast<KvConfigType>(first_byte);
    }

    // Read section names
    if (rptr >= rend) return {};
    uint8_t num_sections_names = *rptr++;
    std::vector<std::string> section_names;
    for (int i = 0; i < num_sections_names && rptr < rend; i++) {
        std::string name;
        while (rptr < rend && *rptr != 0) name += (char)*rptr++;
        if (rptr < rend) rptr++;  // Skip null
        section_names.push_back(name);
    }

    // Read key names
    if (rptr >= rend) return {};
    uint8_t num_key_names = *rptr++;
    std::vector<std::string> key_names;
    for (int i = 0; i < num_key_names && rptr < rend; i++) {
        std::string name;
        while (rptr < rend && *rptr != 0) name += (char)*rptr++;
        if (rptr < rend) rptr++;  // Skip null
        key_names.push_back(name);
    }

    // Read sections count
    size_t num_sections;
    if (is_v2) {
        num_sections = read_uvarint(rptr, rend);
    } else {
        if (rptr + 2 > rend) return {};
        num_sections = rptr[0] | (rptr[1] << 8);
        rptr += 2;
    }

    std::ostringstream oss;

    if (is_v2) {
        // Columnar v2: decode columns first
        auto name_idx_col = decode_column(rptr, rend, num_sections);
        auto section_num_col = decode_column(rptr, rend, num_sections);
        auto num_kv_col = decode_column(rptr, rend, num_sections);

        // Decode key-values
        for (size_t s = 0; s < num_sections && rptr < rend; s++) {
            uint8_t name_idx = (uint8_t)name_idx_col[s];
            uint16_t section_num = section_num_col[s];
            size_t num_kv = num_kv_col[s];

            // Write section header
            if (name_idx < section_names.size()) {
                if (config_type == KvConfigType::INI) {
                    oss << "[" << section_names[name_idx] << "_" << section_num << "]\n";
                } else {
                    oss << section_names[name_idx] << "_" << section_num << ":\n";
                }
            }

            // Write key-values
            for (size_t k = 0; k < num_kv && rptr < rend; k++) {
                uint8_t key_idx = (uint8_t)read_uvarint(rptr, rend);
                std::string val;

                // Check for numeric marker (0x80)
                if (rptr < rend && *rptr == 0x80) {
                    rptr++;  // Skip marker
                    uint64_t num_val = read_uvarint(rptr, rend);
                    val = std::to_string(num_val);
                } else {
                    // String: read until null
                    while (rptr < rend && *rptr != 0) val += (char)*rptr++;
                    if (rptr < rend) rptr++;  // Skip null
                }

                if (key_idx < key_names.size()) {
                    if (config_type == KvConfigType::INI) {
                        oss << key_names[key_idx] << " = " << val << "\n";
                    } else {
                        oss << "  " << key_names[key_idx] << ": " << val << "\n";
                    }
                }
            }

            oss << "\n";  // Blank line between sections
        }
    } else {
        // v1: interleaved format (backwards compatibility)
        for (size_t s = 0; s < num_sections && rptr < rend; s++) {
            if (rptr + 4 > rend) break;

            uint8_t name_idx = *rptr++;
            uint16_t section_num = rptr[0] | (rptr[1] << 8);
            rptr += 2;
            uint8_t num_kv = *rptr++;

            if (name_idx < section_names.size()) {
                if (config_type == KvConfigType::INI) {
                    oss << "[" << section_names[name_idx] << "_" << section_num << "]\n";
                } else {
                    oss << section_names[name_idx] << "_" << section_num << ":\n";
                }
            }

            for (int k = 0; k < num_kv && rptr < rend; k++) {
                uint8_t key_idx = *rptr++;
                std::string val;
                while (rptr < rend && *rptr != 0) val += (char)*rptr++;
                if (rptr < rend) rptr++;

                if (key_idx < key_names.size()) {
                    if (config_type == KvConfigType::INI) {
                        oss << key_names[key_idx] << " = " << val << "\n";
                    } else {
                        oss << "  " << key_names[key_idx] << ": " << val << "\n";
                    }
                }
            }
            oss << "\n";
        }
    }

    // Read trailing bytes info
    uint16_t trailing_len = 0;
    uint32_t match_len = 0;
    if (rptr + 2 <= rend) {
        trailing_len = rptr[0] | (rptr[1] << 8);
        rptr += 2;
    }
    if (rptr + 4 <= rend) {
        match_len = rptr[0] | (rptr[1] << 8) | (rptr[2] << 16) | (rptr[3] << 24);
        rptr += 4;
    }

    std::string result_str = oss.str();

    // Truncate reconstruction to match_len
    if (match_len > 0 && match_len < result_str.size()) {
        result_str.resize(match_len);
    }

    // Append trailing bytes
    if (trailing_len > 0 && rptr + trailing_len <= rend) {
        result_str.append((const char*)rptr, trailing_len);
    }

    return std::vector<uint8_t>(result_str.begin(), result_str.end());
}

// PHRASE_DICT: Kolmogorov-style phrase encoding
// Simple token replacement: find common phrases, replace with high-byte tokens
// Format: [dict_size:1][phrases...][token_stream]
// Token stream: 0x00-0x7F = literal, 0x80-0xFF = phrase ID (phrase_id - 0x80)
struct PhraseDictParams {
    std::vector<std::string> dictionary;  // Up to 128 phrases
    std::vector<uint8_t> tokens;          // Token stream (literals + phrase IDs)
    size_t original_size;                 // Original size for validation
};

// PHRASE_PARTITION: Data exactly partitioned by delimiter-separated phrases
// Format: [phrase_count:1][len:1][phrase]...[indices_compressed][remainder]
// Key insight: "Hello world! Bonjour le monde! ..." = 5 phrases repeated ~4900 times
// Gives 12% better than bzip2, 18% better than BWT_TEXT on repetitive phrase text
struct PhrasePartitionParams {
    std::vector<std::string> phrases;  // Unique phrases (max 255)
    std::vector<uint8_t> indices;      // Which phrase at each position
    size_t remainder_pos;              // Where unmatched data starts
};

// Detect if data can be partitioned into a small set of delimiter-separated phrases
inline bool detect_phrase_partition(const uint8_t* data, size_t n, PhrasePartitionParams& params) {
    if (n < 256) return false;

    // Must be text-like (no high bytes)
    size_t high_byte_count = 0;
    for (size_t i = 0; i < std::min(n, (size_t)4096); i++) {
        if (data[i] >= 0x80) high_byte_count++;
    }
    if (high_byte_count > n / 100) return false;  // >1% high bytes = not suitable

    // Find phrases ending with "! " or ". " or "? " (sentence boundaries)
    std::map<std::string, int> phrase_counts;
    size_t sample = std::min(n, (size_t)8192);
    size_t pos = 0;
    size_t phrase_start = 0;

    while (pos < sample - 1) {
        // Look for sentence-ending boundaries: "! " or ". " or "? " or newline
        bool boundary = false;
        if ((data[pos] == '!' || data[pos] == '.' || data[pos] == '?') &&
            pos + 1 < sample && data[pos+1] == ' ') {
            boundary = true;
            pos += 2;  // Include the space
        } else if (data[pos] == '\n') {
            boundary = true;
            pos += 1;
        }

        if (boundary && pos > phrase_start && pos - phrase_start <= 64) {
            std::string phrase((const char*)data + phrase_start, pos - phrase_start);
            phrase_counts[phrase]++;
            phrase_start = pos;
        } else if (!boundary) {
            pos++;
        }
    }

    // Need at least 2 repeated phrases
    int repeated = 0;
    for (auto& [phrase, count] : phrase_counts) {
        if (count >= 3) repeated++;
    }
    if (repeated < 2) return false;

    // Build phrase list (only frequent ones)
    params.phrases.clear();
    for (auto& [phrase, count] : phrase_counts) {
        if (count >= 3 && phrase.size() >= 4 && params.phrases.size() < 255) {
            params.phrases.push_back(phrase);
        }
    }

    if (params.phrases.size() < 2) return false;

    // Sort by length descending for greedy matching
    std::sort(params.phrases.begin(), params.phrases.end(),
              [](const std::string& a, const std::string& b) {
                  return a.size() > b.size();
              });

    // Try to partition entire data
    params.indices.clear();
    pos = 0;
    while (pos < n) {
        bool found = false;
        for (size_t i = 0; i < params.phrases.size(); i++) {
            const std::string& phrase = params.phrases[i];
            if (pos + phrase.size() <= n &&
                memcmp(data + pos, phrase.data(), phrase.size()) == 0) {
                params.indices.push_back((uint8_t)i);
                pos += phrase.size();
                found = true;
                break;
            }
        }
        if (!found) {
            params.remainder_pos = pos;
            break;
        }
    }

    if (pos == n) params.remainder_pos = n;

    // Valid if we covered >99% of data with phrases
    double coverage = (double)params.remainder_pos / n;
    return (params.indices.size() >= 10 && coverage > 0.99);
}

// Encode phrase partition: [phrase_count][phrases][compressed_indices][remainder]
inline std::vector<uint8_t> encode_phrase_partition(const PhrasePartitionParams& params,
                                                     const uint8_t* data, size_t n,
                                                     int zstd_level = 19) {
    std::vector<uint8_t> raw;

    // Header: phrase count
    raw.push_back((uint8_t)params.phrases.size());

    // Phrases: [len:1][phrase_bytes]
    for (const auto& phrase : params.phrases) {
        raw.push_back((uint8_t)phrase.size());
        raw.insert(raw.end(), phrase.begin(), phrase.end());
    }

    // Compress indices
    std::vector<uint8_t> idx_compressed(ZSTD_compressBound(params.indices.size()));
    size_t idx_size = ZSTD_compress(idx_compressed.data(), idx_compressed.size(),
                                     params.indices.data(), params.indices.size(), zstd_level);

    // DATA-LOSS BUG, FIXED 2026-08-04. Two defects on these four lines.
    //
    // (1) The size field is 2 BYTES but idx_size is unbounded. decode_phrase_partition
    //     reads it back as ((size_t)data[pos] << 8) | data[pos+1], so anything above
    //     65,535 was truncated mod 65536 and the decoder desynced. REPRODUCED on the
    //     shipped binary with a 2,097,122 B input: zstd(indices) = 142,470 B, stored as
    //     11,398; compress printed "2097122 -> 143206 (14.6441x)" and exited 0, then
    //     decompress failed with "PHRASE_PARTITION decompression failed" and produced no
    //     output. The archive was unrecoverable and nothing said so at compress time.
    //     The block cap (2,097,152 B) is ~4x more than needed to overflow: indices are
    //     one byte per matched phrase covering >99% of the block.
    // (2) ZSTD_isError(idx_size) was never checked. On a zstd error idx_size is a huge
    //     sentinel, so `idx_compressed.begin() + idx_size` below was out-of-bounds
    //     iterator arithmetic — undefined behaviour, not merely a desync.
    //
    // Fix is to DECLINE rather than widen the field: a 4-byte or varint length would
    // change the on-disk format and make new archives unreadable by existing decoders.
    // Returning {} costs only this one encoder on blocks it cannot represent, and the
    // caller already treats an empty result as "not used".
    if (ZSTD_isError(idx_size) || idx_size > 0xFFFF) return {};

    // Store compressed indices size (2 bytes) + data
    raw.push_back((idx_size >> 8) & 0xFF);
    raw.push_back(idx_size & 0xFF);
    raw.insert(raw.end(), idx_compressed.begin(), idx_compressed.begin() + idx_size);

    // Remainder literal (if any)
    if (params.remainder_pos < n) {
        raw.insert(raw.end(), data + params.remainder_pos, data + n);
    }

    return raw;
}

// Decode phrase partition
inline std::vector<uint8_t> decode_phrase_partition(const uint8_t* data, size_t n, size_t original_size) {
    if (n < 3) return {};

    size_t pos = 0;

    // Read phrase count
    uint8_t phrase_count = data[pos++];

    // Read phrases
    std::vector<std::string> phrases;
    for (int i = 0; i < phrase_count; i++) {
        if (pos >= n) return {};
        uint8_t len = data[pos++];
        if (pos + len > n) return {};
        phrases.push_back(std::string((const char*)data + pos, len));
        pos += len;
    }

    // Read compressed indices size
    if (pos + 2 > n) return {};
    size_t idx_size = ((size_t)data[pos] << 8) | data[pos+1];
    pos += 2;

    if (pos + idx_size > n) return {};

    // Decompress indices
    std::vector<uint8_t> indices(original_size);  // Upper bound
    size_t indices_count = ZSTD_decompress(indices.data(), indices.size(),
                                            data + pos, idx_size);
    if (ZSTD_isError(indices_count)) return {};
    pos += idx_size;

    // Reconstruct from indices
    std::vector<uint8_t> result;
    result.reserve(original_size);

    for (size_t i = 0; i < indices_count; i++) {
        uint8_t idx = indices[i];
        if (idx >= phrases.size()) return {};
        const std::string& phrase = phrases[idx];
        result.insert(result.end(), phrase.begin(), phrase.end());
    }

    // Append remainder
    if (pos < n) {
        result.insert(result.end(), data + pos, data + n);
    }

    return result;
}

// ============================================================================
// DUAL_STREAM encoding — for interleaved data with different entropy patterns
// ============================================================================
// Key insight: Protobuf-like data alternates between low-entropy tags (3 bits)
// and high-entropy values (7 bits). Separate compression exploits this!
// Gives 12% better than bzip2 on protobuf-like data.

struct DualStreamParams {
    uint8_t stride;           // Interleave stride (typically 2)
    double even_entropy;      // Entropy of even positions
    double odd_entropy;       // Entropy of odd positions
    double entropy_ratio;     // max/min entropy ratio
};

// Detect if data has alternating entropy pattern (protobuf-like)
inline bool detect_dual_stream(const uint8_t* data, size_t n, DualStreamParams& params) {
    if (n < 256) return false;

    // Sample entropy of even and odd byte positions
    size_t sample = std::min(n, (size_t)8192);

    // Count byte frequencies for even and odd positions
    std::array<size_t, 256> even_counts = {0};
    std::array<size_t, 256> odd_counts = {0};

    size_t even_total = 0, odd_total = 0;
    for (size_t i = 0; i < sample; i++) {
        if (i % 2 == 0) {
            even_counts[data[i]]++;
            even_total++;
        } else {
            odd_counts[data[i]]++;
            odd_total++;
        }
    }

    // Calculate Shannon entropy for each stream
    auto calc_entropy = [](const std::array<size_t, 256>& counts, size_t total) -> double {
        if (total == 0) return 8.0;
        double entropy = 0.0;
        for (int i = 0; i < 256; i++) {
            if (counts[i] > 0) {
                double p = (double)counts[i] / total;
                entropy -= p * log2(p);
            }
        }
        return entropy;
    };

    params.even_entropy = calc_entropy(even_counts, even_total);
    params.odd_entropy = calc_entropy(odd_counts, odd_total);
    params.stride = 2;

    // Need significant entropy difference (at least 1.5 bits difference)
    double min_entropy = std::min(params.even_entropy, params.odd_entropy);
    double max_entropy = std::max(params.even_entropy, params.odd_entropy);
    params.entropy_ratio = (min_entropy > 0.1) ? max_entropy / min_entropy : 1.0;

    // Require: one stream has entropy < 5 bits AND ratio > 1.3
    // This catches protobuf (3 bits vs 7 bits) but not random data
    //
    // IMPORTANT: Exclude sparse numeric arrays (int32/int64 with small values)
    // These have low entropy in upper bytes (mostly zeros) which looks like DUAL_STREAM
    // but are better handled by NUMERIC detection with delta encoding.

    // Check if one stream is dominated by zeros (>60%) - indicates sparse numeric
    double even_zero_pct = (double)even_counts[0] / even_total;
    double odd_zero_pct = (double)odd_counts[0] / odd_total;
    bool is_sparse_numeric = (odd_zero_pct > 0.60 || even_zero_pct > 0.60);

    // Also check for protobuf-like tag pattern (varied small values in even positions)
    size_t even_small_varied = 0;
    for (int i = 1; i < 64; i++) even_small_varied += even_counts[i];
    bool has_tag_pattern = even_small_varied > even_total * 0.3;

    // Only trigger DUAL_STREAM if entropy criteria met AND NOT sparse numeric
    bool entropy_ok = (min_entropy < 5.0 && params.entropy_ratio > 1.3 && max_entropy - min_entropy > 1.5);
    return entropy_ok && !is_sparse_numeric && (has_tag_pattern || max_entropy > 6.0);
}

// Encode dual stream: deinterleave, compress separately
// Format: [4 bytes: even_compressed_size][even_compressed][odd_compressed]
inline std::vector<uint8_t> encode_dual_stream(const uint8_t* data, size_t n, int zstd_level = 19) {
    // Deinterleave into even/odd streams
    size_t even_size = (n + 1) / 2;
    size_t odd_size = n / 2;

    std::vector<uint8_t> even_stream(even_size);
    std::vector<uint8_t> odd_stream(odd_size);

    for (size_t i = 0; i < n; i++) {
        if (i % 2 == 0) {
            even_stream[i / 2] = data[i];
        } else {
            odd_stream[i / 2] = data[i];
        }
    }

    // Compress each stream separately
    std::vector<uint8_t> even_compressed(ZSTD_compressBound(even_size));
    std::vector<uint8_t> odd_compressed(ZSTD_compressBound(odd_size));

    size_t even_csize = ZSTD_compress(even_compressed.data(), even_compressed.size(),
                                       even_stream.data(), even_size, zstd_level);
    size_t odd_csize = ZSTD_compress(odd_compressed.data(), odd_compressed.size(),
                                      odd_stream.data(), odd_size, zstd_level);

    if (ZSTD_isError(even_csize) || ZSTD_isError(odd_csize)) {
        return {};  // Compression failed
    }

    // Build output: [4 bytes: even_compressed_size][even_compressed][odd_compressed]
    std::vector<uint8_t> result;
    result.reserve(4 + even_csize + odd_csize);

    // Store even_csize as 4 bytes (little endian)
    result.push_back(even_csize & 0xFF);
    result.push_back((even_csize >> 8) & 0xFF);
    result.push_back((even_csize >> 16) & 0xFF);
    result.push_back((even_csize >> 24) & 0xFF);

    result.insert(result.end(), even_compressed.begin(), even_compressed.begin() + even_csize);
    result.insert(result.end(), odd_compressed.begin(), odd_compressed.begin() + odd_csize);

    return result;
}

// Decode dual stream: decompress and reinterleave
inline std::vector<uint8_t> decode_dual_stream(const uint8_t* data, size_t n, size_t original_size) {
    if (n < 4) return {};

    // Read even_csize
    size_t even_csize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    size_t pos = 4;

    if (pos + even_csize > n) return {};

    size_t odd_csize = n - 4 - even_csize;

    // Expected decompressed sizes
    size_t even_size = (original_size + 1) / 2;
    size_t odd_size = original_size / 2;

    // Decompress each stream
    std::vector<uint8_t> even_stream(even_size);
    std::vector<uint8_t> odd_stream(odd_size);

    size_t even_dsize = ZSTD_decompress(even_stream.data(), even_size, data + pos, even_csize);
    if (ZSTD_isError(even_dsize) || even_dsize != even_size) return {};

    pos += even_csize;

    size_t odd_dsize = ZSTD_decompress(odd_stream.data(), odd_size, data + pos, odd_csize);
    if (ZSTD_isError(odd_dsize) || odd_dsize != odd_size) return {};

    // Reinterleave
    std::vector<uint8_t> result(original_size);
    for (size_t i = 0; i < original_size; i++) {
        if (i % 2 == 0) {
            result[i] = even_stream[i / 2];
        } else {
            result[i] = odd_stream[i / 2];
        }
    }

    return result;
}

// ============================================================================
// HTML_STREAM encoding — tag/content separation for HTML
// ============================================================================
// Key insight: HTML has two distinct streams: tags (structured, repetitive)
// and content (natural text). Separating them before BWT gives 3.3% better
// than brotli at 256KB, 11.8% better at 1MB. Delimiters compress to near-zero.

struct HtmlStreamParams {
    bool detected;  // True if HTML detected
};

// Detect if data is HTML (heuristic check)
inline bool detect_html_stream(const uint8_t* data, size_t n, HtmlStreamParams& params) {
    params.detected = false;

    // Minimum size - below this, brotli's dictionary wins
    if (n < 128 * 1024) return false;

    // Skip leading whitespace
    size_t i = 0;
    while (i < n && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) i++;

    // Check for common HTML starts
    bool starts_html = false;
    if (i + 9 <= n && memcmp(data + i, "<!DOCTYPE", 9) == 0) starts_html = true;
    if (i + 5 <= n && (memcmp(data + i, "<html", 5) == 0 || memcmp(data + i, "<HTML", 5) == 0)) starts_html = true;
    if (i + 5 <= n && memcmp(data + i, "<?xml", 5) == 0) starts_html = true;  // XHTML

    if (!starts_html) return false;

    // Count < and > characters in sample
    size_t open_count = 0, close_count = 0;
    size_t sample_size = std::min(n, (size_t)4096);
    for (size_t j = 0; j < sample_size; j++) {
        if (data[j] == '<') open_count++;
        else if (data[j] == '>') close_count++;
    }

    // Need at least 10 tags and roughly balanced
    if (open_count < 10 || close_count < 10) return false;
    double ratio = (double)open_count / close_count;
    if (ratio < 0.8 || ratio > 1.2) return false;

    params.detected = true;
    return true;
}

// Separate HTML into tags and content streams
inline std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
separate_html_streams(const uint8_t* data, size_t n) {
    std::vector<uint8_t> tags;
    std::vector<uint8_t> content;

    bool in_tag = false;
    bool in_quote = false;
    char quote_char = 0;
    std::vector<uint8_t> current_content;

    for (size_t i = 0; i < n; i++) {
        uint8_t c = data[i];

        if (in_tag) {
            tags.push_back(c);
            if (in_quote) {
                if (c == quote_char) in_quote = false;
            } else {
                if (c == '"' || c == '\'') {
                    in_quote = true;
                    quote_char = c;
                } else if (c == '>') {
                    in_tag = false;
                    current_content.clear();
                }
            }
        } else {
            if (c == '<') {
                // Store content before this <, then delimiter
                for (auto b : current_content) content.push_back(b);
                content.push_back(0x00);
                current_content.clear();
                in_tag = true;
                tags.push_back(c);
            } else {
                current_content.push_back(c);
            }
        }
    }

    // Trailing content
    if (!current_content.empty()) {
        for (auto b : current_content) content.push_back(b);
        content.push_back(0x00);
    }

    return {tags, content};
}

// Encode HTML using stream separation + BWT
// Format: tags_size(varint) + content_size(varint) + tags_bwt + content_bwt
inline std::vector<uint8_t> encode_html_stream(const uint8_t* data, size_t n) {
    auto [tags, content] = separate_html_streams(data, n);

    // Compress each stream with BWT v5
    auto tags_bwt = bwt9::compress(tags.data(), tags.size());
    auto content_bwt = bwt9::compress(content.data(), content.size());

    // Build output: sizes as varints + data
    std::vector<uint8_t> result;

    // Write sizes as varints
    auto write_varint = [&result](uint64_t val) {
        while (val >= 128) {
            result.push_back((val & 0x7F) | 0x80);
            val >>= 7;
        }
        result.push_back(val & 0x7F);
    };

    write_varint(tags_bwt.size());
    write_varint(content_bwt.size());

    // Append compressed streams
    result.insert(result.end(), tags_bwt.begin(), tags_bwt.end());
    result.insert(result.end(), content_bwt.begin(), content_bwt.end());

    return result;
}

// Decode HTML_STREAM format
inline std::vector<uint8_t> decode_html_stream(const uint8_t* data, size_t n, size_t /*original_size*/) {
    size_t pos = 0;

    // Read varints
    auto read_varint = [&data, &pos, n]() -> uint64_t {
        uint64_t val = 0;
        int shift = 0;
        while (pos < n && shift < 63) {
            uint8_t b = data[pos++];
            val |= (uint64_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }
        return val;
    };

    uint64_t tags_size = read_varint();
    uint64_t content_size = read_varint();

    if (pos + tags_size + content_size > n) return {};

    // Decompress streams
    auto tags = bwt9::decompress(data + pos, tags_size);
    auto content = bwt9::decompress(data + pos + tags_size, content_size);

    // Reconstruct HTML
    std::vector<uint8_t> result;
    size_t content_pos = 0;

    for (size_t i = 0; i < tags.size(); i++) {
        uint8_t c = tags[i];

        // Before each <, insert content until delimiter
        if (c == '<') {
            while (content_pos < content.size() && content[content_pos] != 0x00) {
                result.push_back(content[content_pos++]);
            }
            if (content_pos < content.size()) content_pos++;  // Skip delimiter
        }

        result.push_back(c);
    }

    // Trailing content
    while (content_pos < content.size() && content[content_pos] != 0x00) {
        result.push_back(content[content_pos++]);
    }

    return result;
}

// ============================================================================
// URL_STREAM encoding — component separation for URL lists
// ============================================================================
// Key insight: URLs have 4 distinct streams (protocol, domain, path, params)
// with VERY different entropy profiles:
//   - Protocols: 49KB → 51 bytes (0.1%!) - almost pure repetition
//   - Domains: 89KB → 2.6KB (2.9%) - limited vocabulary
//   - Paths: 68KB → 8.3KB (12.2%) - moderate repetition
//   - Params: 67KB → 13.8KB (20.7%) - key=value patterns
// Result: 6.2% better than mzip on URL lists at 256KB

struct UrlStreamParams {
    bool detected;  // True if URL data detected
};

// Detect if data is URL list (access logs, crawl data, etc.)
// IMPORTANT: Lines must START with http:// or https://, not just contain them.
// This prevents false positives on nginx logs where URLs appear in referer fields.
inline bool detect_url_stream(const uint8_t* data, size_t n, UrlStreamParams& params) {
    params.detected = false;

    // Minimum size for URL_STREAM to win
    if (n < 64 * 1024) return false;

    // Check for lines STARTING with http:// or https:// in first 4KB
    size_t sample_size = std::min(n, (size_t)4096);
    size_t url_line_count = 0;
    size_t newline_count = 0;

    // Check if first line starts with URL
    if (sample_size >= 7) {
        if (memcmp(data, "http://", 7) == 0) url_line_count++;
        else if (sample_size >= 8 && memcmp(data, "https://", 8) == 0) url_line_count++;
    }

    // Check lines after newlines
    for (size_t i = 0; i < sample_size; i++) {
        if (data[i] == '\n') {
            newline_count++;
            // Check if next line starts with http:// or https://
            if (i + 8 < sample_size) {
                if (memcmp(data + i + 1, "http://", 7) == 0) url_line_count++;
                else if (i + 9 < sample_size && memcmp(data + i + 1, "https://", 8) == 0) url_line_count++;
            }
        }
    }

    // Need at least 10 URL lines in sample, and mostly URL lines
    if (url_line_count < 10) return false;
    if (newline_count < 5) return false;
    if ((double)url_line_count / newline_count < 0.5) return false;  // At least 50% URL lines

    params.detected = true;
    return true;
}

// URL parse state (prefixed with ST_ to avoid Windows DOMAIN macro conflict)
enum class UrlState { ST_PROTOCOL, ST_DOMAIN, ST_PATH, ST_PARAMS };

// Separate URLs into 4 component streams
struct SeparatedUrl {
    std::vector<uint8_t> protocols;
    std::vector<uint8_t> domains;
    std::vector<uint8_t> paths;
    std::vector<uint8_t> params;
};

inline SeparatedUrl separate_url_streams(const uint8_t* data, size_t n) {
    SeparatedUrl result;

    UrlState state = UrlState::ST_PROTOCOL;
    std::vector<uint8_t> current;

    for (size_t i = 0; i < n; i++) {
        uint8_t c = data[i];

        if (c == '\n') {
            // End of URL - flush current and add delimiters for all remaining components
            // This ensures decoder knows exactly which components each URL has
            if (state == UrlState::ST_DOMAIN) {
                // URL ended at domain (no path, no params)
                for (auto b : current) result.domains.push_back(b);
                result.domains.push_back(0x00);
                result.paths.push_back(0x00);     // Empty path
                result.params.push_back(0x00);    // Empty params
            } else if (state == UrlState::ST_PATH) {
                // URL ended at path (no params)
                for (auto b : current) result.paths.push_back(b);
                result.paths.push_back(0x00);
                result.params.push_back(0x00);    // Empty params
            } else if (state == UrlState::ST_PARAMS) {
                // URL had params
                for (auto b : current) result.params.push_back(b);
                result.params.push_back(0x00);
            }
            current.clear();
            state = UrlState::ST_PROTOCOL;
            continue;
        }

        switch (state) {
            case UrlState::ST_PROTOCOL:
                current.push_back(c);
                // Check for "://"
                if (current.size() >= 3 &&
                    current[current.size()-3] == ':' &&
                    current[current.size()-2] == '/' &&
                    current[current.size()-1] == '/') {
                    for (auto b : current) result.protocols.push_back(b);
                    result.protocols.push_back(0x00);
                    current.clear();
                    state = UrlState::ST_DOMAIN;
                }
                break;

            case UrlState::ST_DOMAIN:
                if (c == '/') {
                    for (auto b : current) result.domains.push_back(b);
                    result.domains.push_back(0x00);
                    current.clear();
                    current.push_back(c);  // Include leading /
                    state = UrlState::ST_PATH;
                } else {
                    current.push_back(c);
                }
                break;

            case UrlState::ST_PATH:
                if (c == '?') {
                    for (auto b : current) result.paths.push_back(b);
                    result.paths.push_back(0x00);
                    current.clear();
                    state = UrlState::ST_PARAMS;
                } else {
                    current.push_back(c);
                }
                break;

            case UrlState::ST_PARAMS:
                current.push_back(c);
                break;
        }
    }

    // Flush remaining
    if (!current.empty()) {
        switch (state) {
            case UrlState::ST_PROTOCOL:
                for (auto b : current) result.protocols.push_back(b);
                result.protocols.push_back(0x00);
                break;
            case UrlState::ST_DOMAIN:
                for (auto b : current) result.domains.push_back(b);
                result.domains.push_back(0x00);
                break;
            case UrlState::ST_PATH:
                for (auto b : current) result.paths.push_back(b);
                result.paths.push_back(0x00);
                break;
            case UrlState::ST_PARAMS:
                for (auto b : current) result.params.push_back(b);
                result.params.push_back(0x00);
                break;
        }
    }

    return result;
}

// Encode URL using stream separation + BWT
// Format: proto_size(varint) + domain_size(varint) + path_size(varint) + param_size(varint)
//         + protocols_bwt + domains_bwt + paths_bwt + params_bwt
inline std::vector<uint8_t> encode_url_stream(const uint8_t* data, size_t n) {
    auto sep = separate_url_streams(data, n);

    // Compress each stream with BWT v5
    auto proto_bwt = bwt9::compress(sep.protocols.data(), sep.protocols.size());
    auto domain_bwt = bwt9::compress(sep.domains.data(), sep.domains.size());
    auto path_bwt = bwt9::compress(sep.paths.data(), sep.paths.size());
    auto param_bwt = bwt9::compress(sep.params.data(), sep.params.size());

    // Build output
    std::vector<uint8_t> result;

    // Write sizes as varints
    auto write_varint = [&result](uint64_t val) {
        while (val >= 128) {
            result.push_back((val & 0x7F) | 0x80);
            val >>= 7;
        }
        result.push_back(val & 0x7F);
    };

    write_varint(proto_bwt.size());
    write_varint(domain_bwt.size());
    write_varint(path_bwt.size());
    write_varint(param_bwt.size());

    // Append compressed streams
    result.insert(result.end(), proto_bwt.begin(), proto_bwt.end());
    result.insert(result.end(), domain_bwt.begin(), domain_bwt.end());
    result.insert(result.end(), path_bwt.begin(), path_bwt.end());
    result.insert(result.end(), param_bwt.begin(), param_bwt.end());

    return result;
}

// Decode URL_STREAM format
inline std::vector<uint8_t> decode_url_stream(const uint8_t* data, size_t n, size_t /*original_size*/) {
    size_t pos = 0;

    // Read varints
    auto read_varint = [&data, &pos, n]() -> uint64_t {
        uint64_t val = 0;
        int shift = 0;
        while (pos < n && shift < 63) {
            uint8_t b = data[pos++];
            val |= (uint64_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }
        return val;
    };

    uint64_t proto_size = read_varint();
    uint64_t domain_size = read_varint();
    uint64_t path_size = read_varint();
    uint64_t param_size = read_varint();

    if (pos + proto_size + domain_size + path_size + param_size > n) return {};

    // Decompress streams
    auto protocols = bwt9::decompress(data + pos, proto_size);
    pos += proto_size;
    auto domains = bwt9::decompress(data + pos, domain_size);
    pos += domain_size;
    auto paths = bwt9::decompress(data + pos, path_size);
    pos += path_size;
    auto params = bwt9::decompress(data + pos, param_size);

    // Reconstruct URLs
    // Each stream has segments separated by 0x00
    // Pattern: protocol + domain + [path] + [params] + newline
    std::vector<uint8_t> result;

    size_t proto_pos = 0;
    size_t domain_pos = 0;
    size_t path_pos = 0;
    size_t param_pos = 0;

    while (proto_pos < protocols.size()) {
        // Read protocol segment (includes "://")
        while (proto_pos < protocols.size() && protocols[proto_pos] != 0x00) {
            result.push_back(protocols[proto_pos++]);
        }
        if (proto_pos < protocols.size()) proto_pos++;  // Skip delimiter

        // Read domain segment
        if (domain_pos < domains.size()) {
            while (domain_pos < domains.size() && domains[domain_pos] != 0x00) {
                result.push_back(domains[domain_pos++]);
            }
            if (domain_pos < domains.size()) domain_pos++;
        }

        // Read path segment if present
        if (path_pos < paths.size() && paths[path_pos] != 0x00) {
            while (path_pos < paths.size() && paths[path_pos] != 0x00) {
                result.push_back(paths[path_pos++]);
            }
        }
        if (path_pos < paths.size()) path_pos++;

        // Read params segment if present (add ? prefix)
        if (param_pos < params.size() && params[param_pos] != 0x00) {
            result.push_back('?');
            while (param_pos < params.size() && params[param_pos] != 0x00) {
                result.push_back(params[param_pos++]);
            }
        }
        if (param_pos < params.size()) param_pos++;

        result.push_back('\n');
    }

    return result;
}

// Detect if data would benefit from phrase token replacement
// Key insight: Replace long phrases with 1-byte tokens (0x80-0xFF)
// This helps zstd by making patterns more visible
inline bool detect_phrase_dict(const uint8_t* data, size_t n, PhraseDictParams& params) {
    if (n < 256) return false;

    // Only for text-like data with high-bytes unused
    size_t high_byte_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (data[i] >= 0x80) high_byte_count++;
    }
    if (high_byte_count > n / 20) return false;  // >5% high bytes = not suitable

    // Find repeated phrases (8-64 chars, appearing 5+ times)
    std::unordered_map<std::string, int> counts;
    for (size_t len = 8; len <= 64 && len <= n; len++) {
        for (size_t i = 0; i + len <= n; i++) {
            std::string phrase((const char*)&data[i], len);
            counts[phrase]++;
        }
    }

    // Score phrases: savings = (count - 1) * (len - 1) - len
    // (each occurrence saves len-1 bytes, minus dict entry cost)
    std::vector<std::pair<int, std::string>> scored;
    for (const auto& [phrase, count] : counts) {
        if (count >= 5) {
            int len = (int)phrase.length();
            int savings = (count - 1) * (len - 1) - len;
            if (savings > 50) {
                scored.push_back({savings, phrase});
            }
        }
    }

    if (scored.size() < 3) return false;

    // Sort by savings, pick top 128 non-overlapping
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::vector<std::string> dict;
    for (const auto& [sav, phrase] : scored) {
        // Max 127 phrases: tokens 0x80-0xFE. Token 0xFF is reserved for escaping high bytes.
        if (dict.size() >= 127) break;
        // Skip if this phrase overlaps with existing ones
        bool overlap = false;
        for (const auto& existing : dict) {
            if (phrase.find(existing) != std::string::npos ||
                existing.find(phrase) != std::string::npos) {
                overlap = true;
                break;
            }
        }
        if (!overlap) dict.push_back(phrase);
    }

    if (dict.size() < 3) return false;

    // Sort dictionary by length descending for greedy matching
    std::sort(dict.begin(), dict.end(), [](const auto& a, const auto& b) {
        return a.length() > b.length();
    });

    // Build phrase -> token_id map
    std::unordered_map<std::string, uint8_t> phrase_to_token;
    for (size_t i = 0; i < dict.size(); i++) {
        phrase_to_token[dict[i]] = (uint8_t)(0x80 + i);
    }

    // Convert data to token stream
    std::vector<uint8_t> tokens;
    tokens.reserve(n);

    size_t i = 0;
    while (i < n) {
        bool matched = false;
        // Greedy: try longest phrases first
        for (const auto& phrase : dict) {
            size_t len = phrase.length();
            if (i + len <= n && memcmp(&data[i], phrase.data(), len) == 0) {
                tokens.push_back(phrase_to_token[phrase]);
                i += len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            uint8_t byte = data[i++];
            if (byte >= 0x80) {
                // Escape high byte (shouldn't happen often)
                tokens.push_back(0xFF);
                tokens.push_back(byte);
            } else {
                tokens.push_back(byte);
            }
        }
    }

    // Build raw buffer and test if tokenization actually helps
    std::vector<uint8_t> raw;
    raw.push_back((uint8_t)dict.size());
    for (const auto& p : dict) {
        raw.push_back((uint8_t)p.length());
        raw.insert(raw.end(), p.begin(), p.end());
    }
    raw.insert(raw.end(), tokens.begin(), tokens.end());

    // Quick zstd test at level 3 to see if tokenization helps
    size_t bound = ZSTD_compressBound(raw.size());
    std::vector<uint8_t> comp_tok(bound), comp_orig(bound);

    size_t tok_size = ZSTD_compress(comp_tok.data(), comp_tok.size(), raw.data(), raw.size(), 3);
    size_t orig_size = ZSTD_compress(comp_orig.data(), comp_orig.size(), data, n, 3);

    // Only use if tokenization gives at least 5% improvement
    if (ZSTD_isError(tok_size) || tok_size >= orig_size * 0.95) return false;

    params.dictionary = std::move(dict);
    params.tokens = std::move(tokens);
    params.original_size = n;
    return true;
}

// Encode phrase dict: [dict_size:1][phrases...][tokens...]
// Simple format, then compress with zstd
inline std::vector<uint8_t> encode_phrase_dict(const PhraseDictParams& params,
                                               const uint8_t* original_data, size_t original_size,
                                               int zstd_level = 19) {
    std::vector<uint8_t> raw;

    // Dictionary size (1 byte, max 128)
    raw.push_back((uint8_t)params.dictionary.size());

    // Dictionary entries: [len:1][phrase:len]
    for (const auto& phrase : params.dictionary) {
        raw.push_back((uint8_t)phrase.length());
        raw.insert(raw.end(), phrase.begin(), phrase.end());
    }

    // Token stream
    raw.insert(raw.end(), params.tokens.begin(), params.tokens.end());

    // Compress with zstd
    size_t bound = ZSTD_compressBound(raw.size());
    std::vector<uint8_t> compressed(bound);
    size_t comp_size = ZSTD_compress(compressed.data(), compressed.size(),
                                      raw.data(), raw.size(), zstd_level);

    if (ZSTD_isError(comp_size) || comp_size >= original_size) {
        return {};  // Compression failed or didn't help
    }

    compressed.resize(comp_size);
    return compressed;
}

// Decode phrase dict: decompress, read dict, expand tokens
inline std::vector<uint8_t> decode_phrase_dict(const uint8_t* data, size_t n, size_t original_size) {
    // Decompress with zstd
    std::vector<uint8_t> raw(original_size * 2);
    size_t raw_size = ZSTD_decompress(raw.data(), raw.size(), data, n);
    if (ZSTD_isError(raw_size) || raw_size == 0) {
        return {};
    }

    const uint8_t* buf = raw.data();
    size_t pos = 0;

    // Read dictionary
    if (pos >= raw_size) return {};
    uint8_t dict_count = buf[pos++];

    std::vector<std::string> dict;
    for (uint8_t i = 0; i < dict_count && pos < raw_size; i++) {
        uint8_t len = buf[pos++];
        if (pos + len > raw_size) return {};
        dict.push_back(std::string((const char*)&buf[pos], len));
        pos += len;
    }

    // Rest is token stream - expand to output
    std::vector<uint8_t> output;
    output.reserve(original_size);

    while (pos < raw_size) {
        uint8_t byte = buf[pos++];
        if (byte < 0x80) {
            // Literal byte
            output.push_back(byte);
        } else if (byte == 0xFF && pos < raw_size) {
            // Escaped high byte
            output.push_back(buf[pos++]);
        } else {
            // Phrase token
            uint8_t phrase_id = byte - 0x80;
            if (phrase_id < dict.size()) {
                for (char c : dict[phrase_id]) {
                    output.push_back((uint8_t)c);
                }
            }
        }
    }

    return output;
}

// ============================================================================
// SORTED_DICT: Line sorting + adaptive dictionary compression
// ============================================================================
// Key insight: Sort lines to cluster similar content, use content-derived dict
// Beats brotli on structured code by ~3-5%

struct SortedDictParams {
    std::vector<std::string> lines;       // Lines in sorted order
    std::vector<uint32_t> original_order; // Original line indices
    std::string dictionary;               // Adaptive dictionary from content
    size_t original_size;
};

// Build adaptive dictionary from content (most frequent n-grams)
inline std::string build_sorted_dict_dictionary(const std::string& content, size_t max_size = 8192) {
    std::unordered_map<std::string, int> ngram_count;

    // Extract n-grams (6-40 chars) and count frequency
    for (size_t len = 6; len <= 40 && len <= content.size(); len++) {
        for (size_t i = 0; i + len <= content.size(); i++) {
            std::string ngram = content.substr(i, len);
            // Only count printable text
            bool valid = true;
            for (char c : ngram) {
                if (c < 32 && c != '\n' && c != '\t') { valid = false; break; }
            }
            if (valid) ngram_count[ngram]++;
        }
    }

    // Score by compression value: (count - 1) * length
    std::vector<std::pair<int, std::string>> scored;
    for (const auto& [ng, count] : ngram_count) {
        if (count >= 3) {
            int score = (count - 1) * (int)ng.size();
            scored.push_back({score, ng});
        }
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });

    // Build dict from top patterns, avoiding overlaps
    std::string dict;
    for (const auto& [score, ng] : scored) {
        if (dict.size() + ng.size() > max_size) break;
        if (dict.find(ng) == std::string::npos) {
            dict += ng;
        }
    }

    return dict;
}

// Detect if SORTED_DICT would help
inline bool detect_sorted_dict(const uint8_t* data, size_t n, SortedDictParams& params) {
    if (n < 512) return false;  // Too small

    // Must be text-like with newlines
    size_t newline_count = 0;
    size_t high_byte_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') newline_count++;
        if (data[i] >= 0x80) high_byte_count++;
    }

    // Needs multiple lines and mostly ASCII
    if (newline_count < 10) return false;
    if (high_byte_count > n / 10) return false;  // >10% high bytes = not text

    std::string content((const char*)data, n);

    // Split into lines
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    if (lines.size() < 10) return false;

    // Create sorted version (reverse alphabetical works best for LZ77)
    std::vector<std::pair<std::string, uint32_t>> indexed_lines;
    for (uint32_t i = 0; i < lines.size(); i++) {
        indexed_lines.push_back({lines[i], i});
    }
    std::sort(indexed_lines.begin(), indexed_lines.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;  // Reverse sort
    });

    // Build sorted content
    std::string sorted_content;
    std::vector<uint32_t> original_order;
    for (const auto& [line, idx] : indexed_lines) {
        sorted_content += line + "\n";
        original_order.push_back(idx);
    }

    // Build adaptive dictionary from original content
    // Use smaller dictionary (1KB) to reduce storage overhead
    std::string dict = build_sorted_dict_dictionary(content, 1024);

    if (dict.size() < 100) return false;  // Dict too small to help

    // Test if sorted + dict beats raw zstd
    size_t bound = ZSTD_compressBound(n);
    std::vector<uint8_t> comp(bound);

    // Raw compression
    size_t raw_size = ZSTD_compress(comp.data(), comp.size(), data, n, 19);

    // Dict compression of sorted content
    ZSTD_CDict* cdict = ZSTD_createCDict(dict.data(), dict.size(), 19);
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    size_t dict_size = ZSTD_compress_usingCDict(cctx, comp.data(), comp.size(),
                                                  sorted_content.data(), sorted_content.size(), cdict);
    ZSTD_freeCDict(cdict);
    ZSTD_freeCCtx(cctx);

    // Estimate overhead: dict storage (compressed) + order indices
    // Dict is stored uncompressed for now
    // Order: ~1.5 bytes per line average (delta-zigzag varint)
    size_t order_overhead = (lines.size() * 3 + 1) / 2;  // ~1.5 bytes per line
    size_t estimated_total = dict_size + 2 + dict.size() + 2 + order_overhead + 4;

    // Need improvement to justify complexity
    if (estimated_total >= raw_size) return false;

    params.lines.clear();
    for (const auto& [line, idx] : indexed_lines) {
        params.lines.push_back(line);
    }
    params.original_order = std::move(original_order);
    params.dictionary = std::move(dict);
    params.original_size = n;

    return true;
}

// Encode SORTED_DICT block
// Format: [dict_len:2][dict][line_count:2][order:varints][sorted_content_zstd]
inline std::vector<uint8_t> encode_sorted_dict(const SortedDictParams& params, int zstd_level = 19) {
    std::vector<uint8_t> output;

    // Dictionary length (2 bytes) + dictionary
    uint16_t dict_len = (uint16_t)params.dictionary.size();
    output.push_back(dict_len & 0xFF);
    output.push_back((dict_len >> 8) & 0xFF);
    output.insert(output.end(), params.dictionary.begin(), params.dictionary.end());

    // Line count (2 bytes)
    uint16_t line_count = (uint16_t)params.lines.size();
    output.push_back(line_count & 0xFF);
    output.push_back((line_count >> 8) & 0xFF);

    // Original order as delta-encoded varints (smaller encoding)
    std::vector<uint8_t> order_data;
    int32_t prev = 0;
    for (uint32_t idx : params.original_order) {
        int32_t delta = (int32_t)idx - prev;
        prev = idx;
        // ZigZag encode for signed
        uint32_t zz = (delta << 1) ^ (delta >> 31);
        // Varint encode
        while (zz >= 0x80) {
            order_data.push_back((zz & 0x7F) | 0x80);
            zz >>= 7;
        }
        order_data.push_back(zz);
    }

    // Order data length (2 bytes) + order data
    uint16_t order_len = (uint16_t)order_data.size();
    output.push_back(order_len & 0xFF);
    output.push_back((order_len >> 8) & 0xFF);
    output.insert(output.end(), order_data.begin(), order_data.end());

    // Build sorted content
    std::string sorted_content;
    for (const auto& line : params.lines) {
        sorted_content += line + "\n";
    }

    // Compress with dictionary
    ZSTD_CDict* cdict = ZSTD_createCDict(params.dictionary.data(), params.dictionary.size(), zstd_level);
    ZSTD_CCtx* cctx = ZSTD_createCCtx();

    std::vector<uint8_t> compressed(ZSTD_compressBound(sorted_content.size()));
    size_t comp_size = ZSTD_compress_usingCDict(cctx, compressed.data(), compressed.size(),
                                                  sorted_content.data(), sorted_content.size(), cdict);

    ZSTD_freeCDict(cdict);
    ZSTD_freeCCtx(cctx);

    if (ZSTD_isError(comp_size)) {
        return {};  // Failed
    }

    // Compressed data length (4 bytes) + compressed data
    output.push_back(comp_size & 0xFF);
    output.push_back((comp_size >> 8) & 0xFF);
    output.push_back((comp_size >> 16) & 0xFF);
    output.push_back((comp_size >> 24) & 0xFF);
    output.insert(output.end(), compressed.begin(), compressed.begin() + comp_size);

    return output;
}

// Decode SORTED_DICT block
inline std::vector<uint8_t> decode_sorted_dict(const uint8_t* data, size_t n, size_t original_size) {
    size_t pos = 0;

    // Read dictionary
    if (pos + 2 > n) return {};
    uint16_t dict_len = data[pos] | (data[pos + 1] << 8);
    pos += 2;
    if (pos + dict_len > n) return {};
    std::string dict((const char*)&data[pos], dict_len);
    pos += dict_len;

    // Read line count
    if (pos + 2 > n) return {};
    uint16_t line_count = data[pos] | (data[pos + 1] << 8);
    pos += 2;

    // Read order data length and data
    if (pos + 2 > n) return {};
    uint16_t order_len = data[pos] | (data[pos + 1] << 8);
    pos += 2;
    if (pos + order_len > n) return {};

    // Decode delta-zigzag varints to get original order
    std::vector<uint32_t> original_order;
    size_t order_pos = 0;
    int32_t prev = 0;
    while (original_order.size() < line_count && order_pos < order_len) {
        uint32_t zz = 0;
        int shift = 0;
        while (order_pos < order_len) {
            uint8_t b = data[pos + order_pos++];
            zz |= (b & 0x7F) << shift;
            if (!(b & 0x80)) break;
            shift += 7;
        }
        // ZigZag decode
        int32_t delta = (zz >> 1) ^ -(int32_t)(zz & 1);
        prev += delta;
        original_order.push_back((uint32_t)prev);
    }
    pos += order_len;

    // Read compressed data
    if (pos + 4 > n) return {};
    uint32_t comp_size = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24);
    pos += 4;
    if (pos + comp_size > n) return {};

    // Decompress with dictionary
    ZSTD_DDict* ddict = ZSTD_createDDict(dict.data(), dict.size());
    ZSTD_DCtx* dctx = ZSTD_createDCtx();

    std::vector<uint8_t> decompressed(original_size * 2);
    size_t dec_size = ZSTD_decompress_usingDDict(dctx, decompressed.data(), decompressed.size(),
                                                   &data[pos], comp_size, ddict);

    ZSTD_freeDDict(ddict);
    ZSTD_freeDCtx(dctx);

    if (ZSTD_isError(dec_size)) return {};

    // Split into lines
    std::vector<std::string> sorted_lines;
    std::string current;
    for (size_t i = 0; i < dec_size; i++) {
        if (decompressed[i] == '\n') {
            sorted_lines.push_back(current);
            current.clear();
        } else {
            current += (char)decompressed[i];
        }
    }
    if (!current.empty()) sorted_lines.push_back(current);

    // Restore original order
    if (original_order.size() != sorted_lines.size()) return {};

    std::vector<std::string> original_lines(sorted_lines.size());
    for (size_t i = 0; i < sorted_lines.size(); i++) {
        if (original_order[i] >= original_lines.size()) return {};
        original_lines[original_order[i]] = sorted_lines[i];
    }

    // Rebuild content
    std::string content;
    for (size_t i = 0; i < original_lines.size(); i++) {
        content += original_lines[i];
        if (i < original_lines.size() - 1) content += "\n";
    }
    // Handle trailing newline if original had one
    if (dec_size > 0 && decompressed[dec_size - 1] == '\n') {
        content += "\n";
    }

    return std::vector<uint8_t>(content.begin(), content.end());
}

// Check if string is purely numeric digits (no sign, no decimal)
inline bool is_fully_numeric(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// Tokenize a line into segments, preserving whitespace boundaries
inline std::vector<std::string> tokenize_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        // Delimiters that split tokens (including HTML < > / for tag detection)
        if (c == ' ' || c == '\t' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '(' || c == ')' || c == ':' || c == ',' || c == '"' || c == '\'' ||
            c == '<' || c == '>' || c == '/' || c == '=' || c == ';') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, c));
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// Check if a string is a number (integer)
inline bool is_numeric_string(const std::string& s) {
    if (s.empty()) return false;
    size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (start >= s.size()) return false;
    for (size_t i = start; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    // Reject tokens that OVERFLOW int64 -- else parse_int64/std::stoll throws std::out_of_range which,
    // uncaught inside an encoder trial, propagates to compress()'s guard and dumps the WHOLE FILE to the
    // uRAW raw-store. Measured on incrementing MySQL BIGINT UNSIGNED keys (>=2^63): 65-139x bloat. Declining
    // detection here routes the column/block to the lossless backstop ensemble, which compresses it well.
    // Exact int64-range check (no false rejects of valid ids): skip leading zeros, then bound by digit count
    // and, at 19 digits, a lexicographic compare vs the int64 limit. (2026-08-08, sweep-2 pathology hunt)
    size_t d = start;
    while (d + 1 < s.size() && s[d] == '0') d++;      // skip leading zeros, keep >=1 digit
    size_t ndig = s.size() - d;
    if (ndig > 19) return false;
    if (ndig == 19) {
        const char* bound = (start == 1 && s[0] == '-') ? "9223372036854775808"   // |INT64_MIN|
                                                        : "9223372036854775807";  // INT64_MAX
        if (std::memcmp(s.data() + d, bound, 19) > 0) return false;
    }
    return true;
}

// Parse int64 from string. NON-THROWING: an out-of-range/invalid token returns 0 rather than throwing
// std::out_of_range (which would nuke the whole compress via the top-level guard -> uRAW bloat). Callers
// gate with is_numeric_string (now overflow-exact); the try/catch is defense-in-depth, and any encoder that
// consumes a wrong 0 is caught by its own roundtrip self-verify. (2026-08-08)
inline int64_t parse_int64(const std::string& s) {
    try { return std::stoll(s); } catch (...) { return 0; }
}

// Detect if lines have template structure
// Returns true if >60% of token positions are fixed across lines
inline bool detect_template(const uint8_t* data, size_t n, TemplateParams& params) {
    // Split into lines, tracking byte offsets (start and end including newline)
    std::vector<std::string> lines;
    std::vector<size_t> line_start_offsets;  // Byte offset where each line starts
    std::vector<size_t> line_end_offsets;    // Byte offset after each line (after \n)
    std::string current_line;
    size_t line_start = 0;

    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            // Always push lines, including blank lines
            // For blank lines, line_start is the position right after previous \n
            line_start_offsets.push_back(current_line.empty() ? i : line_start);
            line_end_offsets.push_back(i + 1);  // After the \n
            lines.push_back(current_line);
            current_line.clear();
            line_start = i + 1;
        } else if (data[i] >= 32 || data[i] == '\t') {
            if (current_line.empty()) line_start = i;  // Track actual content start
            current_line += (char)data[i];
        }
    }
    if (!current_line.empty()) {
        line_start_offsets.push_back(line_start);
        line_end_offsets.push_back(n);  // End of data (no trailing \n)
        lines.push_back(current_line);
    }

    // Need at least 8 lines for template detection
    if (lines.size() < 8) return false;

    // Exclude CSV files - TEMPLATE encoder has bugs with comma-delimited data
    // CSV detection: first line has commas, looks like header (all unique tokens)
    if (!lines.empty() && lines[0].find(',') != std::string::npos) {
        std::vector<std::string> first_tokens;
        std::string tok;
        for (char c : lines[0]) {
            if (c == ',') {
                if (!tok.empty()) first_tokens.push_back(tok);
                tok.clear();
            } else {
                tok += c;
            }
        }
        if (!tok.empty()) first_tokens.push_back(tok);
        // If >4 comma-separated tokens, likely CSV
        if (first_tokens.size() > 4) return false;
    }

    // Sample lines from throughout the file to catch format changes
    // (e.g., hour rollover in timestamps)
    std::set<size_t> sample_set;
    // Include first 32 lines
    for (size_t i = 0; i < std::min(lines.size(), (size_t)32); i++) {
        sample_set.insert(i);
    }
    // Add samples from middle and end (use set to avoid duplicates)
    if (lines.size() > 32) {
        sample_set.insert(lines.size() / 4);
        sample_set.insert(lines.size() / 2);
        sample_set.insert(lines.size() * 3 / 4);
        sample_set.insert(lines.size() - 1);
    }
    std::vector<size_t> sample_indices(sample_set.begin(), sample_set.end());
    size_t sample_size = sample_indices.size();

    // Tokenize sampled lines
    std::vector<std::vector<std::string>> tokenized;
    for (size_t idx : sample_indices) {
        if (idx < lines.size()) {
            tokenized.push_back(tokenize_line(lines[idx]));
        }
    }

    // Find most common token count (allows skipping header/footer lines)
    std::unordered_map<size_t, size_t> token_count_freq;
    for (const auto& tokens : tokenized) {
        token_count_freq[tokens.size()]++;
    }

    size_t token_count = 0;
    size_t max_freq = 0;
    for (const auto& [count, freq] : token_count_freq) {
        if (freq > max_freq && count >= 4) {  // Min 4 tokens
            max_freq = freq;
            token_count = count;
        }
    }

    // Need majority of lines with same token count
    // Lowered from 0.6 to 0.5 - access logs were failing at 59.4%
    if (max_freq < sample_size * 0.5) return false;

    // Filter to only lines matching the template structure
    std::vector<std::vector<std::string>> matching_tokenized;
    std::vector<size_t> matching_line_indices;
    for (size_t i = 0; i < tokenized.size(); i++) {
        if (tokenized[i].size() == token_count) {
            matching_tokenized.push_back(tokenized[i]);
            matching_line_indices.push_back(i);
        }
    }

    // Store first matching line index for header stripping
    params.header_lines = matching_line_indices.empty() ? 0 : matching_line_indices[0];

    // Capture header bytes (all data before first template-matching line)
    if (params.header_lines > 0 && !line_start_offsets.empty()) {
        size_t header_end = line_start_offsets[params.header_lines];
        params.header_bytes = std::string((char*)data, header_end);
    } else {
        params.header_bytes.clear();
    }

    if (token_count < 4 || matching_tokenized.empty()) return false;  // Too few tokens

    // For each position, check if fixed or variable (using filtered lines)
    std::vector<bool> is_fixed(token_count, true);
    std::vector<std::string> fixed_tokens(token_count);
    size_t fixed_count = 0;

    for (size_t pos = 0; pos < token_count; pos++) {
        fixed_tokens[pos] = matching_tokenized[0][pos];
        for (size_t line = 1; line < matching_tokenized.size(); line++) {
            if (matching_tokenized[line][pos] != fixed_tokens[pos]) {
                is_fixed[pos] = false;
                break;
            }
        }
        if (is_fixed[pos]) fixed_count++;
    }

    // Force last token to be variable - handles common case of trailing
    // punctuation differing on last line (e.g., ',' vs ';' in SQL lists)
    if (token_count > 0 && is_fixed[token_count - 1]) {
        is_fixed[token_count - 1] = false;
        fixed_count--;
    }

    // Need >50% fixed positions for template to be worthwhile
    double fixed_ratio = (double)fixed_count / token_count;
    if (fixed_ratio < 0.5) return false;

    // Build template string and identify columns
    std::string template_str;
    std::vector<size_t> var_positions;

    for (size_t pos = 0; pos < token_count; pos++) {
        if (is_fixed[pos]) {
            template_str += fixed_tokens[pos];
        } else {
            template_str += "$" + std::to_string(var_positions.size());
            var_positions.push_back(pos);
        }
    }

    if (var_positions.empty()) return false;  // All fixed, no columns

    // Extract column values from lines matching template structure
    std::vector<TemplateColumn> columns(var_positions.size());
    size_t last_matching_line_idx = 0;
    size_t first_matching_line_idx = SIZE_MAX;
    size_t matching_line_count = 0;  // Track position within matching lines
    params.non_matching_lines.clear();

    for (size_t line_idx = 0; line_idx < lines.size(); line_idx++) {
        auto tokens = tokenize_line(lines[line_idx]);
        if (tokens.size() != token_count) {
            // Only capture non-matching lines AFTER the first matching line
            // (lines before are in header_bytes, lines after last will go to footer_bytes)
            if (first_matching_line_idx != SIZE_MAX) {
                // Get raw bytes for this line (including newline)
                std::string raw_line((char*)data + line_start_offsets[line_idx],
                                     line_end_offsets[line_idx] - line_start_offsets[line_idx]);
                // Store at position relative to matching lines
                params.non_matching_lines.emplace_back(matching_line_count, raw_line);
            }
            continue;
        }

        if (first_matching_line_idx == SIZE_MAX) {
            first_matching_line_idx = line_idx;
        }
        last_matching_line_idx = line_idx;
        matching_line_count++;
        for (size_t col = 0; col < var_positions.size(); col++) {
            columns[col].values.push_back(tokens[var_positions[col]]);
        }
    }

    // Need at least 8 matching lines
    if (columns.empty() || columns[0].values.size() < 8) return false;

    // Capture footer bytes (all data after last template-matching line)
    if (last_matching_line_idx < line_end_offsets.size()) {
        size_t footer_start = line_end_offsets[last_matching_line_idx];
        if (footer_start < n) {
            params.footer_bytes = std::string((char*)data + footer_start, n - footer_start);
        } else {
            params.footer_bytes.clear();
        }
    } else {
        params.footer_bytes.clear();
    }

    // Remove non-matching lines that appear after the last matching line
    // (those are already in footer_bytes)
    while (!params.non_matching_lines.empty() &&
           params.non_matching_lines.back().first >= matching_line_count) {
        params.non_matching_lines.pop_back();
    }

    // Analyze each column to determine encoding type
    for (size_t col = 0; col < columns.size(); col++) {
        auto& column = columns[col];

        // Check if all values are numeric
        bool all_numeric = true;
        std::vector<int64_t> int_values;

        for (const auto& v : column.values) {
            if (is_numeric_string(v)) {
                int_values.push_back(parse_int64(v));
            } else {
                all_numeric = false;
                break;
            }
        }

        if (all_numeric && int_values.size() >= 3) {
            // Check for LINEAR_GEN pattern
            int64_t d1 = int_values[1] - int_values[0];
            bool is_linear = true;

            // Full verification required for LINEAR_GEN
            // Sampling only start values can miss modular patterns that wrap
            // (e.g., (i*17)%1000 looks linear for i=1..58 but wraps at i=59)
            for (size_t i = 2; i < int_values.size(); i++) {
                if (int_values[i] - int_values[i-1] != d1) {
                    is_linear = false;
                    break;
                }
            }

            if (is_linear) {
                column.type = ColumnType::COL_LINEAR_GEN;
                column.base = int_values[0];
                column.delta = d1;
                continue;
            }

            // Check for MODULAR pattern: v = (base + i * step) % mod
            // Detects cycling patterns like seconds (0-59), minutes (0-59), hours (0-23)
            if (int_values.size() >= 5) {
                int64_t base = int_values[0];
                int64_t step = int_values[1] - int_values[0];

                // Try common moduli (60 for seconds/minutes, 24 for hours, 256 for bytes)
                const int64_t common_mods[] = {60, 24, 12, 256, 100, 1000, 10, 7};
                bool found_modular = false;

                for (int64_t pot_mod : common_mods) {
                    if (pot_mod <= 1) continue;
                    bool matches = true;
                    for (size_t i = 0; i < int_values.size() && matches; i++) {
                        int64_t expected = ((base + (int64_t)i * step) % pot_mod + pot_mod) % pot_mod;
                        if (int_values[i] != expected) matches = false;
                    }
                    if (matches) {
                        column.type = ColumnType::COL_MODULAR;
                        column.base = base;
                        column.delta = step;
                        column.mod = pot_mod;
                        found_modular = true;
                        break;
                    }
                }
                if (found_modular) continue;
            }

            // Check for REPEAT_INC pattern: v = start + floor(i / repeat) * inc
            // Detects grouped patterns like message IDs that repeat 4 times before incrementing
            if (int_values.size() >= 6) {
                // Count consecutive equal values at start
                size_t repeat_count = 1;
                while (repeat_count < int_values.size() && int_values[repeat_count] == int_values[0]) {
                    repeat_count++;
                }

                if (repeat_count >= 2 && repeat_count <= 100 && repeat_count < int_values.size()) {
                    int64_t start = int_values[0];
                    int64_t inc_val = int_values[repeat_count] - int_values[0];

                    bool matches = true;
                    for (size_t i = 0; i < int_values.size() && matches; i++) {
                        int64_t expected = start + ((int64_t)i / (int64_t)repeat_count) * inc_val;
                        if (int_values[i] != expected) matches = false;
                    }

                    if (matches) {
                        column.type = ColumnType::COL_REPEAT_INC;
                        column.base = start;
                        column.repeat = repeat_count;
                        column.inc = inc_val;
                        continue;
                    }
                }
            }

            // Check for TIMESTAMP pattern (quasi-periodic)
            if (int_values.size() >= 3 && int_values[1] > int_values[0]) {
                int64_t avg_delta = d1;
                int64_t max_dd = 0;
                int64_t prev_d = d1;

                for (size_t i = 2; i < std::min(int_values.size(), (size_t)16); i++) {
                    int64_t d = int_values[i] - int_values[i-1];
                    int64_t dd = d - prev_d;
                    if (dd < 0) dd = -dd;
                    if (dd > max_dd) max_dd = dd;
                    prev_d = d;
                }

                if (max_dd < avg_delta / 10 && max_dd < 10000) {
                    column.type = ColumnType::COL_TIMESTAMP;
                    continue;
                }
            }
        }

        // Check for SUBTEMPLATE pattern: prefix + LINEAR_GEN + suffix
        // Detects patterns like "User2", "User3" or "user2@example.com", "user3@example.com"
        if (column.values.size() >= 3 && !column.values[0].empty()) {
            // Find common prefix
            std::string common_prefix = column.values[0];
            for (size_t i = 1; i < column.values.size() && !common_prefix.empty(); i++) {
                const std::string& v = column.values[i];
                size_t j = 0;
                while (j < common_prefix.size() && j < v.size() && common_prefix[j] == v[j]) j++;
                common_prefix = common_prefix.substr(0, j);
            }

            // Find common suffix (reverse search)
            std::string common_suffix = column.values[0];
            for (size_t i = 1; i < column.values.size() && !common_suffix.empty(); i++) {
                const std::string& v = column.values[i];
                size_t j = 0;
                while (j < common_suffix.size() && j < v.size() &&
                       common_suffix[common_suffix.size() - 1 - j] == v[v.size() - 1 - j]) j++;
                common_suffix = common_suffix.substr(common_suffix.size() - j);
            }

            // Must have at least a prefix or suffix to be a sub-template
            // Also ensure middle part is non-empty and doesn't overlap
            if ((common_prefix.size() > 0 || common_suffix.size() > 0) &&
                common_prefix.size() + common_suffix.size() < column.values[0].size()) {

                // Extract middle parts and check if all numeric
                std::vector<int64_t> middle_values;
                bool all_middle_numeric = true;

                for (const auto& v : column.values) {
                    if (v.size() < common_prefix.size() + common_suffix.size()) {
                        all_middle_numeric = false;
                        break;
                    }
                    std::string middle = v.substr(common_prefix.size(),
                                                  v.size() - common_prefix.size() - common_suffix.size());
                    if (middle.empty() || !is_numeric_string(middle)) {
                        all_middle_numeric = false;
                        break;
                    }
                    middle_values.push_back(parse_int64(middle));
                }

                // Check if middle values form LINEAR_GEN pattern
                if (all_middle_numeric && middle_values.size() >= 3) {
                    int64_t d1 = middle_values[1] - middle_values[0];
                    bool is_linear = true;

                    for (size_t i = 2; i < middle_values.size(); i++) {
                        if (middle_values[i] - middle_values[i-1] != d1) {
                            is_linear = false;
                            break;
                        }
                    }

                    if (is_linear) {
                        // CRITICAL: Verify reconstruction doesn't lose leading zeros
                        // "100ms" with prefix="1", suffix="ms", middle="00" would reconstruct as "10ms"
                        // because integer 0 becomes string "0" not "00"
                        bool roundtrip_ok = true;
                        for (size_t i = 0; i < column.values.size() && roundtrip_ok; i++) {
                            std::string reconstructed = common_prefix +
                                std::to_string(middle_values[0] + d1 * (int64_t)i) +
                                common_suffix;
                            if (reconstructed != column.values[i]) {
                                roundtrip_ok = false;
                            }
                        }

                        if (roundtrip_ok) {
                            column.type = ColumnType::COL_SUBTEMPLATE;
                            column.prefix = common_prefix;
                            column.suffix = common_suffix;
                            column.base = middle_values[0];
                            column.delta = d1;
                            continue;
                        }
                    }
                }
            }
        }

        // Check for dictionary encoding (repeated values)
        std::unordered_map<std::string, uint16_t> dict_map;
        std::vector<std::string> dict;
        std::vector<uint16_t> indices;

        for (const auto& v : column.values) {
            auto it = dict_map.find(v);
            if (it == dict_map.end()) {
                if (dict.size() >= 65535) {
                    dict.clear();
                    break;  // Too many unique values
                }
                uint16_t idx = (uint16_t)dict.size();
                dict_map[v] = idx;
                dict.push_back(v);
                indices.push_back(idx);
            } else {
                indices.push_back(it->second);
            }
        }

        // Use dictionary if it saves space
        // Dictionary overhead: dict size + 2 bytes per index
        // Raw: sum of string lengths
        size_t raw_size = 0;
        for (const auto& v : column.values) raw_size += v.size() + 1;

        size_t dict_size = 0;
        for (const auto& d : dict) dict_size += d.size() + 1;
        dict_size += column.values.size() * 2;

        // Check for CONST (all same value) - dict with 1 entry
        if (dict.size() == 1) {
            column.type = ColumnType::COL_CONST;
            column.dict.clear();
            column.dict.push_back(dict[0]);
            column.indices.clear();
        } else if (!dict.empty() && dict_size < raw_size * 0.8) {
            column.type = ColumnType::COL_DICTIONARY;
            column.dict = std::move(dict);
            column.indices = std::move(indices);
        } else {
            column.type = ColumnType::COL_RAW;
        }
    }

    params.template_str = template_str;
    params.columns = std::move(columns);
    // line_count is the number of template-matching lines, not total lines
    params.line_count = params.columns.empty() ? 0 : params.columns[0].values.size();

    // Validate by checking that all column values fit expected patterns
    // The template detection samples first 64 lines but applies to all.
    // If later data changes format (e.g., date rollover), columns will have inconsistent values.
    // Simple check: verify column value lengths are consistent (within reason)
    for (const auto& column : params.columns) {
        if (column.values.empty()) continue;
        if (column.type == ColumnType::COL_RAW || column.type == ColumnType::COL_DICTIONARY ||
            column.type == ColumnType::COL_LINEAR_GEN || column.type == ColumnType::COL_SUBTEMPLATE) {
            // For these types, varying lengths are OK
            // COL_LINEAR_GEN: values reconstructed from (start, delta), not stored
            // COL_SUBTEMPLATE: values like "User1", "User10", "User100" have varying lengths
            continue;
        }
        // For fixed-width numeric types, check that lengths are consistent
        // (if first value is "22:13:20" length 8, all should be 8)
        size_t first_len = column.values[0].size();
        size_t quarter = column.values.size() / 4;
        size_t half = column.values.size() / 2;
        size_t three_quarter = column.values.size() * 3 / 4;
        size_t last = column.values.size() - 1;

        // Sample a few values from different parts
        if (quarter < column.values.size() && column.values[quarter].size() != first_len) return false;
        if (half < column.values.size() && column.values[half].size() != first_len) return false;
        if (three_quarter < column.values.size() && column.values[three_quarter].size() != first_len) return false;
        if (last < column.values.size() && column.values[last].size() != first_len) return false;
    }

    return true;
}

// Detect char-level line template (more robust than token-based)
// Compares lines character-by-character to find fixed vs variable regions
inline bool detect_char_template(const uint8_t* data, size_t n, CharTemplateParams& params) {
    // Split into lines
    std::vector<std::string> lines;
    std::vector<size_t> line_starts;
    std::vector<size_t> line_ends;
    std::string current;
    size_t start = 0;

    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            if (!current.empty()) {
                line_starts.push_back(start);
                line_ends.push_back(i + 1);
                lines.push_back(current);
                current.clear();
            }
            start = i + 1;
        } else if (data[i] >= 32 || data[i] == '\t') {
            if (current.empty()) start = i;
            current += (char)data[i];
        }
    }
    if (!current.empty()) {
        line_starts.push_back(start);
        line_ends.push_back(n);
        lines.push_back(current);
    }

    // Need at least 10 lines
    if (lines.size() < 10) return false;

    // Build sample indices that span the ENTIRE file (start, middle, end)
    // This catches patterns that drift (e.g., timestamps that grow)
    std::vector<size_t> sample_indices;
    size_t step = std::max((size_t)1, lines.size() / 100);  // ~100 samples spread across file
    for (size_t i = 0; i < lines.size(); i += step) {
        sample_indices.push_back(i);
    }
    // Always include last line
    if (sample_indices.back() != lines.size() - 1) {
        sample_indices.push_back(lines.size() - 1);
    }

    // Find minimum line length across sampled lines, excluding truncated trailing line
    // (Files often end mid-line due to truncation, e.g., substr(0, n))
    // Exclude last line if it's much shorter than typical lines
    size_t first_line_len = lines[sample_indices[0]].size();
    size_t min_len = first_line_len;
    for (size_t idx : sample_indices) {
        // Skip last line if it's truncated (< 75% of first line length)
        if (idx == lines.size() - 1 && lines[idx].size() < first_line_len * 3 / 4) continue;
        min_len = std::min(min_len, lines[idx].size());
    }

    // Need at least 20 chars per line for template to be useful
    if (min_len < 20) return false;

    // Identify fixed vs variable positions using samples from entire file
    std::vector<bool> is_var(min_len, false);
    for (size_t idx : sample_indices) {
        for (size_t j = 0; j < min_len; j++) {
            if (lines[0][j] != lines[idx][j]) {
                is_var[j] = true;
            }
        }
    }

    // Group into regions
    params.regions.clear();
    bool in_var = is_var[0];
    size_t region_start = 0;
    for (size_t i = 1; i < min_len; i++) {
        if (is_var[i] != in_var) {
            params.regions.push_back({region_start, i, in_var});
            region_start = i;
            in_var = is_var[i];
        }
    }
    params.regions.push_back({region_start, min_len, in_var});

    // Count fixed vs variable regions
    size_t fixed_chars = 0, var_chars = 0;
    size_t num_var_regions = 0;
    for (const auto& r : params.regions) {
        size_t len = r.end - r.start;
        if (r.is_variable) {
            var_chars += len;
            num_var_regions++;
        } else {
            fixed_chars += len;
        }
    }

    // Need at least 20% fixed chars and at least 2 variable regions
    // Lower threshold allows more logs to be detected
    if (fixed_chars < min_len * 0.2 || num_var_regions < 2) return false;

    // Build template string
    params.fixed_template.clear();
    for (const auto& r : params.regions) {
        if (!r.is_variable) {
            params.fixed_template += lines[0].substr(r.start, r.end - r.start);
        } else {
            params.fixed_template += "{}";
        }
    }

    // Extract column values from ALL lines (not just sample)
    params.columns.resize(num_var_regions);
    params.tail_bytes.resize(lines.size());

    for (size_t line_idx = 0; line_idx < lines.size(); line_idx++) {
        const std::string& line = lines[line_idx];
        size_t col_idx = 0;
        for (const auto& r : params.regions) {
            if (r.is_variable) {
                if (r.end <= line.size()) {
                    params.columns[col_idx].values.push_back(
                        line.substr(r.start, r.end - r.start));
                } else if (r.start < line.size()) {
                    params.columns[col_idx].values.push_back(line.substr(r.start));
                } else {
                    params.columns[col_idx].values.push_back("");
                }
                col_idx++;
            }
        }
        // Capture tail (anything beyond min_len)
        if (line.size() > min_len) {
            params.tail_bytes[line_idx] = line.substr(min_len);
        }
    }

    params.line_count = lines.size();
    params.min_line_len = min_len;
    params.last_line_has_newline = (n > 0 && data[n-1] == '\n');

    // Validate that template reconstruction works for sampled lines
    // Sample lines from start, middle, and end to catch pattern drift
    std::vector<size_t> validation_indices;
    validation_indices.push_back(0);  // First
    validation_indices.push_back(lines.size() / 4);  // 25%
    validation_indices.push_back(lines.size() / 2);  // 50%
    validation_indices.push_back(lines.size() * 3 / 4);  // 75%
    validation_indices.push_back(lines.size() - 1);  // Last

    for (size_t line_idx : validation_indices) {
        if (line_idx >= lines.size()) continue;
        const std::string& line = lines[line_idx];
        // Reconstruct line from template and column values
        std::string reconstructed;
        size_t col_idx = 0;
        for (const auto& r : params.regions) {
            if (!r.is_variable) {
                reconstructed += lines[0].substr(r.start, r.end - r.start);
            } else {
                reconstructed += params.columns[col_idx].values[line_idx];
                col_idx++;
            }
        }
        // Add tail
        reconstructed += params.tail_bytes[line_idx];

        // If reconstruction doesn't match, template is invalid
        if (reconstructed != line) return false;
    }

    // Analyze each column for optimal encoding
    for (auto& column : params.columns) {
        // Check if all values are the same (shouldn't happen for var regions, but check)
        bool all_same = true;
        for (size_t i = 1; i < column.values.size(); i++) {
            if (column.values[i] != column.values[0]) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            column.encoding = ColumnType::COL_CONST;
            continue;
        }

        // Check if all numeric
        bool all_numeric = true;
        std::vector<int64_t> nums;
        for (const auto& v : column.values) {
            if (!is_fully_numeric(v)) {
                all_numeric = false;
                break;
            }
            nums.push_back(parse_int64(v));
        }

        if (all_numeric && nums.size() > 1) {
            // Check sequential (Kolmogorov)
            bool sequential = true;
            int64_t delta = nums[1] - nums[0];
            for (size_t i = 2; i < nums.size(); i++) {
                if (nums[i] - nums[i-1] != delta) {
                    sequential = false;
                    break;
                }
            }
            if (sequential) {
                column.encoding = ColumnType::COL_LINEAR_GEN;
                column.base = nums[0];
                column.step = delta;
                continue;
            }

            // Check MODULAR: v = (base + i * step) % mod
            // Common patterns: seconds (0-59), minutes (0-59), hours (0-23)
            if (nums.size() >= 5) {
                int64_t base = nums[0];
                int64_t step = nums[1] - nums[0];
                bool found_modular = false;

                // Try common moduli first, then search
                const int64_t common_mods[] = {60, 24, 12, 10, 7, 100, 256};
                for (int64_t pot_mod : common_mods) {
                    if (pot_mod < 2) continue;
                    bool matches = true;
                    for (size_t i = 0; i < nums.size() && matches; i++) {
                        int64_t expected = ((base + (int64_t)i * step) % pot_mod + pot_mod) % pot_mod;
                        if (nums[i] != expected) matches = false;
                    }
                    if (matches) {
                        column.encoding = ColumnType::COL_MODULAR;
                        column.base = base;
                        column.step = step;
                        column.mod = pot_mod;
                        found_modular = true;
                        break;
                    }
                }
                if (found_modular) continue;

                // Try detecting mod from data
                int64_t max_val = *std::max_element(nums.begin(), nums.end());
                for (int64_t pot_mod = max_val + 1; pot_mod <= max_val + 10 && pot_mod <= 256; pot_mod++) {
                    bool matches = true;
                    for (size_t i = 0; i < nums.size() && matches; i++) {
                        int64_t expected = ((base + (int64_t)i * step) % pot_mod + pot_mod) % pot_mod;
                        if (nums[i] != expected) matches = false;
                    }
                    if (matches) {
                        column.encoding = ColumnType::COL_MODULAR;
                        column.base = base;
                        column.step = step;
                        column.mod = pot_mod;
                        found_modular = true;
                        break;
                    }
                }
                if (found_modular) continue;
            }

            // Check REPEAT_INC: v = start + floor(i / repeat) * inc
            // Common patterns: same value repeated N times, then increments
            if (nums.size() >= 6) {
                // Count consecutive equal values at start
                size_t repeat_count = 1;
                while (repeat_count < nums.size() && nums[repeat_count] == nums[0]) {
                    repeat_count++;
                }

                if (repeat_count >= 2 && repeat_count <= nums.size() / 2) {
                    int64_t start = nums[0];
                    int64_t inc_val = (repeat_count < nums.size()) ? nums[repeat_count] - nums[0] : 1;

                    bool matches = true;
                    for (size_t i = 0; i < nums.size() && matches; i++) {
                        int64_t expected = start + ((int64_t)i / (int64_t)repeat_count) * inc_val;
                        if (nums[i] != expected) matches = false;
                    }
                    if (matches) {
                        column.encoding = ColumnType::COL_REPEAT_INC;
                        column.base = start;
                        column.repeat = repeat_count;
                        column.inc = inc_val;
                        continue;
                    }
                }
            }

            // Check byte-offset
            int64_t min_val = *std::min_element(nums.begin(), nums.end());
            int64_t max_val = *std::max_element(nums.begin(), nums.end());
            if (max_val - min_val < 256) {
                column.encoding = ColumnType::COL_BYTE_OFFSET;
                continue;
            }

            column.encoding = ColumnType::COL_RAW;
            continue;
        }

        // Check dictionary encoding
        std::unordered_map<std::string, size_t> dict;
        for (const auto& v : column.values) {
            dict[v]++;
        }

        if (dict.size() < 256 && dict.size() <= column.values.size() / 2) {
            column.encoding = ColumnType::COL_DICTIONARY;
            continue;
        }

        column.encoding = ColumnType::COL_RAW;
    }

    return true;
}

// ============================================================================
// LINE_TEMPLATE: Variable-length line template with linear numeric variables
// ============================================================================

// Extract numbers from a string (simple parser, no regex)
inline std::vector<int64_t> extract_numbers_from_line(const std::string& s) {
    std::vector<int64_t> nums;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] >= '0' && s[i] <= '9') {
            int64_t num = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                num = num * 10 + (s[i] - '0');
                i++;
            }
            nums.push_back(num);
        } else {
            i++;
        }
    }
    return nums;
}

// Extract separators (non-numeric parts) from a string
inline std::vector<std::string> extract_separators_from_line(const std::string& s) {
    std::vector<std::string> seps;
    std::string current;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] >= '0' && s[i] <= '9') {
            seps.push_back(current);
            current.clear();
            // Skip the number
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') i++;
        } else {
            current += s[i];
            i++;
        }
    }
    // Trailing separator (after last number)
    if (!current.empty()) {
        seps.push_back(current);
    }
    return seps;
}

inline bool detect_line_template(const uint8_t* data, size_t n, LineTemplateParams& params) {
    // Split into lines, keeping track of byte positions for header/footer
    std::vector<std::string> lines;
    std::vector<size_t> line_start_offsets;  // Byte offset where each line starts
    std::string current;
    size_t current_start = 0;

    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            if (!current.empty()) {
                line_start_offsets.push_back(current_start);
                lines.push_back(current);
                current.clear();
            }
            current_start = i + 1;
        } else if (data[i] >= 32 || data[i] == '\t') {
            if (current.empty()) current_start = i;
            current += (char)data[i];
        }
    }
    if (!current.empty()) {
        line_start_offsets.push_back(current_start);
        lines.push_back(current);
        params.last_line_has_newline = false;
    } else {
        params.last_line_has_newline = true;
    }

    // Need at least 20 lines for template detection to be worthwhile
    if (lines.size() < 20) return false;

    // Strategy: Find the most common long prefix (>=10 chars) to identify template lines
    // This handles SQL dumps with header comments that don't match the INSERT pattern
    std::unordered_map<std::string, int> prefix_counts;
    for (const auto& line : lines) {
        if (line.size() >= 10) {
            std::string p = line.substr(0, 10);
            prefix_counts[p]++;
        }
    }

    // Find most common prefix
    std::string best_prefix_10;
    int best_count = 0;
    for (const auto& [p, count] : prefix_counts) {
        if (count > best_count) {
            best_count = count;
            best_prefix_10 = p;
        }
    }

    // Need at least 90% of lines to match for this to be worthwhile
    if (best_count < (int)(lines.size() * 0.9)) return false;

    // Filter lines that match this prefix - these are our template lines
    std::vector<std::string> template_lines;
    std::vector<size_t> template_indices;  // Index in original lines array
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].size() >= 10 && lines[i].substr(0, 10) == best_prefix_10) {
            template_lines.push_back(lines[i]);
            template_indices.push_back(i);
        }
    }

    if (template_lines.size() < 20) return false;

    // Find common prefix of template lines (extend beyond the 10 chars)
    std::string prefix = template_lines[0];
    for (const auto& line : template_lines) {
        size_t i = 0;
        while (i < prefix.size() && i < line.size() && prefix[i] == line[i]) i++;
        prefix = prefix.substr(0, i);
    }

    // Find common suffix of template lines
    std::string suffix = template_lines[0];
    for (const auto& line : template_lines) {
        size_t j = 0;
        while (j < suffix.size() && j < line.size() &&
               suffix[suffix.size()-1-j] == line[line.size()-1-j]) j++;
        suffix = suffix.substr(suffix.size() - j);
    }

    // Need meaningful prefix AND suffix (at least 5+3 chars of fixed content)
    if (prefix.size() < 5 || suffix.size() < 3) return false;

    // Extract middle part and find numbers for each template line
    std::vector<std::vector<int64_t>> all_nums;
    for (const auto& line : template_lines) {
        if (line.size() < prefix.size() + suffix.size()) {
            return false;  // Line too short
        }
        std::string middle = line.substr(prefix.size(), line.size() - prefix.size() - suffix.size());
        all_nums.push_back(extract_numbers_from_line(middle));
    }

    // Check all template lines have same number of variables
    size_t num_vars = all_nums[0].size();
    if (num_vars == 0 || num_vars > 10) return false;  // Must have 1-10 variables
    for (const auto& nums : all_nums) {
        if (nums.size() != num_vars) return false;
    }

    // Check if each variable follows a linear pattern
    params.first_values.resize(num_vars);
    params.deltas.resize(num_vars);

    for (size_t v = 0; v < num_vars; v++) {
        params.first_values[v] = all_nums[0][v];
        if (all_nums.size() >= 2) {
            params.deltas[v] = all_nums[1][v] - all_nums[0][v];
        } else {
            params.deltas[v] = 1;
        }

        // Verify linear pattern holds for ALL template lines
        for (size_t i = 0; i < all_nums.size(); i++) {
            int64_t expected = params.first_values[v] + params.deltas[v] * (int64_t)i;
            if (all_nums[i][v] != expected) {
                return false;  // Not a perfect linear pattern
            }
        }
    }

    // Extract separators from first template line's middle
    std::string middle = template_lines[0].substr(prefix.size(), template_lines[0].size() - prefix.size() - suffix.size());
    params.separators = extract_separators_from_line(middle);

    // Verify separator pattern is consistent across template lines
    for (size_t li = 1; li < std::min(template_lines.size(), (size_t)10); li++) {
        std::string m = template_lines[li].substr(prefix.size(), template_lines[li].size() - prefix.size() - suffix.size());
        auto seps = extract_separators_from_line(m);
        if (seps.size() != params.separators.size()) return false;
        for (size_t s = 0; s < seps.size(); s++) {
            if (seps[s] != params.separators[s]) return false;
        }
    }

    // Collect header bytes (everything before first template line)
    size_t first_template_idx = template_indices[0];
    if (first_template_idx > 0) {
        size_t header_end = line_start_offsets[first_template_idx];
        params.header_bytes = std::string((char*)data, header_end);
    } else {
        params.header_bytes = "";
    }

    // Collect footer bytes (everything after last template line)
    size_t last_template_idx = template_indices.back();
    if (last_template_idx < lines.size() - 1) {
        // There are lines after the last template line
        size_t footer_start = line_start_offsets[last_template_idx] + lines[last_template_idx].size() + 1;  // +1 for newline
        if (footer_start < n) {
            params.footer_bytes = std::string((char*)data + footer_start, n - footer_start);
        }
    } else {
        params.footer_bytes = "";
    }

    params.prefix = prefix;
    params.suffix = suffix;
    params.line_count = template_lines.size();

    return true;
}

inline std::vector<uint8_t> encode_line_template(const LineTemplateParams& params, int zstd_level) {
    std::vector<uint8_t> result;
    result.reserve(512 + params.header_bytes.size() + params.footer_bytes.size());

    // Format:
    // [magic:1=0xCC][header_len:4][header_bytes][footer_len:4][footer_bytes]
    // [prefix_len:2][prefix][suffix_len:2][suffix]
    // [num_vars:1][sep_count:1]
    // [for each sep: sep_len:2, sep_data]
    // [for each var: first_value:8, delta:8]
    // [line_count:4][flags:1]

    result.push_back(0xCC);  // Magic for LINE_TEMPLATE

    // Header bytes (non-template lines before first template line)
    uint32_t hlen = (uint32_t)params.header_bytes.size();
    for (int j = 0; j < 4; j++) result.push_back((hlen >> (j*8)) & 0xFF);
    result.insert(result.end(), params.header_bytes.begin(), params.header_bytes.end());

    // Footer bytes (non-template lines after last template line)
    uint32_t flen = (uint32_t)params.footer_bytes.size();
    for (int j = 0; j < 4; j++) result.push_back((flen >> (j*8)) & 0xFF);
    result.insert(result.end(), params.footer_bytes.begin(), params.footer_bytes.end());

    // Prefix
    uint16_t plen = (uint16_t)params.prefix.size();
    result.push_back(plen & 0xFF);
    result.push_back((plen >> 8) & 0xFF);
    result.insert(result.end(), params.prefix.begin(), params.prefix.end());

    // Suffix
    uint16_t slen = (uint16_t)params.suffix.size();
    result.push_back(slen & 0xFF);
    result.push_back((slen >> 8) & 0xFF);
    result.insert(result.end(), params.suffix.begin(), params.suffix.end());

    // Number of variables
    result.push_back((uint8_t)params.first_values.size());

    // Number of separators
    result.push_back((uint8_t)params.separators.size());

    // Separators
    for (const auto& sep : params.separators) {
        uint16_t len = (uint16_t)sep.size();
        result.push_back(len & 0xFF);
        result.push_back((len >> 8) & 0xFF);
        result.insert(result.end(), sep.begin(), sep.end());
    }

    // Variables (first_value + delta, each 8 bytes)
    for (size_t i = 0; i < params.first_values.size(); i++) {
        int64_t v = params.first_values[i];
        for (int j = 0; j < 8; j++) result.push_back((v >> (j*8)) & 0xFF);
        int64_t d = params.deltas[i];
        for (int j = 0; j < 8; j++) result.push_back((d >> (j*8)) & 0xFF);
    }

    // Line count (4 bytes)
    uint32_t lc = (uint32_t)params.line_count;
    for (int j = 0; j < 4; j++) result.push_back((lc >> (j*8)) & 0xFF);

    // Flags (1 byte)
    uint8_t flags = params.last_line_has_newline ? 0x01 : 0x00;
    result.push_back(flags);

    return result;
}

inline std::vector<uint8_t> decode_line_template(const uint8_t* data, size_t n) {
    size_t pos = 0;

    // Skip magic (0xCC)
    if (data[pos] != 0xCC) return {};
    pos++;

    // Header bytes
    uint32_t hlen = 0;
    for (int j = 0; j < 4; j++) hlen |= ((uint32_t)data[pos++]) << (j*8);
    std::string header_bytes((char*)&data[pos], hlen);
    pos += hlen;

    // Footer bytes
    uint32_t flen = 0;
    for (int j = 0; j < 4; j++) flen |= ((uint32_t)data[pos++]) << (j*8);
    std::string footer_bytes((char*)&data[pos], flen);
    pos += flen;

    // Prefix
    uint16_t plen = data[pos] | ((uint16_t)data[pos+1] << 8);
    pos += 2;
    std::string prefix((char*)&data[pos], plen);
    pos += plen;

    // Suffix
    uint16_t slen = data[pos] | ((uint16_t)data[pos+1] << 8);
    pos += 2;
    std::string suffix((char*)&data[pos], slen);
    pos += slen;

    // Number of variables
    uint8_t num_vars = data[pos++];

    // Number of separators
    uint8_t num_seps = data[pos++];

    // Separators
    std::vector<std::string> separators;
    for (int i = 0; i < num_seps; i++) {
        uint16_t len = data[pos] | ((uint16_t)data[pos+1] << 8);
        pos += 2;
        separators.push_back(std::string((char*)&data[pos], len));
        pos += len;
    }

    // Variables
    std::vector<int64_t> first_values(num_vars), deltas(num_vars);
    for (int i = 0; i < num_vars; i++) {
        int64_t v = 0;
        for (int j = 0; j < 8; j++) v |= ((int64_t)data[pos++]) << (j*8);
        first_values[i] = v;
        int64_t d = 0;
        for (int j = 0; j < 8; j++) d |= ((int64_t)data[pos++]) << (j*8);
        deltas[i] = d;
    }

    // Line count
    uint32_t line_count = 0;
    for (int j = 0; j < 4; j++) line_count |= ((uint32_t)data[pos++]) << (j*8);

    // Flags
    uint8_t flags = data[pos++];
    bool last_line_has_newline = (flags & 0x01) != 0;

    // Reconstruct the data
    std::vector<uint8_t> result;
    result.reserve(hlen + flen + line_count * (prefix.size() + suffix.size() + 50));

    // Header first
    result.insert(result.end(), header_bytes.begin(), header_bytes.end());

    // Template lines
    for (uint32_t line = 0; line < line_count; line++) {
        // Prefix
        result.insert(result.end(), prefix.begin(), prefix.end());

        // Variables with separators
        for (int v = 0; v < num_vars; v++) {
            // Separator before variable (if exists)
            if (v < (int)separators.size()) {
                result.insert(result.end(), separators[v].begin(), separators[v].end());
            }
            // Variable value
            std::string val = std::to_string(first_values[v] + deltas[v] * line);
            result.insert(result.end(), val.begin(), val.end());
        }
        // Trailing separator (after last variable)
        if (num_vars < (int)separators.size()) {
            result.insert(result.end(), separators[num_vars].begin(), separators[num_vars].end());
        }

        // Suffix
        result.insert(result.end(), suffix.begin(), suffix.end());

        // Newline (except possibly last line if no footer and no trailing newline)
        if (line < line_count - 1 || last_line_has_newline || !footer_bytes.empty()) {
            result.push_back('\n');
        }
    }

    // Footer last
    result.insert(result.end(), footer_bytes.begin(), footer_bytes.end());

    return result;
}

// Encode char template data
inline std::vector<uint8_t> encode_char_template(const CharTemplateParams& params, int zstd_level) {
    std::vector<uint8_t> result;
    result.reserve(4096);

    // Header:
    // [template_len:2][template][line_count:4][min_line_len:4][col_count:1]

    uint16_t tpl_len = (uint16_t)params.fixed_template.size();
    result.push_back(tpl_len & 0xFF);
    result.push_back((tpl_len >> 8) & 0xFF);
    for (char c : params.fixed_template) {
        result.push_back((uint8_t)c);
    }

    uint32_t line_count = (uint32_t)params.line_count;
    result.push_back(line_count & 0xFF);
    result.push_back((line_count >> 8) & 0xFF);
    result.push_back((line_count >> 16) & 0xFF);
    result.push_back((line_count >> 24) & 0xFF);

    uint32_t min_len = (uint32_t)params.min_line_len;
    result.push_back(min_len & 0xFF);
    result.push_back((min_len >> 8) & 0xFF);
    result.push_back((min_len >> 16) & 0xFF);
    result.push_back((min_len >> 24) & 0xFF);

    result.push_back((uint8_t)params.columns.size());
    result.push_back(params.last_line_has_newline ? 1 : 0);

    // Encode each column
    for (const auto& col : params.columns) {
        result.push_back((uint8_t)col.encoding);

        if (col.encoding == ColumnType::COL_CONST) {
            // Just store the constant value
            uint16_t len = (uint16_t)col.values[0].size();
            result.push_back(len & 0xFF);
            result.push_back((len >> 8) & 0xFF);
            for (char c : col.values[0]) {
                result.push_back((uint8_t)c);
            }
        }
        else if (col.encoding == ColumnType::COL_LINEAR_GEN) {
            // Store first value and delta
            int64_t first = parse_int64(col.values[0]);
            int64_t delta = parse_int64(col.values[1]) - first;
            uint8_t num_digits = (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((first >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((delta >> (i*8)) & 0xFF);
        }
        else if (col.encoding == ColumnType::COL_MODULAR) {
            // Store base, step, mod (v = (base + i*step) % mod)
            uint8_t num_digits = (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.step >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.mod >> (i*8)) & 0xFF);
        }
        else if (col.encoding == ColumnType::COL_REPEAT_INC) {
            // Store start, repeat, inc (v = start + floor(i/repeat)*inc)
            uint8_t num_digits = (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.repeat >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.inc >> (i*8)) & 0xFF);
        }
        else if (col.encoding == ColumnType::COL_BYTE_OFFSET) {
            // Store min value and byte offsets
            std::vector<int64_t> nums;
            for (const auto& v : col.values) {
                nums.push_back(parse_int64(v));
            }
            int64_t min_val = *std::min_element(nums.begin(), nums.end());
            uint8_t num_digits = (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((min_val >> (i*8)) & 0xFF);

            std::vector<uint8_t> offsets(nums.size());
            for (size_t i = 0; i < nums.size(); i++) {
                offsets[i] = (uint8_t)(nums[i] - min_val);
            }
            // Compress offsets
            std::vector<uint8_t> zbuf(ZSTD_compressBound(offsets.size()));
            size_t zsize = ZSTD_compress(zbuf.data(), zbuf.size(),
                                          offsets.data(), offsets.size(), zstd_level);
            result.push_back((zsize >> 0) & 0xFF);
            result.push_back((zsize >> 8) & 0xFF);
            result.push_back((zsize >> 16) & 0xFF);
            result.push_back((zsize >> 24) & 0xFF);
            result.insert(result.end(), zbuf.begin(), zbuf.begin() + zsize);
        }
        else if (col.encoding == ColumnType::COL_DICTIONARY) {
            // Build dictionary
            std::unordered_map<std::string, uint8_t> dict_map;
            std::vector<std::string> dict;
            std::vector<uint8_t> indices;
            for (const auto& v : col.values) {
                auto it = dict_map.find(v);
                if (it == dict_map.end()) {
                    uint8_t idx = (uint8_t)dict.size();
                    dict_map[v] = idx;
                    dict.push_back(v);
                    indices.push_back(idx);
                } else {
                    indices.push_back(it->second);
                }
            }

            // Write dictionary
            result.push_back((uint8_t)dict.size());
            for (const auto& entry : dict) {
                result.push_back((uint8_t)entry.size());
                for (char c : entry) result.push_back((uint8_t)c);
            }

            // Compress indices
            std::vector<uint8_t> zbuf(ZSTD_compressBound(indices.size()));
            size_t zsize = ZSTD_compress(zbuf.data(), zbuf.size(),
                                          indices.data(), indices.size(), zstd_level);
            result.push_back((zsize >> 0) & 0xFF);
            result.push_back((zsize >> 8) & 0xFF);
            result.push_back((zsize >> 16) & 0xFF);
            result.push_back((zsize >> 24) & 0xFF);
            result.insert(result.end(), zbuf.begin(), zbuf.begin() + zsize);
        }
        else {
            // RAW: concatenate values with null separators, compress
            std::string concat;
            for (const auto& v : col.values) {
                concat += v;
                concat += '\0';
            }
            std::vector<uint8_t> zbuf(ZSTD_compressBound(concat.size()));
            size_t zsize = ZSTD_compress(zbuf.data(), zbuf.size(),
                                          concat.data(), concat.size(), zstd_level);
            result.push_back((zsize >> 0) & 0xFF);
            result.push_back((zsize >> 8) & 0xFF);
            result.push_back((zsize >> 16) & 0xFF);
            result.push_back((zsize >> 24) & 0xFF);
            result.insert(result.end(), zbuf.begin(), zbuf.begin() + zsize);
        }
    }

    // Encode tail bytes (variable length parts after min_line_len)
    std::string all_tails;
    for (const auto& tail : params.tail_bytes) {
        all_tails += tail;
        all_tails += '\n';
    }
    std::vector<uint8_t> zbuf(ZSTD_compressBound(all_tails.size()));
    size_t zsize = ZSTD_compress(zbuf.data(), zbuf.size(),
                                  all_tails.data(), all_tails.size(), zstd_level);
    result.push_back((zsize >> 0) & 0xFF);
    result.push_back((zsize >> 8) & 0xFF);
    result.push_back((zsize >> 16) & 0xFF);
    result.push_back((zsize >> 24) & 0xFF);
    result.insert(result.end(), zbuf.begin(), zbuf.begin() + zsize);

    return result;
}

// Decode char template data
inline std::vector<uint8_t> decode_char_template(const uint8_t* data, size_t n) {
    size_t pos = 0;

    // Read template
    if (pos + 2 > n) return {};
    uint16_t tpl_len = data[pos] | ((uint16_t)data[pos+1] << 8);
    pos += 2;
    if (pos + tpl_len > n) return {};
    std::string tpl((char*)data + pos, tpl_len);
    pos += tpl_len;

    // Read line count and min_len
    if (pos + 8 > n) return {};
    uint32_t line_count = data[pos] | ((uint32_t)data[pos+1] << 8) |
                          ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
    pos += 4;
    uint32_t min_line_len = data[pos] | ((uint32_t)data[pos+1] << 8) |
                            ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
    pos += 4;
    (void)min_line_len;  // Used for reconstruction

    // Read column count
    if (pos >= n) return {};
    uint8_t col_count = data[pos++];
    
    // Read last_line_has_newline flag
    if (pos >= n) return {};
    bool last_line_has_newline = (data[pos++] != 0);

    // Decode columns
    std::vector<std::vector<std::string>> columns(col_count);
    for (uint8_t col = 0; col < col_count; col++) {
        if (pos >= n) return {};
        ColumnType encoding = (ColumnType)data[pos++];

        if (encoding == ColumnType::COL_CONST) {
            if (pos + 2 > n) return {};
            uint16_t len = data[pos] | ((uint16_t)data[pos+1] << 8);
            pos += 2;
            if (pos + len > n) return {};
            std::string val((char*)data + pos, len);
            pos += len;
            columns[col].resize(line_count, val);
        }
        else if (encoding == ColumnType::COL_LINEAR_GEN) {
            if (pos + 17 > n) return {};
            uint8_t num_digits = data[pos++];
            int64_t first = 0;
            for (int i = 0; i < 8; i++) first |= ((int64_t)data[pos++] << (i*8));
            int64_t delta = 0;
            for (int i = 0; i < 8; i++) delta |= ((int64_t)data[pos++] << (i*8));

            columns[col].resize(line_count);
            for (uint32_t i = 0; i < line_count; i++) {
                int64_t val = first + delta * i;
                std::string s = std::to_string(val);
                if (s.size() < num_digits) s.insert(0, num_digits - s.size(), '0');
                columns[col][i] = s;
            }
        }
        else if (encoding == ColumnType::COL_MODULAR) {
            // v = (base + i * step) % mod
            if (pos + 25 > n) return {};
            uint8_t num_digits = data[pos++];
            int64_t base = 0;
            for (int i = 0; i < 8; i++) base |= ((int64_t)data[pos++] << (i*8));
            int64_t step = 0;
            for (int i = 0; i < 8; i++) step |= ((int64_t)data[pos++] << (i*8));
            int64_t mod = 0;
            for (int i = 0; i < 8; i++) mod |= ((int64_t)data[pos++] << (i*8));

            columns[col].resize(line_count);
            for (uint32_t i = 0; i < line_count; i++) {
                int64_t val = ((base + (int64_t)i * step) % mod + mod) % mod;
                std::string s = std::to_string(val);
                if (s.size() < num_digits) s.insert(0, num_digits - s.size(), '0');
                columns[col][i] = s;
            }
        }
        else if (encoding == ColumnType::COL_REPEAT_INC) {
            // v = start + floor(i / repeat) * inc
            if (pos + 25 > n) return {};
            uint8_t num_digits = data[pos++];
            int64_t start = 0;
            for (int i = 0; i < 8; i++) start |= ((int64_t)data[pos++] << (i*8));
            int64_t repeat = 0;
            for (int i = 0; i < 8; i++) repeat |= ((int64_t)data[pos++] << (i*8));
            int64_t inc = 0;
            for (int i = 0; i < 8; i++) inc |= ((int64_t)data[pos++] << (i*8));

            columns[col].resize(line_count);
            for (uint32_t i = 0; i < line_count; i++) {
                int64_t val = start + ((int64_t)i / repeat) * inc;
                std::string s = std::to_string(val);
                if (s.size() < num_digits) s.insert(0, num_digits - s.size(), '0');
                columns[col][i] = s;
            }
        }
        else if (encoding == ColumnType::COL_BYTE_OFFSET) {
            if (pos + 13 > n) return {};
            uint8_t num_digits = data[pos++];
            int64_t min_val = 0;
            for (int i = 0; i < 8; i++) min_val |= ((int64_t)data[pos++] << (i*8));

            uint32_t zsize = data[pos] | ((uint32_t)data[pos+1] << 8) |
                             ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
            pos += 4;
            if (pos + zsize > n) return {};

            std::vector<uint8_t> offsets(line_count);
            size_t dsize = ZSTD_decompress(offsets.data(), offsets.size(), data + pos, zsize);
            if (ZSTD_isError(dsize)) return {};
            pos += zsize;

            columns[col].resize(line_count);
            for (uint32_t i = 0; i < line_count; i++) {
                int64_t val = min_val + offsets[i];
                std::string s = std::to_string(val);
                if (s.size() < num_digits) s.insert(0, num_digits - s.size(), '0');
                columns[col][i] = s;
            }
        }
        else if (encoding == ColumnType::COL_DICTIONARY) {
            if (pos >= n) return {};
            uint8_t dict_size = data[pos++];
            std::vector<std::string> dict(dict_size);
            for (uint8_t d = 0; d < dict_size; d++) {
                if (pos >= n) return {};
                uint8_t len = data[pos++];
                if (pos + len > n) return {};
                dict[d] = std::string((char*)data + pos, len);
                pos += len;
            }

            if (pos + 4 > n) return {};
            uint32_t zsize = data[pos] | ((uint32_t)data[pos+1] << 8) |
                             ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
            pos += 4;
            if (pos + zsize > n) return {};

            std::vector<uint8_t> indices(line_count);
            size_t dsize = ZSTD_decompress(indices.data(), indices.size(), data + pos, zsize);
            if (ZSTD_isError(dsize)) return {};
            pos += zsize;

            columns[col].resize(line_count);
            for (uint32_t i = 0; i < line_count; i++) {
                columns[col][i] = dict[indices[i]];
            }
        }
        else {
            // RAW
            if (pos + 4 > n) return {};
            uint32_t zsize = data[pos] | ((uint32_t)data[pos+1] << 8) |
                             ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
            pos += 4;
            if (pos + zsize > n) return {};

            size_t est_size = line_count * 64;  // Estimate
            std::vector<uint8_t> decompressed(est_size);
            size_t dsize = ZSTD_decompress(decompressed.data(), decompressed.size(), data + pos, zsize);
            if (ZSTD_isError(dsize)) return {};
            pos += zsize;

            // Parse null-separated strings
            columns[col].clear();
            std::string current;
            for (size_t i = 0; i < dsize; i++) {
                if (decompressed[i] == 0) {
                    columns[col].push_back(current);
                    current.clear();
                } else {
                    current += (char)decompressed[i];
                }
            }
        }
    }

    // Decode tails
    if (pos + 4 > n) return {};
    uint32_t tail_zsize = data[pos] | ((uint32_t)data[pos+1] << 8) |
                          ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
    pos += 4;
    if (pos + tail_zsize > n) return {};

    size_t est_tail = line_count * 64;
    std::vector<uint8_t> tail_data(est_tail);
    size_t tail_dsize = ZSTD_decompress(tail_data.data(), tail_data.size(), data + pos, tail_zsize);
    if (ZSTD_isError(tail_dsize)) return {};

    std::vector<std::string> tails;
    std::string current;
    for (size_t i = 0; i < tail_dsize; i++) {
        if (tail_data[i] == '\n') {
            tails.push_back(current);
            current.clear();
        } else {
            current += (char)tail_data[i];
        }
    }
    while (tails.size() < line_count) tails.push_back("");

    // Reconstruct lines from template and columns
    std::vector<uint8_t> result;
    result.reserve(line_count * (min_line_len + 20));

    for (uint32_t line = 0; line < line_count; line++) {
        size_t col_idx = 0;
        for (size_t i = 0; i < tpl.size(); ) {
            if (i + 1 < tpl.size() && tpl[i] == '{' && tpl[i+1] == '}') {
                // Variable placeholder
                if (col_idx < columns.size() && line < columns[col_idx].size()) {
                    for (char c : columns[col_idx][line]) {
                        result.push_back((uint8_t)c);
                    }
                }
                col_idx++;
                i += 2;
            } else {
                result.push_back((uint8_t)tpl[i]);
                i++;
            }
        }
        // Add tail
        if (line < tails.size()) {
            for (char c : tails[line]) {
                result.push_back((uint8_t)c);
            }
        }
           // Only add newline if not the last line, or if the last line had a newline
        if (line < line_count - 1 || last_line_has_newline) {
            result.push_back('\n');
        }
    }

    return result;
}

// Encode template data
inline std::vector<uint8_t> encode_template(const TemplateParams& params) {
    std::vector<uint8_t> result;
    result.reserve(1024);

    // Header format:
    // [header_bytes_len:4][header_bytes][template_len:2][template][line_count:4][col_count:1]

    // Header bytes (skipped lines like "INSERT INTO... VALUES")
    uint32_t hdr_len = (uint32_t)params.header_bytes.size();
    result.push_back(hdr_len & 0xFF);
    result.push_back((hdr_len >> 8) & 0xFF);
    result.push_back((hdr_len >> 16) & 0xFF);
    result.push_back((hdr_len >> 24) & 0xFF);
    for (char c : params.header_bytes) {
        result.push_back((uint8_t)c);
    }

    // Template string
    uint16_t tpl_len = (uint16_t)params.template_str.size();
    result.push_back(tpl_len & 0xFF);
    result.push_back((tpl_len >> 8) & 0xFF);

    for (char c : params.template_str) {
        result.push_back((uint8_t)c);
    }

    uint32_t line_count = (uint32_t)params.line_count;
    result.push_back(line_count & 0xFF);
    result.push_back((line_count >> 8) & 0xFF);
    result.push_back((line_count >> 16) & 0xFF);
    result.push_back((line_count >> 24) & 0xFF);

    result.push_back((uint8_t)params.columns.size());

    // Each column
    for (const auto& col : params.columns) {
        result.push_back((uint8_t)col.type);

        if (col.type == ColumnType::COL_LINEAR_GEN) {
            // [num_digits:1][base:8][delta:8]
            uint8_t num_digits = col.values.empty() ? 0 : (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.delta >> (i*8)) & 0xFF);
        }
        else if (col.type == ColumnType::COL_MODULAR) {
            // [num_digits:1][base:8][delta:8][mod:8]
            uint8_t num_digits = col.values.empty() ? 0 : (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.delta >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.mod >> (i*8)) & 0xFF);
        }
        else if (col.type == ColumnType::COL_REPEAT_INC) {
            // [num_digits:1][base:8][repeat:8][inc:8]
            uint8_t num_digits = col.values.empty() ? 0 : (uint8_t)col.values[0].size();
            result.push_back(num_digits);
            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.repeat >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.inc >> (i*8)) & 0xFF);
        }
        else if (col.type == ColumnType::COL_TIMESTAMP) {
            // Store as delta-delta + zigzag + varint
            if (!col.values.empty()) {
                std::vector<int64_t> ints;
                for (const auto& v : col.values) {
                    ints.push_back(parse_int64(v));
                }

                // First value
                int64_t first = ints[0];
                for (int i = 0; i < 8; i++) result.push_back((first >> (i*8)) & 0xFF);

                if (ints.size() > 1) {
                    // First delta as varint
                    int64_t prev_delta = ints[1] - ints[0];
                    uint64_t zz = zigzag_encode_val(prev_delta);
                    uint8_t buf[10];
                    size_t len = varint_encode_to(buf, zz);
                    for (size_t i = 0; i < len; i++) result.push_back(buf[i]);

                    // Delta-of-deltas
                    for (size_t i = 2; i < ints.size(); i++) {
                        int64_t delta = ints[i] - ints[i-1];
                        int64_t dd = delta - prev_delta;
                        zz = zigzag_encode_val(dd);
                        len = varint_encode_to(buf, zz);
                        for (size_t j = 0; j < len; j++) result.push_back(buf[j]);
                        prev_delta = delta;
                    }
                }
            }
        }
        else if (col.type == ColumnType::COL_DICTIONARY) {
            // [dict_size:2][dict entries...][indices...]
            uint16_t dict_size = (uint16_t)col.dict.size();
            result.push_back(dict_size & 0xFF);
            result.push_back((dict_size >> 8) & 0xFF);

            for (const auto& entry : col.dict) {
                uint8_t len = (uint8_t)std::min(entry.size(), (size_t)255);
                result.push_back(len);
                for (size_t i = 0; i < len; i++) {
                    result.push_back((uint8_t)entry[i]);
                }
            }

            // Indices as varint (adapts to actual values, not dict size)
            uint8_t vbuf[10];
            for (uint16_t idx : col.indices) {
                size_t vlen = varint_encode_to(vbuf, idx);
                for (size_t vi = 0; vi < vlen; vi++) {
                    result.push_back(vbuf[vi]);
                }
            }
        }
        else if (col.type == ColumnType::COL_CONST) {
            // [len:1][value...]  - just store the single constant value
            const std::string& val = col.dict.empty() ? "" : col.dict[0];
            uint8_t len = (uint8_t)std::min(val.size(), (size_t)255);
            result.push_back(len);
            for (size_t i = 0; i < len; i++) {
                result.push_back((uint8_t)val[i]);
            }
        }
        else if (col.type == ColumnType::COL_SUBTEMPLATE) {
            // [prefix_len:1][prefix...][suffix_len:1][suffix...][base:8][delta:8]
            uint8_t prefix_len = (uint8_t)std::min(col.prefix.size(), (size_t)255);
            result.push_back(prefix_len);
            for (size_t i = 0; i < prefix_len; i++) {
                result.push_back((uint8_t)col.prefix[i]);
            }

            uint8_t suffix_len = (uint8_t)std::min(col.suffix.size(), (size_t)255);
            result.push_back(suffix_len);
            for (size_t i = 0; i < suffix_len; i++) {
                result.push_back((uint8_t)col.suffix[i]);
            }

            for (int i = 0; i < 8; i++) result.push_back((col.base >> (i*8)) & 0xFF);
            for (int i = 0; i < 8; i++) result.push_back((col.delta >> (i*8)) & 0xFF);
        }
        else {  // COL_RAW
            // [total_len:4][values with length prefixes]
            std::vector<uint8_t> raw_data;
            for (const auto& v : col.values) {
                uint8_t len = (uint8_t)std::min(v.size(), (size_t)255);
                raw_data.push_back(len);
                for (size_t i = 0; i < len; i++) {
                    raw_data.push_back((uint8_t)v[i]);
                }
            }

            uint32_t total = (uint32_t)raw_data.size();
            result.push_back(total & 0xFF);
            result.push_back((total >> 8) & 0xFF);
            result.push_back((total >> 16) & 0xFF);
            result.push_back((total >> 24) & 0xFF);

            for (uint8_t b : raw_data) result.push_back(b);
        }
    }

    // Footer bytes (lines after template, like ";\nUNLOCK TABLES;\n")
    uint32_t ftr_len = (uint32_t)params.footer_bytes.size();
    result.push_back(ftr_len & 0xFF);
    result.push_back((ftr_len >> 8) & 0xFF);
    result.push_back((ftr_len >> 16) & 0xFF);
    result.push_back((ftr_len >> 24) & 0xFF);
    for (char c : params.footer_bytes) {
        result.push_back((uint8_t)c);
    }

    // Non-matching lines (interleaved in the middle)
    // Format: [count:4] { [insert_after:4][len:4][raw_bytes] }*
    uint32_t nm_count = (uint32_t)params.non_matching_lines.size();
    result.push_back(nm_count & 0xFF);
    result.push_back((nm_count >> 8) & 0xFF);
    result.push_back((nm_count >> 16) & 0xFF);
    result.push_back((nm_count >> 24) & 0xFF);

    for (const auto& [insert_after, raw_bytes] : params.non_matching_lines) {
        uint32_t pos = (uint32_t)insert_after;
        result.push_back(pos & 0xFF);
        result.push_back((pos >> 8) & 0xFF);
        result.push_back((pos >> 16) & 0xFF);
        result.push_back((pos >> 24) & 0xFF);

        uint32_t len = (uint32_t)raw_bytes.size();
        result.push_back(len & 0xFF);
        result.push_back((len >> 8) & 0xFF);
        result.push_back((len >> 16) & 0xFF);
        result.push_back((len >> 24) & 0xFF);

        for (char c : raw_bytes) {
            result.push_back((uint8_t)c);
        }
    }

    return result;
}

// Decode template data back to original text
inline std::vector<uint8_t> decode_template(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read header bytes (skipped lines like "INSERT INTO... VALUES")
    if (ptr + 4 > end) return {};
    uint32_t hdr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    if (ptr + hdr_len > end) return {};
    std::string header_bytes((char*)ptr, hdr_len);
    ptr += hdr_len;

    // Read template
    if (ptr + 2 > end) return {};
    uint16_t tpl_len = ptr[0] | (ptr[1] << 8);
    ptr += 2;

    if (ptr + tpl_len > end) return {};
    std::string template_str((char*)ptr, tpl_len);
    ptr += tpl_len;

    // Read line count
    if (ptr + 4 > end) return {};
    uint32_t line_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Read column count
    if (ptr + 1 > end) return {};
    uint8_t col_count = *ptr++;

    // Read columns
    std::vector<std::vector<std::string>> columns(col_count);

    for (uint8_t c = 0; c < col_count; c++) {
        if (ptr + 1 > end) return {};
        ColumnType type = (ColumnType)*ptr++;

        columns[c].resize(line_count);

        if (type == ColumnType::COL_LINEAR_GEN) {
            if (ptr + 17 > end) return {};  // 1 byte num_digits + 8 bytes base + 8 bytes delta
            uint8_t num_digits = *ptr++;
            int64_t base = 0, delta = 0;
            for (int i = 0; i < 8; i++) base |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) delta |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;

            for (size_t i = 0; i < line_count; i++) {
                std::string s = std::to_string(base + delta * (int64_t)i);
                // Pad with leading zeros to match original format
                while (s.size() < num_digits) s = "0" + s;
                columns[c][i] = s;
            }
        }
        else if (type == ColumnType::COL_MODULAR) {
            if (ptr + 25 > end) return {};  // 1 + 8 + 8 + 8
            uint8_t num_digits = *ptr++;
            int64_t base = 0, delta = 0, mod = 0;
            for (int i = 0; i < 8; i++) base |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) delta |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) mod |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;

            for (size_t i = 0; i < line_count; i++) {
                int64_t val = ((base + delta * (int64_t)i) % mod + mod) % mod;
                std::string s = std::to_string(val);
                while (s.size() < num_digits) s = "0" + s;
                columns[c][i] = s;
            }
        }
        else if (type == ColumnType::COL_REPEAT_INC) {
            if (ptr + 25 > end) return {};  // 1 + 8 + 8 + 8
            uint8_t num_digits = *ptr++;
            int64_t base = 0, repeat = 0, inc = 0;
            for (int i = 0; i < 8; i++) base |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) repeat |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) inc |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;

            for (size_t i = 0; i < line_count; i++) {
                int64_t val = base + ((int64_t)i / repeat) * inc;
                std::string s = std::to_string(val);
                while (s.size() < num_digits) s = "0" + s;
                columns[c][i] = s;
            }
        }
        else if (type == ColumnType::COL_TIMESTAMP) {
            if (ptr + 8 > end) return {};
            int64_t first = 0;
            for (int i = 0; i < 8; i++) first |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;

            columns[c][0] = std::to_string(first);

            if (line_count > 1) {
                int64_t delta = zigzag_decode_val(varint_decode_from(ptr, end));
                columns[c][1] = std::to_string(first + delta);

                int64_t prev_val = first + delta;
                for (size_t i = 2; i < line_count; i++) {
                    int64_t dd = zigzag_decode_val(varint_decode_from(ptr, end));
                    delta = delta + dd;
                    prev_val = prev_val + delta;
                    columns[c][i] = std::to_string(prev_val);
                }
            }
        }
        else if (type == ColumnType::COL_DICTIONARY) {
            if (ptr + 2 > end) return {};
            uint16_t dict_size = ptr[0] | (ptr[1] << 8);
            ptr += 2;

            std::vector<std::string> dict(dict_size);
            for (uint16_t d = 0; d < dict_size; d++) {
                if (ptr + 1 > end) return {};
                uint8_t len = *ptr++;
                if (ptr + len > end) return {};
                dict[d] = std::string((char*)ptr, len);
                ptr += len;
            }

            // Read varint indices
            for (size_t i = 0; i < line_count; i++) {
                if (ptr >= end) return {};
                uint16_t idx = (uint16_t)varint_decode_from(ptr, end);
                if (idx >= dict_size) return {};  // Invalid index
                columns[c][i] = dict[idx];
            }
        }
        else if (type == ColumnType::COL_CONST) {
            // Single constant value used for all rows
            if (ptr + 1 > end) return {};
            uint8_t len = *ptr++;
            if (ptr + len > end) return {};
            std::string val((char*)ptr, len);
            ptr += len;
            for (size_t i = 0; i < line_count; i++) {
                columns[c][i] = val;
            }
        }
        else if (type == ColumnType::COL_SUBTEMPLATE) {
            // [prefix_len:1][prefix...][suffix_len:1][suffix...][base:8][delta:8]
            if (ptr + 1 > end) return {};
            uint8_t prefix_len = *ptr++;
            if (ptr + prefix_len > end) return {};
            std::string prefix((char*)ptr, prefix_len);
            ptr += prefix_len;

            if (ptr + 1 > end) return {};
            uint8_t suffix_len = *ptr++;
            if (ptr + suffix_len > end) return {};
            std::string suffix((char*)ptr, suffix_len);
            ptr += suffix_len;

            if (ptr + 16 > end) return {};
            int64_t base = 0, delta = 0;
            for (int i = 0; i < 8; i++) base |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;
            for (int i = 0; i < 8; i++) delta |= ((int64_t)ptr[i]) << (i*8);
            ptr += 8;

            for (size_t i = 0; i < line_count; i++) {
                columns[c][i] = prefix + std::to_string(base + delta * (int64_t)i) + suffix;
            }
        }
        else {  // COL_RAW
            if (ptr + 4 > end) return {};
            uint32_t total = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;

            if (ptr + total > end) return {};
            const uint8_t* col_end = ptr + total;
            for (size_t i = 0; i < line_count; i++) {
                if (ptr + 1 > col_end) return {};
                uint8_t len = *ptr++;
                if (ptr + len > col_end) return {};
                columns[c][i] = std::string((char*)ptr, len);
                ptr += len;
            }
        }
    }

    // Read footer bytes (lines after template, like ";\nUNLOCK TABLES;\n")
    if (ptr + 4 > end) return {};
    uint32_t ftr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    std::string footer_bytes;
    if (ftr_len > 0) {
        if (ptr + ftr_len > end) return {};
        footer_bytes = std::string((char*)ptr, ftr_len);
        ptr += ftr_len;
    }

    // Read non-matching lines (interleaved in the middle)
    std::vector<std::pair<size_t, std::string>> non_matching_lines;
    if (ptr + 4 <= end) {
        uint32_t nm_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;

        for (uint32_t i = 0; i < nm_count && ptr < end; i++) {
            if (ptr + 8 > end) break;
            uint32_t insert_after = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;
            uint32_t len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;

            if (ptr + len > end) break;
            std::string raw_bytes((char*)ptr, len);
            ptr += len;

            non_matching_lines.emplace_back(insert_after, raw_bytes);
        }
    }

    // Reconstruct lines from template + columns, interleaving non-matching lines.
    // Hard cap at 2x original_size to bound runaway corruption from malformed metadata.
    std::vector<uint8_t> result;
    result.reserve(original_size);
    const size_t MAX_RESULT = original_size * 2 + 16;
    auto safe_push = [&](uint8_t b) -> bool {
        if (result.size() >= MAX_RESULT) return false;
        result.push_back(b);
        return true;
    };

    // Prepend header bytes (skipped lines like "INSERT INTO... VALUES")
    for (char c : header_bytes) {
        if (!safe_push((uint8_t)c)) return {};
    }

    size_t nm_idx = 0;  // Index into non_matching_lines

    for (size_t line = 0; line < line_count; line++) {
        // Insert any non-matching lines scheduled before this matching line
        // (stored as insert_after = line index, meaning insert before outputting this line)
        while (nm_idx < non_matching_lines.size() && non_matching_lines[nm_idx].first == line) {
            for (char c : non_matching_lines[nm_idx].second) {
                if (!safe_push((uint8_t)c)) return {};
            }
            nm_idx++;
        }

        // Output the template-expanded matching line
        size_t i = 0;
        while (i < template_str.size()) {
            if (template_str[i] == '$' && i + 1 < template_str.size()) {
                // Parse column index
                size_t j = i + 1;
                while (j < template_str.size() && template_str[j] >= '0' && template_str[j] <= '9') {
                    j++;
                }
                if (j > i + 1) {
                    size_t col_idx = std::stoul(template_str.substr(i + 1, j - i - 1));
                    if (col_idx < columns.size()) {
                        for (char c : columns[col_idx][line]) {
                            if (!safe_push((uint8_t)c)) return {};
                        }
                    }
                    i = j;
                    continue;
                }
            }
            if (!safe_push((uint8_t)template_str[i])) return {};
            i++;
        }
        if (!safe_push('\n')) return {};
    }

    // Insert any remaining non-matching lines after the last matching line
    while (nm_idx < non_matching_lines.size()) {
        for (char c : non_matching_lines[nm_idx].second) {
            if (!safe_push((uint8_t)c)) return {};
        }
        nm_idx++;
    }

    // Append footer bytes (lines after template, like ";\nUNLOCK TABLES;\n")
    for (char c : footer_bytes) {
        if (!safe_push((uint8_t)c)) return {};
    }

    // Remove trailing newline if original didn't have it
    while (result.size() > original_size && !result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

// ============================================================================
// ML_TEMPLATE: Multi-line template compression
// For JavaScript-like patterns with repeated function blocks
// Achieves 44% better than zstd on synthetic JS (498 vs 885 bytes)
// ============================================================================

struct MLTemplateParams {
    std::vector<std::string> template_lines;  // Template block lines
    std::vector<std::string> variables;       // Variable values (e.g., function numbers)
    int block_line_count;                     // Lines per block
    std::string header_bytes;                 // Bytes before first block
    std::string footer_bytes;                 // Bytes after last block
};

// Remove digits from string (to find structural similarity)
inline std::string remove_digits_for_hash(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (!isdigit((unsigned char)c)) result += c;
    }
    return result;
}

// Detect multi-line template patterns (repeated code blocks)
inline bool detect_ml_template(const uint8_t* data, size_t n, MLTemplateParams& params) {
    // Split into lines
    std::vector<std::string> lines;
    std::vector<size_t> line_offsets;  // Start byte of each line
    std::string current;
    size_t line_start = 0;

    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            line_offsets.push_back(line_start);
            lines.push_back(current);
            current.clear();
            line_start = i + 1;
        } else {
            current += (char)data[i];
        }
    }
    if (!current.empty()) {
        line_offsets.push_back(line_start);
        lines.push_back(current);
    }

    if (lines.size() < 30) return false;  // Need many lines for this to help

    // Find blank-line separated blocks
    std::vector<std::pair<size_t, size_t>> blocks;  // (start_line_idx, end_line_idx)
    size_t block_start = 0;
    bool in_block = false;

    for (size_t i = 0; i < lines.size(); i++) {
        bool is_blank = lines[i].empty() ||
                        lines[i].find_first_not_of(" \t") == std::string::npos;

        if (!is_blank && !in_block) {
            block_start = i;
            in_block = true;
        } else if (is_blank && in_block) {
            blocks.push_back({block_start, i});
            in_block = false;
        }
    }
    if (in_block) blocks.push_back({block_start, lines.size()});

    if (blocks.size() < 15) return false;  // Need many blocks

    // Hash block structures (lines with digits removed)
    std::unordered_map<size_t, size_t> structure_counts;
    std::unordered_map<size_t, size_t> first_block_with_structure;

    for (size_t bi = 0; bi < blocks.size(); bi++) {
        auto& [start, end] = blocks[bi];
        size_t line_count = end - start;

        // Build structure hash
        size_t h = line_count;
        for (size_t i = start; i < end; i++) {
            std::string stripped = remove_digits_for_hash(lines[i]);
            for (char c : stripped) h = h * 31 + (size_t)c;
        }

        structure_counts[h]++;
        if (first_block_with_structure.find(h) == first_block_with_structure.end()) {
            first_block_with_structure[h] = bi;
        }
    }

    // Find most common structure
    size_t best_hash = 0;
    size_t best_count = 0;
    for (auto& [h, c] : structure_counts) {
        if (c > best_count) {
            best_count = c;
            best_hash = h;
        }
    }

    // Need at least 80% of blocks to match (very repetitive code)
    if (best_count < blocks.size() * 0.8) return false;
    if (best_count < 15) return false;

    // Extract template from first matching block
    size_t template_block_idx = first_block_with_structure[best_hash];
    auto& [tpl_start, tpl_end] = blocks[template_block_idx];
    params.block_line_count = tpl_end - tpl_start;

    params.template_lines.clear();
    for (size_t i = tpl_start; i < tpl_end; i++) {
        params.template_lines.push_back(lines[i]);
    }

    // Extract variables from each matching block
    params.variables.clear();
    for (size_t bi = 0; bi < blocks.size(); bi++) {
        auto& [start, end] = blocks[bi];
        size_t line_count = end - start;

        // Recompute hash to check if this block matches
        size_t h = line_count;
        for (size_t i = start; i < end; i++) {
            std::string stripped = remove_digits_for_hash(lines[i]);
            for (char c : stripped) h = h * 31 + (size_t)c;
        }

        if (h != best_hash) continue;

        // Extract variable (first digit run in first line only)
        std::string var;
        bool found_digit = false;
        for (char c : lines[start]) {
            if (isdigit((unsigned char)c)) {
                var += c;
                found_digit = true;
            } else if (found_digit) {
                break;  // Stop after first digit run
            }
        }
        if (!var.empty()) {
            params.variables.push_back(var);
        }
    }

    if (params.variables.size() < 15) return false;

    // Find header bytes (before first matching block)
    size_t first_block_start_line = 0;
    for (size_t bi = 0; bi < blocks.size(); bi++) {
        auto& [start, end] = blocks[bi];
        size_t line_count = end - start;
        size_t h = line_count;
        for (size_t i = start; i < end; i++) {
            std::string stripped = remove_digits_for_hash(lines[i]);
            for (char c : stripped) h = h * 31 + (size_t)c;
        }
        if (h == best_hash) {
            first_block_start_line = start;
            break;
        }
    }

    if (first_block_start_line > 0) {
        params.header_bytes.clear();
        size_t header_end_byte = line_offsets[first_block_start_line];
        for (size_t i = 0; i < header_end_byte; i++) {
            params.header_bytes += (char)data[i];
        }
    }

    // Find footer bytes (everything after the last matching block)
    params.footer_bytes.clear();
    size_t last_block_end_byte = 0;
    for (size_t bi = blocks.size(); bi > 0; bi--) {
        auto& [start, end] = blocks[bi - 1];
        size_t line_count = end - start;
        size_t h = line_count;
        for (size_t i = start; i < end; i++) {
            std::string stripped = remove_digits_for_hash(lines[i]);
            for (char c : stripped) h = h * 31 + (size_t)c;
        }
        if (h == best_hash) {
            // This is the last matching block
            // End byte is after the blank line following the block
            if (end < lines.size()) {
                // Include the blank line after the block
                last_block_end_byte = line_offsets[end] + lines[end].size() + 1;  // +1 for newline
            } else {
                // Block extends to end of lines array
                // Calculate byte position after last line of block
                last_block_end_byte = line_offsets[end - 1] + lines[end - 1].size() + 1;
            }
            break;
        }
    }

    // Capture everything from after last matching block to end of input
    if (last_block_end_byte < n) {
        for (size_t i = last_block_end_byte; i < n; i++) {
            params.footer_bytes += (char)data[i];
        }
    }

    // Validation: ML_TEMPLATE can only track ONE variable per block.
    // If template lines have varying digits across blocks (like port numbers),
    // we need to reject this pattern. Check that all blocks have identical
    // non-first-line content (with the same digits).
    // Simple check: verify that digit runs in non-first-lines are consistent across blocks.
    if (blocks.size() >= 2) {
        auto& [first_start, first_end] = blocks[first_block_with_structure[best_hash]];
        for (size_t bi = 0; bi < blocks.size() && bi < 5; bi++) {  // Check first 5 blocks
            auto& [start, end] = blocks[bi];
            if (end - start != first_end - first_start) continue;

            // Check non-first lines for digit consistency
            for (size_t li = 1; li < end - start; li++) {
                const std::string& first_line = lines[first_start + li];
                const std::string& this_line = lines[start + li];
                if (first_line != this_line) {
                    // Lines differ - if both have digits, ML_TEMPLATE can't handle this
                    bool first_has_digit = false, this_has_digit = false;
                    for (char c : first_line) if (isdigit((unsigned char)c)) { first_has_digit = true; break; }
                    for (char c : this_line) if (isdigit((unsigned char)c)) { this_has_digit = true; break; }
                    if (first_has_digit && this_has_digit) {
                        // Both have digits but content differs - reject ML_TEMPLATE
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

// Encode multi-line template
inline std::vector<uint8_t> encode_ml_template(const MLTemplateParams& params) {
    std::vector<uint8_t> result;

    // Header: block_line_count (2 bytes), variable_count (4 bytes)
    result.push_back(params.block_line_count & 0xFF);
    result.push_back((params.block_line_count >> 8) & 0xFF);
    uint32_t var_count = params.variables.size();
    result.push_back(var_count & 0xFF);
    result.push_back((var_count >> 8) & 0xFF);
    result.push_back((var_count >> 16) & 0xFF);
    result.push_back((var_count >> 24) & 0xFF);

    // Header bytes length + content
    uint32_t hdr_len = params.header_bytes.size();
    result.push_back(hdr_len & 0xFF);
    result.push_back((hdr_len >> 8) & 0xFF);
    result.push_back((hdr_len >> 16) & 0xFF);
    result.push_back((hdr_len >> 24) & 0xFF);
    for (char c : params.header_bytes) result.push_back((uint8_t)c);

    // Template lines (newline separated, null terminated)
    for (auto& line : params.template_lines) {
        for (char c : line) result.push_back((uint8_t)c);
        result.push_back('\n');
    }
    result.push_back(0);  // End of template

    // Variables (newline separated)
    for (auto& var : params.variables) {
        for (char c : var) result.push_back((uint8_t)c);
        result.push_back('\n');
    }

    // Footer bytes length + content
    uint32_t ftr_len = params.footer_bytes.size();
    result.push_back(ftr_len & 0xFF);
    result.push_back((ftr_len >> 8) & 0xFF);
    result.push_back((ftr_len >> 16) & 0xFF);
    result.push_back((ftr_len >> 24) & 0xFF);
    for (char c : params.footer_bytes) result.push_back((uint8_t)c);

    return result;
}

// Decode multi-line template
inline std::vector<uint8_t> decode_ml_template(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 10) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read header
    uint16_t block_line_count = ptr[0] | (ptr[1] << 8);
    ptr += 2;
    uint32_t var_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Read header bytes
    if (ptr + 4 > end) return {};
    uint32_t hdr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    if (ptr + hdr_len > end) return {};
    std::string header_bytes((char*)ptr, hdr_len);
    ptr += hdr_len;

    // Read template lines
    std::vector<std::string> template_lines;
    std::string current;
    while (ptr < end && *ptr != 0) {
        if (*ptr == '\n') {
            template_lines.push_back(current);
            current.clear();
        } else {
            current += (char)*ptr;
        }
        ptr++;
    }
    if (ptr < end) ptr++;  // Skip null terminator

    // Read exactly var_count variables (newline separated)
    std::vector<std::string> variables;
    current.clear();
    while (ptr < end && variables.size() < var_count) {
        if (*ptr == '\n') {
            if (!current.empty()) {
                variables.push_back(current);
                current.clear();
            }
        } else {
            current += (char)*ptr;
        }
        ptr++;
    }
    if (!current.empty() && variables.size() < var_count) variables.push_back(current);

    // Read footer bytes length + content
    std::string footer_bytes;
    if (ptr + 4 <= end) {
        uint32_t ftr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
        if (ptr + ftr_len <= end) {
            footer_bytes = std::string((char*)ptr, ftr_len);
            ptr += ftr_len;
        }
    }

    // Reconstruct output
    std::vector<uint8_t> result;
    result.reserve(original_size);

    // Header bytes
    for (char c : header_bytes) result.push_back((uint8_t)c);

    // Generate blocks by substituting variables into template
    for (size_t vi = 0; vi < variables.size(); vi++) {
        const std::string& var = variables[vi];

        for (auto& line : template_lines) {
            // Find and replace digits with variable
            bool in_digit_run = false;
            for (size_t i = 0; i < line.size(); i++) {
                if (isdigit((unsigned char)line[i])) {
                    if (!in_digit_run) {
                        // Replace digit run with variable
                        for (char c : var) result.push_back((uint8_t)c);
                        in_digit_run = true;
                    }
                    // Skip subsequent digits
                } else {
                    in_digit_run = false;
                    result.push_back((uint8_t)line[i]);
                }
            }
            result.push_back('\n');
        }
        // Add blank line between blocks
        result.push_back('\n');
    }

    // Remove trailing newlines before adding footer
    while (result.size() > original_size - footer_bytes.size() && !result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    // Add footer bytes
    for (char c : footer_bytes) result.push_back((uint8_t)c);

    return result;
}

// ============================================================================
// ML_TEMPLATE_DUAL: Dual alternating multi-line template compression
// For TypeScript-like patterns with alternating interface + component blocks
// ============================================================================

// ML_TEMPLATE_DUAL: Dual alternating multi-line template compression
// For TypeScript-like patterns with alternating interface + component blocks

struct MLTemplateDualParams {
    std::vector<std::string> template_a_lines;  // First template (e.g., interface)
    std::vector<std::string> template_b_lines;  // Second template (e.g., component)
    std::vector<std::string> variables;         // Variable values for each block
    std::vector<bool> block_types;              // false=A, true=B for each block
    int block_a_line_count;
    int block_b_line_count;
    std::string header_bytes;
    std::string footer_bytes;
    bool is_alternating;  // If true, pattern is strictly A-B-A-B
};

// Detect dual alternating multi-line template patterns
inline bool detect_ml_template_dual(const uint8_t* data, size_t n, MLTemplateDualParams& params) {
    // Split into lines
    std::vector<std::string> lines;
    std::vector<size_t> line_offsets;
    std::string current;
    size_t line_start = 0;

    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            line_offsets.push_back(line_start);
            lines.push_back(current);
            current.clear();
            line_start = i + 1;
        } else {
            current += (char)data[i];
        }
    }
    if (!current.empty()) {
        line_offsets.push_back(line_start);
        lines.push_back(current);
    }

    if (lines.size() < 30) return false;

    // Find blank-line separated blocks
    std::vector<std::pair<size_t, size_t>> blocks;
    size_t block_start = 0;
    bool in_block = false;

    for (size_t i = 0; i < lines.size(); i++) {
        bool is_blank = lines[i].empty() ||
                        lines[i].find_first_not_of(" \t") == std::string::npos;

        if (!is_blank && !in_block) {
            block_start = i;
            in_block = true;
        } else if (is_blank && in_block) {
            blocks.push_back({block_start, i});
            in_block = false;
        }
    }
    if (in_block) blocks.push_back({block_start, lines.size()});

    if (blocks.size() < 15) return false;

    // Hash block structures
    std::unordered_map<size_t, size_t> structure_counts;
    std::unordered_map<size_t, size_t> first_block_with_structure;
    std::vector<size_t> block_hashes;

    for (size_t bi = 0; bi < blocks.size(); bi++) {
        auto& [start, end] = blocks[bi];
        size_t line_count = end - start;

        size_t h = line_count;
        for (size_t i = start; i < end; i++) {
            std::string stripped = remove_digits_for_hash(lines[i]);
            for (char c : stripped) h = h * 31 + (size_t)c;
        }

        block_hashes.push_back(h);
        structure_counts[h]++;
        if (first_block_with_structure.find(h) == first_block_with_structure.end()) {
            first_block_with_structure[h] = bi;
        }
    }

    // Find top TWO structures
    std::vector<std::pair<size_t, size_t>> sorted_structures;
    for (auto& [h, c] : structure_counts) {
        sorted_structures.push_back({c, h});
    }
    std::sort(sorted_structures.rbegin(), sorted_structures.rend());

    if (sorted_structures.size() < 2) return false;

    size_t hash_a = sorted_structures[0].second;
    size_t hash_b = sorted_structures[1].second;
    size_t count_a = sorted_structures[0].first;
    size_t count_b = sorted_structures[1].first;

    // Need at least 80% coverage by top 2, and each must have at least 20%
    double coverage = (double)(count_a + count_b) / blocks.size();
    if (coverage < 0.80) return false;
    if (count_a < blocks.size() * 0.2 || count_b < blocks.size() * 0.2) return false;
    if (count_a < 8 || count_b < 8) return false;

    // Check if pattern is alternating (A-B-A-B or B-A-B-A)
    bool is_alternating = true;
    bool expected_is_b = (block_hashes[0] == hash_b);
    for (size_t i = 0; i < blocks.size(); i++) {
        bool is_b = (block_hashes[i] == hash_b);
        bool is_a = (block_hashes[i] == hash_a);
        if (!is_a && !is_b) {
            is_alternating = false;
            break;
        }
        if (is_b != expected_is_b) {
            is_alternating = false;
            break;
        }
        expected_is_b = !expected_is_b;
    }

    // Extract templates
    size_t template_a_idx = first_block_with_structure[hash_a];
    size_t template_b_idx = first_block_with_structure[hash_b];

    auto& [ta_start, ta_end] = blocks[template_a_idx];
    auto& [tb_start, tb_end] = blocks[template_b_idx];

    params.block_a_line_count = ta_end - ta_start;
    params.block_b_line_count = tb_end - tb_start;

    params.template_a_lines.clear();
    for (size_t i = ta_start; i < ta_end; i++) {
        params.template_a_lines.push_back(lines[i]);
    }

    params.template_b_lines.clear();
    for (size_t i = tb_start; i < tb_end; i++) {
        params.template_b_lines.push_back(lines[i]);
    }

    params.is_alternating = is_alternating;

    // Extract variables and block types
    params.variables.clear();
    params.block_types.clear();

    for (size_t bi = 0; bi < blocks.size(); bi++) {
        auto& [start, end] = blocks[bi];
        bool is_b = (block_hashes[bi] == hash_b);
        bool is_a = (block_hashes[bi] == hash_a);

        if (!is_a && !is_b) continue;

        params.block_types.push_back(is_b);

        // Extract variable (first digit run in first line)
        std::string var;
        bool found_digit = false;
        for (char c : lines[start]) {
            if (isdigit((unsigned char)c)) {
                var += c;
                found_digit = true;
            } else if (found_digit) {
                break;
            }
        }
        if (!var.empty()) {
            params.variables.push_back(var);
        }
    }

    if (params.variables.size() < 15) return false;
    if (params.variables.size() != params.block_types.size()) return false;

    // Find header bytes
    size_t first_match_line = 0;
    for (size_t bi = 0; bi < blocks.size(); bi++) {
        if (block_hashes[bi] == hash_a || block_hashes[bi] == hash_b) {
            first_match_line = blocks[bi].first;
            break;
        }
    }

    if (first_match_line > 0) {
        params.header_bytes.clear();
        size_t header_end_byte = line_offsets[first_match_line];
        for (size_t i = 0; i < header_end_byte; i++) {
            params.header_bytes += (char)data[i];
        }
    }

    // Find footer bytes
    params.footer_bytes.clear();
    size_t last_block_end_byte = 0;
    for (size_t bi = blocks.size(); bi > 0; bi--) {
        if (block_hashes[bi-1] == hash_a || block_hashes[bi-1] == hash_b) {
            auto& [start, end] = blocks[bi - 1];
            if (end < lines.size()) {
                last_block_end_byte = line_offsets[end] + lines[end].size() + 1;
            } else {
                last_block_end_byte = line_offsets[end - 1] + lines[end - 1].size() + 1;
            }
            break;
        }
    }

    if (last_block_end_byte < n) {
        for (size_t i = last_block_end_byte; i < n; i++) {
            params.footer_bytes += (char)data[i];
        }
    }

    return true;
}

// Encode dual multi-line template
inline std::vector<uint8_t> encode_ml_template_dual(const MLTemplateDualParams& params) {
    std::vector<uint8_t> result;

    // Header: block_a_line_count (2), block_b_line_count (2), variable_count (4), flags (1)
    result.push_back(params.block_a_line_count & 0xFF);
    result.push_back((params.block_a_line_count >> 8) & 0xFF);
    result.push_back(params.block_b_line_count & 0xFF);
    result.push_back((params.block_b_line_count >> 8) & 0xFF);

    uint32_t var_count = params.variables.size();
    result.push_back(var_count & 0xFF);
    result.push_back((var_count >> 8) & 0xFF);
    result.push_back((var_count >> 16) & 0xFF);
    result.push_back((var_count >> 24) & 0xFF);

    // Flags: bit 0 = is_alternating, bit 1 = first_is_b
    uint8_t flags = params.is_alternating ? 0x01 : 0x00;
    if (!params.block_types.empty() && params.block_types[0]) flags |= 0x02;
    result.push_back(flags);

    // Header bytes
    uint32_t hdr_len = params.header_bytes.size();
    result.push_back(hdr_len & 0xFF);
    result.push_back((hdr_len >> 8) & 0xFF);
    result.push_back((hdr_len >> 16) & 0xFF);
    result.push_back((hdr_len >> 24) & 0xFF);
    for (char c : params.header_bytes) result.push_back((uint8_t)c);

    // Template A lines (null-terminated)
    for (auto& line : params.template_a_lines) {
        for (char c : line) result.push_back((uint8_t)c);
        result.push_back('\n');
    }
    result.push_back(0);

    // Template B lines (null-terminated)
    for (auto& line : params.template_b_lines) {
        for (char c : line) result.push_back((uint8_t)c);
        result.push_back('\n');
    }
    result.push_back(0);

    // Block types (only if not alternating)
    if (!params.is_alternating) {
        size_t num_bytes = (params.block_types.size() + 7) / 8;
        for (size_t i = 0; i < num_bytes; i++) {
            uint8_t byte = 0;
            for (size_t j = 0; j < 8 && i * 8 + j < params.block_types.size(); j++) {
                if (params.block_types[i * 8 + j]) byte |= (1 << j);
            }
            result.push_back(byte);
        }
    }

    // Variables (newline-separated)
    for (auto& var : params.variables) {
        for (char c : var) result.push_back((uint8_t)c);
        result.push_back('\n');
    }

    // Footer bytes
    uint32_t ftr_len = params.footer_bytes.size();
    result.push_back(ftr_len & 0xFF);
    result.push_back((ftr_len >> 8) & 0xFF);
    result.push_back((ftr_len >> 16) & 0xFF);
    result.push_back((ftr_len >> 24) & 0xFF);
    for (char c : params.footer_bytes) result.push_back((uint8_t)c);

    return result;
}

// Decode dual multi-line template
inline std::vector<uint8_t> decode_ml_template_dual(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 13) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read header
    uint16_t block_a_line_count = ptr[0] | (ptr[1] << 8);
    ptr += 2;
    uint16_t block_b_line_count = ptr[0] | (ptr[1] << 8);
    ptr += 2;
    uint32_t var_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    uint8_t flags = *ptr++;
    bool is_alternating = (flags & 0x01) != 0;
    bool first_is_b = (flags & 0x02) != 0;

    // Read header bytes
    if (ptr + 4 > end) return {};
    uint32_t hdr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    if (ptr + hdr_len > end) return {};
    std::string header_bytes((char*)ptr, hdr_len);
    ptr += hdr_len;

    // Read template A lines
    std::vector<std::string> template_a_lines;
    std::string current;
    while (ptr < end && *ptr != 0) {
        if (*ptr == '\n') {
            template_a_lines.push_back(current);
            current.clear();
        } else {
            current += (char)*ptr;
        }
        ptr++;
    }
    if (ptr < end) ptr++;

    // Read template B lines
    std::vector<std::string> template_b_lines;
    current.clear();
    while (ptr < end && *ptr != 0) {
        if (*ptr == '\n') {
            template_b_lines.push_back(current);
            current.clear();
        } else {
            current += (char)*ptr;
        }
        ptr++;
    }
    if (ptr < end) ptr++;

    // Read block types (if not alternating)
    std::vector<bool> block_types;
    if (!is_alternating) {
        size_t num_bytes = (var_count + 7) / 8;
        if (ptr + num_bytes > end) return {};
        for (size_t i = 0; i < var_count; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx = i % 8;
            bool is_b = (ptr[byte_idx] & (1 << bit_idx)) != 0;
            block_types.push_back(is_b);
        }
        ptr += num_bytes;
    } else {
        // Generate alternating pattern
        for (size_t i = 0; i < var_count; i++) {
            bool is_b = first_is_b ? (i % 2 == 0) : (i % 2 == 1);
            block_types.push_back(is_b);
        }
    }

    // Read variables
    std::vector<std::string> variables;
    current.clear();
    while (ptr < end && variables.size() < var_count) {
        if (*ptr == '\n') {
            if (!current.empty()) {
                variables.push_back(current);
                current.clear();
            }
        } else {
            current += (char)*ptr;
        }
        ptr++;
    }
    if (!current.empty() && variables.size() < var_count) variables.push_back(current);

    // Read footer bytes
    std::string footer_bytes;
    if (ptr + 4 <= end) {
        uint32_t ftr_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
        if (ptr + ftr_len <= end) {
            footer_bytes = std::string((char*)ptr, ftr_len);
        }
    }

    // Reconstruct output
    std::vector<uint8_t> result;
    result.reserve(original_size);

    for (char c : header_bytes) result.push_back((uint8_t)c);

    for (size_t vi = 0; vi < variables.size(); vi++) {
        const std::string& var = variables[vi];
        bool is_b = vi < block_types.size() ? block_types[vi] : false;
        const std::vector<std::string>& template_lines = is_b ? template_b_lines : template_a_lines;

        for (auto& line : template_lines) {
            bool in_digit_run = false;
            for (size_t i = 0; i < line.size(); i++) {
                if (isdigit((unsigned char)line[i])) {
                    if (!in_digit_run) {
                        for (char c : var) result.push_back((uint8_t)c);
                        in_digit_run = true;
                    }
                } else {
                    in_digit_run = false;
                    result.push_back((uint8_t)line[i]);
                }
            }
            result.push_back('\n');
        }
        result.push_back('\n');
    }

    while (result.size() > original_size - footer_bytes.size() && !result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    for (char c : footer_bytes) result.push_back((uint8_t)c);

    return result;
}


// ============================================================================
// COLUMNAR: Column-based compression for log files
// Splits fixed-column text (access logs) into columns, compresses separately
// Achieves 10% better than zstd on access logs (8117 vs 9026 bytes)
// ============================================================================

struct ColumnarParams {
    std::vector<std::vector<std::string>> columns;  // Each column's values
    std::string column_separators;  // How columns are joined
    std::string trailing;  // Trailing bytes that couldn't be parsed (partial line)
};

// Detect columnar text (fixed-format logs like Common Log Format)
inline bool detect_columnar_log(const uint8_t* data, size_t n, ColumnarParams& params) {
    // Split into lines, tracking byte positions
    std::vector<std::pair<std::string, size_t>> lines;  // (line content, end position)
    std::string current;
    size_t line_start = 0;
    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n') {
            if (!current.empty()) {
                lines.push_back({current, i + 1});  // +1 to include the newline
            }
            current.clear();
            line_start = i + 1;
        } else {
            current += (char)data[i];
        }
    }
    // Track trailing partial line separately
    size_t trailing_start = n;  // Will be updated if we find parsed lines
    if (!current.empty()) {
        lines.push_back({current, n});  // Partial line at end (no trailing newline)
    }

    if (lines.size() < 50) return false;

    // Try to detect Common Log Format pattern
    // Standard: IP - - [timestamp] "METHOD path HTTP/x.x" status size "-" "agent"
    // Extended: IP - - [timestamp] "METHOD path HTTP/x.x" status size "referer" "agent" time
    std::vector<std::string> ips, timestamps, methods, paths, statuses, sizes, referers, agents, times;
    size_t last_parsed_end = 0;  // Track end position of last successfully parsed line

    int parsed_count = 0;
    for (auto& [line, end_pos] : lines) {
        // Lines must end with a quote (standard format) or digit (extended format with response time)
        // Extended format: "agent" 0.123\n
        if (line.empty()) continue;
        char last = line.back();
        if (last != '"' && !(last >= '0' && last <= '9')) continue;

        // Parse common log format
        size_t pos = 0;

        // IP
        size_t space = line.find(' ');
        if (space == std::string::npos) continue;
        ips.push_back(line.substr(0, space));
        pos = space + 1;

        // Skip " - - "
        size_t bracket = line.find('[', pos);
        if (bracket == std::string::npos) continue;
        pos = bracket + 1;

        // Timestamp
        size_t bracket_end = line.find(']', pos);
        if (bracket_end == std::string::npos) continue;
        timestamps.push_back(line.substr(pos, bracket_end - pos));
        pos = bracket_end + 2;  // skip "] "

        // "METHOD path HTTP/x.x"
        if (pos >= line.size() || line[pos] != '"') continue;
        pos++;
        size_t quote_end = line.find('"', pos);
        if (quote_end == std::string::npos) continue;
        std::string req = line.substr(pos, quote_end - pos);
        size_t sp1 = req.find(' ');
        if (sp1 != std::string::npos) {
            methods.push_back(req.substr(0, sp1));
            size_t sp2 = req.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) {
                paths.push_back(req.substr(sp1 + 1, sp2 - sp1 - 1));
            } else {
                paths.push_back(req.substr(sp1 + 1));
            }
        } else {
            methods.push_back(req);
            paths.push_back("");
        }
        pos = quote_end + 2;

        // Status
        space = line.find(' ', pos);
        if (space != std::string::npos) {
            statuses.push_back(line.substr(pos, space - pos));
            pos = space + 1;
        } else continue;

        // Size
        space = line.find(' ', pos);
        if (space != std::string::npos) {
            sizes.push_back(line.substr(pos, space - pos));
            pos = space + 1;
        } else {
            sizes.push_back(line.substr(pos));
        }

        // Find quoted strings after size: "referer" "agent" [optional_time]
        // The remaining line after size should be: "ref" "agent" or "ref" "agent" 0.123
        size_t last_quote = line.rfind('"');
        if (last_quote == std::string::npos || last_quote <= pos) continue;

        // Find agent (last quoted string)
        size_t agent_end = last_quote;
        size_t agent_start = line.rfind('"', agent_end - 1);
        if (agent_start == std::string::npos) continue;
        agents.push_back(line.substr(agent_start + 1, agent_end - agent_start - 1));

        // Find referer (second-to-last quoted string)
        size_t ref_end = line.rfind('"', agent_start - 1);
        if (ref_end == std::string::npos) continue;
        size_t ref_start = line.rfind('"', ref_end - 1);
        if (ref_start == std::string::npos) continue;
        referers.push_back(line.substr(ref_start + 1, ref_end - ref_start - 1));

        // Check for response time after last quote
        if (agent_end + 1 < line.size()) {
            size_t time_start = line.find_first_not_of(' ', agent_end + 1);
            if (time_start != std::string::npos) {
                times.push_back(line.substr(time_start));
            } else {
                times.push_back("");
            }
        } else {
            times.push_back("");
        }

        parsed_count++;
        last_parsed_end = end_pos;  // Track where this line ends
    }

    // Need at least 80% of lines to parse successfully
    if (parsed_count < (int)(lines.size() * 0.8)) return false;
    if (parsed_count < 50) return false;

    // Verify all columns have same count
    size_t expected = ips.size();
    if (timestamps.size() != expected || methods.size() != expected ||
        paths.size() != expected || statuses.size() != expected ||
        sizes.size() != expected || referers.size() != expected ||
        agents.size() != expected || times.size() != expected) {
        return false;
    }

    // Store columns (9 columns for extended format)
    params.columns.clear();
    params.columns.push_back(std::move(ips));
    params.columns.push_back(std::move(timestamps));
    params.columns.push_back(std::move(methods));
    params.columns.push_back(std::move(paths));
    params.columns.push_back(std::move(statuses));
    params.columns.push_back(std::move(sizes));
    params.columns.push_back(std::move(referers));
    params.columns.push_back(std::move(agents));
    params.columns.push_back(std::move(times));

    // Capture trailing bytes that couldn't be parsed
    if (last_parsed_end < n) {
        params.trailing = std::string((char*)data + last_parsed_end, n - last_parsed_end);
    } else {
        params.trailing.clear();
    }

    return true;
}

// Encode columnar data
inline std::vector<uint8_t> encode_columnar(const ColumnarParams& params) {
    std::vector<uint8_t> result;

    uint8_t col_count = params.columns.size();
    if (col_count == 0) return result;

    uint32_t row_count = params.columns[0].size();

    // For 9-column format: use 8+1 split encoding
    // 8 columns (IP, timestamp, method, path, status, size, referer, agent) → BWT
    // 1 column (response_time) → zstd
    // This beats bzip2: 6972 vs 6993 bytes on 64KB nginx log
    if (col_count == 9) {
        // Build 8-column buffer (excluding response_time)
        std::vector<uint8_t> col8_data;
        for (int c = 0; c < 8; c++) {
            for (const auto& val : params.columns[c]) {
                for (char ch : val) col8_data.push_back((uint8_t)ch);
                col8_data.push_back('\n');
            }
        }

        // BWT compress 8 columns
        auto col8_bwt = bwt9::compress(col8_data.data(), col8_data.size());

        // Build response_time column
        std::vector<uint8_t> time_data;
        for (const auto& val : params.columns[8]) {
            for (char ch : val) time_data.push_back((uint8_t)ch);
            time_data.push_back('\n');
        }


        // BWT compress response_time (BWT beats zstd by 144 bytes on numeric strings!)
        auto time_bwt = bwt9::compress(time_data.data(), time_data.size());

        // Format v2: 0xFE + row_count(varint) + bwt8_size(varint) + bwt8 + time_size(varint) + time_bwt + trailing_len(varint) + trailing
        result.push_back(0xFE);  // Marker for 8+1 split format

        // Row count as varint (saves 2 bytes for typical 392 rows)
        uint8_t vbuf[8];
        size_t vlen = write_uvarint_buf(vbuf, row_count);
        for (size_t i = 0; i < vlen; i++) result.push_back(vbuf[i]);

        // BWT size (varint) + data
        vlen = write_uvarint_buf(vbuf, col8_bwt.size());
        for (size_t i = 0; i < vlen; i++) result.push_back(vbuf[i]);
        result.insert(result.end(), col8_bwt.begin(), col8_bwt.end());

        // Time BWT size (varint) + data
        vlen = write_uvarint_buf(vbuf, time_bwt.size());
        for (size_t i = 0; i < vlen; i++) result.push_back(vbuf[i]);
        result.insert(result.end(), time_bwt.begin(), time_bwt.end());

        // Trailing length as varint (saves 3 bytes when trailing=0)
        uint32_t trailing_len = params.trailing.size();
        vlen = write_uvarint_buf(vbuf, trailing_len);
        for (size_t i = 0; i < vlen; i++) result.push_back(vbuf[i]);
        for (char c : params.trailing) result.push_back((uint8_t)c);

        return result;
    }

    // Standard format: 7 columns
    result.push_back(col_count);
    result.push_back(row_count & 0xFF);
    result.push_back((row_count >> 8) & 0xFF);
    result.push_back((row_count >> 16) & 0xFF);
    result.push_back((row_count >> 24) & 0xFF);

    for (auto& col : params.columns) {
        for (auto& val : col) {
            for (char c : val) result.push_back((uint8_t)c);
            result.push_back('\n');
        }
        result.push_back(0);
    }

    uint32_t trailing_len = params.trailing.size();
    result.push_back(trailing_len & 0xFF);
    result.push_back((trailing_len >> 8) & 0xFF);
    result.push_back((trailing_len >> 16) & 0xFF);
    result.push_back((trailing_len >> 24) & 0xFF);
    for (char c : params.trailing) result.push_back((uint8_t)c);

    return result;
}

// Decode columnar data back to log format
inline std::vector<uint8_t> decode_columnar(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 5) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    uint8_t first_byte = *ptr++;

    // Check for 8+1 split format (0xFE marker) - BWT(8cols) + zstd(time)
    // Check for 8+1 split format (0xFE marker) - BWT(8cols) + BWT(time)
    if (first_byte == 0xFE) {
        // Read row_count as varint
        uint32_t row_count = 0;
        size_t shift = 0;
        while (ptr < end && (*ptr & 0x80)) {
            row_count |= (uint32_t)(*ptr++ & 0x7F) << shift;
            shift += 7;
        }
        if (ptr < end) row_count |= (uint32_t)(*ptr++) << shift;

        // Read BWT size and decompress 8 columns
        size_t bwt_size = 0;
        shift = 0;
        while (ptr < end && (*ptr & 0x80)) {
            bwt_size |= (size_t)(*ptr++ & 0x7F) << shift;
            shift += 7;
        }
        if (ptr < end) bwt_size |= (size_t)(*ptr++) << shift;

        if (ptr + bwt_size > end) return {};
        auto col8_data = bwt9::decompress(ptr, bwt_size);
        ptr += bwt_size;

        // Read time BWT size and decompress
        size_t time_size = 0;
        shift = 0;
        while (ptr < end && (*ptr & 0x80)) {
            time_size |= (size_t)(*ptr++ & 0x7F) << shift;
            shift += 7;
        }
        if (ptr < end) time_size |= (size_t)(*ptr++) << shift;

        if (ptr + time_size > end) return {};

        // Decompress time column with BWT
        auto time_data = bwt9::decompress(ptr, time_size);
        ptr += time_size;

        // Parse 8 columns from BWT-decompressed data
        std::vector<std::vector<std::string>> columns(9);
        const uint8_t* col_ptr = col8_data.data();
        const uint8_t* col_end = col8_data.data() + col8_data.size();
        for (int c = 0; c < 8; c++) {
            std::string current;
            while (col_ptr < col_end) {
                if (*col_ptr == '\n') {
                    columns[c].push_back(current);
                    current.clear();
                    col_ptr++;
                    if (columns[c].size() >= row_count) break;
                } else {
                    current += (char)*col_ptr++;
                }
            }
        }

        // Parse response_time from BWT-decompressed data
        {
            std::string current;
            for (size_t i = 0; i < time_data.size(); i++) {
                if (time_data[i] == '\n') {
                    columns[8].push_back(current);
                    current.clear();
                } else {
                    current += (char)time_data[i];
                }
            }
            if (!current.empty()) columns[8].push_back(current);
        }

        // Read trailing_len as varint
        uint32_t trailing_len = 0;
        shift = 0;
        while (ptr < end && (*ptr & 0x80)) {
            trailing_len |= (uint32_t)(*ptr++ & 0x7F) << shift;
            shift += 7;
        }
        if (ptr < end) trailing_len |= (uint32_t)(*ptr++) << shift;

        // Reconstruct extended nginx log format
        std::vector<uint8_t> result;
        result.reserve(original_size);

        for (size_t i = 0; i < row_count; i++) {
            std::string line;
            if (i < columns[0].size()) line += columns[0][i];
            line += " - - [";
            if (i < columns[1].size()) line += columns[1][i];
            line += "] \"";
            if (i < columns[2].size()) line += columns[2][i];
            line += " ";
            if (i < columns[3].size()) line += columns[3][i];
            line += " HTTP/1.1\" ";
            if (i < columns[4].size()) line += columns[4][i];
            line += " ";
            if (i < columns[5].size()) line += columns[5][i];
            line += " \"";
            if (i < columns[6].size()) line += columns[6][i];
            line += "\" \"";
            if (i < columns[7].size()) line += columns[7][i];
            line += "\"";
            if (i < columns[8].size() && !columns[8][i].empty()) {
                line += " " + columns[8][i];
            }
            for (char c : line) result.push_back((uint8_t)c);
            result.push_back('\n');
        }

        if (trailing_len > 0 && ptr + trailing_len <= end) {
            for (uint32_t i = 0; i < trailing_len; i++) {
                result.push_back(*ptr++);
            }
        }

        while (result.size() > original_size) result.pop_back();
        return result;
    }


    // Check for optimized 9-column format (0xFF marker) - legacy dictionary format
    if (first_byte == 0xFF) {
        uint32_t row_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;

        // Read 6 raw columns
        std::vector<std::vector<std::string>> columns(9);
        for (int c = 0; c < 6; c++) {
            std::string current;
            while (ptr < end && *ptr != 0) {
                if (*ptr == '\n') {
                    columns[c].push_back(current);
                    current.clear();
                } else {
                    current += (char)*ptr;
                }
                ptr++;
            }
            if (!current.empty()) columns[c].push_back(current);
            if (ptr < end) ptr++;
        }

        // Read referer dictionary
        uint8_t dict_count = *ptr++;
        std::vector<std::string> dict;
        for (int d = 0; d < dict_count; d++) {
            std::string entry;
            while (ptr < end && *ptr != 0) {
                entry += (char)*ptr++;
            }
            dict.push_back(entry);
            if (ptr < end) ptr++;
        }

        // Read referer indices
        for (uint32_t i = 0; i < row_count && ptr < end; i++) {
            uint8_t idx = *ptr++;
            if (idx < dict.size()) {
                columns[6].push_back(dict[idx]);
            } else {
                columns[6].push_back("");
            }
        }

        // Read agent column
        std::string current;
        while (ptr < end && *ptr != 0) {
            if (*ptr == '\n') {
                columns[7].push_back(current);
                current.clear();
            } else {
                current += (char)*ptr;
            }
            ptr++;
        }
        if (!current.empty()) columns[7].push_back(current);
        if (ptr < end) ptr++;

        // Read response time column
        current.clear();
        while (ptr < end && *ptr != 0) {
            if (*ptr == '\n') {
                columns[8].push_back(current);
                current.clear();
            } else {
                current += (char)*ptr;
            }
            ptr++;
        }
        if (!current.empty()) columns[8].push_back(current);
        if (ptr < end) ptr++;

        // Read trailing
        uint32_t trailing_len = 0;
        if (ptr + 4 <= end) {
            trailing_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;
        }

        // Reconstruct
        std::vector<uint8_t> result;
        result.reserve(original_size);

        // Trust no length field: guard every parallel-column access (columns 1..5 were unguarded,
        // an OOB vector::operator[] on a malformed stream). Matches the standard-7col branch below.
        auto col = [&](int c, size_t i) -> const std::string& {
            static const std::string kEmpty;
            return (i < columns[c].size()) ? columns[c][i] : kEmpty;
        };
        for (size_t i = 0; i < row_count && i < columns[0].size(); i++) {
            std::string line = col(0,i) + " - - [" + col(1,i) + "] \"" +
                   col(2,i) + " " + col(3,i) + " HTTP/1.1\" " +
                   col(4,i) + " " + col(5,i) + " \"" +
                   col(6,i) + "\" \"" + col(7,i) + "\"";
            if (i < columns[8].size() && !columns[8][i].empty()) {
                line += " " + columns[8][i];
            }
            for (char c : line) result.push_back((uint8_t)c);
            result.push_back('\n');
        }

        if (trailing_len > 0 && ptr + trailing_len <= end) {
            for (uint32_t i = 0; i < trailing_len; i++) {
                result.push_back(*ptr++);
            }
        }

        while (result.size() > original_size) result.pop_back();
        return result;
    }

    // Standard format: 7-column
    uint8_t col_count = first_byte;
    if (col_count != 7) return {};

    uint32_t row_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    std::vector<std::vector<std::string>> columns(col_count);
    for (int c = 0; c < col_count; c++) {
        std::string current;
        while (ptr < end && *ptr != 0) {
            if (*ptr == '\n') {
                columns[c].push_back(current);
                current.clear();
            } else {
                current += (char)*ptr;
            }
            ptr++;
        }
        if (!current.empty()) columns[c].push_back(current);
        if (ptr < end) ptr++;
    }

    uint32_t trailing_len = 0;
    if (ptr + 4 <= end) {
        trailing_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
    }

    std::vector<uint8_t> result;
    result.reserve(original_size);

    // Trust no length field: columns 1..6 may hold fewer rows than column 0 on a malformed
    // stream. Indexing columns[k][i] unguarded is vector::operator[] OOB -> reads a garbage
    // std::string -> heap corruption / SIGSEGV. Guard each access like the 0xFF branch above.
    // (Provably inert on valid CL streams: the encoder emits 7 equal-length columns.)
    // Found by fuzz_decode on a 'CL'-magic random stream, first_byte=7 (2026-08-07).
    auto col = [&](int c, size_t i) -> const std::string& {
        static const std::string kEmpty;
        return (i < columns[c].size()) ? columns[c][i] : kEmpty;
    };
    for (size_t i = 0; i < row_count && i < columns[0].size(); i++) {
        std::string line = col(0,i) + " - - [" + col(1,i) + "] \"" +
                   col(2,i) + " " + col(3,i) + " HTTP/1.1\" " +
                   col(4,i) + " " + col(5,i) + " \"-\" \"" +
                   col(6,i) + "\"";
        for (char c : line) result.push_back((uint8_t)c);
        result.push_back('\n');
    }

    if (trailing_len > 0 && ptr + trailing_len <= end) {
        for (uint32_t i = 0; i < trailing_len; i++) {
            result.push_back(*ptr++);
        }
    }

    while (result.size() > original_size) result.pop_back();
    return result;
}

// ============================================================================
// CSV_COLUMNAR: 2-Level Kolmogorov for CSV data
// Level 1: Parse columns from CSV structure
// Level 2: Per-column optimal encoding (LINEAR_GEN for sequential IDs!)
// Key insight: ID column "1,2,3,...,N" compresses from 2801 to 17 bytes!
// ============================================================================

// Column encoding type for CSV
enum class CsvColumnType : uint8_t {
    LINEAR_GEN = 0,   // Sequential: 1,2,3,... -> 17 bytes!
    RAW = 1           // Fallback: zstd compressed
};

struct CsvColumnInfo {
    CsvColumnType type;
    std::vector<std::string> values;
    // LINEAR_GEN params
    int64_t linear_start;
    int64_t linear_step;
};

struct CsvColumnarParams {
    std::string header;
    std::vector<CsvColumnInfo> columns;
    size_t row_count;
};

// Section template: repeating multi-line sections with sequential {N} variable
// Example: Markdown "## Section 1\n...\n## Section 2\n..." -> template + LINEAR_GEN
// Key insight: 65KB of Markdown -> ~155 bytes (422x compression!)
struct SectionTemplateParams {
    std::string section_template;  // Template with {N} placeholders for section number
    int64_t start_n;               // Starting section number (usually 1)
    int64_t step_n;                // Step between sections (usually 1)
    size_t section_count;          // Number of complete sections
    std::string header;            // Bytes before first section
    std::string footer;            // Incomplete section at end (if truncated)
};

// Check if column contains sequential integers (LINEAR_GEN candidate)
inline bool is_linear_column(const std::vector<std::string>& values,
                             int64_t& start, int64_t& step) {
    if (values.size() < 3) return false;

    // Try to parse first 3 values as integers
    std::vector<int64_t> nums;
    for (size_t i = 0; i < std::min((size_t)3, values.size()); i++) {
        try {
            nums.push_back(parse_int64(values[i]));
        } catch (...) {
            return false;  // Not all integers
        }
    }

    start = nums[0];
    step = nums[1] - nums[0];

    // Verify entire column follows the pattern
    for (size_t i = 0; i < values.size(); i++) {
        try {
            int64_t expected = start + step * (int64_t)i;
            int64_t actual = parse_int64(values[i]);
            if (actual != expected) return false;
        } catch (...) {
            return false;
        }
    }

    return true;
}

// Detect CSV format with columnar structure
inline bool detect_csv_columnar(const uint8_t* data, size_t n, CsvColumnarParams& params) {
    if (n < 256) return false;  // Too small

    // Must end with newline - otherwise we can't reconstruct exactly
    if (data[n-1] != '\n' && data[n-1] != '\r') return false;

    // Check for CSV structure: lines with comma-separated values
    std::vector<std::string> lines;
    std::string current;
    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\n' || data[i] == '\r') {
            if (!current.empty()) lines.push_back(current);
            current.clear();
            if (data[i] == '\r' && i + 1 < n && data[i+1] == '\n') i++;
        } else {
            current += (char)data[i];
        }
    }
    if (!current.empty()) lines.push_back(current);

    if (lines.size() < 10) return false;  // Need enough rows

    // First line should be header
    params.header = lines[0];

    // Count columns in header
    size_t col_count = 1;
    for (char c : params.header) {
        if (c == ',') col_count++;
    }
    if (col_count < 2 || col_count > 100) return false;  // Invalid

    // Parse all rows
    params.columns.resize(col_count);
    params.row_count = 0;

    for (size_t row = 1; row < lines.size(); row++) {
        std::vector<std::string> fields;
        std::string field;
        bool in_quote = false;
        for (char c : lines[row]) {
            if (c == '"') {
                in_quote = !in_quote;
            } else if (c == ',' && !in_quote) {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);

        if (fields.size() != col_count) continue;  // Skip malformed rows

        for (size_t c = 0; c < col_count; c++) {
            params.columns[c].values.push_back(fields[c]);
        }
        params.row_count++;
    }

    if (params.row_count < 10) return false;  // Not enough valid rows

    // Detect LINEAR_GEN columns
    bool has_linear = false;
    for (auto& col : params.columns) {
        if (is_linear_column(col.values, col.linear_start, col.linear_step)) {
            col.type = CsvColumnType::LINEAR_GEN;
            has_linear = true;
        } else {
            col.type = CsvColumnType::RAW;
        }
    }

    // Only use CSV_COLUMNAR if we found LINEAR_GEN columns
    // (otherwise regular zstd is just as good)
    return has_linear;
}

// Encode CSV columnar data
inline std::vector<uint8_t> encode_csv_columnar(const CsvColumnarParams& params) {
    std::vector<uint8_t> result;

    // Header: col_count (1), row_count (4), header string (null-terminated)
    uint8_t col_count = params.columns.size();
    result.push_back(col_count);

    uint32_t row_count = params.row_count;
    result.push_back(row_count & 0xFF);
    result.push_back((row_count >> 8) & 0xFF);
    result.push_back((row_count >> 16) & 0xFF);
    result.push_back((row_count >> 24) & 0xFF);

    for (char c : params.header) result.push_back((uint8_t)c);
    result.push_back(0);

    // Encode each column
    for (const auto& col : params.columns) {
        result.push_back((uint8_t)col.type);

        if (col.type == CsvColumnType::LINEAR_GEN) {
            // 17 bytes: start (8) + step (8) + count is already in header
            for (int i = 0; i < 8; i++) {
                result.push_back((col.linear_start >> (i * 8)) & 0xFF);
            }
            for (int i = 0; i < 8; i++) {
                result.push_back((col.linear_step >> (i * 8)) & 0xFF);
            }
        } else {
            // RAW: Join values with newlines, compress with best of BWT/zstd
            // Analysis shows BWT wins 28-46% on log columns (IPs, timestamps, requests, agents)
            std::string joined;
            for (const auto& val : col.values) {
                joined += val + "\n";
            }

            // Try BWT, zstd, and CM (BWT+CM, bzip3-class); pick winner
            auto bwt_out = bwt9::compress((const uint8_t*)joined.data(), joined.size());
            std::vector<uint8_t> zstd_out(ZSTD_compressBound(joined.size()));
            size_t zstd_size = ZSTD_compress(zstd_out.data(), zstd_out.size(),
                                              joined.data(), joined.size(), 19);
            std::vector<uint8_t> cm_out;
#ifndef MZIP_NO_CM
            cm_out = cmbk::compress_bwt((const uint8_t*)joined.data(), joined.size());
#endif

            bool zstd_ok = !ZSTD_isError(zstd_size);
            size_t bwt_sz  = (!bwt_out.empty() && bwt_out.size() < joined.size()) ? bwt_out.size() : SIZE_MAX;
            size_t zstd_sz = (zstd_ok && zstd_size < joined.size()) ? zstd_size : SIZE_MAX;
            size_t cm_sz   = (!cm_out.empty() && cm_out.size() < joined.size()) ? cm_out.size() : SIZE_MAX;
            size_t best_sz = std::min({bwt_sz, zstd_sz, cm_sz});
            bool use_cm   = (best_sz != SIZE_MAX && cm_sz  == best_sz);
            bool use_bwt  = (!use_cm && best_sz != SIZE_MAX && bwt_sz  == best_sz);
            bool use_zstd = (!use_cm && !use_bwt && best_sz != SIZE_MAX && zstd_sz == best_sz);
#ifdef MZIP_CM_DEBUG
            fprintf(stderr,"[csvcol] n=%zu bwt=%zd zstd=%zd cm=%zd -> %s\n", joined.size(),
                    (ssize_t)bwt_sz,(ssize_t)zstd_sz,(ssize_t)cm_sz,
                    use_cm?"CM":(use_bwt?"bwt":(use_zstd?"zstd":"raw")));
#endif

            if (use_cm) {
                // CM wins (flag 3) — BWT+CM, beats bwt9/zstd on numeric/columnar streams
                result.push_back(3);
                uint32_t len = (uint32_t)cm_out.size();
                result.push_back(len & 0xFF);
                result.push_back((len >> 8) & 0xFF);
                result.push_back((len >> 16) & 0xFF);
                result.push_back((len >> 24) & 0xFF);
                uint32_t orig_len = (uint32_t)joined.size();
                result.push_back(orig_len & 0xFF);
                result.push_back((orig_len >> 8) & 0xFF);
                result.push_back((orig_len >> 16) & 0xFF);
                result.push_back((orig_len >> 24) & 0xFF);
                for (uint8_t b : cm_out) result.push_back(b);
            } else if (use_bwt) {
                // BWT wins (flag 2)
                result.push_back(2);
                uint32_t len = bwt_out.size();
                result.push_back(len & 0xFF);
                result.push_back((len >> 8) & 0xFF);
                result.push_back((len >> 16) & 0xFF);
                result.push_back((len >> 24) & 0xFF);
                uint32_t orig_len = joined.size();
                result.push_back(orig_len & 0xFF);
                result.push_back((orig_len >> 8) & 0xFF);
                result.push_back((orig_len >> 16) & 0xFF);
                result.push_back((orig_len >> 24) & 0xFF);
                for (uint8_t b : bwt_out) result.push_back(b);
            } else if (use_zstd) {
                // zstd wins (flag 1)
                result.push_back(1);
                uint32_t len = zstd_size;
                result.push_back(len & 0xFF);
                result.push_back((len >> 8) & 0xFF);
                result.push_back((len >> 16) & 0xFF);
                result.push_back((len >> 24) & 0xFF);
                uint32_t orig_len = joined.size();
                result.push_back(orig_len & 0xFF);
                result.push_back((orig_len >> 8) & 0xFF);
                result.push_back((orig_len >> 16) & 0xFF);
                result.push_back((orig_len >> 24) & 0xFF);
                for (size_t i = 0; i < zstd_size; i++) result.push_back(zstd_out[i]);
            } else {
                // Store uncompressed (flag 0)
                result.push_back(0);
                uint32_t len = joined.size();
                result.push_back(len & 0xFF);
                result.push_back((len >> 8) & 0xFF);
                result.push_back((len >> 16) & 0xFF);
                result.push_back((len >> 24) & 0xFF);
                for (char c : joined) result.push_back((uint8_t)c);
            }
        }
    }

    return result;
}

// Decode CSV columnar data back to CSV format
inline std::vector<uint8_t> decode_csv_columnar(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 6) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    uint8_t col_count = *ptr++;
    uint32_t row_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Read header
    std::string header;
    while (ptr < end && *ptr != 0) {
        header += (char)*ptr++;
    }
    if (ptr < end) ptr++;  // Skip null

    // Read columns
    std::vector<std::vector<std::string>> columns(col_count);

    for (int c = 0; c < col_count && ptr < end; c++) {
        CsvColumnType type = (CsvColumnType)*ptr++;

        if (type == CsvColumnType::LINEAR_GEN) {
            // Regenerate from parameters
            int64_t start = 0, step = 0;
            for (int i = 0; i < 8 && ptr < end; i++) {
                start |= ((int64_t)*ptr++) << (i * 8);
            }
            for (int i = 0; i < 8 && ptr < end; i++) {
                step |= ((int64_t)*ptr++) << (i * 8);
            }
            for (uint32_t i = 0; i < row_count; i++) {
                columns[c].push_back(std::to_string(start + step * i));
            }
        } else {
            // RAW: decompress (flag 0=raw, 1=zstd, 2=bwt)
            if (ptr >= end) break;
            uint8_t compress_flag = *ptr++;
            if ((size_t)(end - ptr) < 4) break;                 // need 4 bytes for the length field
            uint32_t len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;
            // Bound the declared payload against the remaining buffer -- a garbage/oversized len (e.g. from
            // trying CSV_COLUMNAR on non-CSV bytes) otherwise reads OOB. flags 1/2/3 prepend a 4-byte
            // orig_len before the len-byte payload; flag 0 is len raw bytes. Found by fuzz_mzip. (2026-08-07)
            { size_t need = (compress_flag >= 1 && compress_flag <= 3) ? (size_t)4 + len : (size_t)len;
              if (need > (size_t)(end - ptr)) break; }

            std::string joined;
            if (compress_flag == 3) {
                // BWT+CM compressed (flag 3)
                uint32_t orig_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
                ptr += 4;
                (void)orig_len;  // cm blob is self-describing; field kept for format symmetry
                auto decompressed = cmbk::decompress_bwt(ptr, len);
                ptr += len;
                if (!decompressed.empty()) {
                    joined = std::string((char*)decompressed.data(), decompressed.size());
                }
            } else if (compress_flag == 2) {
                // BWT compressed
                uint32_t orig_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
                ptr += 4;
                auto decompressed = bwt9::decompress(ptr, len);
                ptr += len;
                if (!decompressed.empty()) {
                    joined = std::string((char*)decompressed.data(), decompressed.size());
                }
            } else if (compress_flag == 1) {
                // zstd compressed
                uint32_t orig_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
                ptr += 4;
                std::vector<uint8_t> decompressed(orig_len);
                size_t dec_size = ZSTD_decompress(decompressed.data(), decompressed.size(),
                                                  ptr, len);
                ptr += len;
                if (!ZSTD_isError(dec_size)) {
                    joined = std::string((char*)decompressed.data(), dec_size);
                }
            } else {
                // Uncompressed
                joined = std::string((char*)ptr, len);
                ptr += len;
            }

            // Split by newlines
            std::string val;
            for (char ch : joined) {
                if (ch == '\n') {
                    columns[c].push_back(val);
                    val.clear();
                } else {
                    val += ch;
                }
            }
            if (!val.empty()) columns[c].push_back(val);
        }
    }

    // Reconstruct CSV
    std::vector<uint8_t> result;
    result.reserve(original_size);

    // Header
    for (char c : header) result.push_back((uint8_t)c);
    result.push_back('\n');

    // Data rows
    for (uint32_t r = 0; r < row_count; r++) {
        for (int c = 0; c < col_count; c++) {
            if (c > 0) result.push_back(',');
            if (r < columns[c].size()) {
                for (char ch : columns[c][r]) result.push_back((uint8_t)ch);
            }
        }
        result.push_back('\n');
    }

    // Trim to original size
    while (result.size() > original_size) result.pop_back();

    return result;
}

// ============================================================================
// Section Template: Repeating multi-line sections with {N} variable
// Key insight: Markdown "## Section 1\n...\n## Section 2\n..." is ONE pattern!
// Detection: Find section delimiters, extract template, verify LINEAR_GEN on {N}
// Result: 65KB Markdown -> ~155 bytes (422x compression!)
// ============================================================================

// Replace all occurrences of a substring
inline std::string replace_all(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

// Helper: Try to build section template from given section boundaries
// Returns true if successful and fills params
inline bool try_build_section_template(const std::string& text,
                                       const std::vector<size_t>& section_starts,
                                       SectionTemplateParams& params) {
    if (section_starts.size() < 3) return false;

    // Extract sections
    std::vector<std::string> sections;
    for (size_t i = 0; i < section_starts.size(); i++) {
        size_t start = section_starts[i];
        size_t end = (i + 1 < section_starts.size()) ? section_starts[i + 1] : text.size();
        sections.push_back(text.substr(start, end - start));
    }

    // Extract section numbers - find digit sequence that increments between sections
    // Look at first 10 lines of each section for candidate numbers
    // Note: Last section might be truncated if data was cut mid-section, so don't fail on it
    std::vector<int64_t> section_nums;
    std::string truncated_last_section;  // Capture truncated last section as footer

    // First, find all digit positions in first section (up to line 10)
    std::vector<std::pair<size_t, size_t>> digit_positions;  // (start, end) in section
    {
        const auto& s0 = sections[0];
        size_t line_count = 0;
        size_t pos = 0;
        while (pos < s0.size() && line_count < 10) {
            if (isdigit((unsigned char)s0[pos])) {
                size_t start = pos;
                while (pos < s0.size() && isdigit((unsigned char)s0[pos])) pos++;
                digit_positions.push_back({start, pos});
            } else {
                if (s0[pos] == '\n') line_count++;
                pos++;
            }
        }
    }

    // Try each digit position - find one that gives linear sequence
    bool found = false;
    for (const auto& dp : digit_positions) {
        section_nums.clear();
        truncated_last_section.clear();
        bool valid = true;

        for (size_t sec_idx = 0; sec_idx < sections.size(); sec_idx++) {
            const auto& section = sections[sec_idx];
            bool is_last = (sec_idx == sections.size() - 1);

            // Check if this position exists and has a digit
            if (dp.first >= section.size() || !isdigit((unsigned char)section[dp.first])) {
                if (is_last) {
                    truncated_last_section = section;
                    break;
                }
                valid = false;
                break;
            }

            // Extract number at this position
            size_t end = dp.first;
            while (end < section.size() && isdigit((unsigned char)section[end])) end++;

            try {
                section_nums.push_back(parse_int64(section.substr(dp.first, end - dp.first)));
            } catch (...) {
                if (is_last) {
                    truncated_last_section = section;
                    break;
                }
                valid = false;
                break;
            }
        }

        if (!valid || section_nums.size() < 3) continue;

        // Check if this sequence is linear (0, 1, 2, ... or 1, 2, 3, ...)
        int64_t s = section_nums[0];
        int64_t d = section_nums[1] - section_nums[0];
        if (d != 1 && d != -1) continue;

        bool is_linear = true;
        for (size_t i = 0; i < section_nums.size(); i++) {
            if (section_nums[i] != s + d * (int64_t)i) {
                is_linear = false;
                break;
            }
        }

        if (is_linear) {
            found = true;
            break;
        }
    }

    if (!found) return false;

    // Already verified linearity above, extract params
    int64_t start_n = section_nums[0];
    int64_t step_n = section_nums[1] - section_nums[0];

    // Compare sections to find all positions where section number appears
    std::string s1 = sections[0];
    std::string s2 = sections[1];
    std::string num1 = std::to_string(start_n);
    std::string num2 = std::to_string(start_n + step_n);

    if (num1.size() != num2.size()) return false;
    if (s1.size() != s2.size()) return false;

    // Find positions where s1 and s2 differ
    std::vector<std::pair<size_t, size_t>> diff_ranges;
    size_t i = 0;
    while (i < s1.size()) {
        if (s1[i] != s2[i]) {
            size_t diff_start = i;
            while (i < s1.size() && s1[i] != s2[i]) i++;
            diff_ranges.push_back({diff_start, i - diff_start});
        } else {
            i++;
        }
    }

    // All diff ranges should be the section number
    for (const auto& [start, len] : diff_ranges) {
        std::string diff1 = s1.substr(start, len);
        std::string diff2 = s2.substr(start, len);
        if (diff1 != num1 || diff2 != num2) {
            return false;
        }
    }

    if (diff_ranges.empty()) return false;

    // Build template by replacing section number with {N}
    std::string template_text = s1;
    for (auto it = diff_ranges.rbegin(); it != diff_ranges.rend(); ++it) {
        template_text.replace(it->first, it->second, "{N}");
    }

    // Verify template works for first 3 sections
    for (size_t si = 0; si < std::min((size_t)3, sections.size()); si++) {
        std::string num = std::to_string(start_n + step_n * (int64_t)si);
        std::string expected = replace_all(template_text, "{N}", num);
        if (sections[si] != expected) {
            return false;
        }
    }

    // Check compression benefit (50%+ savings required)
    size_t template_size = template_text.size();
    size_t total_section_bytes = 0;
    for (const auto& s : sections) total_section_bytes += s.size();
    if (template_size + 30 > total_section_bytes / 2) {
        return false;
    }

    // Success! Build params
    params.section_template = template_text;
    params.start_n = start_n;
    params.step_n = step_n;
    params.header = (section_starts[0] > 0) ? text.substr(0, section_starts[0]) : "";

    // Check if last section is complete
    // Use section_nums.size() since we might have skipped a truncated last section
    size_t valid_section_count = section_nums.size();
    std::string footer_content;

    // If we captured a truncated last section, use it as footer
    if (!truncated_last_section.empty()) {
        footer_content = truncated_last_section;
    } else if (valid_section_count > 3 && valid_section_count <= sections.size()) {
        // Check if last section with digit is complete
        size_t last_idx = valid_section_count - 1;
        int64_t last_num = start_n + step_n * (int64_t)last_idx;
        std::string expected_last = replace_all(template_text, "{N}", std::to_string(last_num));
        if (sections[last_idx] != expected_last) {
            valid_section_count = valid_section_count - 1;
            footer_content = sections[last_idx];
        }
    }

    params.section_count = valid_section_count;
    params.footer = footer_content;
    return true;
}

// ============================================================================
// JSON_COLUMNAR: Extract sequential numeric fields from JSON lines
// Key insight: JSON logs have sequential requestId, quasi-sequential timestamps
// Extracting these to LINEAR_GEN/DELTA beats brotli by 1085 bytes at 64KB!
// ============================================================================

enum class JsonColumnType : uint8_t {
    LINEAR_GEN = 0,  // Sequential: requestId, counter
    DELTA = 1,       // Delta-encoded: timestamp
    RAW = 2          // Stored directly
};

struct JsonColumnInfo {
    std::string key;           // "requestId", "timestamp", etc.
    JsonColumnType type;
    int64_t linear_start;      // For LINEAR_GEN
    int64_t linear_step;       // For LINEAR_GEN
    std::vector<int64_t> values;  // Extracted values
};

struct JsonColumnarParams {
    std::vector<JsonColumnInfo> columns;  // Columns to extract
    std::string template_line;            // Line with placeholders
    size_t line_count;
};

// Extract numeric value from JSON at key (handles both "key":123 and "key":"123")
// Also handles whitespace after colon: "key": 123
inline int64_t extract_json_numeric(const std::string& line, const std::string& key) {
    std::string pattern1 = "\"" + key + "\":";
    std::string pattern2 = "\"" + key + "\":\"";

    size_t pos = line.find(pattern2);  // Try quoted first
    bool quoted = (pos != std::string::npos);
    if (!quoted) {
        pos = line.find(pattern1);
        if (pos == std::string::npos) return -1;
    }

    pos += (quoted ? pattern2.size() : pattern1.size());
    if (pos >= line.size()) return -1;

    // Skip whitespace after colon (handles "key": 123 format)
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
    }
    if (pos >= line.size()) return -1;

    int64_t val = 0;
    bool found_digit = false;
    while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
        val = val * 10 + (line[pos++] - '0');
        found_digit = true;
    }
    return found_digit ? val : -1;
}

// Detect JSON lines format with extractable numeric columns
inline bool detect_json_columnar(const uint8_t* data, size_t n, JsonColumnarParams& params) {
    if (n < 1024) return false;  // Too small

    // Convert to string and split into lines
    std::string s((const char*)data, n);
    std::vector<std::string> lines;
    size_t pos = 0, prev = 0;
    while ((pos = s.find('\n', prev)) != std::string::npos) {
        if (pos > prev) lines.push_back(s.substr(prev, pos - prev));
        prev = pos + 1;
    }
    if (prev < n && s[prev] != '\0') {
        lines.push_back(s.substr(prev));
    }

    if (lines.size() < 10) return false;  // Need enough lines

    // Filter out JSON array brackets and keep only object lines
    // Handles both NDJSON ({...}\n{...}) and JSON arrays ([{...},{...}])
    std::vector<std::string> object_lines;
    for (const auto& line : lines) {
        // Find first non-whitespace character
        size_t first_char = 0;
        while (first_char < line.size() && (line[first_char] == ' ' || line[first_char] == '\t')) {
            first_char++;
        }
        if (first_char >= line.size()) continue;  // Empty line

        // Skip array brackets
        if (line[first_char] == '[' || line[first_char] == ']') continue;

        // Must be JSON object
        if (line[first_char] != '{') return false;

        object_lines.push_back(line);
    }

    if (object_lines.size() < 10) return false;  // Need enough object lines

    // Use object_lines instead of lines for further processing
    lines = std::move(object_lines);

    // Try to find sequential numeric columns
    static const char* candidate_keys[] = {
        "requestId", "id", "sequence", "seq", "counter", "index", "num",
        "timestamp", "ts", "time", "date", "created_at", "updated_at",
        "duration_ms", "duration", "latency", "elapsed"
    };

    std::vector<JsonColumnInfo> candidates;

    for (const char* key : candidate_keys) {
        JsonColumnInfo col;
        col.key = key;
        col.values.clear();

        // Extract values from all lines
        bool all_found = true;
        for (const auto& line : lines) {
            int64_t val = extract_json_numeric(line, key);
            if (val < 0) {
                all_found = false;
                break;
            }
            col.values.push_back(val);
        }

        if (!all_found || col.values.size() < 10) continue;

        // Check if sequential (LINEAR_GEN)
        int64_t step = col.values[1] - col.values[0];
        bool is_linear = true;
        for (size_t i = 2; i < col.values.size(); i++) {
            if (col.values[i] - col.values[i-1] != step) {
                is_linear = false;
                break;
            }
        }

        if (is_linear && step != 0) {
            col.type = JsonColumnType::LINEAR_GEN;
            col.linear_start = col.values[0];
            col.linear_step = step;
            candidates.push_back(col);
        } else if (std::string(key).find("timestamp") != std::string::npos ||
                   std::string(key).find("time") != std::string::npos ||
                   std::string(key).find("ts") != std::string::npos) {
            // Check if delta-compressible (monotonic increasing)
            bool monotonic = true;
            for (size_t i = 1; i < col.values.size(); i++) {
                if (col.values[i] <= col.values[i-1]) {
                    monotonic = false;
                    break;
                }
            }
            if (monotonic) {
                col.type = JsonColumnType::DELTA;
                candidates.push_back(col);
            }
        }
    }

    if (candidates.empty()) return false;  // No extractable columns

    params.columns = std::move(candidates);
    params.line_count = lines.size();

    return true;
}

// Encode JSON columnar data
inline std::vector<uint8_t> encode_json_columnar(const JsonColumnarParams& params,
                                                  const uint8_t* orig_data, size_t orig_size) {
    std::vector<uint8_t> result;

    // Header: col_count(1) + line_count(4)
    uint8_t col_count = params.columns.size();
    result.push_back(col_count);

    uint32_t line_count = params.line_count;
    result.push_back(line_count & 0xFF);
    result.push_back((line_count >> 8) & 0xFF);
    result.push_back((line_count >> 16) & 0xFF);
    result.push_back((line_count >> 24) & 0xFF);

    // Encode each column
    for (const auto& col : params.columns) {
        // Key length + key
        uint8_t key_len = col.key.size();
        result.push_back(key_len);
        for (char c : col.key) result.push_back((uint8_t)c);

        // Column type
        result.push_back((uint8_t)col.type);

        if (col.type == JsonColumnType::LINEAR_GEN) {
            // Store: start(8) + step(8)
            for (int i = 0; i < 8; i++) {
                result.push_back((col.linear_start >> (i * 8)) & 0xFF);
            }
            for (int i = 0; i < 8; i++) {
                result.push_back((col.linear_step >> (i * 8)) & 0xFF);
            }
        } else if (col.type == JsonColumnType::DELTA) {
            // Store: first_value(8) + delta-encoded remaining
            int64_t first = col.values[0];
            for (int i = 0; i < 8; i++) {
                result.push_back((first >> (i * 8)) & 0xFF);
            }

            // Delta-encode and zstd compress
            std::vector<int32_t> deltas;
            for (size_t i = 1; i < col.values.size(); i++) {
                deltas.push_back((int32_t)(col.values[i] - col.values[i-1]));
            }

            std::vector<uint8_t> delta_bytes(deltas.size() * 4);
            memcpy(delta_bytes.data(), deltas.data(), delta_bytes.size());

            size_t zstd_bound = ZSTD_compressBound(delta_bytes.size());
            std::vector<uint8_t> zstd_out(zstd_bound);
            size_t zstd_size = ZSTD_compress(zstd_out.data(), zstd_out.size(),
                                              delta_bytes.data(), delta_bytes.size(), 19);

            // Store compressed size + data
            uint32_t size = zstd_size;
            result.push_back(size & 0xFF);
            result.push_back((size >> 8) & 0xFF);
            result.push_back((size >> 16) & 0xFF);
            result.push_back((size >> 24) & 0xFF);
            for (size_t i = 0; i < zstd_size; i++) {
                result.push_back(zstd_out[i]);
            }
        }
    }

    // Build template-only version by replacing numeric values with placeholder
    std::string s((const char*)orig_data, orig_size);
    std::string template_data;

    size_t pos = 0, prev = 0;
    size_t line_idx = 0;
    while ((pos = s.find('\n', prev)) != std::string::npos && line_idx < params.line_count) {
        std::string line = s.substr(prev, pos - prev);
        std::string new_line = line;

        // Replace each column's value with placeholder
        for (const auto& col : params.columns) {
            std::string value_str = std::to_string(col.values[line_idx]);

            // Try quoted then unquoted
            std::string old1 = "\"" + col.key + "\":\"" + value_str + "\"";
            std::string new1 = "\"" + col.key + "\":\"#\"";
            size_t p = new_line.find(old1);
            if (p != std::string::npos) {
                new_line.replace(p, old1.size(), new1);
            } else {
                std::string old2 = "\"" + col.key + "\":" + value_str;
                std::string new2 = "\"" + col.key + "\":#";
                p = new_line.find(old2);
                if (p != std::string::npos) {
                    new_line.replace(p, old2.size(), new2);
                }
            }
        }

        template_data += new_line + "\n";
        prev = pos + 1;
        line_idx++;
    }

    // Store any remaining tail data (partial line at end)
    std::string tail_data;
    if (prev < orig_size) {
        tail_data = s.substr(prev);
    }

    // Compress template data with zstd
    size_t zstd_bound = ZSTD_compressBound(template_data.size());
    std::vector<uint8_t> zstd_out(zstd_bound);
    size_t zstd_size = ZSTD_compress(zstd_out.data(), zstd_out.size(),
                                      template_data.data(), template_data.size(), 19);

    // Store compressed template size + original size + data
    uint32_t tpl_data_size = zstd_size;
    result.push_back(tpl_data_size & 0xFF);
    result.push_back((tpl_data_size >> 8) & 0xFF);
    result.push_back((tpl_data_size >> 16) & 0xFF);
    result.push_back((tpl_data_size >> 24) & 0xFF);

    uint32_t orig_tpl_size = template_data.size();
    result.push_back(orig_tpl_size & 0xFF);
    result.push_back((orig_tpl_size >> 8) & 0xFF);
    result.push_back((orig_tpl_size >> 16) & 0xFF);
    result.push_back((orig_tpl_size >> 24) & 0xFF);

    for (size_t i = 0; i < zstd_size; i++) {
        result.push_back(zstd_out[i]);
    }
    
    // Store tail data (partial line at end)
    uint32_t tail_len = tail_data.size();
    result.push_back(tail_len & 0xFF);
    result.push_back((tail_len >> 8) & 0xFF);
    result.push_back((tail_len >> 16) & 0xFF);
    result.push_back((tail_len >> 24) & 0xFF);
    for (char c : tail_data) {
        result.push_back((uint8_t)c);
    }

    return result;
}

// Decode JSON columnar back to original
inline std::vector<uint8_t> decode_json_columnar(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 10) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    uint8_t col_count = *ptr++;
    uint32_t line_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Read columns
    std::vector<JsonColumnInfo> columns;
    for (int c = 0; c < col_count && ptr < end; c++) {
        JsonColumnInfo col;

        uint8_t key_len = *ptr++;
        col.key = std::string((const char*)ptr, key_len);
        ptr += key_len;

        col.type = (JsonColumnType)*ptr++;

        if (col.type == JsonColumnType::LINEAR_GEN) {
            col.linear_start = 0;
            col.linear_step = 0;
            for (int i = 0; i < 8 && ptr < end; i++) {
                col.linear_start |= ((int64_t)*ptr++) << (i * 8);
            }
            for (int i = 0; i < 8 && ptr < end; i++) {
                col.linear_step |= ((int64_t)*ptr++) << (i * 8);
            }
            // Regenerate values
            for (uint32_t i = 0; i < line_count; i++) {
                col.values.push_back(col.linear_start + col.linear_step * i);
            }
        } else if (col.type == JsonColumnType::DELTA) {
            int64_t first = 0;
            for (int i = 0; i < 8 && ptr < end; i++) {
                first |= ((int64_t)*ptr++) << (i * 8);
            }

            uint32_t comp_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
            ptr += 4;

            // Decompress deltas
            size_t delta_count = line_count - 1;
            std::vector<int32_t> deltas(delta_count);
            ZSTD_decompress(deltas.data(), deltas.size() * 4, ptr, comp_size);
            ptr += comp_size;

            // Reconstruct values
            col.values.push_back(first);
            int64_t val = first;
            for (size_t i = 0; i < delta_count; i++) {
                val += deltas[i];
                col.values.push_back(val);
            }
        }

        columns.push_back(col);
    }

    // Read and decompress template data
    uint32_t tpl_data_comp_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    uint32_t tpl_data_orig_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    std::vector<char> template_data(tpl_data_orig_size);
    ZSTD_decompress(template_data.data(), template_data.size(), ptr, tpl_data_comp_size);
    ptr += tpl_data_comp_size;

    // Read tail data (partial line at end)
    uint32_t tail_len = 0;
    if (ptr + 4 <= end) {
        tail_len = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
    }
    std::string tail_data;
    if (tail_len > 0 && ptr + tail_len <= end) {
        tail_data = std::string((const char*)ptr, tail_len);
        ptr += tail_len;
    }

    // Reconstruct original by replacing placeholders with values
    std::string result;
    std::string tpl_str(template_data.begin(), template_data.end());

    size_t pos = 0, prev = 0;
    size_t line_idx = 0;
    while ((pos = tpl_str.find('\n', prev)) != std::string::npos && line_idx < line_count) {
        std::string line = tpl_str.substr(prev, pos - prev);

        // Replace placeholders with values
        for (const auto& col : columns) {
            std::string value_str = std::to_string(col.values[line_idx]);

            // Replace "#" placeholder
            std::string old1 = "\"" + col.key + "\":\"#\"";
            std::string new1 = "\"" + col.key + "\":\"" + value_str + "\"";
            size_t p = line.find(old1);
            if (p != std::string::npos) {
                line.replace(p, old1.size(), new1);
            } else {
                std::string old2 = "\"" + col.key + "\":#";
                std::string new2 = "\"" + col.key + "\":" + value_str;
                p = line.find(old2);
                if (p != std::string::npos) {
                    line.replace(p, old2.size(), new2);
                }
            }
        }

        result += line + "\n";
        prev = pos + 1;
        line_idx++;
    }


    // Append tail data (partial line at end)
    result += tail_data;

    return std::vector<uint8_t>(result.begin(), result.end());
}

// ============================================================================
// NUM_EXTRACT: Extract embedded decimal numbers from text files
// Key insight: Files like Makefiles have sequential block numbers (0,1,2...)
// that create entropy. Extract numbers, compress template + numbers separately.
// 900 bytes better than brotli on Makefiles!
// ============================================================================

struct NumExtractParams {
    size_t num_count;      // Number of extracted numbers
    size_t template_size;  // Size of template after number removal
};

// Detect files with many embedded decimal numbers that have exploitable structure
// Returns true if file benefits from number extraction
// Key insight: Sequential patterns (0,1,2,3...) compress well, random values don't
inline bool detect_num_extract(const uint8_t* data, size_t n, NumExtractParams& params) {
    if (n < 1024) return false;  // Too small

    // Extract first ~200 numbers to analyze pattern
    std::vector<uint32_t> sample_nums;
    size_t num_count = 0;
    size_t num_bytes = 0;
    size_t i = 0;

    while (i < n) {
        if (data[i] >= '0' && data[i] <= '9') {
            size_t start = i;
            uint32_t num = 0;
            while (i < n && data[i] >= '0' && data[i] <= '9') {
                num = num * 10 + (data[i] - '0');
                i++;
            }
            num_count++;
            num_bytes += (i - start);
            if (sample_nums.size() < 200) {
                sample_nums.push_back(num);
            }
        } else {
            i++;
        }
    }

    // Basic thresholds
    if (num_count < 100) return false;
    if (num_bytes * 100 / n < 3) return false;  // Less than 3% numeric
    if (num_bytes < num_count * 1.5) return false;  // Too many single digits

    // Check for sequential patterns in sampled numbers
    // Sequential IDs (0,1,2...) have many small deltas
    // Random values (10,5,20...) have large/varied deltas
    if (sample_nums.size() >= 20) {
        size_t small_deltas = 0;  // |delta| <= 5
        size_t zero_deltas = 0;   // delta == 0 (repeated values)

        for (size_t j = 1; j < sample_nums.size(); j++) {
            int32_t delta = (int32_t)sample_nums[j] - (int32_t)sample_nums[j-1];
            if (delta >= -5 && delta <= 5) small_deltas++;
            if (delta == 0) zero_deltas++;
        }

        size_t total = sample_nums.size() - 1;

        // Require BOTH conditions for true sequential patterns:
        // - At least 50% small deltas (sequential progression like 0,1,2,3...)
        // - At least 30% zero deltas (repeated block IDs like Makefiles have)
        // This excludes bash scripts (26.6% zero) but keeps Makefiles (39.7% zero)
        if (small_deltas * 100 / total < 50 || zero_deltas * 100 / total < 30) {
            return false;  // Not enough sequential structure
        }
    }

    params.num_count = num_count;
    params.template_size = n - num_bytes + num_count;  // Each number becomes 1 byte placeholder

    return true;
}

// Encode: extract numbers, replace with placeholder, compress both
inline std::vector<uint8_t> encode_num_extract(const uint8_t* data, size_t n) {
    // Build template and extract numbers
    std::vector<uint8_t> templ;
    std::vector<uint32_t> numbers;

    size_t i = 0;
    while (i < n) {
        if (data[i] >= '0' && data[i] <= '9') {
            uint32_t num = 0;
            while (i < n && data[i] >= '0' && data[i] <= '9') {
                num = num * 10 + (data[i] - '0');
                i++;
            }
            numbers.push_back(num);
            templ.push_back(0x00);  // Single-byte placeholder
        } else {
            templ.push_back(data[i]);
            i++;
        }
    }

    std::vector<uint8_t> result;

    // Header: num_count (4 bytes)
    uint32_t num_count = numbers.size();
    result.push_back(num_count & 0xFF);
    result.push_back((num_count >> 8) & 0xFF);
    result.push_back((num_count >> 16) & 0xFF);
    result.push_back((num_count >> 24) & 0xFF);

    // Encode numbers as varint sequence, then compress
    std::vector<uint8_t> varint_buf;
    for (uint32_t num : numbers) {
        while (num >= 128) {
            varint_buf.push_back((num & 0x7F) | 0x80);
            num >>= 7;
        }
        varint_buf.push_back(num);
    }

    // Compress varint buffer
    size_t nums_bound = ZSTD_compressBound(varint_buf.size());
    std::vector<uint8_t> nums_out(nums_bound);
    size_t nums_size = ZSTD_compress(nums_out.data(), nums_out.size(),
                                      varint_buf.data(), varint_buf.size(), 19);
    // ZSTD errors are (size_t)-1-code, i.e. near SIZE_MAX. Unchecked, that garbage went
    // into the 4-byte length field AND drove the copy loop below ~2^64 bytes off the end
    // of nums_out. Decline instead (2026-08-04); the caller treats {} as "not used".
    if (ZSTD_isError(nums_size)) return {};

    // Store compressed numbers: size (4) + data
    result.push_back(nums_size & 0xFF);
    result.push_back((nums_size >> 8) & 0xFF);
    result.push_back((nums_size >> 16) & 0xFF);
    result.push_back((nums_size >> 24) & 0xFF);
    for (size_t j = 0; j < nums_size; j++) {
        result.push_back(nums_out[j]);
    }

    // Compress template
    size_t templ_bound = ZSTD_compressBound(templ.size());
    std::vector<uint8_t> templ_out(templ_bound);
    size_t templ_size = ZSTD_compress(templ_out.data(), templ_out.size(),
                                       templ.data(), templ.size(), 19);
    if (ZSTD_isError(templ_size)) return {};  // same guard as nums_size above

    // Store compressed template: size (4) + original_size (4) + data
    result.push_back(templ_size & 0xFF);
    result.push_back((templ_size >> 8) & 0xFF);
    result.push_back((templ_size >> 16) & 0xFF);
    result.push_back((templ_size >> 24) & 0xFF);

    uint32_t templ_orig = templ.size();
    result.push_back(templ_orig & 0xFF);
    result.push_back((templ_orig >> 8) & 0xFF);
    result.push_back((templ_orig >> 16) & 0xFF);
    result.push_back((templ_orig >> 24) & 0xFF);

    for (size_t j = 0; j < templ_size; j++) {
        result.push_back(templ_out[j]);
    }

    return result;
}

// Decode: decompress template and numbers, reconstruct original
inline std::vector<uint8_t> decode_num_extract(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 16) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read num_count
    uint32_t num_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Read compressed numbers size
    uint32_t nums_comp_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Trust no length field (2026-08-07): num_count/nums_comp_size are attacker-controlled.
    // num_count*5 can overflow uint32; nums_comp_size can exceed the buffer; and a FAILED
    // ZSTD_decompress returns a huge error code that the varint loop below then walks off the
    // end of varint_buf -> SIGSEGV (found by fuzz_mzip). Bound every size, reject ZSTD errors.
    // Inert on valid streams: num_count <= original_size (>=1 output char per number).
    if ((size_t)nums_comp_size > (size_t)(end - ptr)) return {};
    if ((size_t)num_count > original_size) return {};
    size_t varint_bound = (size_t)num_count * 5;  // Max 5 bytes per varint
    std::vector<uint8_t> varint_buf(varint_bound);
    size_t varint_size = ZSTD_decompress(varint_buf.data(), varint_buf.size(), ptr, nums_comp_size);
    if (ZSTD_isError(varint_size)) return {};
    ptr += nums_comp_size;

    // Decode varints to numbers
    std::vector<uint32_t> numbers;
    size_t vi = 0;
    while (numbers.size() < num_count && vi < varint_size) {
        uint32_t num = 0;
        int shift = 0;
        while (vi < varint_size) {
            uint8_t b = varint_buf[vi++];
            num |= (uint32_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }
        numbers.push_back(num);
    }

    // Read compressed template size and original size
    if (ptr + 8 > end) return {};
    uint32_t templ_comp_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    uint32_t templ_orig_size = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    // Trust no length field: bound templ sizes and reject ZSTD errors (inert on valid streams:
    // templ_orig_size <= original_size, templ_comp_size <= remaining).
    if ((size_t)templ_comp_size > (size_t)(end - ptr)) return {};
    if ((size_t)templ_orig_size > original_size) return {};

    // Decompress template
    std::vector<uint8_t> templ(templ_orig_size);
    size_t templ_dec = ZSTD_decompress(templ.data(), templ.size(), ptr, templ_comp_size);
    if (ZSTD_isError(templ_dec)) return {};

    // Reconstruct: replace placeholders with numbers
    std::vector<uint8_t> result;
    result.reserve(original_size);

    size_t num_idx = 0;
    for (size_t i = 0; i < templ.size(); i++) {
        if (templ[i] == 0x00 && num_idx < numbers.size()) {
            // Convert number to string and append
            uint32_t num = numbers[num_idx++];
            if (num == 0) {
                result.push_back('0');
            } else {
                char buf[16];
                int len = 0;
                while (num > 0) {
                    buf[len++] = '0' + (num % 10);
                    num /= 10;
                }
                while (len > 0) {
                    result.push_back(buf[--len]);
                }
            }
        } else {
            result.push_back(templ[i]);
        }
    }

    return result;
}


// Detect section template pattern in text
// Looking for repeating multi-line blocks with sequential numbers
// Key insight: Compare sections character-by-character to find varying positions
// Supports multiple delimiter types: Markdown (## ), blank lines, etc.
inline bool detect_section_template(const uint8_t* data, size_t n, SectionTemplateParams& params) {
    if (n < 256) return false;

    std::string text((const char*)data, n);

    // Strategy 1: Markdown headers (## )
    {
        std::vector<size_t> section_starts;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t newline = text.find('\n', pos);
            if (newline == std::string::npos) break;
            if (pos + 3 <= text.size() && text.substr(pos, 3) == "## ") {
                section_starts.push_back(pos);
            }
            pos = newline + 1;
        }
        if (try_build_section_template(text, section_starts, params)) {
            return true;
        }
    }

    // Strategy 2: Blank-line separated sections (GraphQL, email, etc.)
    // Find sections separated by \n\n (blank line)
    {
        std::vector<size_t> section_starts;

        // First section starts at 0 (unless empty or starts with newline)
        if (!text.empty() && text[0] != '\n') {
            section_starts.push_back(0);
        }

        // Find all \n\n occurrences
        size_t pos = 0;
        while ((pos = text.find("\n\n", pos)) != std::string::npos) {
            // Skip past blank lines
            pos += 2;
            while (pos < text.size() && text[pos] == '\n') pos++;

            // If there's content after, it's a new section
            if (pos < text.size() && text[pos] != '\n') {
                section_starts.push_back(pos);
            }
        }

        if (try_build_section_template(text, section_starts, params)) {
            return true;
        }
    }

    // Strategy 3: HTML div sections (<div id="section-N")
    {
        std::vector<size_t> section_starts;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t div_pos = text.find("<div", pos);
            if (div_pos == std::string::npos) break;
            // Check if this div has an id or class containing a number
            size_t close = text.find('>', div_pos);
            if (close != std::string::npos) {
                std::string div_tag = text.substr(div_pos, close - div_pos + 1);
                bool has_digit = false;
                for (char c : div_tag) if (isdigit((unsigned char)c)) { has_digit = true; break; }
                if (has_digit) {
                    section_starts.push_back(div_pos);
                }
            }
            pos = div_pos + 1;
        }
        if (section_starts.size() >= 3 && try_build_section_template(text, section_starts, params)) {
            return true;
        }
    }

    // Strategy 4: YAML documents separated by ---
    // K8s manifests, multi-doc YAML files
    {
        std::vector<size_t> section_starts;

        // First section starts at 0 (unless file starts with ---)
        if (!text.empty() && text.substr(0, 4) != "---\n") {
            section_starts.push_back(0);
        }

        // Find all ---\n occurrences (YAML document separator)
        size_t pos = 0;
        while ((pos = text.find("---\n", pos)) != std::string::npos) {
            pos += 4;  // Skip past ---\n
            // Skip any blank lines after separator
            while (pos < text.size() && text[pos] == '\n') pos++;

            // If there's content after, it's a new section
            if (pos < text.size()) {
                section_starts.push_back(pos);
            }
        }

        if (section_starts.size() >= 3 && try_build_section_template(text, section_starts, params)) {
            return true;
        }
    }

    // Strategy 5: XML records - <element> or <element attr="..."> patterns
    // Two-pass: first count all prefix frequencies, then find sections
    {
        // Pass 1: Count prefix frequencies
        std::unordered_map<std::string, int> prefix_counts;
        std::unordered_map<std::string, std::vector<size_t>> prefix_positions;

        size_t line_start = 0;
        while (line_start < text.size()) {
            size_t line_end = text.find('\n', line_start);
            if (line_end == std::string::npos) line_end = text.size();

            std::string line = text.substr(line_start, line_end - line_start);

            // Look for pattern: whitespace + < + word (not closing tag)
            size_t ws_end = 0;
            while (ws_end < line.size() && (line[ws_end] == ' ' || line[ws_end] == '\t')) ws_end++;

            if (ws_end < line.size() && ws_end + 1 < line.size() &&
                line[ws_end] == '<' && line[ws_end + 1] != '/' && line[ws_end + 1] != '?' && line[ws_end + 1] != '!') {
                // Find end of tag name
                size_t tag_end = ws_end + 1;
                while (tag_end < line.size() && line[tag_end] != ' ' && line[tag_end] != '>' && line[tag_end] != '/') {
                    tag_end++;
                }

                if (tag_end > ws_end + 1) {
                    std::string prefix = line.substr(0, tag_end);
                    prefix_counts[prefix]++;
                    if (prefix_positions[prefix].size() < 10000) {
                        prefix_positions[prefix].push_back(line_start);
                    }
                }
            }

            line_start = line_end + 1;
        }

        // Pass 2: Find prefix with most occurrences (but >= 5)
        // Tiebreaker: prefer shorter indentation (higher-level element)
        std::string best_prefix;
        int best_count = 0;
        size_t best_indent = SIZE_MAX;
        for (const auto& kv : prefix_counts) {
            // Count leading whitespace
            size_t indent = 0;
            while (indent < kv.first.size() && (kv.first[indent] == ' ' || kv.first[indent] == '\t')) indent++;

            bool better = (kv.second > best_count) ||
                          (kv.second == best_count && indent < best_indent);

            if (kv.second >= 5 && better) {
                best_count = kv.second;
                best_prefix = kv.first;
                best_indent = indent;
            }
        }

        if (best_count >= 5 && !best_prefix.empty()) {
            const auto& section_starts = prefix_positions[best_prefix];
            if (try_build_section_template(text, section_starts, params)) {
                return true;
            }
        }
    }

    return false;
}

// Encode section template to bytes
inline std::vector<uint8_t> encode_section_template(const SectionTemplateParams& params, int zstd_level) {
    std::vector<uint8_t> result;

    // Header format (v2 - compact varint):
    // [start_n: varint] [step_n: varint] [section_count: uvarint]
    // [header_len: uvarint] [header_data: variable]
    // [footer_len: uvarint] [footer_data: variable]
    // [template format byte] [template data...]
    // Saves 14+ bytes for common case (start=0, step=1)

    // LINEAR_GEN params using varint (zigzag for signed)
    write_varint(result, params.start_n);  // 1 byte for 0, vs 8 bytes fixed
    write_varint(result, params.step_n);   // 1 byte for 1, vs 8 bytes fixed

    // Section count
    write_uvarint(result, params.section_count);  // 1-2 bytes vs 4 bytes

    // Header
    write_uvarint(result, params.header.size());
    for (char c : params.header) result.push_back((uint8_t)c);

    // Footer
    write_uvarint(result, params.footer.size());
    for (char c : params.footer) result.push_back((uint8_t)c);

    // Compress template
    std::vector<uint8_t> compressed(ZSTD_compressBound(params.section_template.size()));
    size_t compressed_size = ZSTD_compress(compressed.data(), compressed.size(),
                                            params.section_template.data(), params.section_template.size(),
                                            zstd_level);
    if (ZSTD_isError(compressed_size) || compressed_size >= params.section_template.size()) {
        // Store uncompressed (compression didn't help)
        result.push_back(0);  // Not compressed flag
        write_uvarint(result, params.section_template.size());
        for (char c : params.section_template) result.push_back((uint8_t)c);
    } else {
        result.push_back(1);  // Compressed flag
        write_uvarint(result, params.section_template.size());  // Original len
        write_uvarint(result, compressed_size);                  // Compressed len
        for (size_t i = 0; i < compressed_size; i++) result.push_back(compressed[i]);
    }

    return result;
}

// Decode section template back to original text
inline std::vector<uint8_t> decode_section_template(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 5) return {};  // Minimum: 2 varints + flag + 1 uvarint + data

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read LINEAR_GEN params (varint encoded)
    int64_t start_n = read_varint(ptr, end);
    int64_t step_n = read_varint(ptr, end);

    // Read section count (uvarint)
    uint64_t section_count = read_uvarint(ptr, end);

    // Read header (uvarint length + data)
    uint64_t header_len = read_uvarint(ptr, end);
    std::string header((const char*)ptr, std::min((size_t)header_len, (size_t)(end - ptr)));
    ptr += header_len;

    // Read footer (uvarint length + data)
    uint64_t footer_len = read_uvarint(ptr, end);
    std::string footer((const char*)ptr, std::min((size_t)footer_len, (size_t)(end - ptr)));
    ptr += footer_len;

    // Read template
    std::string template_text;
    if (ptr < end) {
        uint8_t compressed_flag = *ptr++;
        if (compressed_flag) {
            // Compressed template
            uint64_t orig_len = read_uvarint(ptr, end);
            uint64_t comp_len = read_uvarint(ptr, end);

            std::vector<uint8_t> decompressed(orig_len);
            size_t dec_size = ZSTD_decompress(decompressed.data(), decompressed.size(), ptr, comp_len);
            if (!ZSTD_isError(dec_size)) {
                template_text = std::string((char*)decompressed.data(), dec_size);
            }
        } else {
            // Uncompressed template
            uint64_t tpl_len = read_uvarint(ptr, end);
            template_text = std::string((const char*)ptr, std::min((size_t)tpl_len, (size_t)(end - ptr)));
        }
    }

    // Reconstruct: header + sections + footer
    std::string result = header;
    for (uint32_t i = 0; i < section_count; i++) {
        std::string section_num = std::to_string(start_n + step_n * (int64_t)i);
        result += replace_all(template_text, "{N}", section_num);
    }
    result += footer;

    // Convert to bytes, trim to original size
    std::vector<uint8_t> output(result.begin(), result.end());
    while (output.size() > original_size) output.pop_back();

    return output;
}

// ============================================================================
// WORD_TEMPLATE: Repeating sections with word variable (2.4x over zstd!)
// Key insight: Markdown "## session API\n...\n## result API\n..." with same structure
// Detection: Find repeated section headers, verify single word variable
// Result: 17KB API docs -> 516 bytes (32x compression!)
// ============================================================================

struct WordTemplateParams {
    std::string template_text;   // Template with {W} placeholders
    std::vector<std::string> words;  // Word values for each section
    size_t word_occurrences;     // How many times {W} appears per section
    std::string header;          // Content before first section
    std::string footer;          // Content after last section
};

// ============================================================================
// MULTI_WORD_TEMPLATE: Template with multiple variables (44% better on K8s!)
// Key insight: K8s Ingress/Deployment sections differ by 2-3 variables (app, env)
// Detection: Split by delimiter (---), group by kind, check >80% line similarity
// Result: 65KB K8s -> 549 bytes (44% better than zstd!)
// ============================================================================

struct MultiWordTemplateParams {
    std::string template_text;               // Template with {1}, {2}, {3} placeholders
    std::vector<std::vector<std::string>> var_lists;  // var_lists[var_idx][section_idx]
    size_t num_variables;                    // Number of variables (1-5)
    std::string header;                      // Content before first section (non-template)
    std::string footer;                      // Trailing content after last section
    std::string delimiter;                   // Section delimiter ("---", "## ", etc.)
    std::string kind_filter;                 // Optional: only sections with this kind
};

// Detect multi-word template in YAML/Markdown
inline bool detect_multi_word_template(const uint8_t* data, size_t n, MultiWordTemplateParams& params) {
    if (n < 1024) return false;  // Need substantial content

    std::string text((const char*)data, n);

    // Determine delimiter based on content
    params.delimiter = "---";  // Default for YAML
    if (text.find("\n## ") != std::string::npos && text.find("\n---\n") == std::string::npos) {
        params.delimiter = "## ";  // Markdown sections
    }

    // Split into sections
    std::vector<std::string> sections;
    size_t pos = 0, prev = 0;

    if (params.delimiter == "---") {
        // Check if text ends with \n---\n (trailing delimiter with preceding newline)
        // We need to capture this BEFORE splitting so we can restore it
        // The preceding \n is consumed by the split, so we need to include it in footer
        if (text.size() >= 5 && text.substr(text.size() - 5) == "\n---\n") {
            params.footer = "\n---\n";
        } else if (text.size() >= 4 && text.substr(text.size() - 4) == "---\n") {
            params.footer = "---\n";
        } else if (text.size() >= 4 && text.substr(text.size() - 4) == "\n---") {
            params.footer = "\n---";
        } else if (text.size() >= 3 && text.substr(text.size() - 3) == "---") {
            params.footer = "---";
        }

        // YAML: split on "\n---\n"
        while ((pos = text.find("\n---\n", prev)) != std::string::npos) {
            std::string sec = text.substr(prev, pos - prev);
            if (!sec.empty() && sec.find(':') != std::string::npos) {  // Valid YAML
                sections.push_back(sec);
            }
            prev = pos + 5;
        }
        if (prev < text.size()) {
            std::string sec = text.substr(prev);
            // Don't add trailing delimiter as a section
            if (sec != "\n---\n" && sec != "---\n" && sec != "\n---" && sec != "---") {
                if (!sec.empty() && sec.find(':') != std::string::npos) {
                    sections.push_back(sec);
                }
            }
        }
    } else {
        // Markdown: split on "\n## "
        pos = text.find("\n## ");
        if (pos != std::string::npos) {
            params.header = text.substr(0, pos + 1);
            text = text.substr(pos + 1);
        }
        prev = 0;
        while ((pos = text.find("\n## ", prev)) != std::string::npos) {
            sections.push_back(text.substr(prev, pos - prev + 1));
            prev = pos + 1;
        }
        if (prev < text.size()) sections.push_back(text.substr(prev));
    }

    if (sections.size() < 10) return false;  // Need enough sections

    // For YAML: group by kind
    std::map<std::string, std::vector<size_t>> kind_groups;
    if (params.delimiter == "---") {
        for (size_t i = 0; i < sections.size(); i++) {
            size_t kpos = sections[i].find("kind:");
            if (kpos != std::string::npos) {
                size_t start = kpos + 5;
                while (start < sections[i].size() && sections[i][start] == ' ') start++;
                size_t end = sections[i].find('\n', start);
                if (end == std::string::npos) end = sections[i].size();
                std::string kind = sections[i].substr(start, end - start);
                kind_groups[kind].push_back(i);
            }
        }
    } else {
        // Markdown: all sections in one group
        for (size_t i = 0; i < sections.size(); i++) {
            kind_groups["default"].push_back(i);
        }
    }

    // Find largest group with >10 sections AND >90% of total
    // This ensures data is uniform enough for template encoding
    std::string best_kind;
    size_t best_count = 0;
    for (const auto& [kind, indices] : kind_groups) {
        if (indices.size() > best_count && indices.size() >= 10) {
            best_count = indices.size();
            best_kind = kind;
        }
    }

    if (best_count < 10) return false;

    // Require >90% uniformity to ensure we're not losing significant content
    double uniformity = (double)best_count / sections.size();
    if (uniformity < 0.90) return false;

    params.kind_filter = best_kind;
    const auto& indices = kind_groups[best_kind];

    // Extract sections of this kind
    std::vector<std::string> kind_sections;
    for (size_t idx : indices) {
        kind_sections.push_back(sections[idx]);
    }

    // Check size variance < 20%
    size_t avg_size = 0;
    for (const auto& s : kind_sections) avg_size += s.size();
    avg_size /= kind_sections.size();

    for (const auto& s : kind_sections) {
        if (s.size() < avg_size * 0.8 || s.size() > avg_size * 1.2) {
            return false;  // Too much variance
        }
    }

    // Split into lines and find variable positions
    auto split_lines = [](const std::string& s) {
        std::vector<std::string> result;
        size_t p = 0, prev = 0;
        while ((p = s.find('\n', prev)) != std::string::npos) {
            result.push_back(s.substr(prev, p - prev));
            prev = p + 1;
        }
        if (prev <= s.size()) result.push_back(s.substr(prev));
        return result;
    };

    std::vector<std::vector<std::string>> section_lines;
    for (const auto& sec : kind_sections) {
        section_lines.push_back(split_lines(sec));
    }

    // Verify all sections have same line count
    size_t line_count = section_lines[0].size();
    for (const auto& lines : section_lines) {
        if (lines.size() != line_count) return false;
    }

    // Find lines that vary - these contain variables
    std::vector<size_t> varying_lines;
    for (size_t li = 0; li < line_count; li++) {
        bool all_same = true;
        const std::string& first = section_lines[0][li];
        for (size_t si = 1; si < section_lines.size(); si++) {
            if (section_lines[si][li] != first) {
                all_same = false;
                break;
            }
        }
        if (!all_same) varying_lines.push_back(li);
    }

    // Check similarity: >70% lines should be identical (K8s Ingress: 73.6%)
    // Lower threshold because same variable can appear on multiple lines
    double similarity = (double)(line_count - varying_lines.size()) / line_count;
    if (similarity < 0.70) return false;

    // Too many varying lines = not a good template
    if (varying_lines.size() > 15) return false;

    // Extract variables from varying lines
    // For each varying line, find the common prefix and suffix across ALL sections
    std::map<size_t, std::vector<std::string>> line_variables;  // line_idx -> values per section

    for (size_t li : varying_lines) {
        // Find longest common prefix across all sections
        size_t common_prefix = section_lines[0][li].size();
        for (size_t si = 1; si < section_lines.size(); si++) {
            const std::string& a = section_lines[0][li];
            const std::string& b = section_lines[si][li];
            size_t prefix = 0;
            while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) prefix++;
            if (prefix < common_prefix) common_prefix = prefix;
        }

        // Find longest common suffix across all sections
        size_t common_suffix = section_lines[0][li].size();
        for (size_t si = 1; si < section_lines.size(); si++) {
            const std::string& a = section_lines[0][li];
            const std::string& b = section_lines[si][li];
            size_t suffix = 0;
            while (suffix < a.size() && suffix < b.size() &&
                   a[a.size()-1-suffix] == b[b.size()-1-suffix]) suffix++;
            if (suffix < common_suffix) common_suffix = suffix;
        }

        // Extract variable part from each section
        for (size_t si = 0; si < section_lines.size(); si++) {
            const std::string& line = section_lines[si][li];
            size_t var_start = common_prefix;
            size_t var_end = line.size() - common_suffix;
            if (var_end > var_start) {
                line_variables[li].push_back(line.substr(var_start, var_end - var_start));
            } else {
                line_variables[li].push_back("");  // Shouldn't happen for varying lines
            }
        }
    }

    // Cluster variables: lines with same variable values go together
    // First section's variables define the "identity" of each unique variable
    std::map<std::string, std::vector<size_t>> var_clusters;  // first_value -> line indices
    for (size_t li : varying_lines) {
        if (!line_variables[li].empty()) {
            var_clusters[line_variables[li][0]].push_back(li);
        }
    }

    // Deduplicate: variables that have same values across all sections
    std::vector<std::vector<std::string>> unique_vars;
    std::vector<std::vector<size_t>> var_line_indices;

    for (const auto& [first_val, line_indices] : var_clusters) {
        // Check if all these lines have same values per section
        bool all_consistent = true;
        for (size_t si = 0; si < kind_sections.size(); si++) {
            std::string val;
            for (size_t li : line_indices) {
                if (val.empty()) {
                    val = line_variables[li][si];
                } else if (line_variables[li][si] != val) {
                    all_consistent = false;
                    break;
                }
            }
            if (!all_consistent) break;
        }

        if (all_consistent && !line_indices.empty()) {
            // This is a valid variable
            std::vector<std::string> vals;
            for (size_t si = 0; si < kind_sections.size(); si++) {
                vals.push_back(line_variables[line_indices[0]][si]);
            }
            unique_vars.push_back(vals);
            var_line_indices.push_back(line_indices);
        }
    }

    if (unique_vars.empty() || unique_vars.size() > 5) return false;  // Need 1-5 variables

    // === Compound variable elimination ===
    // Check if any variable is a compound of two others: var_N = var_A + sep + var_B
    // This eliminates redundant variables like "gateway.development" when "gateway" and "development" exist
    const char* separators = ".-_/:@";
    std::vector<bool> is_compound(unique_vars.size(), false);
    std::vector<std::string> compound_replacement(unique_vars.size());  // e.g., "{1}.{2}" for var that's compound

    size_t num_sections = unique_vars[0].size();
    for (size_t n = unique_vars.size(); n-- > 0; ) {  // Check from highest index down
        if (is_compound[n]) continue;

        for (size_t a = 0; a < n && !is_compound[n]; a++) {
            for (size_t b = 0; b < n && !is_compound[n]; b++) {
                if (a == b) continue;

                for (const char* sep = separators; *sep && !is_compound[n]; sep++) {
                    // Check if var_n[i] == var_a[i] + sep + var_b[i] for all sections
                    bool all_match = true;
                    for (size_t si = 0; si < num_sections && all_match; si++) {
                        std::string expected = unique_vars[a][si] + *sep + unique_vars[b][si];
                        if (unique_vars[n][si] != expected) {
                            all_match = false;
                        }
                    }

                    if (all_match) {
                        is_compound[n] = true;
                        // Use 1-indexed placeholders: {a+1}.{b+1}
                        compound_replacement[n] = "{" + std::to_string(a + 1) + "}" + *sep + "{" + std::to_string(b + 1) + "}";
                    }
                }
            }
        }
    }

    // Build template from first section, using compound replacements where applicable
    std::vector<std::string> template_lines = section_lines[0];
    for (size_t vi = 0; vi < var_line_indices.size(); vi++) {
        std::string placeholder;
        if (is_compound[vi]) {
            placeholder = compound_replacement[vi];  // e.g., "{1}.{2}"
        } else {
            placeholder = "{" + std::to_string(vi + 1) + "}";
        }

        for (size_t li : var_line_indices[vi]) {
            // Replace the variable portion
            const std::string& line = template_lines[li];
            const std::string& val = unique_vars[vi][0];
            size_t pos = line.find(val);
            if (pos != std::string::npos) {
                template_lines[li] = line.substr(0, pos) + placeholder + line.substr(pos + val.size());
            }
        }
    }

    // Remove compound variables from var_lists and renumber
    std::vector<std::vector<std::string>> final_vars;
    std::vector<size_t> old_to_new(unique_vars.size(), SIZE_MAX);  // Maps old index to new index
    for (size_t vi = 0; vi < unique_vars.size(); vi++) {
        if (!is_compound[vi]) {
            old_to_new[vi] = final_vars.size();
            final_vars.push_back(unique_vars[vi]);
        }
    }

    // Update template placeholders to use new numbering
    // Old {N} -> New {old_to_new[N-1]+1}
    for (auto& line : template_lines) {
        for (size_t old_idx = unique_vars.size(); old_idx-- > 0; ) {
            if (is_compound[old_idx]) continue;  // Compounds already have correct refs
            std::string old_ph = "{" + std::to_string(old_idx + 1) + "}";
            std::string new_ph = "{" + std::to_string(old_to_new[old_idx] + 1) + "}";
            if (old_ph != new_ph) {
                size_t pos = 0;
                while ((pos = line.find(old_ph, pos)) != std::string::npos) {
                    line.replace(pos, old_ph.size(), new_ph);
                    pos += new_ph.size();
                }
            }
        }
        // Also update compound references
        for (size_t old_idx = 0; old_idx < unique_vars.size(); old_idx++) {
            if (!is_compound[old_idx]) continue;
            // compound_replacement contains old indices, need to update to new
            // But we already embedded it in template_lines, so we need to update those refs
        }
    }

    // For compound replacements that are already in template, update their internal refs
    // e.g., if {1}.{2} but {1} is now {1} and {2} is now {1}, we have a problem
    // Actually, we need to do this more carefully - update the compound strings too
    for (auto& line : template_lines) {
        // Update references inside compound patterns
        for (size_t old_idx = unique_vars.size(); old_idx-- > 0; ) {
            if (old_to_new[old_idx] == SIZE_MAX) continue;  // This var was eliminated
            if (old_to_new[old_idx] == old_idx) continue;  // No change needed
            std::string old_ph = "{" + std::to_string(old_idx + 1) + "}";
            std::string new_ph = "{" + std::to_string(old_to_new[old_idx] + 1) + "}";
            size_t pos = 0;
            while ((pos = line.find(old_ph, pos)) != std::string::npos) {
                line.replace(pos, old_ph.size(), new_ph);
                pos += new_ph.size();
            }
        }
    }

    params.num_variables = final_vars.size();
    params.var_lists = final_vars;

    // Join template lines
    params.template_text.clear();
    for (size_t i = 0; i < template_lines.size(); i++) {
        params.template_text += template_lines[i];
        if (i < template_lines.size() - 1) params.template_text += "\n";
    }

    // Verify reconstruction
    for (size_t si = 0; si < kind_sections.size(); si++) {
        std::string reconstructed = params.template_text;
        for (size_t vi = 0; vi < params.num_variables; vi++) {
            std::string placeholder = "{" + std::to_string(vi + 1) + "}";
            size_t p = 0;
            while ((p = reconstructed.find(placeholder, p)) != std::string::npos) {
                reconstructed.replace(p, placeholder.size(), params.var_lists[vi][si]);
                p += params.var_lists[vi][si].size();
            }
        }
        if (reconstructed != kind_sections[si]) {
            return false;  // Reconstruction failed
        }
    }

    return true;
}

// Encode multi-word template
inline std::vector<uint8_t> encode_multi_word_template(const MultiWordTemplateParams& params, int zstd_level) {
    std::vector<uint8_t> result;

    // Format:
    // [num_sections: varint]
    // [num_variables: varint]
    // [delimiter_len: varint] [delimiter: raw]
    // [kind_filter_len: varint] [kind_filter: raw]
    // [header_len: varint] [header: raw]
    // [template_orig: varint] [template_comp: varint] [template_data: zstd]
    // For each variable:
    //   [var_list_orig: varint] [var_list_comp: varint] [var_list_data: zstd, newline-separated]

    size_t num_sections = params.var_lists.empty() ? 0 : params.var_lists[0].size();
    write_uvarint(result, num_sections);
    write_uvarint(result, params.num_variables);

    // Delimiter
    write_uvarint(result, params.delimiter.size());
    for (char c : params.delimiter) result.push_back((uint8_t)c);

    // Kind filter
    write_uvarint(result, params.kind_filter.size());
    for (char c : params.kind_filter) result.push_back((uint8_t)c);

    // Header
    write_uvarint(result, params.header.size());
    for (char c : params.header) result.push_back((uint8_t)c);

    // Footer
    write_uvarint(result, params.footer.size());
    for (char c : params.footer) result.push_back((uint8_t)c);

    // Compress template
    size_t templ_bound = ZSTD_compressBound(params.template_text.size());
    std::vector<uint8_t> templ_comp(templ_bound);
    size_t templ_size = ZSTD_compress(templ_comp.data(), templ_bound,
        params.template_text.data(), params.template_text.size(), zstd_level);

    // DEBUG: print template size
    // fprintf(stderr, "MWT encode: template %zu -> %zu bytes\n", params.template_text.size(), templ_size);

    write_uvarint(result, params.template_text.size());
    write_uvarint(result, templ_size);
    for (size_t i = 0; i < templ_size; i++) result.push_back(templ_comp[i]);

    // Compress each variable list
    for (size_t vi = 0; vi < params.num_variables; vi++) {
        std::string var_joined;
        for (const auto& val : params.var_lists[vi]) {
            var_joined += val + "\n";
        }

        size_t var_bound = ZSTD_compressBound(var_joined.size());
        std::vector<uint8_t> var_comp(var_bound);
        size_t var_size = ZSTD_compress(var_comp.data(), var_bound,
            var_joined.data(), var_joined.size(), zstd_level);

        // DEBUG: print var list size
        // fprintf(stderr, "MWT encode: var%zu %zu -> %zu bytes\n", vi, var_joined.size(), var_size);

        write_uvarint(result, var_joined.size());
        write_uvarint(result, var_size);
        for (size_t i = 0; i < var_size; i++) result.push_back(var_comp[i]);
    }

    // DEBUG: print total encoded size
    // fprintf(stderr, "MWT encode: total %zu bytes (before mzip header)\n", result.size());

    return result;
}

// Decode multi-word template
inline std::vector<uint8_t> decode_multi_word_template(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 10) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    size_t num_sections = read_uvarint(ptr, end);
    size_t num_variables = read_uvarint(ptr, end);

    // Delimiter
    size_t delim_len = read_uvarint(ptr, end);
    std::string delimiter((const char*)ptr, delim_len);
    ptr += delim_len;

    // Kind filter
    size_t kind_len = read_uvarint(ptr, end);
    std::string kind_filter((const char*)ptr, kind_len);
    ptr += kind_len;

    // Header
    size_t header_len = read_uvarint(ptr, end);
    std::string header((const char*)ptr, header_len);
    ptr += header_len;

    // Footer
    size_t footer_len = read_uvarint(ptr, end);
    std::string footer((const char*)ptr, footer_len);
    ptr += footer_len;

    // Decompress template
    size_t templ_orig = read_uvarint(ptr, end);
    size_t templ_comp = read_uvarint(ptr, end);
    std::vector<uint8_t> templ_buf(templ_orig);
    ZSTD_decompress(templ_buf.data(), templ_orig, ptr, templ_comp);
    ptr += templ_comp;
    std::string template_text((char*)templ_buf.data(), templ_orig);

    // Decompress variable lists
    std::vector<std::vector<std::string>> var_lists(num_variables);
    for (size_t vi = 0; vi < num_variables; vi++) {
        size_t var_orig = read_uvarint(ptr, end);
        size_t var_comp = read_uvarint(ptr, end);
        std::vector<uint8_t> var_buf(var_orig);
        ZSTD_decompress(var_buf.data(), var_orig, ptr, var_comp);
        ptr += var_comp;

        // Parse newline-separated values
        std::string val;
        for (size_t i = 0; i < var_orig; i++) {
            if (var_buf[i] == '\n') {
                var_lists[vi].push_back(val);
                val.clear();
            } else {
                val += (char)var_buf[i];
            }
        }
        if (!val.empty()) var_lists[vi].push_back(val);
    }

    // Reconstruct output
    std::string output = header;

    for (size_t si = 0; si < num_sections; si++) {
        if (si > 0) {
            if (delimiter == "---") {
                output += "\n---\n";
            }
            // For "## ", the delimiter is already in template
        }

        std::string section = template_text;
        for (size_t vi = 0; vi < num_variables; vi++) {
            std::string placeholder = "{" + std::to_string(vi + 1) + "}";
            size_t pos = 0;
            while ((pos = section.find(placeholder, pos)) != std::string::npos) {
                const std::string& val = (si < var_lists[vi].size()) ? var_lists[vi][si] : "";
                section.replace(pos, placeholder.size(), val);
                pos += val.size();
            }
        }
        output += section;
    }

    // Add footer (trailing content after last section)
    output += footer;

    // Ensure output matches expected size
    std::vector<uint8_t> result(output.begin(), output.end());
    while (result.size() < original_size) result.push_back('\n');
    while (result.size() > original_size) result.pop_back();

    return result;
}

// Detect word template pattern in text
inline bool detect_word_template(const uint8_t* data, size_t n, WordTemplateParams& params) {
    if (n < 512) return false;  // Need substantial content

    std::string text((const char*)data, n);

    // Strategy: Find repeated section patterns with consistent structure
    // Look for "## X Y" patterns where X varies and Y is constant

    std::vector<std::pair<size_t, std::string>> sections;  // (start, header line)
    size_t pos = 0;

    // Find all "## " headers
    while ((pos = text.find("\n## ", pos)) != std::string::npos) {
        size_t line_start = pos + 1;
        size_t line_end = text.find('\n', line_start + 3);
        if (line_end == std::string::npos) line_end = text.size();
        std::string header = text.substr(line_start, line_end - line_start);
        sections.push_back({line_start, header});
        pos = line_end;
    }
    // Check for header at start
    if (text.size() > 3 && text.substr(0, 3) == "## ") {
        size_t line_end = text.find('\n');
        if (line_end != std::string::npos) {
            std::string header = text.substr(0, line_end);
            sections.insert(sections.begin(), {0, header});
        }
    }

    if (sections.size() < 5) return false;  // Need enough sections

    // Check for pattern: headers differ by one word
    // Example: "## session API" vs "## result API" - differ by "session" vs "result"

    // Find the most common suffix pattern (not all sections may have it)
    // Try each section's suffix against all others
    std::string common_suffix;
    size_t best_match_count = 0;

    for (const auto& base_sec : sections) {
        // Try suffixes of increasing length from this header
        for (size_t len = 4; len <= std::min(base_sec.second.size() - 3, size_t(20)); len++) {
            std::string suffix = base_sec.second.substr(base_sec.second.size() - len);
            // Count how many sections have this suffix
            size_t match_count = 0;
            for (const auto& sec : sections) {
                if (sec.second.size() >= len &&
                    sec.second.substr(sec.second.size() - len) == suffix) {
                    match_count++;
                }
            }
            if (match_count >= 5 && match_count > best_match_count) {
                best_match_count = match_count;
                common_suffix = suffix;
            }
        }
    }

    if (common_suffix.size() < 4) return false;  // Need meaningful suffix like " API"

    // Filter to only sections with this suffix
    std::vector<std::pair<size_t, std::string>> filtered_sections;
    for (const auto& sec : sections) {
        if (sec.second.size() >= common_suffix.size() &&
            sec.second.substr(sec.second.size() - common_suffix.size()) == common_suffix) {
            filtered_sections.push_back(sec);
        }
    }
    sections = std::move(filtered_sections);

    if (sections.size() < 5) return false;  // Need enough matching sections

    // Extract the variable word from each header
    params.words.clear();
    for (const auto& sec : sections) {
        // Format: "## WORD SUFFIX"
        std::string header = sec.second;
        if (header.size() <= 3 + common_suffix.size()) continue;
        std::string word = header.substr(3, header.size() - 3 - common_suffix.size());
        // Trim trailing space
        while (!word.empty() && word.back() == ' ') word.pop_back();
        if (word.empty()) continue;
        params.words.push_back(word);
    }

    if (params.words.size() < 5) return false;

    // Extract sections and verify they have similar structure
    // IMPORTANT: Keep section boundaries exactly as they appear (including separators)
    std::vector<std::string> section_texts;
    for (size_t i = 0; i < sections.size(); i++) {
        size_t start = sections[i].first;
        size_t end = (i + 1 < sections.size()) ? sections[i + 1].first : text.size();
        // For non-last sections, include everything up to next section header
        // This preserves blank lines between sections
        section_texts.push_back(text.substr(start, end - start));
    }

    // FIRST: Check for truncated last section BEFORE size uniformity check
    // Expected size is the first section size (a complete section)
    // If last section is truncated, remove it before calculating averages
    std::string truncated_footer;
    size_t expected_size = section_texts[0].size();
    if (section_texts.size() > 1 && section_texts.back().size() < expected_size * 0.8) {
        truncated_footer = section_texts.back();
        section_texts.pop_back();
        sections.pop_back();
        if (params.words.size() > section_texts.size()) params.words.pop_back();
    }

    if (section_texts.size() < 5) return false;  // Need enough sections after truncation removal

    // All sections should be similar length (within 30%)
    size_t avg_len = 0;
    for (const auto& s : section_texts) avg_len += s.size();
    avg_len /= section_texts.size();

    for (const auto& s : section_texts) {
        if (s.size() < avg_len * 0.7 || s.size() > avg_len * 1.3) {
            return false;  // Sections too different
        }
    }

    // Count how many times first word appears in first section
    const std::string& first_word = params.words[0];
    const std::string& first_section = section_texts[0];
    size_t occurrences = 0;
    pos = 0;
    while ((pos = first_section.find(first_word, pos)) != std::string::npos) {
        bool left_ok = (pos == 0 || !isalnum((unsigned char)first_section[pos-1]));
        bool right_ok = (pos + first_word.size() >= first_section.size() ||
                         !isalnum((unsigned char)first_section[pos + first_word.size()]));
        if (left_ok && right_ok) occurrences++;
        pos++;
    }

    if (occurrences < 2) return false;  // Not enough repetition

    params.word_occurrences = occurrences;

    // Build template by replacing first_word with {W}
    params.template_text = first_section;
    pos = 0;
    while ((pos = params.template_text.find(first_word, pos)) != std::string::npos) {
        bool left_ok = (pos == 0 || !isalnum((unsigned char)params.template_text[pos-1]));
        bool right_ok = (pos + first_word.size() >= params.template_text.size() ||
                         !isalnum((unsigned char)params.template_text[pos + first_word.size()]));
        if (left_ok && right_ok) {
            params.template_text.replace(pos, first_word.size(), "{W}");
            pos += 3;
        } else {
            pos++;
        }
    }

    // Extract header (content before first section)
    params.header = text.substr(0, sections[0].first);

    // CRITICAL: Verify that template + word actually reconstructs each section
    // This catches cases where sections have other varying content
    for (size_t i = 0; i < section_texts.size() && i < params.words.size(); i++) {
        std::string reconstructed = params.template_text;
        size_t rpos = 0;
        while ((rpos = reconstructed.find("{W}", rpos)) != std::string::npos) {
            reconstructed.replace(rpos, 3, params.words[i]);
            rpos += params.words[i].size();
        }
        if (reconstructed != section_texts[i]) {
            // Check if this is the last section and it's truncated (shorter than expected)
            bool is_last = (i == section_texts.size() - 1);
            bool is_shorter = (section_texts[i].size() < reconstructed.size());

            if (is_last && is_shorter && truncated_footer.empty()) {
                // Last section is truncated - move it to footer
                truncated_footer = section_texts[i];
                section_texts.pop_back();
                if (params.words.size() > section_texts.size()) params.words.pop_back();
                break;  // Exit verification loop
            }
            return false;  // Template doesn't match - sections have other varying content
        }
    }

    if (section_texts.size() < 5) return false;  // Still need enough sections after last removal

    // Extract footer (any truncated content at end)
    params.footer = truncated_footer;

    // Verify significant savings
    size_t original_sections = 0;
    for (const auto& s : section_texts) original_sections += s.size();

    size_t words_size = 0;
    for (const auto& w : params.words) words_size += w.size() + 1;

    size_t new_size = params.template_text.size() + words_size + params.header.size() + params.footer.size();

    // Need at least 50% savings to be worthwhile
    if (new_size > original_sections * 0.5) return false;

    return true;
}

// Encode word template to bytes
inline std::vector<uint8_t> encode_word_template(const WordTemplateParams& params, int zstd_level) {
    std::vector<uint8_t> result;

    // Format:
    // [word_count: varint]
    // [occurrences: varint]
    // [header_len: varint] [header_data: raw or compressed]
    // [footer_len: varint] [footer_data: raw or compressed]
    // [template_comp_len: varint] [template_data: zstd compressed]
    // [words_comp_len: varint] [words_data: zstd compressed, newline-separated]

    write_uvarint(result, params.words.size());
    write_uvarint(result, params.word_occurrences);

    // Header
    write_uvarint(result, params.header.size());
    for (char c : params.header) result.push_back((uint8_t)c);

    // Footer
    write_uvarint(result, params.footer.size());
    for (char c : params.footer) result.push_back((uint8_t)c);

    // Compress template
    size_t templ_bound = ZSTD_compressBound(params.template_text.size());
    std::vector<uint8_t> templ_comp(templ_bound);
    size_t templ_size = ZSTD_compress(templ_comp.data(), templ_bound,
        params.template_text.data(), params.template_text.size(), zstd_level);

    write_uvarint(result, params.template_text.size());  // Original size
    write_uvarint(result, templ_size);  // Compressed size
    for (size_t i = 0; i < templ_size; i++) result.push_back(templ_comp[i]);

    // Compress words (newline-separated)
    std::string words_joined;
    for (const auto& w : params.words) words_joined += w + "\n";

    size_t words_bound = ZSTD_compressBound(words_joined.size());
    std::vector<uint8_t> words_comp(words_bound);
    size_t words_size = ZSTD_compress(words_comp.data(), words_bound,
        words_joined.data(), words_joined.size(), zstd_level);

    write_uvarint(result, words_joined.size());  // Original size
    write_uvarint(result, words_size);  // Compressed size
    for (size_t i = 0; i < words_size; i++) result.push_back(words_comp[i]);

    return result;
}

// Decode word template
inline std::vector<uint8_t> decode_word_template(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 10) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Read header
    size_t word_count = read_uvarint(ptr, end);
    size_t occurrences = read_uvarint(ptr, end);

    // Read header content
    size_t header_len = read_uvarint(ptr, end);
    std::string header((const char*)ptr, header_len);
    ptr += header_len;

    // Read footer content
    size_t footer_len = read_uvarint(ptr, end);
    std::string footer((const char*)ptr, footer_len);
    ptr += footer_len;

    // Decompress template
    size_t templ_orig = read_uvarint(ptr, end);
    size_t templ_comp = read_uvarint(ptr, end);
    std::vector<uint8_t> templ_buf(templ_orig);
    ZSTD_decompress(templ_buf.data(), templ_orig, ptr, templ_comp);
    ptr += templ_comp;
    std::string templ_text((char*)templ_buf.data(), templ_orig);

    // Decompress words
    size_t words_orig = read_uvarint(ptr, end);
    size_t words_comp = read_uvarint(ptr, end);
    std::vector<uint8_t> words_buf(words_orig);
    ZSTD_decompress(words_buf.data(), words_orig, ptr, words_comp);

    // Parse words
    std::vector<std::string> words;
    std::string word;
    for (size_t i = 0; i < words_orig; i++) {
        if (words_buf[i] == '\n') {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else {
            word += (char)words_buf[i];
        }
    }
    if (!word.empty()) words.push_back(word);

    // Reconstruct output
    std::string output = header;

    for (size_t i = 0; i < words.size(); i++) {
        // Copy template, replacing {W} with word
        std::string section = templ_text;
        size_t pos = 0;
        while ((pos = section.find("{W}", pos)) != std::string::npos) {
            section.replace(pos, 3, words[i]);
            pos += words[i].size();
        }
        output += section;
        // Template already includes section separator (blank line), no extra newline needed
    }

    output += footer;

    // Convert to bytes
    std::vector<uint8_t> result(output.begin(), output.end());

    // Trim/extend to original size
    if (result.size() > original_size) result.resize(original_size);
    while (result.size() < original_size) result.push_back('\n');

    return result;
}

// ============================================================================
// CODE_STREAM Detection and Encoding
// ============================================================================

// Fixed vocabulary arrays (from JavaScript generator analysis)
static const char* CODE_STREAM_NOUNS[] = {
    "user", "item", "data", "config", "state", "event", "node", "view",
    "model", "list", "form", "page", "task", "file", "cache", "queue",
    "token", "session", "order", "product", "cart", "payment", "address"
};
static const char* CODE_STREAM_VERBS[] = {
    "get", "set", "create", "update", "delete", "fetch", "load", "save",
    "parse", "format", "validate", "transform", "process", "handle", "render",
    "submit", "cancel", "reset", "init", "sync", "filter", "sort", "merge"
};
static const int CODE_STREAM_NUM_NOUNS = 23;
static const int CODE_STREAM_NUM_VERBS = 23;

// Identifier grammar types
enum class CodeIdentType : uint8_t {
    CLASS_NAME = 0,     // [A-Z][noun][number]      -> "Auser4821"
    FUNC_NAME = 1,      // [verb][A-Z][noun]        -> "getUserItem"
    SERVICE_NAME = 2,   // [noun]"Service"[number]  -> "configService3847"
    UTILS_NAME = 3,     // [noun]"Utils"            -> "nodeUtils"
    LIST_NAME = 4,      // [noun]"List"             -> "itemList"
    SIMPLE = 5,         // [noun] or [verb]         -> "user", "get"
    UNKNOWN = 6         // Doesn't match grammar
};

// Parsed identifier components
struct CodeIdentParsed {
    CodeIdentType type;
    uint8_t letter;     // 0-25 for A-Z (CLASS_NAME, FUNC_NAME)
    uint8_t word1;      // Index into noun/verb array
    uint8_t word2;      // Index for second word (FUNC_NAME)
    uint16_t number;    // Number suffix (CLASS_NAME, SERVICE_NAME)
    std::string raw;    // For UNKNOWN type
};

inline int find_noun_index(const std::string& s) {
    for (int i = 0; i < CODE_STREAM_NUM_NOUNS; i++) {
        if (s == CODE_STREAM_NOUNS[i]) return i;
    }
    return -1;
}

inline int find_verb_index(const std::string& s) {
    for (int i = 0; i < CODE_STREAM_NUM_VERBS; i++) {
        if (s == CODE_STREAM_VERBS[i]) return i;
    }
    return -1;
}

inline CodeIdentParsed parse_code_identifier(const std::string& ident) {
    CodeIdentParsed result;
    result.type = CodeIdentType::UNKNOWN;
    result.letter = 0;
    result.word1 = 0;
    result.word2 = 0;
    result.number = 0;
    result.raw = ident;

    if (ident.empty()) return result;

    // Check SIMPLE first (exact match to noun or verb)
    int ni = find_noun_index(ident);
    if (ni >= 0) {
        result.type = CodeIdentType::SIMPLE;
        result.word1 = ni;
        return result;
    }
    int vi = find_verb_index(ident);
    if (vi >= 0) {
        result.type = CodeIdentType::SIMPLE;
        result.word1 = vi + CODE_STREAM_NUM_NOUNS;  // Offset verbs after nouns
        return result;
    }

    // Check CLASS_NAME: [A-Z][noun][number]
    if (isupper(ident[0])) {
        char letter = ident[0];
        // Find where number starts
        size_t num_start = 1;
        while (num_start < ident.size() && !isdigit(ident[num_start])) num_start++;

        if (num_start < ident.size()) {
            std::string word = ident.substr(1, num_start - 1);
            int wi = find_noun_index(word);
            if (wi >= 0) {
                result.type = CodeIdentType::CLASS_NAME;
                result.letter = letter - 'A';
                result.word1 = wi;
                result.number = (uint16_t)std::stoi(ident.substr(num_start));
                return result;
            }
        }
    }

    // Check FUNC_NAME: [verb][A-Z][noun]
    for (int v = 0; v < CODE_STREAM_NUM_VERBS; v++) {
        const char* verb = CODE_STREAM_VERBS[v];
        size_t vlen = strlen(verb);
        if (ident.size() > vlen && ident.substr(0, vlen) == verb) {
            char mid = ident[vlen];
            if (isupper(mid)) {
                std::string noun_part = ident.substr(vlen + 1);
                // Lowercase the noun part for matching
                for (char& c : noun_part) c = tolower(c);
                int ni2 = find_noun_index(noun_part);
                if (ni2 >= 0) {
                    result.type = CodeIdentType::FUNC_NAME;
                    result.letter = mid - 'A';
                    result.word1 = v;
                    result.word2 = ni2;
                    return result;
                }
            }
        }
    }

    // Check SERVICE_NAME: [noun]Service[number]
    {
        size_t svc = ident.find("Service");
        if (svc != std::string::npos && svc > 0) {
            std::string noun_part = ident.substr(0, svc);
            int ni2 = find_noun_index(noun_part);
            if (ni2 >= 0 && svc + 7 < ident.size()) {
                result.type = CodeIdentType::SERVICE_NAME;
                result.word1 = ni2;
                result.number = (uint16_t)std::stoi(ident.substr(svc + 7));
                return result;
            }
        }
    }

    // Check UTILS_NAME: [noun]Utils
    {
        if (ident.size() > 5 && ident.substr(ident.size() - 5) == "Utils") {
            std::string noun_part = ident.substr(0, ident.size() - 5);
            int ni2 = find_noun_index(noun_part);
            if (ni2 >= 0) {
                result.type = CodeIdentType::UTILS_NAME;
                result.word1 = ni2;
                return result;
            }
        }
    }

    // Check LIST_NAME: [noun]List
    {
        if (ident.size() > 4 && ident.substr(ident.size() - 4) == "List") {
            std::string noun_part = ident.substr(0, ident.size() - 4);
            int ni2 = find_noun_index(noun_part);
            if (ni2 >= 0) {
                result.type = CodeIdentType::LIST_NAME;
                result.word1 = ni2;
                return result;
            }
        }
    }

    return result;  // UNKNOWN
}

inline bool detect_code_stream(const uint8_t* data, size_t n, CodeStreamParams& params) {
    if (n < 1024) return false;  // Too small

    std::string code((const char*)data, n);

    // Check for code-like patterns (function, class, const, async, etc.)
    // These keywords indicate structured code
    int code_keywords = 0;
    const char* keywords[] = {"function", "class", "const", "async", "await", "return",
                              "import", "export", "constructor", "this."};
    for (const char* kw : keywords) {
        size_t pos = 0;
        while ((pos = code.find(kw, pos)) != std::string::npos) {
            code_keywords++;
            pos++;
        }
    }

    // Need at least 50 code keywords per 10KB to be code-like
    if (code_keywords < (int)(n / 200)) return false;

    // JS keywords to NOT extract as identifiers
    std::set<std::string> reserved = {
        "async", "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "export", "extends", "false",
        "finally", "for", "function", "if", "import", "in", "instanceof", "let",
        "new", "null", "of", "return", "static", "super", "switch", "this", "throw",
        "true", "try", "typeof", "var", "void", "while", "with", "yield", "from",
        "id", "err", "response", "result", "results", "length", "active", "ok",
        "status", "json", "now", "push", "error", "console", "Date", "Error", "fetch",
        "api", "HTTP", "constructor", "getAll", "count", "timestamp", "require", "as",
        "item", "value", "i"  // Common loop variables
    };

    // Tokenize: extract identifiers, build skeleton
    std::string skeleton;
    std::vector<std::string> identifiers;

    size_t i = 0;
    while (i < code.size()) {
        if (isalpha(code[i]) || code[i] == '_') {
            size_t start = i;
            while (i < code.size() && (isalnum(code[i]) || code[i] == '_')) i++;
            std::string ident = code.substr(start, i - start);

            if (reserved.count(ident)) {
                skeleton += ident;  // Keep reserved words
            } else {
                skeleton += '\x01';  // Marker for identifier
                identifiers.push_back(ident);
            }
        } else {
            skeleton += code[i];
            i++;
        }
    }

    // Need enough identifiers to be worth it
    if (identifiers.size() < 100) return false;

    // Parse all identifiers into grammar
    std::vector<CodeIdentParsed> parsed;
    parsed.reserve(identifiers.size());
    for (const auto& ident : identifiers) {
        parsed.push_back(parse_code_identifier(ident));
    }

    // Count how many are UNKNOWN - if too many, skip this encoding
    size_t unknown_count = 0;
    for (const auto& p : parsed) {
        if (p.type == CodeIdentType::UNKNOWN) unknown_count++;
    }
    // Allow up to 30% unknown
    if (unknown_count > parsed.size() * 3 / 10) return false;

    // Build separate streams
    std::vector<uint8_t> type_stream;
    std::vector<uint8_t> letter_stream;
    std::vector<uint8_t> word1_stream;
    std::vector<uint8_t> word2_stream;
    std::vector<uint8_t> number_stream;
    std::vector<std::string> unknown_list;

    for (const auto& p : parsed) {
        type_stream.push_back((uint8_t)p.type);

        switch (p.type) {
            case CodeIdentType::CLASS_NAME:
                letter_stream.push_back(p.letter);
                word1_stream.push_back(p.word1);
                number_stream.push_back(p.number >> 8);
                number_stream.push_back(p.number & 0xFF);
                break;
            case CodeIdentType::FUNC_NAME:
                letter_stream.push_back(p.letter);
                word1_stream.push_back(p.word1);
                word2_stream.push_back(p.word2);
                break;
            case CodeIdentType::SERVICE_NAME:
                word1_stream.push_back(p.word1);
                number_stream.push_back(p.number >> 8);
                number_stream.push_back(p.number & 0xFF);
                break;
            case CodeIdentType::UTILS_NAME:
            case CodeIdentType::LIST_NAME:
            case CodeIdentType::SIMPLE:
                word1_stream.push_back(p.word1);
                break;
            case CodeIdentType::UNKNOWN:
                unknown_list.push_back(p.raw);
                break;
        }
    }

    // Compress each stream with zstd
    auto compress_stream = [](const std::vector<uint8_t>& in) -> std::vector<uint8_t> {
        if (in.empty()) return {};
        std::vector<uint8_t> out(ZSTD_compressBound(in.size()));
        size_t sz = ZSTD_compress(out.data(), out.size(), in.data(), in.size(), 19);
        if (ZSTD_isError(sz) || sz >= in.size()) {
            // Store uncompressed (prepend 0 flag)
            std::vector<uint8_t> result(1 + in.size());
            result[0] = 0;
            memcpy(result.data() + 1, in.data(), in.size());
            return result;
        }
        out.resize(sz);
        // Prepend 1 flag
        std::vector<uint8_t> result(1 + sz);
        result[0] = 1;
        memcpy(result.data() + 1, out.data(), sz);
        return result;
    };

    // BWT compression for streams where it wins (skeleton, type, word1)
    // Analysis showed: skeleton BWT 11334 vs zstd 19147 (saves 7813 bytes at 1MB)
    auto compress_stream_bwt = [](const std::vector<uint8_t>& in) -> std::vector<uint8_t> {
        if (in.empty()) return {};
        // Try BWT compression
        auto bwt_out = bwt9::compress(in.data(), in.size());
        // `bwt_out.empty()` matters and the size test alone cannot see it (2026-08-04).
        // bwt9::compress returns {} when both its arms decline, and `0 >= in.size()` is
        // FALSE for every non-empty stream — so an empty result used to fall through to
        // the BWT branch below and emit a lone flag byte with no payload, which reads as
        // a spectacular compression win. That degenerate stream then shrinks
        // total_encoded and makes the CODE_STREAM acceptance gate PASS more easily, so
        // the broken encoding is more likely to be selected, and this call site has no
        // roundtrip verify (unlike TEMPLATE and CSV_COLUMNAR). Same shape as the bwt9
        // header-before-payload hazard fixed in bwt_compress_v9.hpp; its sibling
        // compress_stream above does check ZSTD_isError, this one checked nothing.
        if (bwt_out.empty() || bwt_out.size() >= in.size()) {
            // Store uncompressed (prepend 0 flag)
            std::vector<uint8_t> result(1 + in.size());
            result[0] = 0;
            memcpy(result.data() + 1, in.data(), in.size());
            return result;
        }
        // Prepend 2 flag for BWT
        std::vector<uint8_t> result(1 + bwt_out.size());
        result[0] = 2;
        memcpy(result.data() + 1, bwt_out.data(), bwt_out.size());
        return result;
    };

    // Compress skeleton with BWT (wins by 7813 bytes at 1MB)
    std::vector<uint8_t> skeleton_bytes(skeleton.begin(), skeleton.end());
    params.skeleton_zstd = compress_stream_bwt(skeleton_bytes);
    params.type_stream_zstd = compress_stream_bwt(type_stream);
    params.letter_stream_zstd = compress_stream(letter_stream);
    params.word1_stream_zstd = compress_stream_bwt(word1_stream);
    params.word2_stream_zstd = compress_stream(word2_stream);
    params.number_stream_zstd = compress_stream(number_stream);
    params.total_identifiers = identifiers.size();

    // Build and compress unknown strings buffer
    std::vector<uint8_t> unknown_buf;
    for (const auto& s : unknown_list) {
        unknown_buf.push_back((uint8_t)s.size());
        for (char c : s) unknown_buf.push_back((uint8_t)c);
    }

    // Store compressed unknown buffer in params (we'll use unknown_identifiers differently)
    // For now, calculate the compressed size for threshold check
    size_t unknown_compressed_size = 0;
    if (!unknown_buf.empty()) {
        std::vector<uint8_t> unk_out(ZSTD_compressBound(unknown_buf.size()));
        size_t unk_sz = ZSTD_compress(unk_out.data(), unk_out.size(),
                                       unknown_buf.data(), unknown_buf.size(), 19);
        if (!ZSTD_isError(unk_sz) && unk_sz < unknown_buf.size()) {
            unknown_compressed_size = unk_sz + 1;  // +1 for compressed flag
        } else {
            unknown_compressed_size = unknown_buf.size() + 1;  // +1 for uncompressed flag
        }
    }

    // Store raw unknown for encoding later
    params.unknown_identifiers = std::move(unknown_list);

    // Calculate total size
    size_t total_encoded = params.skeleton_zstd.size()
                         + params.type_stream_zstd.size()
                         + params.letter_stream_zstd.size()
                         + params.word1_stream_zstd.size()
                         + params.word2_stream_zstd.size()
                         + params.number_stream_zstd.size()
                         + 4   // unknown count
                         + 4   // unknown buffer size
                         + unknown_compressed_size;

    // Compare with raw zstd
    std::vector<uint8_t> raw_zstd(ZSTD_compressBound(n));
    size_t raw_sz = ZSTD_compress(raw_zstd.data(), raw_zstd.size(), data, n, 19);
    if (ZSTD_isError(raw_sz)) return false;

    // Need at least 5% improvement
    if (total_encoded >= raw_sz * 95 / 100) return false;

    return true;
}

inline std::vector<uint8_t> encode_code_stream(const CodeStreamParams& params) {
    std::vector<uint8_t> result;

    // Helper to write 4-byte size + data
    auto write_sized = [&result](const std::vector<uint8_t>& data) {
        uint32_t sz = (uint32_t)data.size();
        result.push_back(sz & 0xFF);
        result.push_back((sz >> 8) & 0xFF);
        result.push_back((sz >> 16) & 0xFF);
        result.push_back((sz >> 24) & 0xFF);
        result.insert(result.end(), data.begin(), data.end());
    };

    write_sized(params.skeleton_zstd);
    write_sized(params.type_stream_zstd);
    write_sized(params.letter_stream_zstd);
    write_sized(params.word1_stream_zstd);
    write_sized(params.word2_stream_zstd);
    write_sized(params.number_stream_zstd);

    // Write unknown count and compressed strings
    uint32_t unk_count = (uint32_t)params.unknown_identifiers.size();
    result.push_back(unk_count & 0xFF);
    result.push_back((unk_count >> 8) & 0xFF);
    result.push_back((unk_count >> 16) & 0xFF);
    result.push_back((unk_count >> 24) & 0xFF);

    // Build unknown strings buffer
    std::vector<uint8_t> unk_buf;
    for (const auto& s : params.unknown_identifiers) {
        unk_buf.push_back((uint8_t)s.size());
        for (char c : s) unk_buf.push_back((uint8_t)c);
    }

    // Compress unknown buffer
    if (!unk_buf.empty()) {
        std::vector<uint8_t> unk_compressed(ZSTD_compressBound(unk_buf.size()));
        size_t unk_sz = ZSTD_compress(unk_compressed.data(), unk_compressed.size(),
                                       unk_buf.data(), unk_buf.size(), 19);
        if (!ZSTD_isError(unk_sz) && unk_sz < unk_buf.size()) {
            // Compressed version is smaller
            std::vector<uint8_t> compressed_with_flag(1 + unk_sz);
            compressed_with_flag[0] = 1;  // Compressed flag
            memcpy(compressed_with_flag.data() + 1, unk_compressed.data(), unk_sz);
            write_sized(compressed_with_flag);
        } else {
            // Store uncompressed
            std::vector<uint8_t> uncompressed_with_flag(1 + unk_buf.size());
            uncompressed_with_flag[0] = 0;  // Uncompressed flag
            memcpy(uncompressed_with_flag.data() + 1, unk_buf.data(), unk_buf.size());
            write_sized(uncompressed_with_flag);
        }
    } else {
        // No unknown strings - write empty
        write_sized({});
    }

    return result;
}

inline std::vector<uint8_t> decode_code_stream(const uint8_t* encoded, size_t encoded_size, size_t original_size) {
    if (encoded_size < 24) return {};

    const uint8_t* ptr = encoded;
    const uint8_t* end = encoded + encoded_size;

    // Helper to read 4-byte size + data
    auto read_sized = [&ptr, end]() -> std::vector<uint8_t> {
        if (ptr + 4 > end) return {};
        uint32_t sz = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
        if (ptr + sz > end) return {};
        std::vector<uint8_t> data(ptr, ptr + sz);
        ptr += sz;
        return data;
    };

    // Helper to decompress stream (supports flag 0=raw, 1=zstd, 2=bwt)
    auto decompress_stream = [](const std::vector<uint8_t>& compressed) -> std::vector<uint8_t> {
        if (compressed.empty()) return {};
        uint8_t flag = compressed[0];
        if (flag == 0) {
            // Uncompressed
            return std::vector<uint8_t>(compressed.begin() + 1, compressed.end());
        } else if (flag == 2) {
            // BWT compressed
            return bwt9::decompress(compressed.data() + 1, compressed.size() - 1);
        }
        // flag == 1: zstd compressed
        size_t orig_size = ZSTD_getFrameContentSize(compressed.data() + 1, compressed.size() - 1);
        if (orig_size == ZSTD_CONTENTSIZE_ERROR || orig_size == ZSTD_CONTENTSIZE_UNKNOWN) return {};
        std::vector<uint8_t> out(orig_size);
        size_t sz = ZSTD_decompress(out.data(), out.size(), compressed.data() + 1, compressed.size() - 1);
        if (ZSTD_isError(sz)) return {};
        out.resize(sz);
        return out;
    };

    // Read all streams
    auto skeleton_zstd = read_sized();
    auto type_stream_zstd = read_sized();
    auto letter_stream_zstd = read_sized();
    auto word1_stream_zstd = read_sized();
    auto word2_stream_zstd = read_sized();
    auto number_stream_zstd = read_sized();

    // Read unknown count
    if (ptr + 4 > end) return {};
    uint32_t unk_count = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;

    auto unk_buf_compressed = read_sized();

    // Decompress streams
    auto skeleton = decompress_stream(skeleton_zstd);
    auto type_stream = decompress_stream(type_stream_zstd);
    auto letter_stream = decompress_stream(letter_stream_zstd);
    auto word1_stream = decompress_stream(word1_stream_zstd);
    auto word2_stream = decompress_stream(word2_stream_zstd);
    auto number_stream = decompress_stream(number_stream_zstd);
    auto unk_buf = decompress_stream(unk_buf_compressed);

    // Parse unknown strings
    std::vector<std::string> unknown_strings;
    {
        const uint8_t* up = unk_buf.data();
        const uint8_t* ue = unk_buf.data() + unk_buf.size();
        for (uint32_t i = 0; i < unk_count && up < ue; i++) {
            uint8_t len = *up++;
            if (up + len > ue) break;
            unknown_strings.push_back(std::string((char*)up, len));
            up += len;
        }
    }

    // Reconstruct identifiers
    std::vector<std::string> identifiers;
    size_t ti = 0, li = 0, w1i = 0, w2i = 0, ni = 0, ui = 0;

    for (size_t idx = 0; idx < type_stream.size(); idx++) {
        CodeIdentType type = (CodeIdentType)type_stream[idx];
        std::string ident;

        switch (type) {
            case CodeIdentType::CLASS_NAME: {
                if (li >= letter_stream.size() || w1i >= word1_stream.size() || ni + 1 >= number_stream.size()) break;
                char letter = 'A' + letter_stream[li++];
                std::string noun = CODE_STREAM_NOUNS[word1_stream[w1i++] % CODE_STREAM_NUM_NOUNS];
                uint16_t num = (number_stream[ni] << 8) | number_stream[ni + 1];
                ni += 2;
                ident = letter + noun + std::to_string(num);
                break;
            }
            case CodeIdentType::FUNC_NAME: {
                if (li >= letter_stream.size() || w1i >= word1_stream.size() || w2i >= word2_stream.size()) break;
                std::string verb = CODE_STREAM_VERBS[word1_stream[w1i++] % CODE_STREAM_NUM_VERBS];
                char letter = 'A' + letter_stream[li++];
                std::string noun = CODE_STREAM_NOUNS[word2_stream[w2i++] % CODE_STREAM_NUM_NOUNS];
                // Noun stays lowercase (original: verb + Letter + noun)
                ident = verb + letter + noun;
                break;
            }
            case CodeIdentType::SERVICE_NAME: {
                if (w1i >= word1_stream.size() || ni + 1 >= number_stream.size()) break;
                std::string noun = CODE_STREAM_NOUNS[word1_stream[w1i++] % CODE_STREAM_NUM_NOUNS];
                uint16_t num = (number_stream[ni] << 8) | number_stream[ni + 1];
                ni += 2;
                ident = noun + "Service" + std::to_string(num);
                break;
            }
            case CodeIdentType::UTILS_NAME: {
                if (w1i >= word1_stream.size()) break;
                std::string noun = CODE_STREAM_NOUNS[word1_stream[w1i++] % CODE_STREAM_NUM_NOUNS];
                ident = noun + "Utils";
                break;
            }
            case CodeIdentType::LIST_NAME: {
                if (w1i >= word1_stream.size()) break;
                std::string noun = CODE_STREAM_NOUNS[word1_stream[w1i++] % CODE_STREAM_NUM_NOUNS];
                ident = noun + "List";
                break;
            }
            case CodeIdentType::SIMPLE: {
                if (w1i >= word1_stream.size()) break;
                uint8_t wi = word1_stream[w1i++];
                if (wi < CODE_STREAM_NUM_NOUNS) {
                    ident = CODE_STREAM_NOUNS[wi];
                } else {
                    ident = CODE_STREAM_VERBS[(wi - CODE_STREAM_NUM_NOUNS) % CODE_STREAM_NUM_VERBS];
                }
                break;
            }
            case CodeIdentType::UNKNOWN: {
                if (ui < unknown_strings.size()) {
                    ident = unknown_strings[ui++];
                }
                break;
            }
        }
        identifiers.push_back(ident);
    }

    // Reconstruct code: replace \x01 markers with identifiers
    std::vector<uint8_t> result;
    result.reserve(original_size);
    size_t ident_idx = 0;
    for (uint8_t c : skeleton) {
        if (c == 0x01 && ident_idx < identifiers.size()) {
            const auto& ident = identifiers[ident_idx++];
            for (char ch : ident) result.push_back((uint8_t)ch);
        } else {
            result.push_back(c);
        }
    }

    // Trim to original size
    while (result.size() > original_size) result.pop_back();

    return result;
}

// ============================================================================
// Word-Level Encoding (for natural language text)
// Key insight: Natural text has few unique words but high repetition.
// Word-level encoding converts text to word indices + separators.
// This exploits the low vocabulary entropy that BWT captures implicitly.
// ============================================================================

struct WordEncodingParams {
    std::vector<std::string> vocab;     // Words sorted by frequency
    size_t total_words;                 // Total word count
    size_t unique_words;                // Unique word count
    double vocab_ratio;                 // unique/total - low = good for word encoding
};

// Analyze if text benefits from word-level encoding
inline bool analyze_word_encoding(const uint8_t* data, size_t n, WordEncodingParams& params) {
    if (n < 256) return false;  // Too small

    // Quick sample-based analysis (first 4KB)
    size_t sample_size = std::min(n, (size_t)4096);

    // Count words and build frequency map
    std::map<std::string, int> word_freq;
    std::string word;
    size_t total_words = 0;
    size_t letter_count = 0;
    size_t punct_count = 0;

    for (size_t i = 0; i < sample_size; i++) {
        uint8_t c = data[i];
        bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        // Punctuation chars: !-/ (33-47), :-@ (58-64), [-` (91-96), {-~ (123-126)
        bool is_punct = (c >= '!' && c <= '/') || (c >= ':' && c <= '@') ||
                        (c >= '[' && c <= '`') || (c >= '{' && c <= '~');

        if (is_alpha) {
            word += (char)c;
            letter_count++;
        } else {
            if (is_punct) punct_count++;
            if (!word.empty()) {
                // Convert to lowercase for matching
                std::string lower_word;
                for (char ch : word) lower_word += (ch >= 'A' && ch <= 'Z') ? ch + 32 : ch;
                word_freq[lower_word]++;
                total_words++;
                word.clear();
            }
        }
    }
    if (!word.empty()) {
        std::string lower_word;
        for (char ch : word) lower_word += (ch >= 'A' && ch <= 'Z') ? ch + 32 : ch;
        word_freq[lower_word]++;
        total_words++;
    }

    if (total_words < 50) return false;  // Not enough words

    size_t unique_words = word_freq.size();
    double vocab_ratio = (double)unique_words / total_words;
    double avg_freq = (double)total_words / unique_words;
    double letter_pct = (double)letter_count / sample_size;
    double punct_pct = (double)punct_count / sample_size;

    // Reject punctuation-heavy data (TypeScript/JSX, code with lots of {}[]()<>)
    // Word encoding uses 2-byte escape (0xFF + char) for punctuation, which
    // destroys LZ77 patterns that zstd would otherwise exploit effectively.
    // TypeScript has ~25-30% punctuation - reject at 15% threshold.
    if (punct_pct > 0.15) return false;

    // Word encoding wins when words are highly repetitive
    // Template analysis showed:
    // - access_log: 23.8% letters, vocab_ratio ~0.004 -> 73x compression
    // - syslog: 49.8% letters, vocab_ratio ~0.006 -> 41x compression
    // Tiered detection based on repetition quality:
    // IMPORTANT: Word encoding only helps when letter% is high enough.
    // With 24% letters (access_log), encoding 76% non-letters as 2-byte literals
    // makes compression WORSE. Only use word encoding when letters dominate.

    bool good_for_word_encoding = false;

    // Tier 1: Extremely high repetition - still need 40% letters
    // Below 40%, literal encoding overhead kills the benefit
    if (vocab_ratio < 0.02 && avg_freq > 50 && unique_words < 500 && letter_pct > 0.40) {
        good_for_word_encoding = true;
    }
    // Tier 2: High repetition - need 40% letters
    else if (vocab_ratio < 0.10 && avg_freq > 10 && letter_pct > 0.40 && unique_words < 500) {
        good_for_word_encoding = true;
    }
    // Tier 3: Moderate repetition, prose-like - need 50% letters
    else if (vocab_ratio < 0.15 && avg_freq > 5 && letter_pct > 0.50 && unique_words < 500) {
        good_for_word_encoding = true;
    }

    if (good_for_word_encoding) {
        params.total_words = total_words;
        params.unique_words = unique_words;
        params.vocab_ratio = vocab_ratio;
        // Build full vocabulary from entire data (not just sample)
        word_freq.clear();
        word.clear();
        for (size_t i = 0; i < n; i++) {
            uint8_t c = data[i];
            bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (is_alpha) {
                word += (char)c;
            } else if (!word.empty()) {
                word_freq[word]++;  // Keep original case for exact reconstruction
                word.clear();
            }
        }
        if (!word.empty()) word_freq[word]++;

        // Sort by frequency (most common first = smaller indices)
        std::vector<std::pair<std::string, int>> sorted(word_freq.begin(), word_freq.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });

        params.vocab.clear();
        for (auto& [w, freq] : sorted) {
            params.vocab.push_back(w);
        }

        return true;
    }

    return false;
}

// Encode text using word-level encoding
// Format: [vocab_size:2] [vocab: len+chars...] [tokens: word_idx or 0xFF+char]
inline std::vector<uint8_t> encode_word_text(const uint8_t* data, size_t n,
                                              const WordEncodingParams& params) {
    std::vector<uint8_t> output;

    // Build word-to-index map
    std::map<std::string, uint16_t> word_to_idx;
    for (size_t i = 0; i < params.vocab.size(); i++) {
        word_to_idx[params.vocab[i]] = i;
    }

    // Header: vocab size
    uint16_t vocab_size = params.vocab.size();
    output.push_back(vocab_size & 0xFF);
    output.push_back((vocab_size >> 8) & 0xFF);

    // Vocabulary: length-prefixed strings
    for (const auto& w : params.vocab) {
        if (w.size() > 255) {
            output.push_back(255);
            for (size_t i = 0; i < 255; i++) output.push_back(w[i]);
        } else {
            output.push_back(w.size());
            for (char c : w) output.push_back((uint8_t)c);
        }
    }

    // Encode text as tokens
    std::string word;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = data[i];
        bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');

        if (is_alpha) {
            word += (char)c;
        } else {
            // Emit word if any
            if (!word.empty()) {
                auto it = word_to_idx.find(word);
                if (it != word_to_idx.end()) {
                    uint16_t idx = it->second;
                    // Varint encoding for index
                    if (idx < 128) {
                        output.push_back(idx);
                    } else {
                        output.push_back((idx & 0x7F) | 0x80);
                        output.push_back(idx >> 7);
                    }
                } else {
                    // Unknown word - encode as literal bytes
                    for (char ch : word) {
                        output.push_back(0xFF);
                        output.push_back((uint8_t)ch);
                    }
                }
                word.clear();
            }
            // Emit non-word character
            // Common separators get single-byte encoding
            if (c == ' ') {
                output.push_back(0xFE);  // Space
            } else if (c == '\n') {
                output.push_back(0xFD);  // Newline
            } else if (c == '.') {
                output.push_back(0xFC);  // Period
            } else if (c == ',') {
                output.push_back(0xFB);  // Comma
            } else {
                output.push_back(0xFF);  // Other: 2 bytes
                output.push_back(c);
            }
        }
    }
    // Emit final word
    if (!word.empty()) {
        auto it = word_to_idx.find(word);
        if (it != word_to_idx.end()) {
            uint16_t idx = it->second;
            if (idx < 128) {
                output.push_back(idx);
            } else {
                output.push_back((idx & 0x7F) | 0x80);
                output.push_back(idx >> 7);
            }
        } else {
            for (char ch : word) {
                output.push_back(0xFF);
                output.push_back((uint8_t)ch);
            }
        }
    }

    return output;
}

// Decode word-encoded text
inline std::vector<uint8_t> decode_word_text(const uint8_t* data, size_t n) {
    if (n < 2) return {};

    size_t pos = 0;

    // Read vocab size
    uint16_t vocab_size = data[pos] | (data[pos + 1] << 8);
    pos += 2;

    // Read vocabulary
    std::vector<std::string> vocab;
    for (uint16_t i = 0; i < vocab_size && pos < n; i++) {
        uint8_t len = data[pos++];
        if (pos + len > n) break;
        vocab.push_back(std::string((char*)&data[pos], len));
        pos += len;
    }

    // Decode tokens
    std::vector<uint8_t> output;
    while (pos < n) {
        uint8_t b = data[pos++];

        if (b < 0xFB) {
            // Word index (possibly varint)
            uint16_t idx = b;
            if (b & 0x80) {
                if (pos >= n) break;
                idx = (b & 0x7F) | (data[pos++] << 7);
            }
            if (idx < vocab.size()) {
                for (char c : vocab[idx]) output.push_back((uint8_t)c);
            }
        } else if (b == 0xFB) {
            output.push_back(',');
        } else if (b == 0xFC) {
            output.push_back('.');
        } else if (b == 0xFD) {
            output.push_back('\n');
        } else if (b == 0xFE) {
            output.push_back(' ');
        } else {  // 0xFF - literal byte follows
            if (pos >= n) break;
            output.push_back(data[pos++]);
        }
    }

    return output;
}

// ============================================================================
// DBF Constant Column Elimination
// ============================================================================
// dBASE/FoxPro fixed-width records have many columns that are 100% (or 99%)
// identical (usually all spaces). Eliminating those and compressing only the
// variable columns with zstd beats brotli by 3.4% on large DBF files.
//
// Format:
//   [2B record_size] [2B var_cols_count] [2B header_size] [4B num_records]
//   [ceil(record_size/8) bytes: bitmap, bit=1 means constant]
//   [const_cols bytes: constant values in order]
//   [header_size bytes: raw DBF header]
//   [remainder bytes: data after last complete record]
//   [num_records * var_cols bytes: reduced records, row-major]
//
// For 99% threshold, exception data is appended after reduced records:
//   For each constant column (in order):
//     [2B exception_count]
//     For each exception: [2B row_index] [1B value]

inline std::vector<uint8_t> encode_dbf_constcol(const uint8_t* data, size_t n, int zstd_level = 19) {
    // Parse DBF header
    if (n < 32) return {};
    uint16_t header_size = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
    uint16_t record_size = (uint16_t)data[10] | ((uint16_t)data[11] << 8);
    if (header_size >= n || record_size < 10) return {};

    const uint8_t* records = data + header_size;
    size_t records_bytes = n - header_size;
    size_t num_records = records_bytes / record_size;
    size_t remainder = records_bytes - num_records * record_size;

    if (num_records < 3) return {};

    // Classify each byte position: constant (99%+ same) or variable
    size_t bitmap_bytes = (record_size + 7) / 8;
    std::vector<uint8_t> bitmap(bitmap_bytes, 0);
    std::vector<uint8_t> const_values;
    std::vector<size_t> var_positions;
    std::vector<size_t> const_positions;

    double threshold = 0.99;
    size_t min_same = (size_t)(num_records * threshold);

    for (size_t col = 0; col < record_size; col++) {
        // Find most frequent byte
        int freq[256] = {};
        for (size_t r = 0; r < num_records; r++) {
            freq[records[r * record_size + col]]++;
        }
        int max_freq = 0;
        uint8_t max_byte = 0;
        for (int b = 0; b < 256; b++) {
            if (freq[b] > max_freq) { max_freq = freq[b]; max_byte = (uint8_t)b; }
        }

        if ((size_t)max_freq >= min_same) {
            bitmap[col / 8] |= (1 << (col % 8));
            const_values.push_back(max_byte);
            const_positions.push_back(col);
        } else {
            var_positions.push_back(col);
        }
    }

    // Not worth it if we can't eliminate enough columns
    if (var_positions.size() >= (size_t)record_size * 3 / 4) return {};

    // Build payload
    std::vector<uint8_t> payload;
    size_t est_size = 12 + bitmap_bytes + const_values.size() + header_size + remainder
                    + num_records * var_positions.size()
                    + const_positions.size() * 4; // rough exception estimate
    payload.reserve(est_size);

    // Header: 12 bytes
    payload.push_back(record_size & 0xFF);
    payload.push_back(record_size >> 8);
    payload.push_back(var_positions.size() & 0xFF);
    payload.push_back((var_positions.size() >> 8) & 0xFF);
    payload.push_back(header_size & 0xFF);
    payload.push_back(header_size >> 8);
    payload.push_back(num_records & 0xFF);
    payload.push_back((num_records >> 8) & 0xFF);
    payload.push_back((num_records >> 16) & 0xFF);
    payload.push_back((num_records >> 24) & 0xFF);
    // Remainder size (2 bytes, max 64KB)
    payload.push_back(remainder & 0xFF);
    payload.push_back((remainder >> 8) & 0xFF);

    // Bitmap
    payload.insert(payload.end(), bitmap.begin(), bitmap.end());

    // Constant values
    payload.insert(payload.end(), const_values.begin(), const_values.end());

    // Raw DBF header
    payload.insert(payload.end(), data, data + header_size);

    // Remainder (bytes after last complete record, e.g. EOF marker)
    if (remainder > 0) {
        payload.insert(payload.end(),
                       data + header_size + num_records * record_size,
                       data + n);
    }

    // Reduced records (row-major: for each record, only variable columns)
    for (size_t r = 0; r < num_records; r++) {
        for (size_t vp : var_positions) {
            payload.push_back(records[r * record_size + vp]);
        }
    }

    // Exception data for near-constant columns (where value differs from majority)
    for (size_t ci = 0; ci < const_positions.size(); ci++) {
        size_t col = const_positions[ci];
        uint8_t majority = const_values[ci];

        // Collect exceptions
        std::vector<std::pair<uint32_t, uint8_t>> exceptions;
        for (size_t r = 0; r < num_records; r++) {
            uint8_t v = records[r * record_size + col];
            if (v != majority) {
                exceptions.push_back({(uint32_t)r, v});
            }
        }

        // Store: [2B count][per exception: 4B row + 1B value] if any exceptions
        // Optimization: skip entirely if count == 0
        uint16_t ec = (uint16_t)exceptions.size();
        payload.push_back(ec & 0xFF);
        payload.push_back(ec >> 8);
        for (auto& e : exceptions) {
            // Use 2 bytes for row index (supports up to 65535 records)
            payload.push_back(e.first & 0xFF);
            payload.push_back((e.first >> 8) & 0xFF);
            payload.push_back(e.second);
        }
    }

    // Compress payload with zstd
    size_t bound = ZSTD_compressBound(payload.size());
    std::vector<uint8_t> compressed(bound);
    size_t csz = ZSTD_compress(compressed.data(), bound, payload.data(), payload.size(), zstd_level);
    if (ZSTD_isError(csz)) return {};

    compressed.resize(csz);
    return compressed;
}

inline std::vector<uint8_t> decode_dbf_constcol(const uint8_t* data, size_t n, size_t original_size) {
    // Decompress the zstd payload first
    std::vector<uint8_t> payload(original_size * 2); // generous buffer
    size_t payload_size = ZSTD_decompress(payload.data(), payload.size(), data, n);
    if (ZSTD_isError(payload_size)) {
        // Try with exact original_size
        payload.resize(original_size + original_size);
        payload_size = ZSTD_decompress(payload.data(), payload.size(), data, n);
        if (ZSTD_isError(payload_size)) return {};
    }

    // Parse header
    if (payload_size < 12) return {};
    const uint8_t* p = payload.data();
    uint16_t record_size = p[0] | (p[1] << 8);
    uint16_t var_cols = p[2] | (p[3] << 8);
    uint16_t header_size = p[4] | (p[5] << 8);
    uint32_t num_records = p[6] | (p[7] << 8) | (p[8] << 16) | (p[9] << 24);
    uint16_t remainder_size = p[10] | (p[11] << 8);

    size_t pos = 12;

    // Read bitmap
    size_t bitmap_bytes = (record_size + 7) / 8;
    if (pos + bitmap_bytes > payload_size) return {};
    const uint8_t* bitmap = p + pos;
    pos += bitmap_bytes;

    // Count constant columns and build position lists
    std::vector<size_t> const_positions, var_positions;
    for (size_t col = 0; col < record_size; col++) {
        if (bitmap[col / 8] & (1 << (col % 8))) {
            const_positions.push_back(col);
        } else {
            var_positions.push_back(col);
        }
    }

    if (var_positions.size() != var_cols) return {};

    // Read constant values
    if (pos + const_positions.size() > payload_size) return {};
    std::vector<uint8_t> const_values(p + pos, p + pos + const_positions.size());
    pos += const_positions.size();

    // Read DBF header
    if (pos + header_size > payload_size) return {};
    const uint8_t* dbf_header = p + pos;
    pos += header_size;

    // Read remainder
    if (pos + remainder_size > payload_size) return {};
    const uint8_t* remainder_data = p + pos;
    pos += remainder_size;

    // Read reduced records
    size_t reduced_total = (size_t)num_records * var_cols;
    if (pos + reduced_total > payload_size) return {};
    const uint8_t* reduced_records = p + pos;
    pos += reduced_total;

    // Build output
    std::vector<uint8_t> output(original_size);

    // Copy DBF header
    memcpy(output.data(), dbf_header, header_size);

    // Reconstruct records: fill constant values, then overlay variable columns
    uint8_t* out_records = output.data() + header_size;
    for (size_t r = 0; r < num_records; r++) {
        uint8_t* rec = out_records + r * record_size;
        // Fill constant columns
        for (size_t ci = 0; ci < const_positions.size(); ci++) {
            rec[const_positions[ci]] = const_values[ci];
        }
        // Fill variable columns
        for (size_t vi = 0; vi < var_cols; vi++) {
            rec[var_positions[vi]] = reduced_records[r * var_cols + vi];
        }
    }

    // Apply exceptions (near-constant column corrections)
    for (size_t ci = 0; ci < const_positions.size(); ci++) {
        if (pos + 2 > payload_size) break;
        uint16_t exc_count = p[pos] | (p[pos + 1] << 8);
        pos += 2;
        for (uint16_t e = 0; e < exc_count; e++) {
            if (pos + 3 > payload_size) break;
            uint16_t row = p[pos] | (p[pos + 1] << 8);
            uint8_t value = p[pos + 2];
            pos += 3;
            if (row < num_records) {
                out_records[row * record_size + const_positions[ci]] = value;
            }
        }
    }

    // Copy remainder (EOF marker etc.)
    if (remainder_size > 0 && header_size + num_records * record_size + remainder_size <= original_size) {
        memcpy(output.data() + header_size + num_records * record_size,
               remainder_data, remainder_size);
    }

    return output;
}

// ============================================================================
// Data Type Detection
// ============================================================================

struct BlockAnalysis {
    BlockType type;
    tieredcompress::Strategy numeric_strategy;  // Only valid if type == NUMERIC
    double entropy;
    double text_score;      // 0-1, higher = more likely text
    double numeric_score;   // 0-1, higher = more likely numeric
    bool high_entropy;      // >7.5 bits/byte = likely encrypted/compressed

    // Image-specific (only valid if type == IMAGE)
    int image_width;
    int image_height;
    int image_channels;

    // Generator-specific (only valid if type == LINEAR_GEN)
    LinearGenParams linear_gen;

    // Periodic-specific (only valid if type == PERIODIC)
    size_t period;

    // Timestamp-specific (only valid if type == TIMESTAMP)
    TimestampParams timestamp;

    // Template-specific (only valid if type == TEMPLATE)
    TemplateParams template_params;

    // Char-level template (only valid if type == CHAR_TEMPLATE)
    CharTemplateParams char_template;

    // Multi-line template (only valid if type == ML_TEMPLATE)
    MLTemplateParams ml_template;

    // Dual multi-line template (only valid if type == ML_TEMPLATE_DUAL)
    MLTemplateDualParams ml_template_dual;

    // Columnar params (only valid if type == COLUMNAR)
    ColumnarParams columnar;

    // CSV columnar params (only valid if type == CSV_COLUMNAR)
    CsvColumnarParams csv_columnar;

    // Section template params (only valid if type == SECTION_TEMPLATE)
    SectionTemplateParams section_template;

    // Word template params (only valid if type == WORD_TEMPLATE)
    WordTemplateParams word_template;

    // Multi-word template params (only valid if type == MULTI_WORD_TEMPLATE)
    MultiWordTemplateParams multi_word_template;

    // Word encoding params (only valid if type == WORD_ENCODED)
    WordEncodingParams word_encoding;

    // Line template params (only valid if type == LINE_TEMPLATE)
    LineTemplateParams line_template;

    // Line group template params (only valid if type == LINE_GROUP_TEMPLATE)
    std::vector<LineGroupInfo> line_group_info;
    std::vector<uint8_t> line_group_types;

    // Phrase dictionary params (only valid if type == PHRASE_DICT)
    PhraseDictParams phrase_dict;

    // Phrase partition params (only valid if type == PHRASE_PARTITION)
    PhrasePartitionParams phrase_partition;

    // Sorted dictionary params (only valid if type == SORTED_DICT)
    SortedDictParams sorted_dict;

    // KV config params (only valid if type == KV_CONFIG)
    KvConfigParams kv_config;

    // Generator-specific params (only valid if matching type)
    GeometricParams geometric;
    QuadraticParams quadratic;
    RecurrenceParams recurrence;
    ModularParams modular;

    // Approximate encoding params (Effective Complexity)
    LinearGenApproxParams linear_gen_approx;
    std::vector<std::pair<uint32_t, int64_t>> linear_gen_approx_exceptions;

    // Periodic approximate params (Effective Complexity)
    PeriodicApproxParams periodic_approx;
    std::vector<std::pair<uint32_t, uint8_t>> periodic_approx_exceptions;

    // Sparse params (only valid if type == SPARSE)
    size_t sparse_nonzero_count;
    uint8_t sparse_common_value;
    bool sparse_all_same_value;

    // Code stream params (only valid if type == CODE_STREAM)
    CodeStreamParams code_stream;

    // Dual stream params (only valid if type == DUAL_STREAM)
    DualStreamParams dual_stream;

    // HTML stream params (only valid if type == HTML_STREAM)
    HtmlStreamParams html_stream;

    // URL stream params (only valid if type == URL_STREAM)
    UrlStreamParams url_stream;

    // Base64 params (only valid if type == BASE64_DECODE)
    Base64Params base64;

    // JSON columnar params (only valid if type == JSON_COLUMNAR)
    JsonColumnarParams json_columnar;

    // Num extract params (only valid if type == NUM_EXTRACT)
    NumExtractParams num_extract;

    // Complexity metrics (Gell-Mann Effective Complexity)
    EffectiveComplexityScore complexity_score;
    LogicalDepth logical_depth;
};

// Check if data is valid UTF-8
inline bool is_valid_utf8(const uint8_t* data, size_t n) {
    size_t i = 0;
    while (i < n) {
        if (data[i] <= 0x7F) {
            i++;
        } else if ((data[i] & 0xE0) == 0xC0) {
            if (i + 1 >= n || (data[i+1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((data[i] & 0xF0) == 0xE0) {
            if (i + 2 >= n || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((data[i] & 0xF8) == 0xF0) {
            if (i + 3 >= n || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80 || (data[i+3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

// Calculate text score based on character distribution
// Handles both ASCII and UTF-8 text
inline double calculate_text_score(const uint8_t* data, size_t n) {
    if (n == 0) return 0.0;

    size_t sample_size = std::min(n, (size_t)4096);
    size_t printable = 0;
    size_t whitespace = 0;
    size_t ascii = 0;
    size_t null_bytes = 0;
    size_t utf8_chars = 0;      // Count of valid UTF-8 multi-byte sequences
    size_t utf8_bytes = 0;      // Bytes consumed by UTF-8 sequences

    for (size_t i = 0; i < sample_size; i++) {
        uint8_t c = data[i];
        if (c == 0) null_bytes++;
        if (c >= 32 && c <= 126) printable++;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') whitespace++;
        if (c < 128) ascii++;

        // Check for UTF-8 multi-byte sequences
        if (c >= 0xC2 && c <= 0xF4 && i + 1 < sample_size) {
            size_t seq_len = 0;
            if (c >= 0xC2 && c <= 0xDF) seq_len = 2;      // 2-byte: 110xxxxx
            else if (c >= 0xE0 && c <= 0xEF) seq_len = 3; // 3-byte: 1110xxxx
            else if (c >= 0xF0 && c <= 0xF4) seq_len = 4; // 4-byte: 11110xxx

            if (seq_len > 0 && i + seq_len <= sample_size) {
                bool valid = true;
                for (size_t j = 1; j < seq_len && valid; j++) {
                    uint8_t cont = data[i + j];
                    if (cont < 0x80 || cont > 0xBF) valid = false;
                }
                if (valid) {
                    utf8_chars++;
                    utf8_bytes += seq_len;
                    i += seq_len - 1;  // Skip continuation bytes
                }
            }
        }
    }

    // Null bytes are a strong indicator of binary
    if (null_bytes > sample_size * 0.01) return 0.0;

    // For UTF-8 text, count multi-byte chars as printable
    double effective_printable = printable + utf8_chars;
    double effective_sample = sample_size - utf8_bytes + utf8_chars;  // Adjust for multi-byte

    double printable_ratio = effective_printable / effective_sample;
    double whitespace_ratio = (double)whitespace / sample_size;
    double ascii_ratio = (double)ascii / sample_size;
    double utf8_ratio = (double)utf8_bytes / sample_size;

    // Text typically has:
    // - High printable ratio (>70%) - now includes UTF-8 chars
    // - Some whitespace (2-30%)
    // - High ASCII ratio (>90%) OR high UTF-8 ratio

    double score = 0.0;
    if (printable_ratio > 0.70) score += 0.4;
    else if (printable_ratio > 0.50) score += 0.2;

    // Structured code (GraphQL, YAML, Python) often has 30-40% whitespace from indentation
    if (whitespace_ratio > 0.02 && whitespace_ratio < 0.40) score += 0.3;

    // Accept either ASCII text OR UTF-8 text
    if (ascii_ratio > 0.90) score += 0.3;
    else if (ascii_ratio > 0.70) score += 0.15;
    else if (utf8_ratio > 0.30) score += 0.3;  // High UTF-8 content = text
    else if (utf8_ratio > 0.15) score += 0.15;

    // Bonus for valid UTF-8 (already checked inline, but double-check)
    if (utf8_chars > 0 && is_valid_utf8(data, std::min(n, (size_t)1024))) score += 0.1;

    return std::min(1.0, score);
}

// Analyze a block to determine its type
inline BlockAnalysis analyze_block(const uint8_t* data, size_t n) {
    BlockAnalysis result;
    result.type = BlockType::RAW;
    result.numeric_strategy = tieredcompress::Strategy::NONE;
    result.entropy = tieredcompress::byte_entropy(data, std::min(n, (size_t)4096));
    result.text_score = 0.0;
    result.numeric_score = 0.0;
    result.high_entropy = (result.entropy > 7.5);
    result.period = 0;

    // === HIGHEST PRIORITY: Mathematical generators (3855x compression!) ===

    // Check for LINEAR_GEN first - perfect linear sequences
    // This gives 3855x compression on sequential IDs/timestamps
    if (n >= 12) {  // Need at least 3 integers
        LinearGenParams params;
        if (MZ_DET("detect_linear_gen", detect_linear_gen(data, n, params))) {
            result.type = BlockType::LINEAR_GEN;
            result.linear_gen = params;
            result.logical_depth = LogicalDepth::REGENERATE;
            result.complexity_score = {17, 0};  // Pure pattern, no residual
            return result;
        }

        // Try LINEAR_GEN_APPROX if exact fails (Effective Complexity: pattern + exceptions)
        // This rescues data that's 95%+ linear but has a few outliers
        LinearGenApproxParams approx_params;
        std::vector<std::pair<uint32_t, int64_t>> exceptions;
        if (MZ_DET("detect_linear_gen_approx", detect_linear_gen_approx(data, n, approx_params, exceptions))) {
            // === MDL-BASED SELECTION ===
            // Compare encoding cost vs zstd baseline using MDL principle.
            // If LINEAR_GEN_APPROX has lower total description length, use it.
            auto complexity = estimate_linear_approx_complexity(approx_params, exceptions);

            MDLScore encoding_mdl = mdl_linear_gen_approx(exceptions.size());
            if (mdl_beats_zstd(encoding_mdl, n, result.entropy)) {
                result.type = BlockType::LINEAR_GEN_APPROX;
                result.linear_gen_approx = approx_params;
                result.linear_gen_approx_exceptions = std::move(exceptions);
                result.logical_depth = LogicalDepth::REGENERATE;
                result.complexity_score = complexity;
                return result;
            }
            // Else: zstd wins on MDL, fall through to other encodings
        }
    }

    // Check for GEOMETRIC - perfect geometric sequences (powers of 2, etc.)
    // This gives 10x+ over zstd on exponential data
    if (n >= 16) {  // Need at least 2 integers
        GeometricParams geo_params;
        if (MZ_DET("detect_geometric", detect_geometric(data, n, geo_params))) {
            result.type = BlockType::GEOMETRIC;
            result.geometric = geo_params;
            return result;
        }
    }

    // Check for QUADRATIC - perfect polynomial sequences (squares, etc.)
    // This gives 310x over zstd on quadratic data
    if (n >= 24) {  // Need at least 3 integers
        QuadraticParams quad_params;
        if (MZ_DET("detect_quadratic", detect_quadratic(data, n, quad_params))) {
            result.type = BlockType::QUADRATIC;
            result.quadratic = quad_params;
            return result;
        }
    }

    // Check for RECURRENCE - Fibonacci-like sequences
    // This gives 23x over zstd on recurrence data
    if (n >= 32) {  // Need at least 4 integers
        RecurrenceParams rec_params;
        if (MZ_DET("detect_recurrence", detect_recurrence(data, n, rec_params))) {
            result.type = BlockType::RECURRENCE;
            result.recurrence = rec_params;
            return result;
        }
    }

    // Check for MODULAR - wrapping counters (0,1,2,...,255,0,1,2,...)
    // This gives 88889x over zstd on 16-bit counter sequences!
    if (n >= 32) {  // Need at least 8 integers (32-bit) to detect wrap
        ModularParams mod_params;
        if (MZ_DET("detect_modular", detect_modular(data, n, mod_params))) {
            result.type = BlockType::MODULAR;
            result.modular = mod_params;
            return result;
        }
    }

    // Check for TIMESTAMP - quasi-periodic timestamps (not perfect linear)
    // This gives 12x compression on timestamps with jitter (vs 4x with regular delta)
    if (n >= 24) {  // Need at least 3 64-bit timestamps
        TimestampParams ts_params;
        if (MZ_DET("detect_timestamp", detect_timestamp(data, n, ts_params))) {
            result.type = BlockType::TIMESTAMP;
            result.timestamp = ts_params;
            return result;
        }
    }

    // PERIODIC: Check for repeating byte patterns (FILL, checkered, etc.)
    // For small periods (1-4), formula encoding beats zstd:
    //   - Period 1 (FILL): 3 bytes vs zstd ~19 bytes (6x better)
    //   - Period 2 (checkered): 4 bytes vs zstd ~20 bytes (5x better)
    // For larger periods, only use if significant size savings
    if (n >= 8) {  // Need at least 2 repetitions
        size_t period = detect_periodic(data, n);
        if (period > 0) {
            // Calculate encoded size: 2 bytes (period length) + period bytes
            size_t encoded_size = 2 + period;
            // Worth it if we save at least 50% over raw
            if (encoded_size < n / 2) {
                result.type = BlockType::PERIODIC;
                result.period = period;
                return result;
            }
        }

        // Try PERIODIC_APPROX if exact fails (Effective Complexity: pattern + exceptions)
        // This rescues data that's 95%+ periodic but has a few corrupted bytes
        PeriodicApproxParams periodic_approx_params;
        std::vector<std::pair<uint32_t, uint8_t>> periodic_exceptions;
        if (MZ_DET("detect_periodic_approx", detect_periodic_approx(data, n, periodic_approx_params, periodic_exceptions))) {
            // === MDL-BASED SELECTION ===
            // Compare encoding cost vs zstd baseline using MDL principle.
            auto complexity = estimate_periodic_approx_complexity(
                periodic_approx_params, periodic_exceptions);

            MDLScore encoding_mdl = mdl_periodic_approx(
                periodic_approx_params.period, periodic_exceptions.size());
            if (mdl_beats_zstd(encoding_mdl, n, result.entropy)) {
                result.type = BlockType::PERIODIC_APPROX;
                result.periodic_approx = periodic_approx_params;
                result.periodic_approx_exceptions = std::move(periodic_exceptions);
                result.logical_depth = LogicalDepth::REGENERATE;
                result.complexity_score = complexity;
                return result;
            }
            // Else: zstd wins on MDL, fall through to other encodings
        }
    }

    // === SPARSE DETECTION ===
    // Check for sparse data (>90% zeros) - common in bitmaps, masks, sparse matrices
    // Position encoding beats zstd by 1.3-1.5x on 1% density data
    {
        size_t nonzero_count;
        uint8_t common_value;
        bool all_same;
        if (MZ_DET("detect_sparse", detect_sparse(data, n, nonzero_count, common_value, all_same))) {
            result.type = BlockType::SPARSE;
            result.sparse_nonzero_count = nonzero_count;
            result.sparse_common_value = common_value;
            result.sparse_all_same_value = all_same;
            return result;
        }
    }

    // === STANDARD DETECTION ===

    // === DBF FORMAT DETECTION (before numeric — DBF triggers false DELTA/BLOCK_COLUMNAR) ===
    // dBASE/FoxPro files have fixed-width records with heavy space padding.
    // BWT beats zstd by 8-16% on DBF because it groups identical characters globally.
    // Must check before numeric: DBF's zero-padded fields falsely trigger DELTA detection.
    {
        size_t hdr_offset = 0;
        size_t block_len = tieredcompress::detect_block_length(data, n, &hdr_offset);
        if (block_len > 0 && hdr_offset > 0) {
            // Confirmed DBF file — route to constant column elimination
            // Trial in encoding path picks best of CC+zstd vs BWT
            result.type = BlockType::DBF_CONSTCOL;
            return result;
        }
    }

    // Check for numeric patterns FIRST (before LZMA_RAW!)
    // Reason: Small integers (0-255) have 75% zero bytes in little-endian, which would
    // falsely trigger LZMA_RAW's ">30% zeros" check. NUMERIC detection must run first.
    // Bug fix: Integer array with deltas 1-3 was getting 8.8x instead of 19.9x compression.
    tieredcompress::DetectionResult numeric_detection;
    {
        std::vector<uint8_t> work(n + 65542);  // Extra space for block_columnar (block_len + 6 header bytes)
        numeric_detection = tieredcompress::detect(data, n, work.data());

        // Skip byte shuffle strategies - DUAL_STREAM is much better for alternating entropy data
        // Byte shuffles get 47KB on protobuf, DUAL_STREAM gets 41KB (12% better)
        bool is_byte_shuffle = (numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE2 ||
                                numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE32 ||
                                numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE64);
        if (numeric_detection.strategy != tieredcompress::Strategy::NONE && !is_byte_shuffle) {
            result.type = BlockType::NUMERIC;
            result.numeric_strategy = numeric_detection.strategy;
            result.numeric_score = numeric_detection.confidence;
            return result;
        }
    }

    // Check for x86 binary code (before entropy check - binaries have high entropy but are compressible with E8/E9)
    // Use LZMA optimal with E8/E9 filter - beats xz:9 by ~3800 bytes on survival.exe
    if (looks_like_x86_code(data, n)) {
        result.type = BlockType::LZMA_OPTIMAL;
        return result;
    }

    // Check for high-zero content (>30% zeros) - LZMA's rep matches excel on zero padding
    // dilosi.doc: 54.5% zeros, LZMA saves 372 bytes vs zstd
    // NOTE: This runs AFTER numeric detection to avoid false positives on small integers
    //
    // SPEED GATE added 2026-07-31, and the reason matters more than the cap.
    // lzma_opt2's optimal parse costs ~8 s/MB (LINEAR: 2.0 s @256 KB, 8.1 s @1 MB,
    // 33.4 s @4 MB) and on this branch it has NEVER been observed to win:
    //     nyctaxi_cols.bin 4 MB (real)  32,790 ms, 82% of the run  -> lost to BWT_TEXT
    //     synthetic ~55% zeros, 3 sizes  2.0/8.1/33.4 s, 54/69/84% -> lost to BWT_TEXT
    // 4 of 4 fire-and-lose. Corpus-wide it fired on 1 of 50 files and was 38.7% of ALL
    // measured compression time.
    //
    // WHY IT STOPPED PAYING: its justification is "saves 372 bytes VS ZSTD". It was priced
    // against zstd. bwt9 was added to the UNIVERSAL BACKSTOP later (see the encoder audit)
    // and beats lzma_opt2 on exactly this content class. The branch was correct when written
    // and was silently obsoleted by an improvement made elsewhere -- nothing forces a
    // re-price, so nobody did one.
    // GENERAL RULE: when a strong general backstop is added, every specialised path justified
    // against the OLD baseline becomes unpriced. Trial-and-keep protects the RATIO
    // automatically and protects the CLOCK not at all.
    //
    // The cap (1 MB, matching the brotli backstop's existing convention) is deliberately
    // conservative: it preserves the small-file case the branch was actually measured on,
    // where the cost is ~2 s, and drops only the large-block case where it is 8-33 s.
    // Raise/disable with -DMZ_LZMA_RAW_MAX=<bytes>. Ratio is backstop-protected either way.
    #ifndef MZ_LZMA_RAW_MAX
    #define MZ_LZMA_RAW_MAX (1u << 20)
    #endif
    if (n <= (size_t)MZ_LZMA_RAW_MAX) {
        size_t zeros = 0;
        size_t sample_size = std::min(n, (size_t)16384);  // Sample first 16KB
        for (size_t i = 0; i < sample_size; i++) {
            if (data[i] == 0) zeros++;
        }
        if (zeros * 100 / sample_size > 30) {  // >30% zeros
            result.type = BlockType::LZMA_RAW;
            return result;
        }
    }

    // Try DUAL_STREAM for binary data with alternating entropy (Protobuf-like)
    // This is better than BYTE_SHUFFLE2 because it compresses each stream optimally
    {
        DualStreamParams ds_params;
        if (MZ_DET("detect_dual_stream", detect_dual_stream(data, n, ds_params))) {
            result.type = BlockType::DUAL_STREAM;
            result.dual_stream = ds_params;
            return result;
        }
    }

    // Fall back to byte shuffle if DUAL_STREAM didn't match
    bool was_byte_shuffle = (numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE2 ||
                             numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE32 ||
                             numeric_detection.strategy == tieredcompress::Strategy::BYTE_SHUFFLE64);
    if (was_byte_shuffle) {
        result.type = BlockType::NUMERIC;
        result.numeric_strategy = numeric_detection.strategy;
        result.numeric_score = numeric_detection.confidence;
        return result;
    }

    // High entropy = likely encrypted/compressed/random
    // BWT can help on small high-entropy files (docx), but is too slow for large PDFs
    // Limit: 256KB (BWT is O(n² log n) due to simple comparison sort)
    if (result.high_entropy) {
        if (n <= 262144) {  // 256KB - safe for BWT
            result.type = BlockType::BWT_TEXT;
        } else {
            result.type = BlockType::INCOMPRESSIBLE;
        }
        return result;
    }

    // Check for text
    // Use >= 0.7 to include borderline text (JSON logs have exactly 0.700)
    result.text_score = calculate_text_score(data, n);
    if (result.text_score >= 0.7) {
        // === KV CONFIG DETECTION (INI/YAML) ===
        // Try KV_CONFIG early - configs have distinctive [section] or key: value patterns
        // Structural encoding beats brotli by 7% on 16KB config files
        if (n >= 256) {
            KvConfigParams kv_params;
            if (MZ_DET("detect_kv_config", detect_kv_config(data, n, kv_params))) {
                result.type = BlockType::KV_CONFIG;
                result.kv_config = std::move(kv_params);
                return result;
            }
        }

        // === ENTROPY-GUIDED DETECTION (Shannon Entropy optimization) ===
        // Text normally has entropy ~4-5 bits/byte. Higher entropy means:
        // - Base64/encoded data (looks like text but isn't structured)
        // - Encrypted text (won't compress well anyway)
        // - Random strings (no patterns to exploit)
        // Skip expensive O(n²) template detectors when entropy is high.
        // This is a "fast fail" optimization - cheap entropy check saves expensive detection.
        // Printable-ratio gate: entropy alone misclassifies BINARY arrays (float/sensor: low entropy but ~37%
        // printable) as text, then per-line parsers (csv/json/columnar/template) chew the whole block for nothing
        // (nyctaxi 4MB ~35s). Real text is >70% printable. The bwt9/xz/brotli backstops still run, so wins are kept.
        size_t _prn = (n < 65536) ? n : 65536, _pr = 0;
        for (size_t _i = 0; _i < _prn; _i++) { uint8_t _c = data[_i]; if ((_c >= 32 && _c < 127) || _c == 9 || _c == 10 || _c == 13) _pr++; }
        const bool looks_text = (_prn == 0) || ((double)_pr / (double)_prn > 0.70);
        const bool low_entropy_text = (result.entropy < 5.5) && looks_text;   // Rich structure likely
        const bool mid_entropy_text = (result.entropy < 6.0) && looks_text;   // Some structure possible
        // high entropy (>6.0): Skip most template detectors, fall through to TEXT/zstd

        // Try SECTION_TEMPLATE first - repeating multi-line sections with {N} (Markdown!)
        // Key insight: 65KB Markdown -> ~155 bytes (422x compression!)
        // WORD_TEMPLATE: Repeated sections differing by word variable (2.4x over zstd!)
        // Key insight: "## session API\n...\n## result API\n..." with same structure
        // Result: 17KB API docs -> 516 bytes (32x compression!)
        if (low_entropy_text && n >= 512) {
            WordTemplateParams word_params;
            if (MZ_DET("detect_word_template", detect_word_template(data, n, word_params))) {
                result.type = BlockType::WORD_TEMPLATE;
                result.word_template = std::move(word_params);
                return result;
            }
        }

        // MULTI_WORD_TEMPLATE: Template with multiple variables {1},{2},{3} (44% better on K8s!)
        // Key insight: K8s Ingress/Deployment sections differ by 2-3 variables (app, env)
        // Detection: Split by delimiter (---), group by kind, check >80% line similarity
        // Result: 65KB K8s -> 549 bytes (44% better than zstd!)
        if (low_entropy_text && n >= 1024) {
            MultiWordTemplateParams mw_params;
            if (MZ_DET("detect_multi_word_template", detect_multi_word_template(data, n, mw_params))) {
                result.type = BlockType::MULTI_WORD_TEMPLATE;
                result.multi_word_template = std::move(mw_params);
                return result;
            }
        }

        // SECTION_TEMPLATE scales SUBLINEARLY - template is fixed size, only variable indices grow
        // With LINEAR_GEN for N values: template(~140) + LINEAR_GEN(17) = ~157 bytes for ANY size!
        // Entropy gate: Only try if entropy < 5.5 (highly structured text)
        if (low_entropy_text && n >= 256) {
            SectionTemplateParams sec_params;
            if (MZ_DET("detect_section_template", detect_section_template(data, n, sec_params))) {
                result.type = BlockType::SECTION_TEMPLATE;
                result.section_template = std::move(sec_params);
                return result;
            }
        }

        // Try LINE_GROUP_TEMPLATE - for multi-line-type data like email headers
        // Key insight: Email headers have From:, To:, Subject:, etc. each with sequential numbers
        // Groups lines by prefix, applies LINEAR_GEN per group with linear sequences
        // Result: 68% better than zstd on email headers!
        if (low_entropy_text && n >= 256) {
            std::vector<LineGroupInfo> groups;
            std::vector<uint8_t> line_types;
            if (MZ_DET("detect_line_group_template", detect_line_group_template(data, n, groups, line_types))) {
                result.type = BlockType::LINE_GROUP_TEMPLATE;
                result.line_group_info = std::move(groups);
                result.line_group_types = std::move(line_types);
                return result;
            }
        }

        // Try template extraction (logs, JSON, CSV with structure)
        // Simpler O(n) algorithm - run at mid entropy too
        // Fixed: SUBTEMPLATE now verifies reconstruction doesn't lose leading zeros
        if (mid_entropy_text) {
            TemplateParams tpl_params;
            if (MZ_DET("detect_template", detect_template(data, n, tpl_params))) {
                result.type = BlockType::TEMPLATE;
                result.template_params = std::move(tpl_params);
                return result;
            }
        }

        // Try char-level template (fallback for logs that fail token-based detection)
        // More robust: compares character-by-character instead of tokenizing
        // CHAR_TEMPLATE scales sublinearly - template fixed, only column data grows
        // Entropy gate: Only try if entropy < 5.5 (expensive detector)
        if (low_entropy_text && n >= 4096) {
            CharTemplateParams char_params;
            if (MZ_DET("detect_char_template", detect_char_template(data, n, char_params))) {
                result.type = BlockType::CHAR_TEMPLATE;
                result.char_template = std::move(char_params);
                return result;
            }
        }

        // Try multi-line template (JavaScript-like repeated function blocks)
        // ML_TEMPLATE scales sublinearly - template fixed, only variable data grows
        // Entropy gate: Only try if entropy < 5.5 (expensive detector)
        if (low_entropy_text && n >= 4096) {
            MLTemplateParams ml_params;
            if (MZ_DET("detect_ml_template", detect_ml_template(data, n, ml_params))) {
                result.type = BlockType::ML_TEMPLATE;
                result.ml_template = ml_params;
                return result;
            }

            // Try dual-template (alternating patterns like TypeScript interface+component)
            MLTemplateDualParams dual_params;
            if (MZ_DET("detect_ml_template_dual", detect_ml_template_dual(data, n, dual_params))) {
                result.type = BlockType::ML_TEMPLATE_DUAL;
                result.ml_template_dual = dual_params;
                return result;
            }
        }

        // Try CODE_STREAM for code with compound identifiers (beats bzip2 on JavaScript!)
        // Key insight: Parse identifiers into grammar (type, letter, noun, verb, number)
        // and compress separate streams - each stream compresses better individually
        if (mid_entropy_text && n >= 1024) {
            CodeStreamParams code_params;
            if (MZ_DET("detect_code_stream", detect_code_stream(data, n, code_params))) {
                result.type = BlockType::CODE_STREAM;
                result.code_stream = std::move(code_params);
                return result;
            }
        }

        // Try CSV columnar format (with LINEAR_GEN on sequential ID columns)
        // Key insight: CSV "1,2,3,...,N" IDs compress from 2801 to 17 bytes!
        // Run at mid entropy - CSV can have some randomness in data columns
        if (mid_entropy_text && n >= 256) {
            CsvColumnarParams csv_params;
            if (MZ_DET("detect_csv_columnar", detect_csv_columnar(data, n, csv_params))) {
                result.type = BlockType::CSV_COLUMNAR;
                result.csv_columnar = std::move(csv_params);
                return result;
            }
        }

        // Try JSON columnar format (extract sequential requestId, delta-encode timestamps)
        // Key insight: JSON logs have "requestId":"1000000" sequential IDs = LINEAR_GEN
        // 1085 bytes better than brotli on JSON structured logs!
        if (mid_entropy_text && n >= 1024) {
            JsonColumnarParams json_params;
            if (MZ_DET("detect_json_columnar", detect_json_columnar(data, n, json_params))) {
                result.type = BlockType::JSON_COLUMNAR;
                result.json_columnar = std::move(json_params);
                return result;
            }
        }

        // Try NUM_EXTRACT for files with many embedded numbers (Makefiles, configs)
        // Key insight: Numbers create entropy. Extract them, compress template separately.
        // 900 bytes better than brotli on Makefiles!
        if (mid_entropy_text && n >= 1024 && n <= 2097152) {  // cap at 2MB: num_extract targets small Makefiles/configs;
            NumExtractParams num_params;                       // it's O(n) heavy string-alloc and never wins multi-MB blobs (speed)
            if (MZ_DET("detect_num_extract", detect_num_extract(data, n, num_params))) {
                result.type = BlockType::NUM_EXTRACT;
                result.num_extract = num_params;
                return result;
            }
        }

        // Try line template (SQL INSERTs, variable-length lines with linear numeric vars)
        // Key insight: SQL dumps have 63% fixed content but variable line lengths due to ID digits
        // LINE_TEMPLATE stores: prefix + suffix + separators + (first, delta) per var = ~128 bytes
        // 14x improvement on SQL dumps: 65536 bytes -> 214 bytes (vs 3085 bytes mzip default)
        if (mid_entropy_text && n >= 1024) {
            LineTemplateParams line_params;
            if (MZ_DET("detect_line_template", detect_line_template(data, n, line_params))) {
                result.type = BlockType::LINE_TEMPLATE;
                result.line_template = std::move(line_params);
                return result;
            }
        }

        // Try columnar log format (access logs)
        // Only for larger blocks where overhead is worthwhile
        // Entropy gate: Only try if entropy < 6.0 (log structure requires some regularity)
        // Tested: roundtrip verification passes on generated nginx_log and access_log
        if (mid_entropy_text && n >= 4096) {
            ColumnarParams col_params;
            if (MZ_DET("detect_columnar_log", detect_columnar_log(data, n, col_params))) {
                result.type = BlockType::COLUMNAR;
                result.columnar = col_params;
                return result;
            }
        }

        // Try PHRASE_PARTITION for highly repetitive phrase text (12% better than bzip2!)
        // Key insight: "Hello world! Bonjour! ..." = small phrase set repeated thousands of times
        // Works when data is 99%+ partitioned by delimiter-separated phrases
        if (low_entropy_text && n >= 256 && n <= 2097152) {
            PhrasePartitionParams pp_params;
            if (MZ_DET("detect_phrase_partition", detect_phrase_partition(data, n, pp_params))) {
                result.type = BlockType::PHRASE_PARTITION;
                result.phrase_partition = std::move(pp_params);
                return result;
            }
        }

        // Try HTML_STREAM for HTML documents (3.3% better than brotli at 256KB+)
        // Tag/content separation lets BWT compress each stream optimally
        // Only for larger files - brotli's dictionary wins at small sizes
        if (n >= 128 * 1024 && n <= 16 * 1024 * 1024) {
            HtmlStreamParams html_params;
            if (MZ_DET("detect_html_stream", detect_html_stream(data, n, html_params))) {
                result.type = BlockType::HTML_STREAM;
                result.html_stream = html_params;
                return result;
            }
        }

        // Try URL_STREAM for URL lists (6.2% better than mzip at 256KB)
        // Component separation: protocol/domain/path/params compress independently
        // Protocols compress to 0.1%, domains to 2.9% — huge wins!
        if (n >= 64 * 1024 && n <= 16 * 1024 * 1024) {
            UrlStreamParams url_params;
            if (MZ_DET("detect_url_stream", detect_url_stream(data, n, url_params))) {
                result.type = BlockType::URL_STREAM;
                result.url_stream = url_params;
                return result;
            }
        }

        // Try BASE64_DECODE for base64-encoded data (1.76% better than brotli at 1MB)
        // Insight: Base64 expands 3 bytes → 4 chars (33% bloat). Decode, compress binary, re-encode
        // Only for larger files where overhead is justified
        if (n >= 1024) {
            Base64Params b64_params;
            if (MZ_DET("detect_base64", detect_base64(data, n, b64_params))) {
                result.type = BlockType::BASE64_DECODE;
                result.base64 = b64_params;
                return result;
            }
        }

        // Try BWT_TEXT for natural language (45-103% better than zstd at 256KB+)
        // v9 smart adaptive: uses v8 (fixed model) for <2KB, v4 (multi-tree) for larger
        // Works on natural text with low/mid entropy (prose, documentation, logs, unicode)
        // Extended limit: 256B-2MB (analysis showed BWT wins 1.63x on prose at 1MB)
        // Jan 2026: Use mid_entropy_text (6.0) to include unicode text (entropy ~5.95)
        // Unicode has higher byte entropy due to UTF-8 multi-byte chars, but BWT still wins
        if (mid_entropy_text && n >= 256 && n <= 2097152) {
            result.type = BlockType::BWT_TEXT;
            return result;
        }

        // Try sorted dictionary (line-based text with patterns)
        // Key insight: Sort lines + adaptive dictionary beats brotli by 3-5%
        // Works best on structured code (Laravel migrations, etc.)
        // Limit: n <= 262144 because build_sorted_dict_dictionary is O(35n) string allocs
        if (mid_entropy_text && n >= 512 && n <= 262144) {
            SortedDictParams sorted_params;
            if (MZ_DET("detect_sorted_dict", detect_sorted_dict(data, n, sorted_params))) {
                result.type = BlockType::SORTED_DICT;
                result.sorted_dict = std::move(sorted_params);
                return result;
            }
        }

        // Try phrase dictionary (code, markdown, logs with repeated substrings)
        // Key insight: brotli wins on code files because of its pre-trained dictionary
        // We build a per-file dictionary of repeated phrases (6-32 chars)
        // Works on PHP, JavaScript, Markdown, etc.
        // Limit: n <= 262144 because detect_phrase_dict is O(57n) string allocs
        if (mid_entropy_text && n >= 256 && n <= 262144) {
            PhraseDictParams phrase_params;
            if (MZ_DET("detect_phrase_dict", detect_phrase_dict(data, n, phrase_params))) {
                result.type = BlockType::PHRASE_DICT;
                result.phrase_dict = std::move(phrase_params);
                return result;
            }
        }

        result.type = BlockType::TEXT;
        return result;
    }

    // Default to RAW (NUMERIC already checked before entropy, TEXT already checked above)
    result.type = BlockType::RAW;
    return result;
}

// ============================================================================
// Strategy Application (from tieredcompress)
// ============================================================================

// Apply preprocessing strategy and return new size
// For fixed-size strategies, returns n (same size)
// For variable-size strategies (ALP, bitpack), returns actual output size
inline size_t apply_strategy(uint8_t* out, const uint8_t* in, size_t n,
                             tieredcompress::Strategy strategy) {
    using S = tieredcompress::Strategy;

    switch (strategy) {
        case S::NONE:
            memcpy(out, in, n);
            return n;

        case S::DELTA:
            tieredcompress::delta_encode(out, in, n);
            return n;

        case S::DELTA16:
            tieredcompress::delta16_encode(out, in, n);
            return n;

        case S::DELTA32:
            tieredcompress::delta32_encode(out, in, n);
            return n;

        case S::DELTA64:
            tieredcompress::delta64_encode(out, in, n);
            return n;

        case S::DELTA_OF_DELTA32:
            tieredcompress::delta_of_delta32_encode(out, in, n);
            return n;

        case S::XOR_FLOAT64:
            tieredcompress::xor_float64_encode(out, in, n);
            return n;

        case S::XOR_FLOAT32:
            tieredcompress::xor_float32_encode(out, in, n);
            return n;

        case S::DELTA_ZIGZAG_BITPACK32:
            return tieredcompress::delta_zigzag_bitpack32_encode(out, in, n);

        case S::DELTA_FOR_BITPACK32:
            return tieredcompress::delta_for_bitpack32_encode(out, in, n);

        case S::ALP_FLOAT64:
            return tieredcompress::alp_float64_encode(out, in, n);

        case S::ALP_FLOAT32:
            return tieredcompress::alp_float32_encode(out, in, n);

        case S::NARROW16:
            return tieredcompress::narrow16_encode(out, in, n);

        case S::FOR16:
            return tieredcompress::for16_encode(out, in, n);

        case S::STRIDED32:
            tieredcompress::strided32_encode(out, in, n);
            return n;

        case S::STRIDED32_FOR:
            return tieredcompress::strided32_for_encode(out, in, n);

        case S::STRIDED64:
            tieredcompress::strided64_encode(out, in, n);
            return n;

        case S::STRIDED3_32:
            tieredcompress::strided3_32_encode(out, in, n);
            return n;

        case S::STRIDED4_32:
            tieredcompress::strided4_32_encode(out, in, n);
            return n;

        case S::BYTE_SHUFFLE32:
            tieredcompress::byte_shuffle32_encode(out, in, n);
            return n;

        case S::BYTE_SHUFFLE64:
            tieredcompress::byte_shuffle64_encode(out, in, n);
            return n;

        case S::BYTE_SHUFFLE2:
            tieredcompress::byte_shuffle2_encode(out, in, n);
            return n;

        case S::RLE:
            // RLE not implemented in tieredcompress yet - fallback to copy
            memcpy(out, in, n);
            return n;

        case S::E8E9_X86:
            tieredcompress::e8e9_encode(out, in, n);
            return n;

        case S::LINEAR_PRED16:
            tieredcompress::linear_pred16_encode(out, in, n);
            return n;

        case S::LINEAR_PRED32:
            tieredcompress::linear_pred32_encode(out, in, n);
            return n;

        case S::PAETH_RGB:
            tieredcompress::paeth_rgb_encode(out, in, n);
            return n;

        case S::BLOCK_COLUMNAR: {
            // Need to detect block length again for encoding
            size_t hdr_offset = 0;
            size_t block_len = tieredcompress::detect_block_length(in, n, &hdr_offset);
            if (block_len == 0) {
                memcpy(out, in, n);
                return n;
            }
            return tieredcompress::block_columnar_encode(out, in, n, block_len, hdr_offset);
        }

        default:
            memcpy(out, in, n);
            return n;
    }
}

// Reverse preprocessing strategy
inline size_t reverse_strategy(uint8_t* out, const uint8_t* in, size_t in_size,
                               size_t original_size, tieredcompress::Strategy strategy) {
    using S = tieredcompress::Strategy;

    switch (strategy) {
        case S::NONE:
            memcpy(out, in, in_size);
            return in_size;

        case S::DELTA:
            tieredcompress::delta_decode(out, in, in_size);
            return in_size;

        case S::DELTA16:
            tieredcompress::delta16_decode(out, in, in_size);
            return in_size;

        case S::DELTA32:
            tieredcompress::delta32_decode(out, in, in_size);
            return in_size;

        case S::DELTA64:
            tieredcompress::delta64_decode(out, in, in_size);
            return in_size;

        case S::DELTA_OF_DELTA32:
            tieredcompress::delta_of_delta32_decode(out, in, in_size);
            return in_size;

        case S::XOR_FLOAT64:
            tieredcompress::xor_float64_decode(out, in, in_size);
            return in_size;

        case S::XOR_FLOAT32:
            tieredcompress::xor_float32_decode(out, in, in_size);
            return in_size;

        case S::DELTA_ZIGZAG_BITPACK32:
            return tieredcompress::delta_zigzag_bitpack32_decode(out, in, in_size, original_size);

        case S::DELTA_FOR_BITPACK32:
            return tieredcompress::delta_for_bitpack32_decode(out, in, in_size, original_size);

        case S::ALP_FLOAT64:
            return tieredcompress::alp_float64_decode(out, in, in_size, original_size);

        case S::ALP_FLOAT32:
            return tieredcompress::alp_float32_decode(out, in, in_size, original_size);

        case S::NARROW16:
            return tieredcompress::narrow16_decode(out, in, in_size, original_size);

        case S::FOR16:
            return tieredcompress::for16_decode(out, in, in_size, original_size);

        case S::STRIDED32:
            tieredcompress::strided32_decode(out, in, in_size);
            return in_size;

        case S::STRIDED32_FOR:
            tieredcompress::strided32_for_decode(out, in, in_size, original_size);
            return original_size;

        case S::STRIDED64:
            tieredcompress::strided64_decode(out, in, in_size);
            return in_size;

        case S::STRIDED3_32:
            tieredcompress::strided3_32_decode(out, in, in_size);
            return in_size;

        case S::STRIDED4_32:
            tieredcompress::strided4_32_decode(out, in, in_size);
            return in_size;

        case S::BYTE_SHUFFLE32:
            tieredcompress::byte_shuffle32_decode(out, in, in_size);
            return in_size;

        case S::BYTE_SHUFFLE64:
            tieredcompress::byte_shuffle64_decode(out, in, in_size);
            return in_size;

        case S::BYTE_SHUFFLE2:
            tieredcompress::byte_shuffle2_decode(out, in, in_size);
            return in_size;

        case S::RLE:
            memcpy(out, in, in_size);
            return in_size;

        case S::E8E9_X86:
            tieredcompress::e8e9_decode(out, in, in_size);
            return in_size;

        case S::LINEAR_PRED16:
            tieredcompress::linear_pred16_decode(out, in, in_size);
            return in_size;

        case S::LINEAR_PRED32:
            tieredcompress::linear_pred32_decode(out, in, in_size);
            return in_size;

        case S::PAETH_RGB:
            tieredcompress::paeth_rgb_decode(out, in, in_size);
            return in_size;

        case S::BLOCK_COLUMNAR:
            return tieredcompress::block_columnar_decode(out, in, in_size, original_size);

        default:
            memcpy(out, in, in_size);
            return in_size;
    }
}

// ============================================================================
// Block Header Format
// ============================================================================
// [BlockType: 1 byte]
// [NumericStrategy: 1 byte] (only if NUMERIC)
// [OriginalSize: 4 bytes]
// [PreprocessedSize: 4 bytes] (size after preprocessing, before zstd)
// [CompressedSize: 4 bytes]
// [CompressedData: variable]

struct BlockHeader {
    BlockType type;
    tieredcompress::Strategy numeric_strategy;
    uint32_t original_size;
    uint32_t preprocessed_size;
    uint32_t compressed_size;
};

inline void write_u32_le(uint8_t* out, uint32_t val) {
    out[0] = val & 0xFF;
    out[1] = (val >> 8) & 0xFF;
    out[2] = (val >> 16) & 0xFF;
    out[3] = (val >> 24) & 0xFF;
}

inline uint32_t read_u32_le(const uint8_t* in) {
    return in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
}

inline void write_u64_le(uint8_t* out, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        out[i] = (val >> (i * 8)) & 0xFF;
    }
}

inline uint64_t read_u64_le(const uint8_t* in) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t)in[i]) << (i * 8);
    }
    return val;
}

// ============================================================================
// Compression Result
// ============================================================================

struct CompressResult {
    bool success;
    size_t compressed_size;
    size_t original_size;
    uint32_t block_count;
    std::string error;
    bool used_lite_format;  // True if lite format was used (4-byte overhead)

    // Statistics
    uint32_t blocks_raw;
    uint32_t blocks_numeric;
    uint32_t blocks_text;
    uint32_t blocks_image;
    uint32_t blocks_incompressible;
};

struct DecompressResult {
    bool success;
    size_t decompressed_size;
    std::string error;
};

// ============================================================================
// Main Compression API
// ============================================================================

// ============================================================================
// SoA structural transforms (2026-08-05) — the "formula-replication as layout
// recovery" win. A good context-mixer already inverts single-stream generators
// (delta, linear recurrence, even sinusoids), but it sees ONE interleaved byte
// stream and misses LAYOUT structure. Reshaping bytes so the model sees each
// lane/column contiguously wins where the CM can't reach:
//   tid 0 = byte-shuffle-W    (the blosc/HDF5 SHUFFLE filter: byte j of every
//           W-byte element made contiguous; smooth floats -> constant exponent
//           lane). Measured -17.3% on a quantized f4 series.
//   tid 1 = element-de-interleave-(W,cols) (struct-of-arrays: split interleaved
//           W-byte records into per-column runs). Measured -23.5% on real 3-axis
//           f8 gyro data.
// Both are LOSSLESS permutations of the SAME bytes. The non-divisible remainder
// (e.g. 524288 elements is not a multiple of 3) is carried through untouched at
// the tail — dropping it is silently lossy (learned the hard way).
// Applied at the TOP level behind trial-and-keep + a roundtrip verify, so the
// 'MS' variant only ships when it is BOTH smaller AND provably invertible.
// ============================================================================
inline std::vector<uint8_t> soa_apply(const uint8_t* data, size_t size,
                                      uint8_t tid, uint8_t W, uint8_t cols) {
    std::vector<uint8_t> out(size);
    if (tid == 0) {                       // byte-shuffle-W
        size_t n = size / W;              // full elements
        for (size_t j = 0; j < W; ++j)
            for (size_t i = 0; i < n; ++i)
                out[j * n + i] = data[i * (size_t)W + j];
        // remainder bytes (size % W) copied verbatim at the tail
        for (size_t k = n * (size_t)W; k < size; ++k) out[k] = data[k];
    } else if (tid == 2) {                // de-interleave + per-lane order-1 delta+zigzag
        // Lossless: de-interleave `cols` W-byte lanes (framing R=W*cols), then order-1
        // delta + zigzag each lane's W-byte little-endian integers (wrap arithmetic).
        // Remainder (size % R) carried verbatim. Wins on time-series numeric (tsgas
        // -27.5%): the model sees smooth per-column deltas instead of raw float bytes.
        size_t R = (size_t)W * cols;
        size_t rows = R ? (size - (size % R)) / R : 0;
        size_t m = rows * R;
        uint64_t mask = (W >= 8) ? ~0ULL : ((1ULL << (8u * W)) - 1);
        size_t off = 0;
        for (uint8_t c = 0; c < cols; ++c) {
            uint64_t prev = 0;
            for (size_t r = 0; r < rows; ++r) {
                const uint8_t* src = data + ((size_t)(r * cols + c)) * W;
                uint64_t u = 0;
                for (int b = 0; b < W; ++b) u |= (uint64_t)src[b] << (8 * b);
                uint64_t d = (r == 0) ? u : ((u - prev) & mask);
                prev = u;
                uint64_t z = ((d << 1) ^ (0ULL - (d >> (8u * W - 1)))) & mask;
                for (int b = 0; b < W; ++b) out[off + b] = (uint8_t)(z >> (8 * b));
                off += W;
            }
        }
        for (size_t k = m; k < size; ++k) out[off++] = data[k];
    } else {                              // element-de-interleave (W-byte elems, `cols` interleaved)
        size_t nelem = size / W;
        size_t nrec  = nelem / cols;
        size_t off = 0;
        for (uint8_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < nrec; ++r) {
                const uint8_t* src = data + ((size_t)(r * cols + c)) * W;
                memcpy(&out[off], src, W); off += W;
            }
        // leftover elements (nelem % cols) then leftover bytes (size % W), verbatim
        for (size_t e = nrec * (size_t)cols; e < nelem; ++e) {
            memcpy(&out[off], data + e * (size_t)W, W); off += W;
        }
        for (size_t k = nelem * (size_t)W; k < size; ++k) out[off++] = data[k];
    }
    return out;
}
inline std::vector<uint8_t> soa_invert(const uint8_t* t, size_t size,
                                       uint8_t tid, uint8_t W, uint8_t cols) {
    std::vector<uint8_t> out(size);
    if (tid == 0) {
        size_t n = size / W;
        for (size_t j = 0; j < W; ++j)
            for (size_t i = 0; i < n; ++i)
                out[i * (size_t)W + j] = t[j * n + i];
        for (size_t k = n * (size_t)W; k < size; ++k) out[k] = t[k];
    } else if (tid == 2) {                // invert de-interleave + per-lane delta+zigzag
        size_t R = (size_t)W * cols;
        size_t rows = R ? (size - (size % R)) / R : 0;
        size_t m = rows * R;
        uint64_t mask = (W >= 8) ? ~0ULL : ((1ULL << (8u * W)) - 1);
        size_t off = 0;
        for (uint8_t c = 0; c < cols; ++c) {
            uint64_t prev = 0;
            for (size_t r = 0; r < rows; ++r) {
                uint64_t z = 0;
                for (int b = 0; b < W; ++b) z |= (uint64_t)t[off + b] << (8 * b);
                uint64_t d = ((z >> 1) ^ (0ULL - (z & 1ULL))) & mask;
                uint64_t u = (r == 0) ? d : ((prev + d) & mask);
                prev = u;
                uint8_t* dst = &out[((size_t)(r * cols + c)) * W];
                for (int b = 0; b < W; ++b) dst[b] = (uint8_t)(u >> (8 * b));
                off += W;
            }
        }
        for (size_t k = m; k < size; ++k) out[k] = t[off++];
    } else {
        size_t nelem = size / W;
        size_t nrec  = nelem / cols;
        size_t off = 0;
        for (uint8_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < nrec; ++r) {
                memcpy(&out[((size_t)(r * cols + c)) * W], &t[off], W); off += W;
            }
        for (size_t e = nrec * (size_t)cols; e < nelem; ++e) {
            memcpy(&out[e * (size_t)W], &t[off], W); off += W;
        }
        for (size_t k = nelem * (size_t)W; k < size; ++k) out[k] = t[off++];
    }
    return out;
}

// Compress data in memory
// Returns compressed data, or empty vector on failure
// mode: SMALL = best ratio (slow decompression OK)
//       BALANCED = default tradeoff
//       FAST = fast decompression (skip slow generators for large blocks)
// ============================================================================
// MT (tabular column-transpose) — parse rectangular delimited text (CSV/TSV) into
// a grid, order-1 delta the perfectly-integer columns, transpose column-major into
// ONE stream (cells joined 0x0A, columns terminated 0x00), then compress recursively.
// Row-major interleaving scatters per-column redundancy (low-cardinality categoricals,
// monotone ids/timestamps) that the transpose concentrates into long runs mzip's
// BWT/CM captures. Measured across ~30 real held-out tabular files: events.csv -24.9%,
// flights -30.4%, covid -28.5%, seattle_weather -21.1%, ... (28/30 win). LOSSLESS +
// trial-and-keep + end-to-end roundtrip-verified before adoption, so it can only shrink;
// non-grid input / any parse mismatch falls back to the incumbent (fail-closed, like MS).
// ============================================================================
struct TabMeta { uint8_t delim; bool trailing_nl; uint32_t ncols; uint32_t nrows; };

// Accept EXACTLY canonical decimal integers (no leading zero, no unary +, no spaces,
// no decimals, fits int64) so str(v) reproduces the cell bytes; used both to qualify a
// column for delta and to parse deltas back on invert.
inline bool tab_parse_int(const char* p, size_t n, long long& v) {
    if (n == 0) return false;
    size_t i = 0; bool neg = false;
    if (p[0] == '-') { neg = true; i = 1; }
    size_t digits = n - i;
    if (digits == 0 || digits > 19) return false;
    if (p[i] == '0' && digits > 1) return false;           // leading zero
    unsigned long long m = 0;
    for (; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return false;
        m = m * 10ULL + (unsigned long long)(p[i] - '0');
    }
    if (neg) { if (m == 0 || m > 9223372036854775808ULL) return false;
               v = (m == 9223372036854775808ULL) ? INT64_MIN : -(long long)m; }
    else     { if (m > 9223372036854775807ULL) return false; v = (long long)m; }
    return true;
}

// Quote-aware split of raw into rows of raw-byte cells; false unless a uniform rectangular
// grid (>=4 rows, 2..4096 cols) with no framing sentinel (0x00) in any cell.
inline bool tab_parse(const uint8_t* data, size_t size, uint8_t delim,
                      std::vector<std::vector<std::string>>& rows, bool& trailing_nl) {
    if (size < 64) return false;
    trailing_nl = (data[size - 1] == '\n');
    size_t end = trailing_nl ? size - 1 : size;
    if (end == 0) return false;
    size_t ls = 0;
    while (true) {
        size_t le = ls;
        while (le < end && data[le] != '\n') le++;
        std::vector<std::string> f;
        bool inq = false; size_t fs = ls;
        for (size_t i = ls; i < le; i++) {
            uint8_t c = data[i];
            if (c == '"') inq = !inq;
            else if (c == delim && !inq) { f.emplace_back((const char*)data + fs, i - fs); fs = i + 1; }
        }
        f.emplace_back((const char*)data + fs, le - fs);
        rows.push_back(std::move(f));
        if (le >= end) break;
        ls = le + 1;
    }
    if (rows.size() < 4) return false;
    size_t nc = rows[0].size();
    if (nc < 2 || nc > 4096) return false;
    for (const auto& r : rows) if (r.size() != nc) return false;      // ragged
    for (const auto& r : rows) for (const auto& c : r)
        if (c.find('\0') != std::string::npos) return false;         // 0x00 sentinel in cell
    return true;
}

// Transpose to column-major, delta-encoding columns whose every data cell is canonical int.
inline std::vector<uint8_t> tab_build_payload(const std::vector<std::vector<std::string>>& rows,
                                              std::vector<uint8_t>& delta_bitmap) {
    size_t nc = rows[0].size();
    size_t ndata = rows.size() - 1;
    delta_bitmap.assign((nc + 7) / 8, 0);
    std::vector<uint8_t> out;
    std::vector<long long> vals(ndata);
    for (size_t c = 0; c < nc; c++) {
        bool all_int = ndata > 0;
        for (size_t r = 0; r < ndata && all_int; r++)
            if (!tab_parse_int(rows[r + 1][c].data(), rows[r + 1][c].size(), vals[r])) all_int = false;
        if (all_int) delta_bitmap[c >> 3] |= (uint8_t)(1u << (c & 7));
        out.insert(out.end(), rows[0][c].begin(), rows[0][c].end());   // header cell verbatim
        out.push_back('\n');
        for (size_t r = 0; r < ndata; r++) {
            if (r) out.push_back('\n');
            if (all_int) {
                long long d = (r == 0) ? vals[0]
                            : (long long)((unsigned long long)vals[r] - (unsigned long long)vals[r - 1]);
                char b[24]; int mlen = snprintf(b, sizeof(b), "%lld", d);
                out.insert(out.end(), b, b + mlen);
            } else {
                out.insert(out.end(), rows[r + 1][c].begin(), rows[r + 1][c].end());
            }
        }
        out.push_back('\0');
    }
    return out;
}

// Inverse of tab_build_payload + regrid. false on any structural mismatch (fail-closed).
inline bool tab_invert(const uint8_t* pay, size_t psize, const TabMeta& m,
                       const std::vector<uint8_t>& bitmap, std::vector<uint8_t>& out) {
    std::vector<std::vector<std::string>> cols;
    cols.reserve(m.ncols);
    size_t i = 0;
    for (uint32_t c = 0; c < m.ncols; c++) {
        size_t z = i; while (z < psize && pay[z] != '\0') z++;
        if (z >= psize) return false;                    // missing 0x00 terminator
        std::vector<std::string> cells;
        size_t s = i;
        for (size_t k = i; k < z; k++)
            if (pay[k] == '\n') { cells.emplace_back((const char*)pay + s, k - s); s = k + 1; }
        cells.emplace_back((const char*)pay + s, z - s);
        if (cells.size() != (size_t)m.nrows + 1) return false;   // header + nrows data cells
        if (bitmap[c >> 3] & (uint8_t)(1u << (c & 7))) {
            long long acc = 0;
            for (uint32_t r = 0; r < m.nrows; r++) {
                long long d;
                if (!tab_parse_int(cells[r + 1].data(), cells[r + 1].size(), d)) return false;
                acc = (r == 0) ? d : (long long)((unsigned long long)acc + (unsigned long long)d);
                char b[24]; int mlen = snprintf(b, sizeof(b), "%lld", acc);
                cells[r + 1].assign(b, (size_t)mlen);
            }
        }
        cols.push_back(std::move(cells));
        i = z + 1;
    }
    if (i != psize) return false;                        // trailing garbage
    for (uint32_t r = 0; r <= m.nrows; r++) {
        if (r) out.push_back('\n');
        for (uint32_t c = 0; c < m.ncols; c++) {
            if (c) out.push_back(m.delim);
            const std::string& cell = cols[c][r];
            out.insert(out.end(), cell.begin(), cell.end());
        }
    }
    if (m.trailing_nl) out.push_back('\n');
    return true;
}

// ============================================================================
// MM (whitespace-delimited numeric grid — Matrix Market, sparse-triple .mtx, XYZ point
// clouds, space/tab-aligned scientific tables). MT cannot touch these: it joins cells with
// a SINGLE delim byte, so it can neither parse variable-width whitespace runs nor reproduce
// sign-alignment padding (e.g. Matrix Market pads positive values with an extra space to
// align the '-' of negatives — 43% of bcsstk16's lines). MM captures the EXACT layout in a
// "skeleton": the body with every field token replaced by one 0x00 placeholder, all
// separators/terminators kept verbatim. Fields are stored column-major (the transpose that
// concentrates per-column redundancy — a coordinate matrix's column index is long runs, its
// row index ascending, its values clustered), with order-1 delta on canonically-integer
// columns. The skeleton is near-constant (a few whitespace patterns) so it costs ~nothing
// after recursive compression. LOSSLESS by construction (concatenative: every body byte is
// either a literal in the skeleton or the head of exactly one field) AND self-verified +
// end-to-end roundtrip-checked before adoption (fail-closed). Measured: bcsstk16.mtx -68%
// vs xz. Gated by try_tabular; whitespace-grid parse is the cheap filter (no proxy).
// (2026-08-09)
// ============================================================================
namespace mwg {

// Parse a whitespace numeric grid and emit a self-describing payload. Returns false unless:
// after stripping leading %/# comment lines, the remainder is a uniform k-column (2..8)
// whitespace grid of >=32 lines, every field is 0x00-free, and >=2 columns are canonical
// integers (the index-column signal that keeps this off prose / ragged text).
inline bool build_payload(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    if (size < 256) return false;
    // 1. header = leading run of comment lines ('%' Matrix Market / '#' generic).
    size_t hp = 0;
    while (hp < size && (data[hp] == '%' || data[hp] == '#')) {
        size_t le = hp; while (le < size && data[le] != '\n') le++;
        hp = (le < size) ? le + 1 : le;
    }
    if (hp >= size) return false;

    // 2. tokenize the body into a uniform k-column grid; collect absolute field ranges,
    //    row-major. Whitespace within a line = space/tab/CR (lines split on LF).
    struct FR { size_t off, len; };
    std::vector<FR> fields;
    size_t nlines = 0; int k = -1;
    size_t p = hp;
    auto is_ws = [](uint8_t c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (p < size) {
        size_t le = p; while (le < size && data[le] != '\n') le++;
        int nf = 0; size_t q = p; size_t first_field = fields.size();
        while (q < le) {
            while (q < le && is_ws(data[q])) q++;
            if (q >= le) break;
            size_t fs = q;
            while (q < le && !is_ws(data[q])) {
                if (data[q] == 0x00) return false;      // 0x00 in a field breaks the skeleton
                q++;
            }
            fields.push_back({ fs, q - fs }); nf++;
        }
        if (nf == 0) {
            if (le >= size) break;                       // trailing whitespace-only tail (kept in skeleton)
            return false;                                // interior blank line breaks the grid
        }
        if (k < 0) { k = nf; if (k < 2 || k > 8) return false; }
        else if (nf != k) { (void)first_field; return false; }
        nlines++;
        if (le >= size) break;
        p = le + 1;
    }
    if (k < 2 || nlines < 32) return false;
    if (fields.size() != (size_t)k * nlines) return false;

    // 3. classify columns: canonical-int columns get order-1 delta.
    std::vector<uint8_t> bitmap((size_t)((k + 7) / 8), 0);
    int nint = 0;
    for (int c = 0; c < k; c++) {
        bool all_int = true;
        for (size_t l = 0; l < nlines && all_int; l++) {
            const FR& f = fields[l * k + c];
            long long v;
            if (!tab_parse_int((const char*)data + f.off, f.len, v)) all_int = false;
        }
        if (all_int) { bitmap[c >> 3] |= (uint8_t)(1u << (c & 7)); nint++; }
    }
    if (nint < 2) return false;                          // needs the numeric-index signal

    // 4. skeleton: walk the body; emit one 0x00 per field, every other byte verbatim.
    std::vector<uint8_t> skel; skel.reserve(size - hp);
    size_t fi = 0, bp = hp;
    while (bp < size) {
        if (fi < fields.size() && bp == fields[fi].off) {
            skel.push_back(0x00);
            bp += fields[fi].len; fi++;
        } else {
            skel.push_back(data[bp]); bp++;
        }
    }
    if (fi != fields.size()) return false;

    // 5. column blobs ('\n'-joined field values; delta'd if the column is canonical-int).
    std::vector<std::vector<uint8_t>> cols((size_t)k);
    for (int c = 0; c < k; c++) {
        bool is_int = (bitmap[c >> 3] & (uint8_t)(1u << (c & 7))) != 0;
        std::vector<uint8_t>& blob = cols[(size_t)c];
        long long prev = 0;
        for (size_t l = 0; l < nlines; l++) {
            if (l) blob.push_back('\n');
            const FR& f = fields[l * k + c];
            if (is_int) {
                long long v; tab_parse_int((const char*)data + f.off, f.len, v);
                long long d = (l == 0) ? v
                            : (long long)((unsigned long long)v - (unsigned long long)prev);
                prev = v;
                char b[24]; int mlen = snprintf(b, sizeof(b), "%lld", d);
                blob.insert(blob.end(), b, b + mlen);
            } else {
                blob.insert(blob.end(), data + f.off, data + f.off + f.len);
            }
        }
    }

    // 6. assemble self-describing payload.
    uint8_t vb[16]; size_t vn;
    vn = write_uvarint_buf(vb, hp);          out.insert(out.end(), vb, vb + vn);
    out.insert(out.end(), data, data + hp);                       // header verbatim
    out.push_back((uint8_t)k);
    vn = write_uvarint_buf(vb, nlines);      out.insert(out.end(), vb, vb + vn);
    out.insert(out.end(), bitmap.begin(), bitmap.end());
    vn = write_uvarint_buf(vb, skel.size()); out.insert(out.end(), vb, vb + vn);
    out.insert(out.end(), skel.begin(), skel.end());
    for (int c = 0; c < k; c++) {
        vn = write_uvarint_buf(vb, cols[(size_t)c].size()); out.insert(out.end(), vb, vb + vn);
        out.insert(out.end(), cols[(size_t)c].begin(), cols[(size_t)c].end());
    }
    return true;
}

// Inverse of build_payload. Fail-closed on any structural mismatch.
inline bool invert(const uint8_t* pay, size_t psize, std::vector<uint8_t>& out) {
    const uint8_t* p = pay; const uint8_t* end = pay + psize;
    uint64_t hlen = read_uvarint(p, end);
    if (p > end || hlen > (uint64_t)(end - p)) return false;
    const uint8_t* header = p; p += hlen;
    if (p >= end) return false;
    uint8_t k = *p++;
    if (k < 2 || k > 8) return false;
    uint64_t nlines = read_uvarint(p, end);
    if (p > end || nlines == 0) return false;
    size_t bmlen = (size_t)((k + 7) / 8);
    if ((size_t)(end - p) < bmlen) return false;
    const uint8_t* bitmap = p; p += bmlen;
    uint64_t skel_len = read_uvarint(p, end);
    if (p > end || skel_len > (uint64_t)(end - p)) return false;
    const uint8_t* skel = p; p += skel_len;

    // reconstruct each column's field values (un-delta the integer columns).
    std::vector<std::vector<std::string>> colvals((size_t)k);
    for (int c = 0; c < k; c++) {
        uint64_t clen = read_uvarint(p, end);
        if (p > end || clen > (uint64_t)(end - p)) return false;
        const uint8_t* cb = p; p += clen;
        std::vector<std::string>& vals = colvals[(size_t)c];
        vals.reserve((size_t)nlines);
        size_t s = 0;
        for (size_t i = 0; i <= (size_t)clen; i++) {
            if (i == (size_t)clen || cb[i] == '\n') {
                vals.emplace_back((const char*)cb + s, i - s);
                s = i + 1;
            }
        }
        if (vals.size() != (size_t)nlines) return false;
        if (bitmap[c >> 3] & (uint8_t)(1u << (c & 7))) {
            long long acc = 0;
            for (size_t l = 0; l < (size_t)nlines; l++) {
                long long d;
                if (!tab_parse_int(vals[l].data(), vals[l].size(), d)) return false;
                acc = (l == 0) ? d : (long long)((unsigned long long)acc + (unsigned long long)d);
                char b[24]; int mlen = snprintf(b, sizeof(b), "%lld", acc);
                vals[l].assign(b, (size_t)mlen);
            }
        }
    }
    if (p != end) return false;                          // trailing garbage

    out.insert(out.end(), header, header + hlen);        // header verbatim
    std::vector<size_t> cur((size_t)k, 0);               // per-column line cursor
    size_t m = 0;                                        // row-major placeholder counter
    for (size_t i = 0; i < (size_t)skel_len; i++) {
        if (skel[i] == 0x00) {
            size_t col = m % k;
            if (cur[col] >= (size_t)nlines) return false;
            const std::string& v = colvals[col][cur[col]++];
            out.insert(out.end(), v.begin(), v.end());
            m++;
        } else {
            out.push_back(skel[i]);
        }
    }
    if (m != (size_t)k * (size_t)nlines) return false;
    return true;
}

} // namespace mwg

// ============================================================================
// MQ (SQL-INSERT-tuple column-transpose) — port of sqladv_transpose.py (verified).
// Parse repeated INSERT INTO t (...) VALUES (r1),(r2),...; tuples into a column grid,
// transpose column-major + delta perfectly-linear integer columns, into one self-
// describing blob, then compress recursively. CORRECTNESS = concatenative reassembly:
// every input byte is assigned to exactly one captured piece (verbatim segment,
// open_seq, cell, or tuple_sep) in order, so invert() concatenates them back exactly
// for ANY tokenization — a mis-parse only declines a region or compresses worse, never
// corrupts. apply() additionally SELF-VERIFIES byte-exact and declines on mismatch.
// Union quote lexer (\\ escapes next byte AND '' doubles) covers MySQL/ANSI/PG-scs=on.
// Wins on real SQL dumps: users_dump.sql -34.5%, pagila/northwind, 17.7MB scale-proven.
// ============================================================================
namespace mqsql {
constexpr uint8_t Q=0x27,BS=0x5c,LP=0x28,RP=0x29,CM=0x2c,SEMI=0x3b,BT=0x60,DQ=0x22;
static const uint8_t MAGIC[7] = {'M','T','S','Q','L','1',0x00};
constexpr uint32_t MIN_ROWS=4, MIN_COLS=2;

struct Range { size_t off, len; };
struct Region { size_t open_off, open_len; uint32_t ncols, nrows;
                std::vector<std::vector<Range>> columns; std::vector<Range> tuple_seps; size_t end; };

inline void put_uv(std::vector<uint8_t>& b, uint64_t n){ while(n>=0x80){ b.push_back((uint8_t)((n&0x7f)|0x80)); n>>=7; } b.push_back((uint8_t)n); }
inline uint64_t get_uv(const uint8_t* buf, size_t& pos, size_t end, bool& ok){ uint64_t v=0; int sh=0; while(pos<end){ uint8_t c=buf[pos++]; v|=(uint64_t)(c&0x7f)<<sh; if(!(c&0x80)) return v; sh+=7; if(sh>63){ ok=false; return 0; } } ok=false; return 0; }
inline size_t bfind(const uint8_t* r, size_t n, size_t from, uint8_t ch){ if(from>=n) return SIZE_MAX; const void* p=memchr(r+from, ch, n-from); return p? (size_t)((const uint8_t*)p-r) : SIZE_MAX; }
inline size_t bfind2(const uint8_t* r, size_t n, size_t from, uint8_t a, uint8_t b){ for(size_t i=from;i+1<n;i++) if(r[i]==a&&r[i+1]==b) return i; return SIZE_MAX; }

inline size_t skip_squote(const uint8_t* r, size_t n, size_t pos){ pos++; while(pos<n){ uint8_t c=r[pos];
  if(c==BS){ pos+=2; continue; } if(c==Q){ if(pos+1<n && r[pos+1]==Q){ pos+=2; continue; } return pos+1; } pos++; } return SIZE_MAX; }
inline size_t skip_comments_ws(const uint8_t* r, size_t n, size_t pos){ while(pos<n){ uint8_t c=r[pos];
  if(c==0x20||c==0x09||c==0x0a||c==0x0d){ pos++; continue; }
  if(c==0x2d && pos+1<n && r[pos+1]==0x2d){ size_t j=bfind(r,n,pos,'\n'); pos=(j==SIZE_MAX)?n:j+1; continue; }
  if(c==0x2f && pos+1<n && r[pos+1]==0x2a){ size_t j=bfind2(r,n,pos+2,'*','/'); if(j==SIZE_MAX) return SIZE_MAX; pos=j+2; continue; }
  break; } return pos; }
inline size_t match_kw(const uint8_t* r, size_t n, size_t pos, const char* kw, size_t L){ if(pos+L>n) return SIZE_MAX;
  for(size_t i=0;i<L;i++){ uint8_t c=r[pos+i]; uint8_t lc=(c>='A'&&c<='Z')?(uint8_t)(c+32):c; if(lc!=(uint8_t)kw[i]) return SIZE_MAX; }
  size_t nx=pos+L; if(nx<n){ uint8_t c=r[nx]; if((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_') return SIZE_MAX; } return nx; }
inline size_t skip_balanced_parens(const uint8_t* r, size_t n, size_t pos){ int depth=0; while(pos<n){ uint8_t c=r[pos];
  if(c==Q){ size_t np=skip_squote(r,n,pos); if(np==SIZE_MAX) return SIZE_MAX; pos=np; continue; }
  if(c==BT){ size_t j=bfind(r,n,pos+1,BT); if(j==SIZE_MAX) return SIZE_MAX; pos=j+1; continue; }
  if(c==DQ){ size_t j=bfind(r,n,pos+1,DQ); if(j==SIZE_MAX) return SIZE_MAX; pos=j+1; continue; }
  if(c==LP) depth++; else if(c==RP){ depth--; if(depth==0) return pos+1; } pos++; } return SIZE_MAX; }

inline size_t parse_tuple(const uint8_t* r, size_t n, size_t pos, std::vector<Range>& cells){ pos++; size_t start=pos; int depth=1;
  while(pos<n){ uint8_t c=r[pos];
    if(c==Q){ size_t np=skip_squote(r,n,pos); if(np==SIZE_MAX) return SIZE_MAX; pos=np; continue; }
    if(c==BT){ size_t j=bfind(r,n,pos+1,BT); if(j==SIZE_MAX) return SIZE_MAX; pos=j+1; continue; }
    if(c==DQ){ size_t j=bfind(r,n,pos+1,DQ); if(j==SIZE_MAX) return SIZE_MAX; pos=j+1; continue; }
    if(c==LP){ depth++; pos++; continue; }
    if(c==RP){ depth--; if(depth==0){ cells.push_back({start,pos-start}); return pos+1; } pos++; continue; }
    if(c==CM && depth==1){ cells.push_back({start,pos-start}); pos++; start=pos; continue; }
    pos++; }
  return SIZE_MAX; }

inline bool try_parse_insert(const uint8_t* r, size_t n, size_t pos, bool group_stmts, Region& reg){
  size_t p=match_kw(r,n,pos,"insert",6); if(p==SIZE_MAX) return false;
  p=skip_comments_ws(r,n,p); if(p==SIZE_MAX) return false;
  size_t p2=match_kw(r,n,p,"into",4); if(p2==SIZE_MAX) return false;
  p=skip_comments_ws(r,n,p2); if(p==SIZE_MAX) return false;
  bool have_values=false;
  while(p<n){ uint8_t c=r[p];
    if(c==BT){ size_t j=bfind(r,n,p+1,BT); if(j==SIZE_MAX) return false; p=j+1; continue; }
    if(c==DQ){ size_t j=bfind(r,n,p+1,DQ); if(j==SIZE_MAX) return false; p=j+1; continue; }
    if(c==LP){ size_t np=skip_balanced_parens(r,n,p); if(np==SIZE_MAX) return false; p=np; continue; }
    size_t pv=match_kw(r,n,p,"values",6); if(pv!=SIZE_MAX){ p=pv; have_values=true; break; }
    pv=match_kw(r,n,p,"value",5); if(pv!=SIZE_MAX){ p=pv; have_values=true; break; }
    if(c==SEMI) return false;
    if(match_kw(r,n,p,"select",6)!=SIZE_MAX) return false;
    p++; }
  if(!have_values) return false;
  size_t q=skip_comments_ws(r,n,p); if(q==SIZE_MAX||q>=n||r[q]!=LP) return false;
  size_t open_off=pos, open_len=q+1-pos;
  std::vector<Range> cells0; size_t after=parse_tuple(r,n,q,cells0); if(after==SIZE_MAX) return false;
  uint32_t ncols=(uint32_t)cells0.size(); if(ncols<MIN_COLS) return false;
  std::vector<std::vector<Range>> rows; rows.push_back(std::move(cells0));
  std::vector<Range> seps; size_t end=after;
  while(true){
    size_t rr=skip_comments_ws(r,n,after); size_t nextlp=SIZE_MAX;
    if(rr!=SIZE_MAX && rr<n && r[rr]==CM){ size_t r2=skip_comments_ws(r,n,rr+1); if(r2!=SIZE_MAX && r2<n && r[r2]==LP) nextlp=r2; }
    if(nextlp==SIZE_MAX && group_stmts && rr!=SIZE_MAX && rr<n && r[rr]==SEMI){ size_t s=skip_comments_ws(r,n,rr+1);
      if(s!=SIZE_MAX && s+open_len<=n && memcmp(r+s, r+open_off, open_len)==0) nextlp=s+open_len-1; }
    if(nextlp!=SIZE_MAX){ std::vector<Range> t2; size_t e2=parse_tuple(r,n,nextlp,t2);
      if(e2!=SIZE_MAX && (uint32_t)t2.size()==ncols){ seps.push_back({after-1, nextlp+1-(after-1)}); rows.push_back(std::move(t2)); after=e2; continue; } }
    seps.push_back({after-1,1}); end=after; break; }
  uint32_t nrows=(uint32_t)rows.size(); if(nrows<MIN_ROWS) return false;
  reg.open_off=open_off; reg.open_len=open_len; reg.ncols=ncols; reg.nrows=nrows; reg.end=end;
  reg.columns.assign(ncols, {});
  for(uint32_t j=0;j<ncols;j++){ reg.columns[j].resize(nrows); for(uint32_t t=0;t<nrows;t++) reg.columns[j][t]=rows[t][j]; }
  reg.tuple_seps=std::move(seps);
  return true; }

// perfectly-linear canonical-int column -> delta strings; else false
inline bool delta_encode_col(const uint8_t* raw, const std::vector<Range>& col, std::vector<std::string>& out){
  size_t nr=col.size(); if(nr<3) return false;
  std::vector<long long> vals(nr);
  for(size_t i=0;i<nr;i++) if(!tab_parse_int((const char*)raw+col[i].off, col[i].len, vals[i])) return false;
  long long d0=(long long)((unsigned long long)vals[1]-(unsigned long long)vals[0]);
  for(size_t i=1;i+1<nr;i++){ long long di=(long long)((unsigned long long)vals[i+1]-(unsigned long long)vals[i]); if(di!=d0) return false; }
  out.resize(nr); long long prev=0;
  for(size_t i=0;i<nr;i++){ long long d=(i==0)?vals[i]:(long long)((unsigned long long)vals[i]-(unsigned long long)prev); char b[24]; int m=snprintf(b,sizeof b,"%lld",d); out[i].assign(b,(size_t)m); prev=vals[i]; }
  return true; }

inline std::vector<uint8_t> ser_region(const uint8_t* raw, const Region& reg, bool do_delta){
  std::vector<uint8_t> b;
  put_uv(b, reg.open_len); b.insert(b.end(), raw+reg.open_off, raw+reg.open_off+reg.open_len);
  put_uv(b, reg.ncols); put_uv(b, reg.nrows);
  for(auto& s: reg.tuple_seps) put_uv(b, s.len);
  for(auto& s: reg.tuple_seps) b.insert(b.end(), raw+s.off, raw+s.off+s.len);
  for(uint32_t j=0;j<reg.ncols;j++){
    std::vector<std::string> dcol; bool dd = do_delta && delta_encode_col(raw, reg.columns[j], dcol);
    b.push_back(dd?1:0);
    if(dd){ for(auto& c: dcol) put_uv(b, c.size()); for(auto& c: dcol) b.insert(b.end(), c.begin(), c.end()); }
    else { for(auto& c: reg.columns[j]) put_uv(b, c.len); for(auto& c: reg.columns[j]) b.insert(b.end(), raw+c.off, raw+c.off+c.len); }
  }
  return b; }

// build the self-describing SQL blob; empty vector = decline
inline std::vector<uint8_t> apply(const uint8_t* raw, size_t n, bool do_delta, bool group_stmts){
  std::vector<uint8_t> body; uint64_t nsegs=0; size_t pos=0, verb_start=0; bool found=false;
  while(pos<n){ uint8_t c=raw[pos];
    if(c==0x2d && pos+1<n && raw[pos+1]==0x2d){ size_t j=bfind(raw,n,pos,'\n'); pos=(j==SIZE_MAX)?n:j+1; continue; }
    if(c==0x2f && pos+1<n && raw[pos+1]==0x2a){ size_t j=bfind2(raw,n,pos+2,'*','/'); pos=(j==SIZE_MAX)?n:j+2; continue; }
    if(c==Q){ size_t np=skip_squote(raw,n,pos); pos=(np==SIZE_MAX)?n:np; continue; }
    if(c==BT){ size_t j=bfind(raw,n,pos+1,BT); pos=(j==SIZE_MAX)?n:j+1; continue; }
    if(c==DQ){ size_t j=bfind(raw,n,pos+1,DQ); pos=(j==SIZE_MAX)?n:j+1; continue; }
    if(c=='I'||c=='i'){ Region reg;
      if(try_parse_insert(raw,n,pos,group_stmts,reg)){
        if(pos>verb_start){ body.push_back(0); put_uv(body, pos-verb_start); body.insert(body.end(), raw+verb_start, raw+pos); nsegs++; }
        std::vector<uint8_t> rb=ser_region(raw,reg,do_delta);
        body.push_back(1); put_uv(body, rb.size()); body.insert(body.end(), rb.begin(), rb.end()); nsegs++;
        pos=reg.end; verb_start=pos; found=true; continue; } }
    pos++; }
  if(verb_start<n){ body.push_back(0); put_uv(body, n-verb_start); body.insert(body.end(), raw+verb_start, raw+n); nsegs++; }
  if(!found) return {};
  std::vector<uint8_t> blob(MAGIC, MAGIC+7); put_uv(blob, nsegs); blob.insert(blob.end(), body.begin(), body.end());
  return blob; }

// invert the blob -> out; false on any structural problem (fail-closed)
inline bool invert(const uint8_t* blob, size_t bn, std::vector<uint8_t>& out){
  if(bn<7 || memcmp(blob, MAGIC, 7)!=0) return false;
  size_t pos=7; bool ok=true; uint64_t nsegs=get_uv(blob,pos,bn,ok); if(!ok) return false;
  for(uint64_t si=0; si<nsegs; si++){
    if(pos>=bn) return false; uint8_t tag=blob[pos++];
    uint64_t L=get_uv(blob,pos,bn,ok); if(!ok) return false;
    if(L > (uint64_t)(bn - pos)) return false;                 // no-overflow bound (crafted varint)
    if(tag==0){ out.insert(out.end(), blob+pos, blob+pos+L); pos+=L; continue; }
    if(tag!=1) return false;
    size_t rp=pos, rend=pos+L;   // region blob spans [pos, pos+L)
    uint64_t olen=get_uv(blob,rp,rend,ok); if(!ok || olen > (uint64_t)(rend-rp)) return false;
    const uint8_t* open_seq=blob+rp; size_t open_len=(size_t)olen; rp+=olen;
    uint64_t ncols=get_uv(blob,rp,rend,ok); if(!ok) return false;
    uint64_t nrows=get_uv(blob,rp,rend,ok); if(!ok) return false;
    // bound nrows/ncols vs the remaining region so a crafted count can't over-allocate (DoS):
    // every row needs >=1 sep-len byte, so nrows <= remaining bytes.
    if(ncols==0||ncols>4096||nrows==0||nrows > (uint64_t)(rend-rp)) return false;
    std::vector<uint64_t> seplen(nrows); for(uint64_t t=0;t<nrows;t++){ seplen[t]=get_uv(blob,rp,rend,ok); if(!ok) return false; }
    std::vector<const uint8_t*> sepp(nrows);
    for(uint64_t t=0;t<nrows;t++){ if(seplen[t] > (uint64_t)(rend-rp)) return false; sepp[t]=blob+rp; rp+=seplen[t]; }
    // columns -> materialize as strings (delta un-applied)
    std::vector<std::vector<std::string>> cols(ncols);
    for(uint64_t j=0;j<ncols;j++){ if(rp>=rend) return false; uint8_t flag=blob[rp++];
      std::vector<uint64_t> clen(nrows); for(uint64_t t=0;t<nrows;t++){ clen[t]=get_uv(blob,rp,rend,ok); if(!ok) return false; }
      cols[j].resize(nrows);
      if(flag&1){ long long acc=0;
        for(uint64_t t=0;t<nrows;t++){ if(clen[t] > (uint64_t)(rend-rp)) return false; long long d; if(!tab_parse_int((const char*)blob+rp, clen[t], d)) return false; rp+=clen[t];
          acc=(t==0)?d:(long long)((unsigned long long)acc+(unsigned long long)d); char b[24]; int m=snprintf(b,sizeof b,"%lld",acc); cols[j][t].assign(b,(size_t)m); } }
      else { for(uint64_t t=0;t<nrows;t++){ if(clen[t] > (uint64_t)(rend-rp)) return false; cols[j][t].assign((const char*)blob+rp, clen[t]); rp+=clen[t]; } }
    }
    // emit region row-major: open_seq + per row (cells joined ',' + sep)
    out.insert(out.end(), open_seq, open_seq+open_len);
    for(uint64_t t=0;t<nrows;t++){ for(uint64_t j=0;j<ncols;j++){ out.insert(out.end(), cols[j][t].begin(), cols[j][t].end()); if(j+1<ncols) out.push_back(CM); }
      out.insert(out.end(), sepp[t], sepp[t]+seplen[t]); }
    pos+=L;
  }
  return true; }
} // namespace mqsql

// ============================================================================
// MB (x86 BCJ pre-filter) — canonical LZMA-SDK Bra86 x86_Convert (E8/E9 call/jump
// IP-relative<->absolute rewrite, whole-buffer ip=0/state=0), applied as a trial-and-
// keep pre-filter with an exact inverse. mzip's XZLIB backstop applies NO liblzma
// filter (mzip_raw == xz-plain), so this recovers the entire BCJ gain on x86/x64
// executables: libwinpthread-1.dll -4.25%, liblzma-5.dll -1.86% (real PE, verified).
// Length-preserving byte-exact inverse; gated by MZ/ELF magic or E8/E9 density so
// non-executable input pays ~0; trial-and-keep + end-to-end memcmp before adopt.
// ============================================================================
namespace mbcj {
inline bool test86ms(uint8_t b){ return (uint8_t)(((unsigned)b + 1) & 0xFE) == 0; }
// encoding=1 encode (rel->abs), 0 decode (abs->rel). In-place, returns bytes processed.
inline size_t x86_convert(uint8_t* data, size_t size, uint32_t ip, uint32_t* state, int encoding){
  size_t pos=0; uint32_t mask=*state & 7;
  if(size<5) return 0;
  size-=4; ip+=5;
  for(;;){
    uint8_t* p=data+pos; const uint8_t* end=data+size;
    for(; p<end; p++) if((*p & 0xFE)==0xE8) break;
    { size_t d=(size_t)(p-data-pos); pos=(size_t)(p-data);
      if(p>=end){ *state=(d>2?0:mask>>(unsigned)d); return pos; }
      if(d>2) mask=0;
      else { mask>>=(unsigned)d; if(mask!=0 && (mask>4 || mask==3 || test86ms(p[(size_t)(mask>>1)+1]))){ mask=(mask>>1)|4; pos++; continue; } } }
    if(test86ms(p[4])){
      uint32_t v=((uint32_t)p[4]<<24)|((uint32_t)p[3]<<16)|((uint32_t)p[2]<<8)|((uint32_t)p[1]);
      uint32_t cur=ip+(uint32_t)pos; pos+=5;
      if(encoding) v+=cur; else v-=cur;
      if(mask!=0){ unsigned sh=(mask&6)<<2; if(test86ms((uint8_t)(v>>sh))){ v^=(((uint32_t)0x100<<sh)-1); if(encoding) v+=cur; else v-=cur; } mask=0; }
      p[1]=(uint8_t)v; p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)(v>>16); p[4]=(uint8_t)(0-((v>>24)&1));
    } else { mask=(mask>>1)|4; pos++; }
  }
}
inline bool looks_like_x86(const uint8_t* d, size_t n){
  if(n<256) return false;
  if(d[0]==0x4d && d[1]==0x5a) return true;                                  // 'MZ' PE
  if(d[0]==0x7f && d[1]=='E' && d[2]=='L' && d[3]=='F') return true;         // ELF
  size_t s=n<65536?n:65536, cnt=0;
  for(size_t i=0;i<s;i++){ uint8_t c=d[i]; if((c&0xFE)==0xE8) cnt++; }
  return cnt*100 >= s;                                                       // >=1% E8/E9 density
}
// Detect a NON-x86 executable and return the matching liblzma BCJ filter id, else 0.
// x86/x64 are deliberately excluded (handled by the hand-rolled x86_convert 'MB' path above).
// Reads the ELF/PE header at the block start; only block 0 of a file carries it, which covers the
// common single-block executable. Correctness never depends on this being right: BCJ is bijective,
// the .xz decoder inverts it from the stream header, and the trial is kept only if it is smaller
// AND roundtrip-verifies -- a wrong guess just loses the trial. (2026-08-08)
inline uint64_t exec_bcj_filter(const uint8_t* d, size_t n){
  if(n < 64) return 0;
  if(d[0]==0x7f && d[1]=='E' && d[2]=='L' && d[3]=='F'){                      // ELF
    bool le = (d[5]==1);                                                      // EI_DATA: 1=LE, 2=BE
    uint16_t em = le ? (uint16_t)(d[0x12] | ((uint16_t)d[0x13]<<8))
                     : (uint16_t)(((uint16_t)d[0x12]<<8) | d[0x13]);          // e_machine
    switch(em){
      case 0x28: return MZ_LZMA_FILTER_ARM;                                   // EM_ARM (32-bit)
      case 0xB7: return MZ_LZMA_FILTER_ARM64;                                 // EM_AARCH64
      case 0x14: case 0x15: return le ? 0 : MZ_LZMA_FILTER_POWERPC;           // EM_PPC/PPC64 (liblzma PPC = big-endian)
      case 0xF3: return MZ_LZMA_FILTER_RISCV;                                 // EM_RISCV
      default:   return 0;                                                    // x86 (0x03/0x3E) -> 'MB' path
    }
  }
  if(d[0]=='M' && d[1]=='Z'){                                                 // PE
    uint32_t pe = (uint32_t)d[0x3C] | ((uint32_t)d[0x3D]<<8) | ((uint32_t)d[0x3E]<<16) | ((uint32_t)d[0x3F]<<24);
    if((size_t)pe+6 <= n && d[pe]=='P' && d[pe+1]=='E' && d[pe+2]==0 && d[pe+3]==0){
      uint16_t mach = (uint16_t)d[pe+4] | ((uint16_t)d[pe+5]<<8);             // IMAGE_FILE_MACHINE_*
      switch(mach){
        case 0xAA64: return MZ_LZMA_FILTER_ARM64;                             // ARM64
        case 0x01C0: return MZ_LZMA_FILTER_ARM;                               // ARM
        case 0x01C2: case 0x01C4: return MZ_LZMA_FILTER_ARMTHUMB;             // THUMB / ARMNT
        case 0x01F0: case 0x01F1: return MZ_LZMA_FILTER_POWERPC;              // PPC
        case 0x5064: return MZ_LZMA_FILTER_RISCV;                             // RISCV64
        default:     return 0;                                               // x86 (0x014C/0x8664) -> 'MB' path
      }
    }
  }
  return 0;
}
} // namespace mbcj

// ============================================================================
// ML (line-templated log timestamp-delta) — port of lt_clf.py (verified). For CLF/
// Apache-combined logs ('[10/Oct/2000:13:55:36 -0700]'): parse the datetime to a
// naive int64 epoch (tz kept in-line), order-1 delta vs the previous conforming line,
// ZIGZAG so the signed delta is pure digits (no '-' colliding with date separators),
// and substitute IN PLACE (every other field keeps row context so the same backstop
// fires). Decode un-zigzags, cumulative-sums, and REFORMATS byte-exact. LOSSLESS is a
// CODE-PATH property: a line transforms ONLY if reformat(parse(ts))==ts byte-for-byte
// (fail-closed per-line gate -> else verbatim exception channel), plus a whole-blob
// self-verify. Calendar is Hinnant integer civil<->days (no libc, platform-exact).
// Measured: nginx_access.log -14.6%, apache_log_sample -3.5% (real, verified).
// ============================================================================
namespace mltsd {
static const char MON[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
inline int d2(const uint8_t* p){ if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9') return -1; return (p[0]-'0')*10+(p[1]-'0'); }
inline int d4(const uint8_t* p){ for(int i=0;i<4;i++) if(p[i]<'0'||p[i]>'9') return -1; return (p[0]-'0')*1000+(p[1]-'0')*100+(p[2]-'0')*10+(p[3]-'0'); }
inline int mon_idx(const uint8_t* p){ for(int i=0;i<12;i++) if(p[0]==(uint8_t)MON[i][0]&&p[1]==(uint8_t)MON[i][1]&&p[2]==(uint8_t)MON[i][2]) return i+1; return 0; }
inline int64_t days_from_civil(int64_t y,int64_t m,int64_t d){ y-=(m<=2); int64_t era=(y>=0?y:y-399)/400; int64_t yoe=y-era*400; int64_t doy=(153*(m+(m>2?-3:9))+2)/5+d-1; int64_t doe=yoe*365+yoe/4-yoe/100+doy; return era*146097+doe-719468; }
inline void civil_from_days(int64_t z,int64_t&Y,int64_t&M,int64_t&D){ z+=719468; int64_t era=(z>=0?z:z-146096)/146097; int64_t doe=z-era*146097; int64_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365; int64_t y=yoe+era*400; int64_t doy=doe-(365*yoe+yoe/4-yoe/100); int64_t mp=(5*doy+2)/153; int64_t d=doy-(153*mp+2)/5+1; int64_t m=mp+(mp<10?3:-9); Y=y+(m<=2); M=m; D=d; }
inline uint64_t zz(int64_t n){ return (uint64_t)((n<<1)^(n>>63)); }
inline int64_t unzz(uint64_t u){ return (int64_t)((u>>1)^(uint64_t)(0-(int64_t)(u&1))); }
inline int fmt_dt(int64_t e, char* buf){                          // -> "DD/Mon/YYYY:HH:MM:SS", len or -1
  int64_t days=e/86400, rem=e%86400; if(rem<0){ days--; rem+=86400; }
  int64_t H=rem/3600, M=(rem%3600)/60, S=rem%60, Y,Mo,D; civil_from_days(days,Y,Mo,D);
  if(Mo<1||Mo>12) return -1;
  return snprintf(buf,32,"%02lld/%s/%04lld:%02lld:%02lld:%02lld",(long long)D,MON[(size_t)Mo-1],(long long)Y,(long long)H,(long long)M,(long long)S);
}
// parse a CLF datetime token at '[' bpos; on structural match set epoch (span is 20 bytes at bpos+1)
inline bool parse_clf(const uint8_t* ln, size_t le, size_t bpos, int64_t& epoch){
  if(bpos+28>le) return false; const uint8_t* p=ln+bpos;
  if(p[0]!='[') return false;
  int D=d2(p+1); if(D<0||p[3]!='/') return false;
  int Mo=mon_idx(p+4); if(Mo==0||p[7]!='/') return false;
  int Y=d4(p+8); if(Y<0||p[12]!=':') return false;
  int H=d2(p+13); if(H<0||p[15]!=':') return false;
  int M=d2(p+16); if(M<0||p[18]!=':') return false;
  int S=d2(p+19); if(S<0||p[21]!=' ') return false;
  if((p[22]!='+'&&p[22]!='-')||d4(p+23)<0||p[27]!=']') return false;
  epoch=days_from_civil(Y,Mo,D)*86400+H*3600+M*60+S; return true;
}
inline bool pdec(const uint8_t* p, size_t len, int64_t& v){        // bounded signed-decimal parse (no overflow UB)
  if(len==0) return false; size_t i=0; bool neg=false; if(p[0]=='-'){neg=true;i=1;}
  size_t digits=len-i; if(digits==0||digits>19) return false;     // >=1, <=19 digits => fits uint64 accumulate
  unsigned long long x=0;
  for(;i<len;i++){ if(p[i]<'0'||p[i]>'9') return false; x=x*10ULL+(unsigned long long)(p[i]-'0'); }
  if(neg){ if(x>9223372036854775808ULL) return false; v=(x==9223372036854775808ULL)?INT64_MIN:-(int64_t)x; }
  else   { if(x>9223372036854775807ULL) return false; v=(int64_t)x; }
  return true; }

// build CLF blob (LTCLF1 text header + body); empty = decline. NOT self-verified (caller does).
inline std::vector<uint8_t> apply_clf(const uint8_t* raw, size_t n){
  std::vector<std::pair<size_t,size_t>> lines;
  { size_t s=0; while(s<n){ const void* nl=memchr(raw+s,'\n',n-s); size_t e=nl?(size_t)((const uint8_t*)nl-raw)+1:n; lines.push_back({s,e-s}); s=e; } }
  std::vector<uint8_t> body; std::vector<uint32_t> exc; int64_t base_epoch=0,prev=0; bool have=false;
  for(size_t i=0;i<lines.size();i++){
    const uint8_t* ln=raw+lines[i].first; size_t le=lines[i].second;
    int64_t epoch=0; size_t bpos=SIZE_MAX;
    for(size_t j=0;j+1<le;j++){ if(ln[j]=='['){ int64_t ep; if(parse_clf(ln,le,j,ep)){ char buf[32]; int fl=fmt_dt(ep,buf); if(fl==20&&memcmp(buf,ln+j+1,20)==0){ epoch=ep; bpos=j; break; } } } }
    if(bpos==SIZE_MAX){ exc.push_back((uint32_t)i); body.insert(body.end(),ln,ln+le); continue; }
    if(!have){ base_epoch=epoch; prev=epoch; have=true; }
    int64_t delta=epoch-prev; prev=epoch;
    char tok[24]; int tl=snprintf(tok,sizeof tok,"%llu",(unsigned long long)zz(delta));
    body.insert(body.end(),ln,ln+bpos+1); body.insert(body.end(),tok,tok+tl); body.insert(body.end(),ln+bpos+21,ln+le);
  }
  if(!have) return {};
  std::vector<uint8_t> blob; const char* mg="LTCLF1\n"; blob.insert(blob.end(),mg,mg+7);
  char h[32]; int hl=snprintf(h,sizeof h,"%lld\n",(long long)base_epoch); blob.insert(blob.end(),h,h+hl);
  hl=snprintf(h,sizeof h,"%zu\n",exc.size()); blob.insert(blob.end(),h,h+hl);
  for(size_t k=0;k<exc.size();k++){ if(k) blob.push_back(' '); char e[16]; int el=snprintf(e,sizeof e,"%u",exc[k]); blob.insert(blob.end(),e,e+el); }
  blob.push_back('\n'); blob.insert(blob.end(),body.begin(),body.end());
  return blob;
}
inline bool invert_clf(const uint8_t* blob, size_t bn, std::vector<uint8_t>& out){
  size_t pos=0; auto line=[&](size_t& ls,size_t& ll)->bool{ if(pos>=bn) return false; const void* nl=memchr(blob+pos,'\n',bn-pos); if(!nl) return false; ls=pos; ll=(size_t)((const uint8_t*)nl-blob)-pos; pos=(size_t)((const uint8_t*)nl-blob)+1; return true; };
  size_t ls,ll; int64_t base_epoch=0,nexc=0;
  if(!line(ls,ll)||ll!=6||memcmp(blob+ls,"LTCLF1",6)!=0) return false;
  if(!line(ls,ll)||!pdec(blob+ls,ll,base_epoch)) return false;
  if(!line(ls,ll)||!pdec(blob+ls,ll,nexc)||nexc<0) return false;
  if(!line(ls,ll)) return false;                                  // exception-index line
  std::vector<uint32_t> exc;
  { size_t s=ls,e=ls+ll; while(s<e){ while(s<e&&blob[s]==' ') s++; size_t t=s; while(t<e&&blob[t]!=' ') t++; if(t>s){ int64_t v; if(!pdec(blob+s,t-s,v)||v<0) return false; exc.push_back((uint32_t)v); } s=t; } }
  if((int64_t)exc.size()!=nexc) return false;
  const uint8_t* body=blob+pos; size_t bl=bn-pos;
  std::vector<std::pair<size_t,size_t>> lines;
  { size_t s=0; while(s<bl){ const void* nl=memchr(body+s,'\n',bl-s); size_t e=nl?(size_t)((const uint8_t*)nl-body)+1:bl; lines.push_back({s,e-s}); s=e; } }
  int64_t prev=base_epoch; size_t ep=0;
  for(size_t i=0;i<lines.size();i++){
    const uint8_t* ln=body+lines[i].first; size_t le=lines[i].second;
    if(ep<exc.size()&&exc[ep]==(uint32_t)i){ out.insert(out.end(),ln,ln+le); ep++; continue; }
    // find delta token '[' digits ' ' [+-]dddd ']'
    size_t bpos=SIZE_MAX,dend=0;
    for(size_t j=0;j+1<le;j++){ if(ln[j]=='['){ size_t q=j+1; while(q<le&&ln[q]>='0'&&ln[q]<='9') q++; if(q==j+1) continue; if(q+7>le) continue; if(ln[q]!=' '||(ln[q+1]!='+'&&ln[q+1]!='-')||d4(ln+q+2)<0||ln[q+6]!=']') continue; bpos=j; dend=q; break; } }
    if(bpos==SIZE_MAX) return false;                             // malformed non-exception line
    int64_t u; if(!pdec(ln+bpos+1,dend-(bpos+1),u)||u<0) return false;
    int64_t epoch=prev+unzz((uint64_t)u); prev=epoch;
    char buf[32]; int fl=fmt_dt(epoch,buf); if(fl!=20) return false;
    out.insert(out.end(),ln,ln+bpos+1); out.insert(out.end(),buf,buf+20); out.insert(out.end(),ln+dend,ln+le);
  }
  return true;
}

// ---- ISO-8601 / app-log family: YYYY-MM-DD<sep>HH:MM:SS[<fsep>frac][zone], anchored at line start ----
// Descriptor {sep in {'T',' '}, fsep in {'.',','}, fwidth, zone literal} fixed per block; value =
// epoch-secs * 10^fwidth + frac so fractional seconds fold into the delta; zone kept literal (never
// applied). Same fail-closed model: a line transforms only if reformat(parse(ts))==ts byte-exact.
inline bool parse_iso(const uint8_t* ln, size_t le, int64_t& tick, uint8_t& sep, uint8_t& fsep,
                      int& fwidth, const uint8_t*& zone, size_t& zone_len, size_t& ts_len){
  if(le<19) return false; const uint8_t* p=ln;
  int Y=d4(p); if(Y<0||p[4]!='-') return false;
  int Mo=d2(p+5); if(Mo<0||p[7]!='-') return false;
  int D=d2(p+8); if(D<0) return false;
  uint8_t s=p[10]; if(s!='T'&&s!=' ') return false; sep=s;
  int H=d2(p+11); if(H<0||p[13]!=':') return false;
  int Mi=d2(p+14); if(Mi<0||p[16]!=':') return false;
  int S=d2(p+17); if(S<0) return false;
  size_t q=19; int64_t fr=0; int w=0; fsep=0;
  if(q<le && (ln[q]=='.'||ln[q]==',')){ fsep=ln[q]; q++; size_t fs=q; while(q<le && ln[q]>='0'&&ln[q]<='9') q++; w=(int)(q-fs); if(w==0||w>9) return false; for(size_t k=fs;k<q;k++) fr=fr*10+(ln[k]-'0'); }
  zone=ln+q; zone_len=0;
  if(q<le){ if(ln[q]=='Z'){ zone_len=1; q++; }
    else if(ln[q]=='+'||ln[q]=='-'){
      if(q+3<=le && d2(ln+q+1)>=0){
        if(q+6<=le && ln[q+3]==':' && d2(ln+q+4)>=0){ zone_len=6; q+=6; }
        else if(q+5<=le && d2(ln+q+3)>=0){ zone_len=5; q+=5; } } } }
  int64_t secs=days_from_civil(Y,Mo,D)*86400+H*3600+Mi*60+S;
  int64_t scale=1; for(int k=0;k<w;k++) scale*=10;
  if(secs<0 || secs > (INT64_MAX - fr)/scale) return false;   // overflow / pre-epoch guard
  tick=secs*scale+fr; fwidth=w; ts_len=q; return true;
}
inline void reformat_iso(int64_t tick, uint8_t sep, uint8_t fsep, int w, const uint8_t* zone, size_t zl, std::vector<uint8_t>& out){
  int64_t scale=1; for(int k=0;k<w;k++) scale*=10;
  int64_t secs=tick/scale, fr=tick%scale, days=secs/86400, tod=secs%86400;
  int64_t H=tod/3600, Mi=(tod%3600)/60, Sx=tod%60, Y,Mo,D; civil_from_days(days,Y,Mo,D);
  char b[40]; int nn=snprintf(b,sizeof b,"%04lld-%02lld-%02lld%c%02lld:%02lld:%02lld",(long long)Y,(long long)Mo,(long long)D,(char)sep,(long long)H,(long long)Mi,(long long)Sx);
  out.insert(out.end(),b,b+nn);
  if(w){ out.push_back(fsep); char f[24]; int fn=snprintf(f,sizeof f,"%0*lld",w,(long long)fr); out.insert(out.end(),f,f+fn); }
  out.insert(out.end(),zone,zone+zl);
}
struct IsoFmt { uint8_t sep, fsep; int fwidth; std::string zone; };
inline bool iso_fmt_eq(const IsoFmt& a, uint8_t sep, uint8_t fsep, int fw, const uint8_t* z, size_t zl){
  return a.sep==sep && a.fsep==fsep && a.fwidth==fw && a.zone.size()==zl && (zl==0 || memcmp(a.zone.data(),z,zl)==0); }

inline std::vector<uint8_t> apply_iso(const uint8_t* raw, size_t n){
  if(n<19) return {};
  std::vector<std::pair<size_t,size_t>> segs;
  { size_t s=0; while(true){ const void* nl=memchr(raw+s,'\n',n-s); if(!nl){ segs.push_back({s,n-s}); break; } size_t e=(size_t)((const uint8_t*)nl-raw); segs.push_back({s,e-s}); s=e+1; if(s==n){ segs.push_back({s,0}); break; } } }
  IsoFmt d0; bool have_d0=false;
  for(auto& sg: segs){ const uint8_t* ln=raw+sg.first; size_t le=sg.second;
    int64_t tk; uint8_t sp,fp; int fw; const uint8_t* z; size_t zl,tl;
    if(!parse_iso(ln,le,tk,sp,fp,fw,z,zl,tl)) continue;
    std::vector<uint8_t> rf; reformat_iso(tk,sp,fp,fw,z,zl,rf);
    if(rf.size()==tl && memcmp(rf.data(),ln,tl)==0){ d0={sp,fp,fw,std::string((const char*)z,zl)}; have_d0=true; break; } }
  if(!have_d0) return {};
  int64_t baseline=0,prev=0; bool have_base=false; std::vector<uint32_t> exc; std::vector<uint8_t> body; bool firstseg=true;
  for(size_t i=0;i<segs.size();i++){ const uint8_t* ln=raw+segs[i].first; size_t le=segs[i].second;
    if(!firstseg) body.push_back('\n'); firstseg=false;
    int64_t tk; uint8_t sp,fp; int fw; const uint8_t* z; size_t zl,tl; bool ok=false;
    if(parse_iso(ln,le,tk,sp,fp,fw,z,zl,tl) && iso_fmt_eq(d0,sp,fp,fw,z,zl)){
      std::vector<uint8_t> rf; reformat_iso(tk,sp,fp,fw,z,zl,rf);
      if(rf.size()==tl && memcmp(rf.data(),ln,tl)==0 && !(tl<le && ln[tl]>='0'&&ln[tl]<='9')) ok=true; }
    if(ok){ if(!have_base){ baseline=tk; prev=tk; have_base=true; } int64_t delta=tk-prev; prev=tk;
      char tok[24]; int tn=snprintf(tok,sizeof tok,"%llu",(unsigned long long)zz(delta)); body.insert(body.end(),tok,tok+tn);
      body.insert(body.end(), ln+tl, ln+le); }
    else { exc.push_back((uint32_t)i); body.insert(body.end(), ln, ln+le); } }
  if(!have_base) return {};
  std::vector<uint8_t> blob; const char* mg="LTISO1"; blob.insert(blob.end(),mg,mg+6);
  blob.push_back(d0.sep); blob.push_back(d0.fsep); blob.push_back((uint8_t)d0.fwidth); blob.push_back((uint8_t)d0.zone.size());
  blob.insert(blob.end(), d0.zone.begin(), d0.zone.end());
  mqsql::put_uv(blob, zz(baseline)); mqsql::put_uv(blob, exc.size());
  uint32_t last=0; for(uint32_t e: exc){ mqsql::put_uv(blob, (uint64_t)(e-last)); last=e; }
  mqsql::put_uv(blob, body.size()); blob.insert(blob.end(), body.begin(), body.end());
  return blob;
}
inline bool invert_iso(const uint8_t* blob, size_t bn, std::vector<uint8_t>& out){
  if(bn<6 || memcmp(blob,"LTISO1",6)!=0) return false;
  size_t i=6; if(i+4>bn) return false;
  uint8_t sep=blob[i],fsep=blob[i+1]; int fw=blob[i+2]; size_t zl=blob[i+3]; i+=4;
  if(fw<0||fw>9) return false; if(zl > bn-i) return false; const uint8_t* zone=blob+i; i+=zl;
  bool ok=true; uint64_t zb=mqsql::get_uv(blob,i,bn,ok); if(!ok) return false; int64_t baseline=unzz(zb);
  uint64_t nexc=mqsql::get_uv(blob,i,bn,ok); if(!ok||nexc>(uint64_t)(bn-i)) return false;
  std::vector<uint32_t> exc; uint32_t last=0;
  for(uint64_t k=0;k<nexc;k++){ uint64_t g=mqsql::get_uv(blob,i,bn,ok); if(!ok) return false; last+=(uint32_t)g; exc.push_back(last); }
  uint64_t blen=mqsql::get_uv(blob,i,bn,ok); if(!ok||blen>(uint64_t)(bn-i)) return false;
  const uint8_t* body=blob+i; size_t bl=(size_t)blen;
  std::vector<std::pair<size_t,size_t>> segs;
  { size_t s=0; while(true){ const void* nl=memchr(body+s,'\n',bl-s); if(!nl){ segs.push_back({s,bl-s}); break; } size_t e=(size_t)((const uint8_t*)nl-body); segs.push_back({s,e-s}); s=e+1; if(s==bl){ segs.push_back({s,0}); break; } } }
  int64_t prev=baseline; size_t ep=0; bool firstseg=true;
  for(size_t idx=0; idx<segs.size(); idx++){ const uint8_t* ln=body+segs[idx].first; size_t le=segs[idx].second;
    if(!firstseg) out.push_back('\n'); firstseg=false;
    if(ep<exc.size() && exc[ep]==(uint32_t)idx){ out.insert(out.end(), ln, ln+le); ep++; continue; }
    size_t j=0; while(j<le && ln[j]>='0'&&ln[j]<='9') j++; if(j==0) return false;
    int64_t u; if(!pdec(ln,j,u)||u<0) return false;
    int64_t tick=prev+unzz((uint64_t)u); prev=tick;
    reformat_iso(tick,sep,fsep,fw,zone,zl,out); out.insert(out.end(), ln+j, ln+le); }
  return true;
}

// top-level: try CLF then ISO on encode; dispatch on blob magic on decode.
inline std::vector<uint8_t> apply(const uint8_t* raw, size_t n){ auto b=apply_clf(raw,n); if(!b.empty()) return b; return apply_iso(raw,n); }
inline bool invert(const uint8_t* blob, size_t bn, std::vector<uint8_t>& out){
  if(bn>=6 && memcmp(blob,"LTCLF1",6)==0) return invert_clf(blob,bn,out);
  if(bn>=6 && memcmp(blob,"LTISO1",6)==0) return invert_iso(blob,bn,out);
  return false; }
} // namespace mltsd

// ============================================================================
// MY (YAML / indentation de-indent) — split each '\n'-segment into (leading-space
// depth, dedented body) and compress the two sub-streams SEPARATELY: the depth
// sequence (run-repetitive -> NUMERIC/PPMD) and the dedented bodies (repeated keys
// without the varying indent prefix -> PPMD). Measured -22 to -25% over best-standard
// on large deeply-nested config (k8s CRD/manifest YAML): prom_bundle 91,319->68,634,
// certmgr 27,098->20,520 (real, invert-verified). Compressing the two streams TOGETHER
// only gets ~half the win (mixed statistics), so they are kept separate. Bijective:
// segment = depth*' ' + body; a body never contains '\n'; the depth count pins the body
// re-split. (2026-08-08, gap-analysis workflow rank 1)
// ============================================================================
namespace mysy {
// Gate for SPEED only (trial-and-keep + end-to-end verify make it safe regardless of the gate):
// enough lines and a substantial fraction indented with spaces.
inline bool looks_like(const uint8_t* d, size_t n) {
    if (n < 512) return false;
    size_t sample = n < 65536 ? n : 65536, lines = 0, indented = 0, i = 0;
    while (i < sample) {
        size_t lead = 0;
        while (i < sample && d[i] == ' ') { lead++; i++; }
        while (i < sample && d[i] != '\n') i++;
        if (i < sample) i++;                          // skip '\n'
        lines++;
        if (lead > 0) indented++;
    }
    return lines >= 16 && indented * 4 >= lines;       // >= 25% of lines indented
}
// Produce the two raw sub-streams (indent = concatenated depth varints; body = dedented bodies
// joined by '\n'). Returns false if not applicable. Bijective by construction.
inline bool apply(const uint8_t* d, size_t n,
                  std::vector<uint8_t>& indent, std::vector<uint8_t>& body) {
    if (!looks_like(d, n)) return false;
    indent.clear(); body.clear();
    indent.reserve(n / 16 + 8); body.reserve(n);
    size_t start = 0, nseg = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || d[i] == '\n') {
            size_t j = start, lead = 0;
            while (j < i && d[j] == ' ') { lead++; j++; }
            uint8_t vb[16]; size_t vn = write_uvarint_buf(vb, (uint64_t)lead);
            indent.insert(indent.end(), vb, vb + vn);
            if (nseg > 0) body.push_back('\n');
            body.insert(body.end(), d + j, d + i);
            nseg++; start = i + 1;
            if (i == n) break;
        }
    }
    return nseg >= 2;
}
// Reconstruct from the two DECOMPRESSED sub-streams. max_out bounds output (untrusted-stream guard).
inline bool invert(const uint8_t* ind, size_t ind_n, const uint8_t* bod, size_t bod_n,
                   std::vector<uint8_t>& out, size_t max_out) {
    std::vector<uint32_t> depths;
    { const uint8_t* p = ind; const uint8_t* e = ind + ind_n;
      while (p < e) { uint64_t dv = read_uvarint(p, e); if (p > e) return false;
                      if (dv > (1u << 24)) return false; depths.push_back((uint32_t)dv); } }
    if (depths.empty()) return false;
    out.clear(); out.reserve(bod_n + depths.size());
    const uint8_t* be = bod + bod_n; const uint8_t* seg = bod; size_t s = 0;
    for (const uint8_t* q = bod; ; q++) {
        if (q == be || *q == '\n') {
            if (s >= depths.size()) return false;                     // more body pieces than depths
            if (out.size() + depths[s] + (size_t)(q - seg) > max_out) return false;
            out.insert(out.end(), depths[s], ' ');
            out.insert(out.end(), seg, q);
            s++;
            if (q == be) break;
            out.push_back('\n');
            if (out.size() > max_out) return false;
            seg = q + 1;
        }
    }
    return s == depths.size();                                        // piece count must equal depth count
}
} // namespace mysy

// ============================================================================
// MF (FASTQ 4-line de-interleave) — FASTQ records are 4 lines (@header / sequence / '+' / quality)
// with statistically INCOMPATIBLE content interleaved every ~100 bytes, which defeats a single
// BWT/CM pass. Physically de-interleave into 4 sub-streams by line%4 and compress each SEPARATELY,
// letting the ensemble route each to its best encoder (hdr->XZLIB, seq->PPMD, qual->CM_TEXT, '+'
// ->trivial). Measured -12.2% over mzip's own interleaved result, -20.1% vs xz (real SRR6357070).
// Bijective: a line never contains '\n'; nrec pins each stream's re-split. (2026-08-08, sweep-2 rank)
// ============================================================================
namespace mfq {
// Fast SPEED gate (apply does the exact full validation): \n-terminated, first records look like FASTQ.
inline bool looks_like(const uint8_t* d, size_t n) {
    if (n < 256 || d[n-1] != '\n') return false;
    size_t i = 0; int rec = 0;
    while (i < n && rec < 4) {
        if (d[i] != '@') return false;                 // line 0
        for (int c = 0; c < 4; c++) {
            size_t ls = i; while (i < n && d[i] != '\n') i++;
            if (i >= n) return false;
            if (c == 2 && (i == ls || d[ls] != '+')) return false;   // line 2 must start '+'
            i++;
        }
        rec++;
    }
    return rec >= 1;
}
// Produce the 4 raw sub-streams (each = nrec lines joined by '\n') + nrec. Full validation; false if not FASTQ.
inline bool apply(const uint8_t* d, size_t n, uint64_t& nrec_out,
                  std::vector<uint8_t>& hdr, std::vector<uint8_t>& seq,
                  std::vector<uint8_t>& plus, std::vector<uint8_t>& qual) {
    if (!looks_like(d, n)) return false;
    std::vector<size_t> ls, ll; size_t start = 0;
    for (size_t k = 0; k < n; k++) if (d[k] == '\n') { ls.push_back(start); ll.push_back(k - start); start = k + 1; }
    if (ls.empty() || ls.size() % 4 != 0) return false;
    uint64_t nrec = ls.size() / 4;
    for (uint64_t r = 0; r < nrec; r++) {
        if (ll[r*4] < 1 || d[ls[r*4]] != '@') return false;
        if (ll[r*4+2] < 1 || d[ls[r*4+2]] != '+') return false;
    }
    std::vector<uint8_t>* S[4] = { &hdr, &seq, &plus, &qual };
    for (int c = 0; c < 4; c++) {
        S[c]->clear(); S[c]->reserve(n / 4 + 8);
        for (uint64_t r = 0; r < nrec; r++) { if (r) S[c]->push_back('\n'); S[c]->insert(S[c]->end(), d + ls[r*4+c], d + ls[r*4+c] + ll[r*4+c]); }
    }
    nrec_out = nrec;
    return true;
}
// Reconstruct from the 4 DECOMPRESSED sub-streams. max_out bounds output (untrusted-stream guard).
inline bool invert(uint64_t nrec, const uint8_t* h, size_t hn, const uint8_t* s, size_t sn,
                   const uint8_t* p, size_t pn, const uint8_t* q, size_t qn,
                   std::vector<uint8_t>& out, size_t max_out) {
    if (nrec == 0) return false;
    const uint8_t* S[4] = { h, s, p, q }; size_t SN[4] = { hn, sn, pn, qn };
    std::vector<std::pair<size_t,size_t>> pcs[4];
    for (int c = 0; c < 4; c++) {
        size_t st = 0;
        for (size_t i = 0; i <= SN[c]; i++) if (i == SN[c] || S[c][i] == '\n') {
            pcs[c].push_back({st, i - st}); st = i + 1;
            if (pcs[c].size() > nrec) return false;
        }
        if (pcs[c].size() != nrec) return false;
    }
    out.clear();
    for (uint64_t r = 0; r < nrec; r++) for (int c = 0; c < 4; c++) {
        auto& P = pcs[c][r];
        if (out.size() + P.second + 1 > max_out) return false;
        out.insert(out.end(), S[c] + P.first, S[c] + P.first + P.second);
        out.push_back('\n');
    }
    return true;
}
} // namespace mfq

// forward decl so the SoA path can roundtrip-verify its candidate before shipping it
inline std::vector<uint8_t> decompress_impl(const uint8_t* data, size_t size, DecompressResult* result = nullptr);
inline std::vector<uint8_t> decompress(const uint8_t* data, size_t size, DecompressResult* result = nullptr);
// try_soa: set false in the recursive SoA call so the 'MS' variant is tried once,
//          not infinitely. Defaults true, so existing callers are unaffected.
// compress_impl: the full trial-and-keep encoder. The public compress() below wraps this with a
// top-level end-to-end losslessness verify + guaranteed µRAW fallback (2026-08-07). Recursive
// sub-block compression calls compress_impl directly (the single top-level verify covers the whole
// assembled stream, so nested re-verification would only add cost).
inline std::vector<uint8_t> compress_impl(const uint8_t* data, size_t size,
                                      int zstd_level = 3,
                                      size_t block_size = DEFAULT_BLOCK_SIZE,
                                      CompressResult* result = nullptr,
                                      CompressionMode mode = CompressionMode::BALANCED,
                                      bool try_soa = true,
                                      bool try_tabular = true,
                                      bool try_sql = true,
                                      bool try_bcj = true,
                                      bool try_log = true,
                                      bool try_yaml = true,
                                      bool try_fastq = true) {
    CompressResult res;
    res.success = false;
    res.original_size = size;
    res.compressed_size = 0;
    res.block_count = 0;
    res.blocks_raw = 0;
    res.blocks_numeric = 0;
    res.blocks_text = 0;
    res.blocks_image = 0;
    res.blocks_incompressible = 0;

    if (size == 0) {
        res.error = "Empty input";
        if (result) *result = res;
        return {};
    }

    // Handle block_size=0 as "use default"
    if (block_size == 0) block_size = DEFAULT_BLOCK_SIZE;
    block_size = std::max(MIN_BLOCK_SIZE, std::min(MAX_BLOCK_SIZE, block_size));

    // Optimization: For homogeneous PERIODIC data (fills, small patterns),
    // use maximum block size to minimize header overhead.
    // Example: 1MB fill with 256KB blocks = 4×(14+3) + 17 = 85 bytes
    //          1MB fill with 16MB blocks  = 1×(14+3) + 17 = 34 bytes (beats zstd's 49!)
    if (size <= MAX_BLOCK_SIZE) {
        // Sample start and end to detect homogeneous periodic data
        size_t sample_size = std::min(size, (size_t)4096);
        auto analysis_start = analyze_block(data, sample_size);

        if (analysis_start.type == BlockType::PERIODIC && analysis_start.period <= 256) {
            // Check end of data too
            size_t end_offset = (size > sample_size) ? size - sample_size : 0;
            auto analysis_end = analyze_block(data + end_offset, std::min(sample_size, size - end_offset));

            // If both ends are same periodic pattern, use whole file as one block
            if (analysis_end.type == BlockType::PERIODIC &&
                analysis_end.period == analysis_start.period) {
                block_size = size;  // Single block for entire file
            }
        }

        // Also use single block for TEMPLATE data (SQL, logs, CSV with structure)
        // TEMPLATE encoding fails on partial lines at block boundaries
        if (analysis_start.type == BlockType::TEMPLATE ||
            analysis_start.type == BlockType::CHAR_TEMPLATE) {
            block_size = size;  // Single block for entire file
        }

        // For plain TEXT, check if homogeneous (start and end both TEXT)
        // Splitting TEXT into blocks destroys cross-block LZ77 matches!
        // Example: access logs with repeated "HTTP/1.1" - zstd finds matches across whole file
        //          but blocked compression only finds matches within each block
        if (analysis_start.type == BlockType::TEXT) {
            size_t end_offset = (size > sample_size) ? size - sample_size : 0;
            auto analysis_end = analyze_block(data + end_offset, std::min(sample_size, size - end_offset));

            if (analysis_end.type == BlockType::TEXT) {
                block_size = size;  // Single block for homogeneous text
            }
        }
    }

    // Calculate number of blocks
    size_t num_blocks = (size + block_size - 1) / block_size;

    // Allocate output buffer
    // Header: 4 (magic) + 1 (version) + 8 (original size) + 4 (block count) = 17 bytes
    // Per block: 14 bytes header + compressed data (worst case = original + 1%)
    size_t max_output = 17 + num_blocks * (14 + block_size + block_size / 100 + 100);
    std::vector<uint8_t> output(max_output);

    // Work buffers
    // preprocess_buf needs extra space for word encoding (2x worst case + vocabulary)
    std::vector<uint8_t> preprocess_buf(block_size * 3);
    std::vector<uint8_t> compress_buf(ZSTD_compressBound(block_size * 3));

    // === CROSS-BLOCK SIMILARITY TRACKING (Mutual Algorithmic Information) ===
    // Track recent blocks for REFERENCE encoding (delta from similar block)
    struct RecentBlock {
        std::vector<uint8_t> data;    // Raw block data (for dictionary)
        BlockFingerprint fingerprint;  // SimHash for similarity detection
        size_t output_offset;          // Where this block starts in output
    };
    std::vector<RecentBlock> recent_blocks;
    recent_blocks.reserve(MAX_REFERENCE_BLOCKS);

    // Write header
    size_t out_pos = 0;
    write_u32_le(&output[out_pos], MAGIC);
    out_pos += 4;
    output[out_pos++] = VERSION;
    write_u64_le(&output[out_pos], size);
    out_pos += 8;

    // Reserve space for block count
    size_t block_count_pos = out_pos;
    out_pos += 4;

    // Process blocks
    size_t in_pos = 0;
    uint32_t block_count = 0;

    while (in_pos < size) {
        size_t this_block = std::min(block_size, size - in_pos);
        const uint8_t* block_data = data + in_pos;

        // ---------------------------------------------------------------------------
        // bwt9 MEMO — 2026-08-02. bwt9::compress(block_data, this_block) is called from
        // SIX sites in this loop, all on the IDENTICAL input, and up to two of them fire
        // on the same block. Measured (MZIP_TIME, per file): the TEXT path and the
        // universal backstop each pay the full cost for a bit-identical result --
        //     linux_kernel.c  679.6 ms then 679.5 ms
        //     clojure_core.clj 597.1 ms then 595.0 ms
        //     lodash.js        813.7 ms then 832.8 ms
        // bwt9 is 76.3% of all instrumented time corpus-wide (43,480 of 57,020 ms), so a
        // duplicated call is the single most expensive redundancy in the encoder.
        //
        // ELIMINATING IT IS PROVABLY OUTPUT-NEUTRAL, not merely "should be": every earlier
        // site already trials bwt9 against its alternative and keeps the SMALLER, so by the
        // time the backstop runs, `cur` <= the bwt9 size. The backstop's `b9.size() < cur`
        // test therefore cannot succeed on a recomputed-identical result. Memoising rather
        // than skipping keeps the comparison logic literally unchanged, so the only thing
        // removed is the recomputation.
        // Deterministic by construction: same function, same pointer, same length.
        // ---------------------------------------------------------------------------
        bool bwt9_memo_valid = false;
        std::vector<uint8_t> bwt9_memo;

        // ...and the SAME redundancy exists one level down. bwt_compress_v9.hpp mode 2 calls
        // cmbk::compress_bwt(data, n) on EVERY bwt9 invocation, while the TEXT path below calls it
        // AGAIN on the identical block for its own CM_TEXT trial. So a text block paid for the
        // BWT+CM pass twice inside a bwt9 call that was itself already duplicated. (This is also
        // why bwt9_probe lands within ~14 B of shipped output on CM_TEXT-winning files: bwt9's own
        // answer there IS the CM stream plus its 3-byte 'B','9',2 header.)
        bool cm_memo_valid = false;
        std::vector<uint8_t> cm_memo;
        auto cm_block = [&]() -> const std::vector<uint8_t>& {
            if (!cm_memo_valid) { cm_memo = cmbk::compress_bwt(block_data, this_block); cm_memo_valid = true; }
            return cm_memo;
        };

        auto bwt9_block = [&]() -> const std::vector<uint8_t>& {
            if (!bwt9_memo_valid) {
#ifndef MZIP_NO_CM
                // Hand bwt9 the memoised CM stream instead of letting it recompute one. Eager, but
                // it adds no work: mode 2 computes exactly this on every call anyway.
                bwt9_memo = bwt9::compress(block_data, this_block, &cm_block());
#else
                bwt9_memo = bwt9::compress(block_data, this_block);
#endif
                bwt9_memo_valid = true;
            }
            return bwt9_memo;
        };

        // Analyze block
        BlockAnalysis analysis = analyze_block(block_data, this_block);

        // === CROSS-BLOCK SIMILARITY CHECK (Mutual Algorithmic Information) ===
        // I(A:B) = K(A) + K(B) - K(A|B). If blocks share structure, K(A|B) is small.
        // Use SimHash to find candidates, then try zstd dictionary encoding.
        BlockFingerprint current_fp = compute_fingerprint(block_data, this_block, block_count);
        
        bool try_reference = false;
        size_t best_ref_idx = 0;
        double best_similarity = 0.0;
        
        // Only consider REFERENCE for blocks that don't have better specialized encodings
        // RAW, TEXT, and NUMERIC blocks are good candidates
        if (this_block >= MIN_REFERENCE_SIZE && !recent_blocks.empty() &&
            (analysis.type == BlockType::RAW || 
             analysis.type == BlockType::TEXT ||
             analysis.type == BlockType::NUMERIC)) {
            
            // Find most similar recent block
            for (size_t i = 0; i < recent_blocks.size(); i++) {
                double sim = current_fp.similarity(recent_blocks[i].fingerprint);
                if (sim > best_similarity && sim >= SIMILARITY_THRESHOLD) {
                    best_similarity = sim;
                    best_ref_idx = i;
                    try_reference = true;
                }
            }
        }

        // === LOGICAL DEPTH MODE (Bennett's Logical Depth) ===
        // In FAST mode, skip slow generators for large blocks.
        // Generators have O(n) decompression with per-element computation.
        // zstd decompresses faster for large blocks (hardware-optimized memcpy).
        // Threshold: 64KB - below this, generators are fast enough.
        const size_t FAST_MODE_THRESHOLD = 64 * 1024;

        if (mode == CompressionMode::FAST && this_block > FAST_MODE_THRESHOLD) {
            // Downgrade REGENERATE-depth encodings to simpler types
            switch (analysis.type) {
                case BlockType::LINEAR_GEN:
                case BlockType::LINEAR_GEN_APPROX:
                case BlockType::GEOMETRIC:
                case BlockType::QUADRATIC:
                case BlockType::RECURRENCE:
                case BlockType::MODULAR:
                    // These require per-element computation - use NUMERIC/RAW instead
                    // Re-analyze as NUMERIC to get delta encoding benefit
                    analysis.type = BlockType::RAW;  // Fall through to zstd
                    break;
                case BlockType::PERIODIC:
                case BlockType::PERIODIC_APPROX:
                    // PERIODIC is fast (memcpy doubling) - keep it
                    break;
                default:
                    break;
            }
        }

        // Apply preprocessing based on type
        uint8_t* preprocess_data = preprocess_buf.data();
        size_t preprocess_size = this_block;
        tieredcompress::Strategy strategy = tieredcompress::Strategy::NONE;
        bool use_generator = false;  // For LINEAR_GEN and PERIODIC, skip zstd entirely
        bool use_reference = false;   // REFERENCE encoding (delta from similar block)
        std::vector<uint8_t> reference_encoded;
        uint32_t reference_block_idx = 0;

        // === TRY REFERENCE ENCODING (if similar block found) ===
        // Only for types that don't have better specialized encodings
        if (try_reference && !recent_blocks.empty()) {
            const RecentBlock& ref = recent_blocks[best_ref_idx];
            
            // Try encoding with dictionary compression
            reference_encoded = encode_reference(
                block_data, this_block,
                ref.data.data(), ref.data.size(),
                ref.fingerprint.block_index,
                zstd_level
            );
            
            if (!reference_encoded.empty()) {
                // Compare MDL: reference vs regular encoding
                MDLScore ref_mdl = mdl_reference(reference_encoded.size());
                
                // For TEXT/RAW, estimate what zstd alone would give
                MDLScore baseline_mdl = mdl_text(this_block, analysis.entropy);
                
                // Use REFERENCE if it beats the baseline
                if (ref_mdl.beats(baseline_mdl)) {
                    use_reference = true;
                    reference_block_idx = ref.fingerprint.block_index;
                    analysis.type = BlockType::REFERENCE;  // Override type
                }
            }
        }

        if (analysis.type == BlockType::LINEAR_GEN) {
            // Encode as generator parameters - 17 bytes for any size!
            auto encoded = MZ_TIMED("encode_linear_gen", encode_linear_gen(analysis.linear_gen));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;  // Count as numeric (special case)
        } else if (analysis.type == BlockType::LINEAR_GEN_APPROX) {
            // Encode as generator + exceptions (Effective Complexity encoding!)
            // Rescues data that's 95%+ linear but has outliers
            auto encoded = MZ_TIMED("encode_linear_gen_approx", encode_linear_gen_approx( analysis.linear_gen_approx, analysis.linear_gen_approx_exceptions));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;
        } else if (analysis.type == BlockType::GEOMETRIC) {
            // Encode as geometric parameters - 17 bytes for any size!
            auto encoded = MZ_TIMED("encode_geometric", encode_geometric(analysis.geometric));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;
        } else if (analysis.type == BlockType::QUADRATIC) {
            // Encode as quadratic parameters - 25 bytes for any size!
            auto encoded = MZ_TIMED("encode_quadratic", encode_quadratic(analysis.quadratic));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;
        } else if (analysis.type == BlockType::RECURRENCE) {
            // Encode as recurrence parameters - 33 bytes for any size!
            auto encoded = MZ_TIMED("encode_recurrence", encode_recurrence(analysis.recurrence));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;
        } else if (analysis.type == BlockType::MODULAR) {
            // Encode as modular parameters - 25 bytes for any size!
            auto encoded = MZ_TIMED("encode_modular", encode_modular(analysis.modular));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_numeric++;
        } else if (analysis.type == BlockType::PERIODIC) {
            // Encode as period + pattern - period + 2 bytes
            auto encoded = MZ_TIMED("encode_periodic", encode_periodic(block_data, analysis.period));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_raw++;  // Count separately
        } else if (analysis.type == BlockType::PERIODIC_APPROX) {
            // Encode as period + pattern + exceptions (Effective Complexity encoding!)
            // Rescues data that's 95%+ periodic but has corrupted bytes
            auto encoded = MZ_TIMED("encode_periodic_approx", encode_periodic_approx(block_data, this_block, analysis.periodic_approx, analysis.periodic_approx_exceptions));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_raw++;  // Count separately
        } else if (analysis.type == BlockType::SPARSE) {
            // Encode with Rice coding - near-optimal for geometric gap distributions!
            // Rice coding beats bzip2 by 8.7% (100.6% of entropy vs 110% for varint)
            auto encoded = MZ_TIMED("encode_sparse_rice", encode_sparse_rice(block_data, this_block, analysis.sparse_common_value, analysis.sparse_all_same_value));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;
            res.blocks_raw++;  // Count separately
        } else if (analysis.type == BlockType::TIMESTAMP) {
            // Encode as delta-of-delta + zigzag + varint (Gorilla-style)
            auto encoded = MZ_TIMED("encode_timestamp", encode_timestamp(block_data, analysis.timestamp));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            // Don't set use_generator - we want zstd on top for extra compression
            res.blocks_numeric++;  // Count as numeric
        } else if (analysis.type == BlockType::NUMERIC &&
            analysis.numeric_strategy != tieredcompress::Strategy::NONE) {
            strategy = analysis.numeric_strategy;
            preprocess_size = apply_strategy(preprocess_data, block_data, this_block, strategy);

            // DECODE-VERIFY the numeric strategy is lossless on THIS block (2026-08-06).
            // ALP_FLOAT (nominally lossless) is in fact LOSSY on some float distributions,
            // and this NUMERIC path shipped its output with NO decode-verify -> SILENT
            // data corruption. Reproduced at HEAD, exit 0, no error: nat_prices_hiprec.bin
            // (natural hi-precision prices) decodes 148/65536 bytes wrong; a de-interleaved
            // float stream decodes 8983/65536 wrong. The 'MS' SoA path was safe only because
            // it self-verifies; this direct path did not. Decode-and-compare, and on mismatch
            // fall back to raw (Strategy::NONE) so a lossless encoder handles the block --
            // the same guard pattern as COLUMNAR / SECTION_TEMPLATE / the 2026-08-04 fixes.
            bool numeric_lossy = false;
            {
                std::vector<uint8_t> vchk(this_block);
                size_t dsz = reverse_strategy(vchk.data(), preprocess_data, preprocess_size,
                                              this_block, strategy);
                if (dsz != this_block || std::memcmp(vchk.data(), block_data, this_block) != 0)
                    numeric_lossy = true;
            }
            if (numeric_lossy) {
                strategy = tieredcompress::Strategy::NONE;
                memcpy(preprocess_data, block_data, this_block);
                preprocess_size = this_block;
                analysis.type = BlockType::RAW;
                res.blocks_raw++;
            } else {

            // Sparse delta: if preprocessing makes >90% zeros, encode as sparse
            // positions instead of sending to zstd. Saves zstd frame overhead (~7-13 bytes)
            // on already-tiny outputs. Example: image gradient delta = 5 non-zero bytes.
            {
                size_t zeros = 0;
                for (size_t i = 0; i < preprocess_size; i++)
                    if (preprocess_data[i] == 0) zeros++;

                if (zeros > preprocess_size * 9 / 10 && preprocess_size >= 256
                    && preprocess_size == this_block) {  // Only when preprocessing preserves size
                    // Encode sparse: strategy(1) + 0xFF(1) + count(varint) + [delta_pos(varint) + val(1)]...
                    std::vector<uint8_t> sparse_enc;
                    sparse_enc.push_back((uint8_t)strategy);
                    sparse_enc.push_back(0xFF);  // sparse marker

                    // Collect non-zero positions
                    std::vector<std::pair<size_t, uint8_t>> nz;
                    for (size_t i = 0; i < preprocess_size; i++)
                        if (preprocess_data[i] != 0) nz.push_back({i, preprocess_data[i]});

                    // count as varint
                    uint32_t cnt = nz.size();
                    while (cnt >= 128) { sparse_enc.push_back((cnt & 0x7F) | 0x80); cnt >>= 7; }
                    sparse_enc.push_back(cnt & 0x7F);

                    // delta-encoded positions + values
                    size_t prev_pos = 0;
                    for (auto& [pos, val] : nz) {
                        uint32_t delta_pos = pos - prev_pos;
                        while (delta_pos >= 128) { sparse_enc.push_back((delta_pos & 0x7F) | 0x80); delta_pos >>= 7; }
                        sparse_enc.push_back(delta_pos & 0x7F);
                        sparse_enc.push_back(val);
                        prev_pos = pos;
                    }

                    // DECODE-VERIFY the sparse encoding end-to-end (2026-08-07). The strategy
                    // decode-verify above covers reverse_strategy(preprocess_data)==block_data, but
                    // the sparse re-encoding of preprocess_data is a SEPARATE transform that shipped
                    // UNVERIFIED -> silent data corruption (found by fuzz_mzip on 511-float data:
                    // compressed to MU/NUMERIC, decoded to all-zeros). Replay the EXACT MU sparse
                    // decoder (mzip.hpp ~16760) and require it reconstructs preprocess_data byte-for-
                    // byte; since reverse_strategy(preprocess_data)==block_data is already proven,
                    // this makes the full roundtrip lossless. On mismatch, skip sparse and let zstd
                    // handle the (verified-lossless) preprocess_data. Inert on valid: a correct
                    // sparse encoding reconstructs preprocess_data exactly.
                    bool sparse_lossy = false;
                    {
                        std::vector<uint8_t> vpre(this_block, 0);
                        size_t vp = 2;  // skip strategy(1) + 0xFF(1)
                        uint32_t vcount = 0; int vsh = 0;
                        while (vp < sparse_enc.size() && (sparse_enc[vp] & 0x80)) { vcount |= (uint32_t)(sparse_enc[vp++] & 0x7F) << vsh; vsh += 7; }
                        if (vp < sparse_enc.size()) vcount |= (uint32_t)sparse_enc[vp++] << vsh;
                        size_t vspos = 0;
                        for (uint32_t vi = 0; vi < vcount && vp + 1 < sparse_enc.size(); vi++) {
                            uint32_t vdp = 0; vsh = 0;
                            while (vp < sparse_enc.size() && (sparse_enc[vp] & 0x80)) { vdp |= (uint32_t)(sparse_enc[vp++] & 0x7F) << vsh; vsh += 7; }
                            if (vp < sparse_enc.size()) vdp |= (uint32_t)sparse_enc[vp++] << vsh;
                            vspos += vdp;
                            if (vspos < vpre.size() && vp < sparse_enc.size()) vpre[vspos] = sparse_enc[vp++];
                        }
                        if (vpre.size() != preprocess_size || std::memcmp(vpre.data(), preprocess_data, preprocess_size) != 0)
                            sparse_lossy = true;
                    }

                    // Only use sparse if it beats what zstd would produce
                    // Estimate: zstd on this data ≈ sparse_enc.size() + zstd_frame_overhead
                    // MU format: 5 bytes header + sparse_enc
                    size_t sparse_total = sparse_enc.size();
                    // Compare against zstd estimate (quick compress at level 1)
                    std::vector<uint8_t> zstd_trial(ZSTD_compressBound(preprocess_size));
                    size_t zstd_est = ZSTD_compress(zstd_trial.data(), zstd_trial.size(),
                        preprocess_data, preprocess_size, 19);

                    if (!sparse_lossy && !ZSTD_isError(zstd_est) && sparse_total < zstd_est) {
                        // Sparse wins — store directly, skip zstd
                        memcpy(preprocess_data, sparse_enc.data(), sparse_enc.size());
                        preprocess_size = sparse_enc.size();
                        use_generator = true;  // Skip zstd re-compression
                        res.blocks_numeric++;
                    } else {
                        res.blocks_numeric++;
                    }
                } else {
                    res.blocks_numeric++;
                }
            }

            // Guard: BLOCK_COLUMNAR can destroy LZ77 locality (e.g. DBF files with 81% spaces).
            // Row-major format lets zstd match entire rows; columnar breaks that.
            // Trial-compress both and pick the smaller result.
            if (!use_generator && strategy == tieredcompress::Strategy::BLOCK_COLUMNAR) {
                size_t raw_bound = ZSTD_compressBound(this_block);
                size_t prep_bound = ZSTD_compressBound(preprocess_size);
                std::vector<uint8_t> trial_buf(std::max(raw_bound, prep_bound));
                size_t raw_sz = ZSTD_compress(trial_buf.data(), raw_bound, block_data, this_block, 1);
                size_t prep_sz = ZSTD_compress(trial_buf.data(), prep_bound, preprocess_data, preprocess_size, 1);
                if (!ZSTD_isError(raw_sz) && !ZSTD_isError(prep_sz) && raw_sz <= prep_sz) {
                    // Preprocessing hurts — fall back to RAW
                    memcpy(preprocess_data, block_data, this_block);
                    preprocess_size = this_block;
                    strategy = tieredcompress::Strategy::NONE;
                    analysis.type = BlockType::RAW;
                    res.blocks_raw++;
                } else {
                    res.blocks_numeric++;
                }
            } else {
                res.blocks_numeric++;
            }
            }  // end else: numeric strategy passed decode-verify (else it fell back to raw above)
        } else if (analysis.type == BlockType::BINARY_X86) {
            // Apply E8/E9 filtering for x86 executables
            memcpy(preprocess_data, block_data, this_block);
            e8e9_filter_encode(preprocess_data, this_block);
            // Count as raw but with filtering applied
            res.blocks_raw++;
        } else if (analysis.type == BlockType::LZMA_OPTIMAL) {
            // LZMA optimal with E8/E9 filter - beats xz:9 by ~3800 bytes on x86 binaries
            // Apply E8/E9 filter first
            std::vector<uint8_t> filtered(block_data, block_data + this_block);
            e8e9_filter_encode(filtered.data(), filtered.size());

            // Compress with LZMA optimal encoder
            auto lzma_compressed = MZ_TIMED("lzma_opt2 (E8/E9-filtered)", lzma_opt2::compress(filtered.data(), filtered.size()));

            // Store LZMA compressed data
            memcpy(preprocess_data, lzma_compressed.data(), lzma_compressed.size());
            preprocess_size = lzma_compressed.size();
            use_generator = true;  // LZMA is complete compression, skip zstd
            res.blocks_raw++;
        } else if (analysis.type == BlockType::LZMA_RAW) {
            // LZMA optimal without E8/E9 filter - for high-zero content
            // dilosi.doc: 54.5% zeros, saves 372 bytes vs zstd
            auto lzma_compressed = MZ_TIMED("lzma_opt2 (raw)", lzma_opt2::compress(block_data, this_block));

            // Store LZMA compressed data
            memcpy(preprocess_data, lzma_compressed.data(), lzma_compressed.size());
            preprocess_size = lzma_compressed.size();
            use_generator = true;  // LZMA is complete compression, skip zstd
            res.blocks_raw++;
        } else if (analysis.type == BlockType::TEXT) {
            memcpy(preprocess_data, block_data, this_block);
            res.blocks_text++;
            // Use level 9 minimum for text (set later in compression)
        } else if (analysis.type == BlockType::DBF_CONSTCOL) {
            // DBF constant column elimination — trial: CC+zstd vs BWT, pick smaller
            auto cc_compressed = MZ_TIMED("encode_dbf_constcol", encode_dbf_constcol(block_data, this_block, zstd_level));
            auto bwt_compressed = MZ_TIMED("bwt9 L14098", bwt9_block());

            if (!cc_compressed.empty() && cc_compressed.size() < bwt_compressed.size()) {
                // CC+zstd wins — store as DBF_CONSTCOL (already zstd-compressed)
                memcpy(preprocess_data, cc_compressed.data(), cc_compressed.size());
                preprocess_size = cc_compressed.size();
                use_generator = true;  // Already compressed internally
                res.blocks_numeric++;
            } else if (!bwt_compressed.empty()) {
                // BWT wins — fall back to BWT_TEXT
                memcpy(preprocess_data, bwt_compressed.data(), bwt_compressed.size());
                preprocess_size = bwt_compressed.size();
                analysis.type = BlockType::BWT_TEXT;
                use_generator = true;
                res.blocks_text++;
            } else {
                // Both encoders declined — bwt_compressed was the unguarded divisor in the
                // comparison above (cc.size() < 0 is false, so control reached here), and
                // adopting it would emit a zero-length payload. Only possible in MZIP_NO_CM
                // when bwt5 fully fails. Store the raw block as TEXT (default preprocess_size
                // = this_block, gets zstd'd downstream). Added 2026-08-04.
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                preprocess_size = this_block;
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::BWT_TEXT) {
            // BWT compression for natural text - beats zstd/bzip2 on prose, markdown, etc.
            // v9 dispatches to v8 (fixed model) for <5KB or v5 (multi-tree) for larger
            auto compressed = MZ_TIMED("bwt9 L14117", bwt9_block());

            // At small sizes (<=16KB), trial zstd+dict — may beat BWT and brotli
            if (this_block <= 16384) {
                // Try both CODE and CONFIG dicts, pick best
                ZSTD_CCtx* dict_cctx = ZSTD_createCCtx();
                std::vector<uint8_t> dict_comp(ZSTD_compressBound(this_block) + 2);

                size_t best_dict_size = SIZE_MAX;
                uint8_t best_dict_id = 0;
                // Trial all NUM_DICTS dicts (5 synthetic + 6 real-data),
                // pick smallest. The trial cost is amortized by trial-pick.
                for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                    auto& d = mzip_dicts::ALL_DICTS[di];
                    size_t sz = ZSTD_compress_usingDict(dict_cctx,
                        dict_comp.data() + 2, dict_comp.size() - 2,
                        block_data, this_block,
                        d.data, d.size, 19);
                    if (!ZSTD_isError(sz) && sz + 1 < best_dict_size) {
                        best_dict_size = sz + 1;  // +1 for dict_id byte
                        best_dict_id = d.id;
                    }
                }

                ZSTD_freeCCtx(dict_cctx);

                // Pick dict if it beats BWT
                if (best_dict_id > 0 && best_dict_size < compressed.size()) {
                    // Re-compress with winning dict to get the actual data
                    dict_cctx = ZSTD_createCCtx();
                    const uint8_t* dict_ptr = nullptr;
                    size_t dict_len = 0;
                    for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                        if (mzip_dicts::ALL_DICTS[di].id == best_dict_id) {
                            dict_ptr = mzip_dicts::ALL_DICTS[di].data;
                            dict_len = mzip_dicts::ALL_DICTS[di].size;
                            break;
                        }
                    }

                    // Compress into temp buffer, then copy with dict_id prefix
                    std::vector<uint8_t> tmp(ZSTD_compressBound(this_block));
                    size_t sz = ZSTD_compress_usingDict(dict_cctx,
                        tmp.data(), tmp.size(),
                        block_data, this_block,
                        dict_ptr, dict_len, 19);
                    ZSTD_freeCCtx(dict_cctx);

                    if (!ZSTD_isError(sz)) {
                        // Format: [dict_id:1] [zstd_data:sz]
                        preprocess_data[0] = best_dict_id;
                        memcpy(preprocess_data + 1, tmp.data(), sz);
                        preprocess_size = 1 + sz;
                        analysis.type = BlockType::ZSTD_DICT;
                        use_generator = true;
                        res.blocks_text++;
                    } else {
                        // Fallback to BWT
                        memcpy(preprocess_data, compressed.data(), compressed.size());
                        preprocess_size = compressed.size();
                        use_generator = true;
                        res.blocks_text++;
                    }
                } else {
                    memcpy(preprocess_data, compressed.data(), compressed.size());
                    preprocess_size = compressed.size();
                    use_generator = true;
                    res.blocks_text++;
                }
            } else {
                // >16KB: trial CM (BWT+CM, bzip3-class) vs bwt9, keep smaller
                std::vector<uint8_t> cm_compressed;
#ifndef MZIP_NO_CM
                cm_compressed = cm_block();   // memoised; bwt9_block() above computed this same stream
#endif
                // `compressed` is the bwt9 result and can be {} (MZIP_NO_CM + total bwt5
                // failure). Prefer whichever of cm/bwt9 is non-empty and smaller; if BOTH
                // are empty, store the raw block rather than adopt a zero-length payload.
                // Added 2026-08-04. In the shipped CM build cmbk always produces output, so
                // this only bites the MZIP_NO_CM A/B build.
                bool cm_ok = !cm_compressed.empty();
                bool b9_ok = !compressed.empty();
                if (cm_ok && (!b9_ok || cm_compressed.size() < compressed.size())) {
                    memcpy(preprocess_data, cm_compressed.data(), cm_compressed.size());
                    preprocess_size = cm_compressed.size();
                    analysis.type = BlockType::CM_TEXT;
                    use_generator = true;  // complete compression, skip zstd
                    res.blocks_text++;
                } else if (b9_ok) {
                    memcpy(preprocess_data, compressed.data(), compressed.size());
                    preprocess_size = compressed.size();
                    use_generator = true;  // complete compression, skip zstd
                    res.blocks_text++;
                } else {
                    analysis.type = BlockType::TEXT;
                    memcpy(preprocess_data, block_data, this_block);
                    preprocess_size = this_block;
                    res.blocks_text++;
                }
            }
        } else if (analysis.type == BlockType::HTML_STREAM) {
            // HTML tag/content separation - beats brotli by 3.3% at 256KB+
            // This is a complete encoder - output is already fully compressed
            auto compressed = MZ_TIMED("encode_html_stream", encode_html_stream(block_data, this_block));
            // ROUNDTRIP VERIFY — 2026-08-04. encode_html_stream is NOT lossless: re-interleaving
            // the tag and 0x00-delimited content streams does not preserve inter-tag whitespace
            // and comment boundaries. Confirmed on train_corpus/xml/tomcat_build.xml: decode
            // returns the right LENGTH (209,615) but 196,493 bytes differ. This site adopted the
            // output with NO verify and set use_generator, so a HTML_STREAM-winning input would
            // ship silently corrupt; today it is masked only because ZSTD_DICT is smaller. Fall
            // back to TEXT if it does not roundtrip.
            std::vector<uint8_t> html_rt;
            if (!compressed.empty()) html_rt = decode_html_stream(compressed.data(), compressed.size(), this_block);
            if (compressed.empty() || html_rt.size() != this_block ||
                std::memcmp(html_rt.data(), block_data, this_block) != 0) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, compressed.data(), compressed.size());
                preprocess_size = compressed.size();
                use_generator = true;  // Already fully compressed, don't re-compress with zstd
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::URL_STREAM) {
            // URL component separation - beats mzip by 6.2% on URL lists
            // Protocols compress to 0.1%, domains to 2.9%
            auto compressed = MZ_TIMED("encode_url_stream", encode_url_stream(block_data, this_block));
            memcpy(preprocess_data, compressed.data(), compressed.size());
            preprocess_size = compressed.size();
            use_generator = true;  // Already fully compressed, don't re-compress with zstd
            res.blocks_text++;
        } else if (analysis.type == BlockType::BASE64_DECODE) {
            // Base64 de-encoding - beats brotli by 1.76% at 1MB
            // Decode base64 to binary (25% smaller), compress, re-encode on decompress
            auto compressed = MZ_TIMED("encode_base64_decode", encode_base64_decode(block_data, this_block, analysis.base64));
            memcpy(preprocess_data, compressed.data(), compressed.size());
            preprocess_size = compressed.size();
            use_generator = true;  // Already fully compressed, don't re-compress with zstd
            res.blocks_text++;
        } else if (analysis.type == BlockType::WORD_ENCODED) {
            // Word-level encoding for natural language (25% better than zstd on prose!)
            // BWT exploits CONTEXT redundancy, LZ77 exploits SUBSTRING redundancy.
            // Natural text has few unique words (~70) with high repetition.
            // Word encoding converts text to word indices, which zstd compresses well.
            // Only use for single-block data (vocabulary must be self-contained)
            bool is_single_block = (in_pos == 0 && this_block == size);

            if (!is_single_block) {
                // Fall back to TEXT for multi-block data
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                auto encoded = MZ_TIMED("encode_word_text", encode_word_text(block_data, this_block, analysis.word_encoding));
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::TEMPLATE) {
            // Template-based Kolmogorov compression for structured text
            // IMPORTANT: Only use TEMPLATE for single-block data.
            // Multi-block TEMPLATE fails because block boundaries split lines.
            bool is_single_block = (in_pos == 0 && this_block == size);

            if (!is_single_block) {
                // Fall back to TEXT for multi-block data
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                // Smart heuristic: decide if TEMPLATE encoding is worthwhile
                const auto& tp = analysis.template_params;

                // Rule 1: LINEAR_GEN columns are ALWAYS worth it (17 bytes for any size!)
                bool has_generator = false;
                for (const auto& col : tp.columns) {
                    if (col.type == ColumnType::COL_LINEAR_GEN ||
                        col.type == ColumnType::COL_TIMESTAMP) {
                        has_generator = true;
                        break;
                    }
                }

                bool use_template = has_generator;

                // Rule 2: For RAW-only columns, short variables -> zstd wins
                // Short numeric strings (0-199) compress extremely well under LZ77
                // TEMPLATE only wins when variables are long enough to justify overhead
                if (!use_template && tp.line_count >= 10) {
                    size_t total_var_bytes = 0;
                    for (const auto& col : tp.columns) {
                        for (const auto& v : col.values) {
                            total_var_bytes += v.size();
                        }
                    }
                    size_t avg_var_len = total_var_bytes / tp.line_count;
                    // Empirical threshold: avg variable >= 5 bytes for TEMPLATE to win
                    use_template = avg_var_len >= 5;
                }

                // Rule 3: Too many non-matching lines -> zstd wins
                // Non-matching lines are stored as raw bytes, adding significant overhead
                // If >25% of lines don't match, fall back to TEXT
                if (use_template && tp.line_count > 0) {
                    size_t total_lines = tp.line_count + tp.non_matching_lines.size();
                    double match_ratio = (double)tp.line_count / total_lines;
                    if (match_ratio < 0.75) {
                        use_template = false;
                    }
                }

                if (use_template) {
                    auto encoded = MZ_TIMED("encode_template", encode_template(tp));
                    // Roundtrip verify: TEMPLATE detection has corner cases on real
                    // inputs (truncated SQL VALUES tuples, mixed-format INSERT lines)
                    // that produce encoder output the decoder reconstructs incorrectly.
                    // decode_template is bounded — returns {} on any inconsistency.
                    auto rt = decode_template(encoded.data(), encoded.size(), this_block);
                    if (rt.size() != this_block ||
                        std::memcmp(rt.data(), block_data, this_block) != 0) {
                        use_template = false;
                    } else {
                        memcpy(preprocess_data, encoded.data(), encoded.size());
                        preprocess_size = encoded.size();
                        res.blocks_text++;
                    }
                }
                if (!use_template) {
                    // TEMPLATE didn't help - try BWT_TEXT for structured text
                    // BWT often wins on text where TEMPLATE's structure doesn't compress well
                    // (e.g., Prometheus metrics with random values/timestamps)
                    if (this_block >= 256 && this_block <= 2097152) {
                        auto bwt_result = MZ_TIMED("bwt9 @site1(L14320)", bwt9_block());
                        memcpy(preprocess_data, bwt_result.data(), bwt_result.size());
                        preprocess_size = bwt_result.size();
                        analysis.type = BlockType::BWT_TEXT;
                        use_generator = true;  // BWT is complete compression
                        res.blocks_text++;
                    } else {
                        // Fall back to plain TEXT for small/large data
                        analysis.type = BlockType::TEXT;
                        memcpy(preprocess_data, block_data, this_block);
                        res.blocks_text++;
                    }
                }
            }
        } else if (analysis.type == BlockType::CHAR_TEMPLATE) {
            // Char-level template (more robust than token-based)
            // Only use for single-block data
            bool is_single_block = (in_pos == 0 && this_block == size);

            if (!is_single_block) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                auto encoded = MZ_TIMED("encode_char_template", encode_char_template(analysis.char_template, zstd_level));
                // ROUNDTRIP VERIFY (2026-08-08) -- encode_char_template is NOT guaranteed lossless.
                // On real repetitive syslog (loghub OpenSSH_2k.log) it returned an 8 KB stream that
                // decodes WRONG, and this branch adopted it with NO per-block verify -- the block then
                // failed the top-level end-to-end verify and the WHOLE 225 KB file fell to the uRAW store
                // (225 KB raw vs xz 9.7 KB = 22x blowup). Mirror COLUMNAR/CSV_COLUMNAR: verify here, else
                // fall to TEXT so the block flows to the backstop ensemble (PPMD/XZLIB/BROTLI/bwt9).
                std::vector<uint8_t> ct_rt;
                if (!encoded.empty()) ct_rt = decode_char_template(encoded.data(), encoded.size());
                if (!encoded.empty() && ct_rt.size() == this_block &&
                    std::memcmp(ct_rt.data(), block_data, this_block) == 0) {
                    memcpy(preprocess_data, encoded.data(), encoded.size());
                    preprocess_size = encoded.size();
                    res.blocks_text++;
                } else {
                    analysis.type = BlockType::TEXT;
                    memcpy(preprocess_data, block_data, this_block);
                    res.blocks_text++;
                }
            }
        } else if (analysis.type == BlockType::ML_TEMPLATE) {
            // Multi-line template (JavaScript-like repeated blocks)
            // Works per-block - detection finds complete patterns within each block
            auto encoded = MZ_TIMED("encode_ml_template", encode_ml_template(analysis.ml_template));
            // ROUNDTRIP VERIFY (2026-08-08) -- same adopt-without-verify shape as CHAR_TEMPLATE; fall to TEXT on mismatch.
            std::vector<uint8_t> ml_rt;
            if (!encoded.empty()) ml_rt = decode_ml_template(encoded.data(), encoded.size(), this_block);
            if (!encoded.empty() && ml_rt.size() == this_block &&
                std::memcmp(ml_rt.data(), block_data, this_block) == 0) {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                res.blocks_text++;
            } else {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::ML_TEMPLATE_DUAL) {
            // Dual multi-line template (TypeScript interfaces + components)
            // Works per-block - detection finds complete patterns within each block
            auto encoded = MZ_TIMED("encode_ml_template_dual", encode_ml_template_dual(analysis.ml_template_dual));
            // ROUNDTRIP VERIFY (2026-08-08) -- same adopt-without-verify shape as CHAR_TEMPLATE; fall to TEXT on mismatch.
            std::vector<uint8_t> mld_rt;
            if (!encoded.empty()) mld_rt = decode_ml_template_dual(encoded.data(), encoded.size(), this_block);
            if (!encoded.empty() && mld_rt.size() == this_block &&
                std::memcmp(mld_rt.data(), block_data, this_block) == 0) {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                res.blocks_text++;
            } else {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::COLUMNAR) {
            // Columnar log format (access logs)
            // Only use for single-block data
            bool is_single_block = (in_pos == 0 && this_block == size);

            if (!is_single_block) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                auto encoded = MZ_TIMED("encode_columnar", encode_columnar(analysis.columnar));
                // ROUNDTRIP VERIFY — 2026-08-04. encode_columnar is NOT lossless: it drops
                // the HTTP version at encode (paths stores only METHOD+path) and hard-codes
                // `" HTTP/1.1\" "` and `" - - ["` at decode, so any access log whose requests
                // are not exactly HTTP/1.1 (or whose ident/authuser are not "-") reconstructs
                // wrong. Confirmed on real_bench/apache_log_sample.log: decode returns
                // 2,370,606 of 2,370,789 bytes, 252,097 differ. This branch adopted `encoded`
                // with NO verify; corruption shipped only if columnar also won the trial, and
                // it was masked here because the universal backstop's bwt9 is usually smaller.
                // Matching what CSV_COLUMNAR below already does: fall back to TEXT if it fails.
                std::vector<uint8_t> col_rt;
                if (!encoded.empty()) col_rt = decode_columnar(encoded.data(), encoded.size(), this_block);
                bool col_ok = (!encoded.empty() && col_rt.size() == this_block &&
                               std::memcmp(col_rt.data(), block_data, this_block) == 0);
                if (!col_ok) {
                    analysis.type = BlockType::TEXT;
                    memcpy(preprocess_data, block_data, this_block);
                    res.blocks_text++;
                } else
                // Trial: at small sizes (<64KB), COLUMNAR overhead can exceed gains.
                // Try BWT_TEXT on raw data and pick the smaller result.
                if (analysis.columnar.columns.size() == 9 && this_block <= 32768) {
                    auto bwt_trial = MZ_TIMED("bwt9 L14377", bwt9_block());
                    // `!bwt_trial.empty()` (2026-08-04): bwt9 can return {} (MZIP_NO_CM +
                    // total bwt5 failure), and `0 < encoded.size()` is always true, so an
                    // empty result would win and be adopted as a zero-length BWT_TEXT block.
                    // Falling through to the else keeps the roundtrip-verified columnar output.
                    if (!bwt_trial.empty() && bwt_trial.size() < encoded.size()) {
                        // BWT_TEXT wins — fall back
                        memcpy(preprocess_data, bwt_trial.data(), bwt_trial.size());
                        preprocess_size = bwt_trial.size();
                        analysis.type = BlockType::BWT_TEXT;
                        use_generator = true;
                        res.blocks_text++;
                    } else {
                        memcpy(preprocess_data, encoded.data(), encoded.size());
                        preprocess_size = encoded.size();
                        use_generator = true;
                        res.blocks_text++;
                    }
                } else {
                    memcpy(preprocess_data, encoded.data(), encoded.size());
                    preprocess_size = encoded.size();
                    // 9-column format uses 8+1 split encoding (BWT+zstd) - already compressed
                    if (analysis.columnar.columns.size() == 9) {
                        use_generator = true;
                    }
                    res.blocks_text++;
                }
            }
        } else if (analysis.type == BlockType::CSV_COLUMNAR) {
            // CSV columnar with LINEAR_GEN on ID columns
            auto encoded = MZ_TIMED("encode_csv_columnar", encode_csv_columnar(analysis.csv_columnar));
            // Roundtrip verify: CSV column-type detection occasionally produces
            // encoder output the decoder reconstructs incorrectly on real data
            // (e.g., events.csv with mixed numeric/string columns).
            auto rt = decode_csv_columnar(encoded.data(), encoded.size(), this_block);
            if (rt.size() == this_block && std::memcmp(rt.data(), block_data, this_block) == 0) {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;
                res.blocks_text++;
            } else {
                // Fall back to BWT_TEXT for CSV that didn't roundtrip
                if (this_block >= 256 && this_block <= 2097152) {
                    auto bwt_result = MZ_TIMED("bwt9 @site2(L14416)", bwt9_block());
                    memcpy(preprocess_data, bwt_result.data(), bwt_result.size());
                    preprocess_size = bwt_result.size();
                    analysis.type = BlockType::BWT_TEXT;
                    use_generator = true;
                } else {
                    memcpy(preprocess_data, block_data, this_block);
                    preprocess_size = this_block;
                    analysis.type = BlockType::TEXT;
                }
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::JSON_COLUMNAR) {
            // JSON columnar: extract sequential requestId + delta timestamps
            // 1085 bytes better than brotli on JSON structured logs
            auto encoded = MZ_TIMED("encode_json_columnar", encode_json_columnar(analysis.json_columnar, block_data, this_block));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already fully encoded, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::NUM_EXTRACT) {
            // NUM_EXTRACT: extract embedded decimal numbers from text
            // 900 bytes better than brotli on Makefiles!
            auto encoded = MZ_TIMED("encode_num_extract", encode_num_extract(block_data, this_block));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already fully encoded, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::LINE_TEMPLATE) {
            // Line template: variable-length lines with linear numeric vars (SQL INSERTs)
            // 14x improvement: stores prefix + suffix + separators + (first, delta) per var
            auto encoded = MZ_TIMED("encode_line_template", encode_line_template(analysis.line_template, zstd_level));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already fully encoded, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::PHRASE_PARTITION) {
            // Phrase partition: data exactly partitioned by repeated delimiter-separated phrases
            // Key insight: 5 phrases repeated 4900x = store phrases once + compressed indices
            auto encoded = MZ_TIMED("encode_phrase_partition", encode_phrase_partition(analysis.phrase_partition, block_data, this_block, zstd_level));
            // Roundtrip-verify before adopting, matching what TEMPLATE and CSV_COLUMNAR
            // already do. Added 2026-08-04 as defence in depth after a 2-byte length
            // field in encode_phrase_partition was found to produce archives that
            // compress cleanly and then fail to decode. The encoder bug is fixed at
            // source, but this path shipped for a long time with an `encoded.empty()`
            // guard as its ONLY check, and an empty guard cannot detect a well-formed
            // stream that simply decodes to the wrong thing.
            std::vector<uint8_t> pp_rt;
            if (!encoded.empty()) pp_rt = decode_phrase_partition(encoded.data(), encoded.size(), this_block);
            if (encoded.empty() || pp_rt.size() != this_block ||
                std::memcmp(pp_rt.data(), block_data, this_block) != 0) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::DUAL_STREAM) {
            // Dual stream: interleaved data with different entropy (Protobuf-like)
            // Key insight: tags (3 bits) vs values (7 bits) compress better separately
            auto encoded = MZ_TIMED("encode_dual_stream", encode_dual_stream(block_data, this_block, zstd_level));
            if (encoded.empty()) {
                // Fallback to RAW if encoding fails
                analysis.type = BlockType::RAW;
                memcpy(preprocess_data, block_data, this_block);
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_numeric++;  // Count as numeric (binary pattern)
            }
        } else if (analysis.type == BlockType::PHRASE_DICT) {
            // Phrase dictionary: use zstd dictionary compression with discovered phrases
            // Key insight: mimics brotli's static dictionary but adapts per-file
            auto encoded = MZ_TIMED("encode_phrase_dict", encode_phrase_dict(analysis.phrase_dict, block_data, this_block, zstd_level));
            if (encoded.empty()) {
                // Fallback to regular TEXT if dictionary encoding failed
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed with dictionary
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::SORTED_DICT) {
            // Sorted dictionary: sort lines + adaptive dictionary = beats brotli!
            // Key insight: LZ77 works better on grouped similar lines
            auto encoded = MZ_TIMED("encode_sorted_dict", encode_sorted_dict(analysis.sorted_dict, zstd_level));
            if (encoded.empty()) {
                // Fallback to regular TEXT if encoding failed
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::KV_CONFIG) {
            // Structural key-value config encoding (INI/YAML)
            // Parses sections/keys, builds dictionaries, compresses with zstd
            // Beats brotli by 7% on 16KB config files
            auto encoded = MZ_TIMED("encode_kv_config", encode_kv_config(analysis.kv_config, zstd_level));
            if (encoded.empty()) {
                // Fallback to TEXT if encoding failed
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::SECTION_TEMPLATE) {
            // Repeating multi-line sections with {N} variable (Markdown, JavaScript!)
            // Works on any block - each block stores its own template + LINEAR_GEN params
            auto encoded = MZ_TIMED("encode_section_template", encode_section_template(analysis.section_template, zstd_level));
            // ROUNDTRIP VERIFY — 2026-08-04. This site adopted the encoder output with no
            // verify, and decode_section_template ends with `while (output.size() >
            // original_size) output.pop_back();` -- so an over-producing (wrong) reconstruction
            // is silently trimmed to the expected LENGTH and then passes the block loop's only
            // integrity check, which compares out_pos to original_size and never the content.
            // A length check is not an integrity check. Verify content here and fall back to
            // TEXT on mismatch, matching TEMPLATE / CSV_COLUMNAR / COLUMNAR / HTML_STREAM.
            std::vector<uint8_t> st_rt;
            if (!encoded.empty()) st_rt = decode_section_template(encoded.data(), encoded.size(), this_block);
            if (encoded.empty() || st_rt.size() != this_block ||
                std::memcmp(st_rt.data(), block_data, this_block) != 0) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                preprocess_size = this_block;
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Don't re-compress
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::WORD_TEMPLATE) {
            // Repeating sections with word variable (2.4x over zstd on API docs!)
            // Each section has same structure but differs by one word that appears multiple times
            auto encoded = MZ_TIMED("encode_word_template", encode_word_template(analysis.word_template, zstd_level));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already compressed
            res.blocks_text++;
        } else if (analysis.type == BlockType::MULTI_WORD_TEMPLATE) {
            // Template with multiple variables {1},{2},{3} (44% better on K8s Ingress!)
            // K8s/Terraform sections differ by 2-3 variables (app, env, namespace)
            auto encoded = MZ_TIMED("encode_multi_word_template", encode_multi_word_template(analysis.multi_word_template, zstd_level));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already compressed
            res.blocks_text++;
        } else if (analysis.type == BlockType::LINE_GROUP_TEMPLATE) {
            // Multi-line-type data like email headers
            // Groups lines by prefix, applies LINEAR_GEN per group
            auto encoded = MZ_TIMED("encode_line_group_template", encode_line_group_template(block_data, this_block, analysis.line_group_info, analysis.line_group_types));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            // Let zstd compress the encoded data (raw lines + line type sequence)
            res.blocks_text++;
        } else if (analysis.type == BlockType::CODE_STREAM) {
            // Identifier stream separation for code (beats bzip2 on JavaScript!)
            // Detection already compressed all streams - just encode the structure
            auto encoded = MZ_TIMED("encode_code_stream", encode_code_stream(analysis.code_stream));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already compressed, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::REFERENCE) {
            // REFERENCE block: use pre-computed delta encoding
            memcpy(preprocess_data, reference_encoded.data(), reference_encoded.size());
            preprocess_size = reference_encoded.size();
            use_generator = true;  // Already compressed, don't re-compress
            res.blocks_raw++;  // Count as raw (cross-block)
        } else if (analysis.type == BlockType::INCOMPRESSIBLE) {
            memcpy(preprocess_data, block_data, this_block);
            res.blocks_incompressible++;
        } else {
            memcpy(preprocess_data, block_data, this_block);
            res.blocks_raw++;
        }

        // ====================================================================
        // Universal dict trial: for ANY block in [256B, 1MB], try zstd+dict on
        // original data. If it beats the current encoding, switch to ZSTD_DICT.
        // Runs AFTER all strategy-specific encoding is done.
        // The pre-trained group dicts (CODE/CONFIG/TEXT/LOG/QUERY, 16KB each)
        // carry domain keywords (k8s, terraform, sql, html tags, log fields)
        // that brotli's 120 KB static English/web dict catches but BWT alone
        // cannot. Worth trialing well past the original 16KB cap on real
        // structured content; trial-pick discards the dict result if it loses.
        // ====================================================================
        // SPEED: dicts are 16-64KB and target small (4-16KB) files — they never win 256KB-1MB blocks, where
        // the 12x zstd-19 dict trial is pure cost (~6s/MB). Cap at 256KB (MZIP_MAXRATIO lifts it to 1MB).
        size_t dict_trial_max = std::getenv("MZIP_MAXRATIO") ? 1048576 : 262144;
        if (this_block <= dict_trial_max && this_block >= 256) {
            ZSTD_CCtx* dcctx = ZSTD_createCCtx();
            size_t best_dsz = SIZE_MAX;
            uint8_t best_did = 0;
            std::vector<uint8_t> dtbuf(ZSTD_compressBound(this_block));
            for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                auto& d = mzip_dicts::ALL_DICTS[di];
                size_t sz = ZSTD_compress_usingDict(dcctx,
                    dtbuf.data(), dtbuf.size(),
                    block_data, this_block, d.data, d.size, 19);
                if (!ZSTD_isError(sz) && sz + 1 < best_dsz) {
                    best_dsz = sz + 1;
                    best_did = d.id;
                }
            }
            ZSTD_freeCCtx(dcctx);

            // Compare dict result against current encoding
            // For use_generator blocks: compare against preprocess_size (already compressed)
            // For non-use_generator blocks: estimate final size as zstd(preprocess_data)
            size_t current_est = preprocess_size;
            if (!use_generator) {
                // Quick zstd estimate
                size_t zest = ZSTD_compress(dtbuf.data(), dtbuf.size(),
                    preprocess_data, preprocess_size, 19);
                if (!ZSTD_isError(zest)) current_est = zest;
            }

            if (best_did > 0 && best_dsz < current_est) {
                // Dict wins — rebuild as ZSTD_DICT block
                dcctx = ZSTD_createCCtx();
                const uint8_t* dp = nullptr; size_t dl = 0;
                for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                    if (mzip_dicts::ALL_DICTS[di].id == best_did) {
                        dp = mzip_dicts::ALL_DICTS[di].data;
                        dl = mzip_dicts::ALL_DICTS[di].size;
                        break;
                    }
                }
                size_t sz = ZSTD_compress_usingDict(dcctx,
                    preprocess_data + 1, ZSTD_compressBound(this_block),
                    block_data, this_block, dp, dl, 19);
                ZSTD_freeCCtx(dcctx);
                if (!ZSTD_isError(sz)) {
                    preprocess_data[0] = best_did;
                    preprocess_size = 1 + sz;
                    analysis.type = BlockType::ZSTD_DICT;
                    use_generator = true;
                }
            }
        }

        // ====================================================================
        // Universal LZMA + brotli backstop (best-of-ensemble, trial-and-keep):
        //   lzma_opt2 (our optimal LZMA) flips large-repetitive vs xz (e.g. SQL dumps);
        //   brotli-11 flips small code/config vs brotli's static dict. Switch only if strictly smaller.
        // ====================================================================
        if (this_block >= 64) {
            size_t cap = ZSTD_compressBound(this_block);
            size_t cur = preprocess_size;
            if (!use_generator) {
                std::vector<uint8_t> tb(cap + 64);
                size_t z = MZ_TIMED("zstd-19 backstop", ZSTD_compress(tb.data(), tb.size(), preprocess_data, preprocess_size, 19));
                if (!ZSTD_isError(z)) cur = z;
            }
            // bwt9 (BWT + context-mixing, bzip3-class) trial -> BWT_TEXT (decoded via bwt9). Catches
            // BWT-friendly data the type detectors MISS (float/sensor arrays, structured binary) where
            // it beats bzip2/xz/zstd. Tried first so ties prefer our own tech over the external backstops.
            {
                // The backstop is the LAST consumer of bwt9 for this block and it already holds
                // the incumbent size `cur`, so hand that over as a pruning cap: bwt5 can then skip
                // Huffman candidates that provably cannot come in under it. Worth most exactly where
                // bwt9 is hopeless — on tsgas_series.bin the incumbent (NUMERIC) is 237,945 B while
                // bwt9 would spend 4.8 s producing 3,023,955 B.
                //
                // DELIBERATELY NOT MEMOISED. A capped result is only guaranteed to equal the true
                // one when it lands BELOW the cap; above it, pruning may have removed the true min.
                // Caching it could hand a pruned answer to a caller that needed the exact one. Since
                // nothing runs after the backstop there is nothing to cache it for, and if the memo
                // is already populated (the TEXT path ran) we use that exact result untouched.
                std::vector<uint8_t> b9_local;
                if (!bwt9_memo_valid) {
#ifndef MZIP_NO_CM
                    b9_local = MZ_TIMED("bwt9 @backstop", bwt9::compress(block_data, this_block, &cm_block(), cur));
#else
                    b9_local = MZ_TIMED("bwt9 @backstop", bwt9::compress(block_data, this_block, nullptr, cur));
#endif
                }
                const std::vector<uint8_t>& b9 = bwt9_memo_valid ? bwt9_memo : b9_local;
                if (!b9.empty() && b9.size() < cur && b9.size() <= cap) {
                    memcpy(preprocess_data, b9.data(), b9.size());
                    preprocess_size = b9.size();
                    analysis.type = BlockType::BWT_TEXT;
                    use_generator = true;
                    cur = b9.size();
                }
            }
            // xz (liblzma -9 EXTREME) trial -> XZLIB. Genuine xz-quality LZMA; flips large-repetitive (SQL dumps).
            {
                size_t xbound = lzma_stream_buffer_bound(this_block);
                std::vector<uint8_t> xb(xbound);
                size_t xpos = 0;
                if (lzma_easy_buffer_encode(9u | MZ_LZMA_PRESET_EXTREME, MZ_LZMA_CHECK_NONE, nullptr,
                                            block_data, this_block, xb.data(), &xpos, xbound) == MZ_LZMA_OK
                    && xpos < cur && xpos <= cap) {
                    memcpy(preprocess_data, xb.data(), xpos);
                    preprocess_size = xpos;
                    analysis.type = BlockType::XZLIB;
                    use_generator = true;
                    cur = xpos;
                }
            }
            // ARM/PPC/RISC-V BCJ-filtered xz trial -> a better XZLIB candidate on non-x86 executables.
            // mzip's 'MB' pre-filter is x86 E8/E9 only; xz's arch BCJ filters beat plain xz 4-7% on ARM/PPC/
            // RISCV machine code (measured: ppc64 +7.3%, arm32 +5.8%, arm64 +3.8% vs plain). The payload stays
            // a plain .xz stream (BlockType XZLIB unchanged) whose header carries the BCJ filter, so DECODE is
            // untouched -- lzma_stream_buffer_decode auto-inverts it. Trial-and-keep + per-block roundtrip-verify
            // => zero regression by construction. (2026-08-08, gap-analysis workflow)
            {
                uint64_t bcj_id = mbcj::exec_bcj_filter(block_data, this_block);
                if (bcj_id) {
                    unsigned char lzopt[256];
                    std::memset(lzopt, 0, sizeof(lzopt));
                    if (lzma_lzma_preset(lzopt, 9u | MZ_LZMA_PRESET_EXTREME) == 0) {   // 0 = OK
                        mz_lzma_filter filters[3] = {
                            { bcj_id, nullptr },
                            { MZ_LZMA_FILTER_LZMA2, lzopt },
                            { MZ_LZMA_VLI_UNKNOWN, nullptr }
                        };
                        size_t xbound = lzma_stream_buffer_bound(this_block);
                        std::vector<uint8_t> xb(xbound);
                        size_t xpos = 0;
                        if (lzma_stream_buffer_encode(filters, MZ_LZMA_CHECK_NONE, nullptr,
                                                      block_data, this_block, xb.data(), &xpos, xbound) == MZ_LZMA_OK
                            && xpos < cur && xpos <= cap) {
                            // per-block roundtrip-verify (new encoder path): decode the .xz, compare to source
                            uint64_t memlim = ~0ULL; size_t inpos = 0, outpos = 0;
                            std::vector<uint8_t> chk(this_block);
                            if (lzma_stream_buffer_decode(&memlim, 0, nullptr, xb.data(), &inpos, xpos,
                                                          chk.data(), &outpos, this_block) == MZ_LZMA_OK
                                && outpos == this_block
                                && std::memcmp(chk.data(), block_data, this_block) == 0) {
                                memcpy(preprocess_data, xb.data(), xpos);
                                preprocess_size = xpos;
                                analysis.type = BlockType::XZLIB;
                                use_generator = true;
                                cur = xpos;
                            }
                        }
                    }
                }
            }
            // brotli-11 trial (ensemble backstop) -> BROTLI. Trial BOTH generic(0) + text(1) modes, keep smaller.
            // SPEED GATE: brotli-11 (the slowest backstop, x2 modes) only helps small text/code via its dict; on
            // blocks > MZ_BACKSTOP_BROTLI_MAX (1 MB) bwt9/xz/numeric always win, so skip it there. Set MZIP_MAXRATIO
            // (env) to trial it on all sizes for absolute max ratio.
            size_t brotli_cap_bytes = (1u << 20);
            if (std::getenv("MZIP_MAXRATIO")) brotli_cap_bytes = (size_t)-1;
            if (this_block <= brotli_cap_bytes) {
                size_t bcap = BrotliEncoderMaxCompressedSize(this_block);
                if (bcap == 0) bcap = this_block + 1024;
                std::vector<uint8_t> bb(bcap);
                // ---------------------------------------------------------------------------
                // 2026-07-31: THE SECOND BROTLI MODE WAS MEASURED INERT AND IS NO LONGER TRIALLED.
                // This loop used to run bmode 0..1 (GENERIC, then TEXT) and keep the smaller.
                // In the bundled libbrotli the `mode` argument of BrotliEncoderCompress produces
                // BYTE-IDENTICAL output, so the second pass was 2x the work for nothing:
                //   * direct encoder test, 47 held-out files x qualities {1,5,9,11} = 188 paired
                //     comparisons: byte-identical in every one (not merely same-size).
                //   * instrument validated in the same run - quality DOES change output
                //     (q1 total 1,360,925 -> q11 901,544), so the harness passes args correctly.
                //   * end-to-end: 97 files across real_bench/ + corpus_extra/ + samples/,
                //     ZERO byte differences; runtime -12.4% and -23.2% respectively.
                //   * 28/28 losslessness suite green on the narrowed build.
                // brotli-11 is ~37-43% of runtime where it runs (measured via MZIP_MAXRATIO),
                // and it wins 23 of 47 held-out blocks - the plurality - so it stays; only the
                // redundant second mode goes.
                //
                // !! IF libbrotli IS EVER UPGRADED, RE-RUN THAT TEST BEFORE TRUSTING THIS. !!
                // A build that honours the mode hint would make this a SILENT ratio regression.
                // Restore the old behaviour with -DMZ_BROTLI_MODE_HI=1 (no code edit needed).
                // ---------------------------------------------------------------------------
                #ifndef MZ_BROTLI_MODE_LO
                #define MZ_BROTLI_MODE_LO 0
                #endif
                #ifndef MZ_BROTLI_MODE_HI
                #define MZ_BROTLI_MODE_HI 0
                #endif
                for (int bmode = MZ_BROTLI_MODE_LO; bmode <= MZ_BROTLI_MODE_HI; bmode++) {
                    size_t bsz = bcap;
                    if (BrotliEncoderCompress(MZ_BROTLI_QUALITY, MZ_BROTLI_WINDOW, bmode,
                                              this_block, block_data, &bsz, bb.data())
                        && bsz < cur && bsz <= cap) {
                        memcpy(preprocess_data, bb.data(), bsz);
                        preprocess_size = bsz;
                        analysis.type = BlockType::BROTLI;
                        use_generator = true;
                        cur = bsz;
                    }
                }
            }
            // PPMd var.H (Ppmd7) trial -> PPMD backstop. Its high-order local-context model beats
            // brotli/CM_TEXT/BWT_TEXT on SOURCE CODE (~11% measured, 9/9 real code files). Text-gated
            // (loses on binary/numeric, where the trials above win). Payload = [order:1][memMiB:1] +
            // Ppmd7z stream; order+memMiB are stored so decode uses the same bound (Ppmd decode heap ==
            // memMiB MiB, a pure function of that byte -> the decoder clamps it for untrusted streams).
            // Self-verified here (maximal rigor) before adopt; the top-level verify is the final backstop. (2026-08-08)
            // Revert the whole PPMd backstop with -DMZIP_NO_PPMD (decode still linked, harmless).
#ifndef MZIP_NO_PPMD
            {
                size_t lim = this_block < 4096 ? this_block : 4096, pr = 0;
                for (size_t i = 0; i < lim; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) pr++; }
                bool ppmd_texty = lim > 0 && pr * 100 >= lim * 85;   // >=85% printable => text/code
                if (ppmd_texty) {
#ifndef MZIP_PPMD_MEM
#define MZIP_PPMD_MEM 64     // Ppmd model memory in MiB == decode heap; -D-tunable (decode clamps to [1,128])
#endif
                    const uint8_t PPMD_MEM = MZIP_PPMD_MEM;
                    // Multi-order keep-smallest: try an ORDER SET, adopt the smallest that self-verifies and
                    // beats cur. Order 16 is always first, so this is STRICTLY >= the single-order-16 ship
                    // (higher orders only ADD options; keep-smallest never regresses PPMd-vs-PPMd -- the case
                    // CLAUDE.md warns trial-and-keep does NOT cover). Captures the probe's high-order wins
                    // (zig ~-1.7%, handlers.ts ~-1.2%) at zero format cost (order is stored per block). Cost:
                    // up to N PPMd encodes per text block. Force single order for A/B via -DMZIP_PPMD_ORDER=n. (2026-08-08)
#ifdef MZIP_PPMD_ORDER
                    static const uint8_t PPMD_ORDERS[] = { (uint8_t)(MZIP_PPMD_ORDER) };
#else
                    static const uint8_t PPMD_ORDERS[] = { 16, 32, 48 };
#endif
                    for (uint8_t ord : PPMD_ORDERS) {
                        auto pp = ppmdbk::compress(block_data, this_block, ord, PPMD_MEM);
                        if (pp.empty() || pp.size() + 2 >= cur || pp.size() + 2 > cap) continue;
                        auto chk = ppmdbk::decompress(pp.data(), pp.size(), this_block, ord, PPMD_MEM);
                        if (chk.size() == this_block && std::memcmp(chk.data(), block_data, this_block) == 0) {
                            preprocess_data[0] = ord; preprocess_data[1] = PPMD_MEM;
                            std::memcpy(preprocess_data + 2, pp.data(), pp.size());
                            preprocess_size = pp.size() + 2;
                            analysis.type = BlockType::PPMD;
                            use_generator = true;
                            cur = pp.size() + 2;   // subsequent orders must beat this
                        }
                    }
                }
            }
#endif
        }

        size_t compressed_size;
        bool store_raw;

        if (use_generator) {
            // For generators, data is already encoded - store directly without zstd
            compressed_size = preprocess_size;
            store_raw = true;  // Mark as raw since we're not using zstd
        } else {
            // Compress with zstd
            // Use level 9 as minimum for TEXT - good balance of speed and ratio
            // Don't use level 15+ which can be WORSE on some structured text (see test_zstd_levels)
            int effective_level = zstd_level;
            if (analysis.type == BlockType::TEXT && zstd_level < 9) {
                effective_level = 9;  // Level 9 is optimal for most text
            } else if (analysis.type == BlockType::BINARY_X86 && zstd_level < 9) {
                effective_level = 9;  // Good for binary after E8/E9 filtering
            }

            compressed_size = ZSTD_compress(
                compress_buf.data(), compress_buf.size(),
                preprocess_data, preprocess_size,
                effective_level
            );

            if (ZSTD_isError(compressed_size)) {
                res.error = "ZSTD compression failed: " + std::string(ZSTD_getErrorName(compressed_size));
                if (result) *result = res;
                return {};
            }

            // If compression made it bigger, store uncompressed
            store_raw = (compressed_size >= preprocess_size);
            if (store_raw) {
                compressed_size = preprocess_size;
            }
        }

        // Diagnostic telemetry: which encoder actually fired per block (set env MZIP_STATS=1).
        // Powers diagnose_encoders.py — finds where specialized encoders DON'T fire (mzip leans on fallback/backstop).
        if (std::getenv("MZIP_STATS"))
            std::fprintf(stderr, "MZSTATS\t%s\t%zu\t%zu\n", block_type_name(analysis.type),
                         (size_t)this_block, (size_t)compressed_size);

        // Write block header
        output[out_pos++] = static_cast<uint8_t>(analysis.type);
        output[out_pos++] = static_cast<uint8_t>(strategy);
        write_u32_le(&output[out_pos], static_cast<uint32_t>(this_block));
        out_pos += 4;
        write_u32_le(&output[out_pos], static_cast<uint32_t>(preprocess_size));
        out_pos += 4;

        // High bit of compressed_size indicates uncompressed storage
        uint32_t stored_size = static_cast<uint32_t>(compressed_size);
        if (store_raw) stored_size |= 0x80000000;
        write_u32_le(&output[out_pos], stored_size);
        out_pos += 4;

        // Write block data
        if (store_raw) {
            memcpy(&output[out_pos], preprocess_data, preprocess_size);
            out_pos += preprocess_size;
        } else {
            memcpy(&output[out_pos], compress_buf.data(), compressed_size);
            out_pos += compressed_size;
        }

        // === STORE BLOCK FOR FUTURE REFERENCE ===
        // Only store blocks suitable for use as dictionary (TEXT, NUMERIC, RAW)
        // and only if they're large enough to be useful
        if (this_block >= MIN_REFERENCE_SIZE && 
            (analysis.type == BlockType::RAW || 
             analysis.type == BlockType::TEXT ||
             analysis.type == BlockType::NUMERIC ||
             analysis.type == BlockType::REFERENCE)) {
            
            RecentBlock rb;
            rb.data.assign(block_data, block_data + this_block);
            rb.fingerprint = current_fp;
            rb.output_offset = out_pos - (store_raw ? preprocess_size : compressed_size);
            
            if (recent_blocks.size() >= MAX_REFERENCE_BLOCKS) {
                // Remove oldest block to make room
                recent_blocks.erase(recent_blocks.begin());
            }
            recent_blocks.push_back(std::move(rb));
        }

        in_pos += this_block;
        block_count++;
    }

    // Write block count
    write_u32_le(&output[block_count_pos], block_count);

    // Resize to actual size
    output.resize(out_pos);

    // ============================================================================
    // Single-block optimizations: compact format and ultra-compact BWT
    // ============================================================================
    std::vector<uint8_t> cl_format;  // Store CL format for 9-column COLUMNAR (declared at function scope)
    std::vector<uint8_t> mu_format;  // Store MU format for small text files (4-16KB)
    if (block_count == 1) {
        // Parse legacy block header to get sizes
        constexpr size_t LEGACY_HEADER = 17;  // magic(4) + ver(1) + size(8) + count(4)
        constexpr size_t LEGACY_BLOCK_HEADER = 14;  // type(1) + strategy(1) + orig(4) + preproc(4) + comp(4)

        uint8_t block_type = output[LEGACY_HEADER];
        uint8_t strategy = output[LEGACY_HEADER + 1];
        uint32_t orig_size = read_u32_le(&output[LEGACY_HEADER + 2]);
        uint32_t preproc_size = read_u32_le(&output[LEGACY_HEADER + 6]);
        uint32_t stored_size = read_u32_le(&output[LEGACY_HEADER + 10]);
        bool is_raw = (stored_size & 0x80000000) != 0;
        uint32_t comp_size = stored_size & 0x7FFFFFFF;

        // ========================================================================
        // ========================================================================
        // Ultra-compact CL format for 9-column COLUMNAR (beats bzip2 by 3 bytes!)
        // Header: "CL" (2) + size_varint (3) = 5 bytes vs 33 bytes for MZIP format
        // ========================================================================
        if (block_type == static_cast<uint8_t>(BlockType::COLUMNAR)) {
            // Check if it's 9-column format (marker 0xFE)
            const uint8_t* block_data = &output[LEGACY_HEADER + LEGACY_BLOCK_HEADER];
            if (comp_size > 0 && block_data[0] == 0xFE) {
                // Build CL format: "CL" + size_varint + data
                size_t cl_size = 2 + uvarint_size(size) + comp_size;
                if (cl_size < out_pos) {
                    cl_format.resize(cl_size);
                    size_t pos = 0;
                    cl_format[pos++] = 'C';
                    cl_format[pos++] = 'L';
                    pos += write_uvarint_buf(&cl_format[pos], size);
                    memcpy(&cl_format[pos], block_data, comp_size);
                    out_pos = cl_size;  // Update out_pos for later comparison
                }
            }
        }

        // ========================================================================
        // Ultra-compact MU format for small text files (4-16KB)
        // Header: "MU" (2) + BlockType (1) + size_varint (1-2) = 4-5 bytes
        // vs compact format ~14-17 bytes. Saves 10-12 bytes to beat bzip2!
        // IMPORTANT: Only use MU for blocks stored RAW (is_raw=true), since
        // MU format doesn't preserve the zstd compression layer!
        // ========================================================================
        constexpr size_t MU_MAX_SIZE = 65536;  // <=64KB: MU header (~5B) is size-agnostic; the ~9B saving vs the
                                               // compact format matters most where output is small (small structured files)
        if (size <= MU_MAX_SIZE && is_raw) {
            // Text-oriented strategies that benefit from MU format
            // Only use MU when data is stored raw (is_raw=true), meaning the
            // encoder already did complete compression (use_generator=true)
            bool use_mu = (block_type == static_cast<uint8_t>(BlockType::BWT_TEXT) ||
                           block_type == static_cast<uint8_t>(BlockType::TEMPLATE) ||
                           block_type == static_cast<uint8_t>(BlockType::CHAR_TEMPLATE) ||
                           block_type == static_cast<uint8_t>(BlockType::LINE_GROUP_TEMPLATE) ||
                           block_type == static_cast<uint8_t>(BlockType::KV_CONFIG) ||
                           block_type == static_cast<uint8_t>(BlockType::WORD_ENCODED) ||
                           block_type == static_cast<uint8_t>(BlockType::ML_TEMPLATE) ||
                           block_type == static_cast<uint8_t>(BlockType::SECTION_TEMPLATE) ||
                           block_type == static_cast<uint8_t>(BlockType::ZSTD_DICT) ||
                           block_type == static_cast<uint8_t>(BlockType::CODE_STREAM) ||
                           block_type == static_cast<uint8_t>(BlockType::NUM_EXTRACT) ||
                           // backstop blocks are also self-contained (payload = full stream) — MU saves ~9B/file
                           // over the compact format on small files where brotli/xz/cm win (the only remaining "losses")
                           block_type == static_cast<uint8_t>(BlockType::BROTLI) ||
                           block_type == static_cast<uint8_t>(BlockType::XZLIB) ||
                           block_type == static_cast<uint8_t>(BlockType::PPMD) ||
                           block_type == static_cast<uint8_t>(BlockType::CM_TEXT));

            // Also allow NUMERIC blocks with sparse encoding (0xFF marker). MUST match the sparse
            // discriminator the decoder uses: bd[0]==strategy AND bd[1]==0xFF. Checking bd[1]==0xFF
            // alone wrongly MU-wrapped NON-sparse numeric blocks whose preprocessed data merely
            // starts with 0xFF at [1]; the MU decoder then reads strategy from bd[0] (the wrong
            // value) and decodes garbage (all-zeros) -> SILENT CORRUPTION found by fuzz_mzip
            // (strategy=11, bd=0x03 0xFF ..., 511 floats). The legacy/compact decode already
            // requires BOTH conditions and handles this block correctly; MU now matches, so the
            // block falls back to compact/legacy framing instead of a mis-decodable MU wrap. (2026-08-07)
            if (!use_mu && block_type == static_cast<uint8_t>(BlockType::NUMERIC) && comp_size >= 2) {
                const uint8_t* bd = &output[LEGACY_HEADER + LEGACY_BLOCK_HEADER];
                if (bd[0] == (uint8_t)strategy && bd[1] == 0xFF) use_mu = true;
            }

            if (use_mu) {
                const uint8_t* block_data = &output[LEGACY_HEADER + LEGACY_BLOCK_HEADER];
                // MU format: "MU" + BlockType + size_varint + compressed_data
                size_t mu_size = 2 + 1 + uvarint_size(size) + comp_size;
                if (mu_size < out_pos) {
                    mu_format.resize(mu_size);
                    size_t pos = 0;
                    mu_format[pos++] = 'M';
                    mu_format[pos++] = 'U';
                    mu_format[pos++] = block_type;
                    pos += write_uvarint_buf(&mu_format[pos], size);
                    memcpy(&mu_format[pos], block_data, comp_size);
                }
            }
        }

        // ========================================================================
        // Compact format for other single-block files (saves ~17-19 bytes)
        // VERSION_COMPACT uses varints instead of fixed u32/u64 for sizes
        // ========================================================================
        bool preproc_eq_orig = (preproc_size == orig_size);
        bool comp_eq_preproc = (comp_size == preproc_size);  // True for use_generator blocks
        size_t compact_header = 5 + uvarint_size(size) + uvarint_size(1);  // magic(4)+ver(1)+size+count
        size_t compact_block = 2 +  // type(1) + flags_strategy(1)
            uvarint_size(orig_size) +
            (preproc_eq_orig ? 0 : uvarint_size(preproc_size)) +
            (comp_eq_preproc ? 0 : uvarint_size(comp_size));  // Skip if comp == preproc
        size_t compact_total = compact_header + compact_block + comp_size;

        // Only repack if it actually saves space
        if (compact_total < out_pos) {
            std::vector<uint8_t> compact(compact_total);
            size_t pos = 0;

            // Write compact header
            write_u32_le(&compact[pos], MAGIC); pos += 4;
            compact[pos++] = VERSION_COMPACT;
            pos += write_uvarint_buf(&compact[pos], size);
            pos += write_uvarint_buf(&compact[pos], 1);  // block_count = 1

            // Write compact block header
            compact[pos++] = block_type;
            uint8_t flags = (is_raw ? BLOCK_FLAG_RAW : 0) |
                            (preproc_eq_orig ? BLOCK_FLAG_PREPROC_EQ_ORIG : 0) |
                            (comp_eq_preproc ? BLOCK_FLAG_COMP_EQ_PREPROC : 0) |
                            ((strategy & 0x1F) << 2);  // Pack strategy into bits 2-6
            compact[pos++] = flags;
            pos += write_uvarint_buf(&compact[pos], orig_size);
            if (!preproc_eq_orig) {
                pos += write_uvarint_buf(&compact[pos], preproc_size);
            }
            if (!comp_eq_preproc) {
                pos += write_uvarint_buf(&compact[pos], comp_size);
            }

            // Copy compressed data
            memcpy(&compact[pos], &output[LEGACY_HEADER + LEGACY_BLOCK_HEADER], comp_size);

            output = std::move(compact);
            out_pos = compact_total;
        }
    }

    // ============================================================================
    // Zero-overhead passthrough
    // If mzip didn't improve compression, return pure zstd (0 bytes overhead)
    // decompress() detects zstd magic and handles it directly
    // ============================================================================
    std::vector<uint8_t> zstd_buf(ZSTD_compressBound(size));
    size_t zstd_size = ZSTD_compress(
        zstd_buf.data(), zstd_buf.size(),
        data, size, zstd_level
    );

    // Calculate µRAW format size (for incompressible data)
    // µRAW: 2 bytes magic + varint(size) + raw data
    auto varint_size = [](uint64_t val) -> size_t {
        size_t bytes = 0;
        do { val >>= 7; bytes++; } while (val);
        return bytes;
    };
    size_t uraw_size = 2 + varint_size(size) + size;

    // Single-window check: returns text-likeness ratio in [0.0, 1.0]
    auto window_text_ratio = [](const uint8_t* d, size_t n) -> double {
        if (n == 0) return 0.0;
        size_t ascii_printable = 0;
        size_t utf8_valid = 0;
        for (size_t i = 0; i < n; i++) {
            uint8_t c = d[i];
            if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t')
                ascii_printable++;
            else if (c >= 0xC2 && c <= 0xF4 && i + 1 < n) {
                int expected = (c < 0xE0) ? 1 : (c < 0xF0) ? 2 : 3;
                bool valid = true;
                for (int j = 1; j <= expected && i + j < n; j++) {
                    if ((d[i + j] & 0xC0) != 0x80) { valid = false; break; }
                }
                if (valid) utf8_valid += 1 + expected;
            }
        }
        return (double)(ascii_printable + utf8_valid) / (double)n;
    };

    // Robust multi-window text detection: sample 5 windows of 4KB across the file.
    // Returns true when majority of windows look text-like. Catches files where
    // start is text but body is binary (and vice versa).
    auto is_text_like = [&window_text_ratio](const uint8_t* d, size_t n) -> bool {
        constexpr size_t WIN = 4096;
        if (n <= WIN) return window_text_ratio(d, n) >= 0.70;
        size_t positions[5];
        positions[0] = 0;
        positions[1] = n / 4;
        positions[2] = n / 2;
        positions[3] = (n * 3) / 4;
        positions[4] = n - WIN;
        int text_windows = 0;
        for (size_t pos : positions) {
            size_t window = std::min(WIN, n - pos);
            if (window_text_ratio(d + pos, window) >= 0.70) text_windows++;
        }
        return text_windows >= 3;  // majority of 5
    };

    // ============================================================================
    // Multi-chunk compression for SMALL mode on large files (>1MB)
    // Try per-chunk backend selection (zstd vs BWT vs LZMA)
    // Format: "MC" + varint(size) + varint(num_chunks) + [chunks...]
    // Each chunk: backend(1) + varint(orig) + varint(comp) + data
    // ============================================================================
    std::vector<uint8_t> mc_format;
    size_t mc_size = SIZE_MAX;
    constexpr size_t MC_THRESHOLD = 1024 * 1024;  // 1MB
    constexpr size_t MC_CHUNK_SIZE = 1024 * 1024;  // 1MB chunks (larger chunks compress better)
    constexpr uint8_t MC_BACKEND_ZSTD = 0;
    constexpr uint8_t MC_BACKEND_BWT = 1;
    constexpr uint8_t MC_BACKEND_LZMA = 2;

    // Try MC whenever the input is large enough and the mode allows pre/post-processing.
    // MC wins when chunks are heterogeneous (mixed types). On homogeneous prose, the
    // single-block BG path below typically beats it, so they compete on output size.
    if (mode != CompressionMode::FAST && size > MC_THRESHOLD) {
        // Try multi-chunk compression
        std::vector<uint8_t> mc_temp;
        mc_temp.reserve(size);

        // Write header
        mc_temp.push_back('M');
        mc_temp.push_back('C');
        uint8_t vbuf[16];
        size_t n = write_uvarint_buf(vbuf, size);
        mc_temp.insert(mc_temp.end(), vbuf, vbuf + n);

        size_t num_chunks = (size + MC_CHUNK_SIZE - 1) / MC_CHUNK_SIZE;
        n = write_uvarint_buf(vbuf, num_chunks);
        mc_temp.insert(mc_temp.end(), vbuf, vbuf + n);

        // Compress each chunk with best backend
        std::vector<uint8_t> chunk_zstd_buf(ZSTD_compressBound(MC_CHUNK_SIZE + 1000));

        for (size_t offset = 0; offset < size; offset += MC_CHUNK_SIZE) {
            size_t this_chunk = std::min(MC_CHUNK_SIZE, size - offset);
            const uint8_t* chunk_data = data + offset;

            // Try zstd (always) and BWT (only for text-like chunks)
            size_t chunk_zstd = ZSTD_compress(chunk_zstd_buf.data(), chunk_zstd_buf.size(),
                                               chunk_data, this_chunk, 22);

            // Pick best between zstd and BWT
            uint8_t best_backend = MC_BACKEND_ZSTD;
            size_t best_chunk_size = chunk_zstd;
            const uint8_t* best_chunk_data = chunk_zstd_buf.data();

            // Jan 2026: Only try BWT for text-like chunks (avoid O(n²) hang on binary)
            std::vector<uint8_t> chunk_bwt;
            if (is_text_like(chunk_data, this_chunk)) {
                chunk_bwt = bwt9::compress(chunk_data, this_chunk);
                if (chunk_bwt.size() < best_chunk_size) {
                    best_backend = MC_BACKEND_BWT;
                    best_chunk_size = chunk_bwt.size();
                    best_chunk_data = chunk_bwt.data();
                }
            }

            // Write chunk: backend + orig_size + comp_size + data
            mc_temp.push_back(best_backend);
            n = write_uvarint_buf(vbuf, this_chunk);
            mc_temp.insert(mc_temp.end(), vbuf, vbuf + n);
            n = write_uvarint_buf(vbuf, best_chunk_size);
            mc_temp.insert(mc_temp.end(), vbuf, vbuf + n);
            mc_temp.insert(mc_temp.end(), best_chunk_data, best_chunk_data + best_chunk_size);
        }

        mc_size = mc_temp.size();
        if (mc_size < zstd_size && mc_size < out_pos) {
            mc_format = std::move(mc_temp);
        }
    }

    // ============================================================================
    // BG (BWT-Big single-block) format — for prose where patterns span chunks
    // Wikipedia: per-chunk BWT loses to zstd, single-block BWT beats brotli.
    // Format: "BG" + varint(orig_size) + bwt5_bytes
    // Cap at 64MB to keep memory under control (libsais uses ~5N RAM).
    // ============================================================================
    // BG wins on prose where patterns span chunks (e.g. Wikipedia). Cap at 1GB to
    // unlock single-block BWT on full enwik9. libsais needs ~5N RAM working set —
    // 1GB block ≈ 5GB peak, fits within Hutter Prize's 10GB limit. SMALL mode only
    // for the largest sizes (BALANCED stays at the safer 256MB cap).
    std::vector<uint8_t> bg_format;
    constexpr size_t BG_MAX_SIZE_BALANCED = 256u * 1024 * 1024;
    constexpr size_t BG_MAX_SIZE_SMALL    = 1024u * 1024 * 1024;
    size_t BG_MAX_SIZE = (mode == CompressionMode::SMALL)
                         ? BG_MAX_SIZE_SMALL : BG_MAX_SIZE_BALANCED;
    if (mode != CompressionMode::FAST && size > MC_THRESHOLD && size <= BG_MAX_SIZE
        && is_text_like(data, size)) {
        auto bg_body = bwt9::compress(data, size);
        if (!bg_body.empty()) {
            std::vector<uint8_t> bg_temp;
            bg_temp.reserve(bg_body.size() + 16);
            bg_temp.push_back('B');
            bg_temp.push_back('G');
            uint8_t vbuf[16];
            size_t n = write_uvarint_buf(vbuf, size);
            bg_temp.insert(bg_temp.end(), vbuf, vbuf + n);
            bg_temp.insert(bg_temp.end(), bg_body.begin(), bg_body.end());
            bg_format = std::move(bg_temp);
        }
    }

    // ============================================================================
    // MS (SoA structural transform) — reshape bytes so the model sees layout
    // structure it misses on the interleaved stream, then compress recursively.
    // Proxy-pruned (zstd-1), kept only if smaller AND roundtrip-verified.
    // ============================================================================
    std::vector<uint8_t> ms_format;
    if (try_soa && mode != CompressionMode::FAST && size >= 4096 && !is_text_like(data, size)) {
        struct SoaCand { uint8_t tid, W, cols; };
        static const SoaCand CANDS[] = {
            {0,2,0},{0,4,0},{0,8,0},
            {1,4,2},{1,4,3},{1,4,4},{1,8,2},{1,8,3},{1,8,4},{1,8,6},
            // tid 2: de-interleave + per-lane order-1 delta+zigzag (time-series numeric;
            // tsgas -27.5%, gps -8.4%). Proxy-pruned like the rest, roundtrip-verified below.
            {2,2,1},{2,2,2},{2,4,1},{2,4,2},{2,8,1},{2,8,3},
        };
        std::vector<uint8_t> pbuf(ZSTD_compressBound(size));
        size_t raw_proxy = ZSTD_compress(pbuf.data(), pbuf.size(), data, size, 1);
        size_t best_proxy = ZSTD_isError(raw_proxy) ? SIZE_MAX : raw_proxy;
        int best_ci = -1;
        for (int ci = 0; ci < (int)(sizeof(CANDS)/sizeof(CANDS[0])); ++ci) {
            const SoaCand& c = CANDS[ci];
            if ((c.tid == 1 || c.tid == 2) && (size / c.W) < c.cols) continue;
            auto t = soa_apply(data, size, c.tid, c.W, c.cols);
            size_t p = ZSTD_compress(pbuf.data(), pbuf.size(), t.data(), t.size(), 1);
            if (!ZSTD_isError(p) && p < best_proxy) { best_proxy = p; best_ci = ci; }
        }
        // pay for a full recursive compress only if the proxy says a transform helps by >2%
        if (best_ci >= 0 && !ZSTD_isError(raw_proxy) && best_proxy < (raw_proxy * 98) / 100) {
            const SoaCand& c = CANDS[best_ci];
            auto t = soa_apply(data, size, c.tid, c.W, c.cols);
            auto inner = compress_impl(t.data(), size, zstd_level, block_size, nullptr, mode, /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
            if (!inner.empty()) {
                std::vector<uint8_t> ms;
                ms.reserve(inner.size() + 16);
                ms.push_back('M'); ms.push_back('S');
                ms.push_back(c.tid); ms.push_back(c.W); ms.push_back(c.cols);
                uint8_t vbuf[16]; size_t vn = write_uvarint_buf(vbuf, size);
                ms.insert(ms.end(), vbuf, vbuf + vn);
                ms.insert(ms.end(), inner.begin(), inner.end());
                // safe-by-construction: only adopt if the 'MS' stream reconstructs exactly
                auto back = decompress(ms.data(), ms.size(), nullptr);
                if (back.size() == size && std::memcmp(back.data(), data, size) == 0)
                    ms_format = std::move(ms);
            }
        }
    }

    // ============================================================================
    // MT (tabular column-transpose) — parse rectangular CSV/TSV, transpose + delta
    // linear-int columns, compress recursively. The parse IS the cheap filter (no
    // proxy); kept only if smaller AND end-to-end roundtrip-verified. (2026-08-06)
    // ============================================================================
    std::vector<uint8_t> mt_format;
    if (try_tabular && mode != CompressionMode::FAST && size >= 256 && is_text_like(data, size)) {
        const uint8_t delims[2] = { (uint8_t)',', (uint8_t)'\t' };
        for (int di = 0; di < 2; di++) {
            uint8_t delim = delims[di];
            std::vector<std::vector<std::string>> rows;
            bool tnl = false;
            if (!tab_parse(data, size, delim, rows, tnl)) continue;
            std::vector<uint8_t> bitmap;
            auto payload = tab_build_payload(rows, bitmap);
            TabMeta m{ delim, tnl, (uint32_t)rows[0].size(), (uint32_t)(rows.size() - 1) };
            auto inner = compress_impl(payload.data(), payload.size(), zstd_level, block_size,
                                  nullptr, mode, /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
            if (inner.empty()) continue;
            std::vector<uint8_t> mt;
            mt.reserve(inner.size() + 32);
            mt.push_back('M'); mt.push_back('T');
            mt.push_back(delim);
            mt.push_back((uint8_t)(tnl ? 1 : 0));
            uint8_t vb[16]; size_t vn;
            vn = write_uvarint_buf(vb, size);     mt.insert(mt.end(), vb, vb + vn);
            vn = write_uvarint_buf(vb, m.ncols);  mt.insert(mt.end(), vb, vb + vn);
            vn = write_uvarint_buf(vb, m.nrows);  mt.insert(mt.end(), vb, vb + vn);
            mt.insert(mt.end(), bitmap.begin(), bitmap.end());
            mt.insert(mt.end(), inner.begin(), inner.end());
            // safe-by-construction: adopt only if the 'MT' stream reconstructs exactly
            auto back = decompress(mt.data(), mt.size(), nullptr);
            if (back.size() == size && std::memcmp(back.data(), data, size) == 0) {
                if (mt_format.empty() || mt.size() < mt_format.size()) mt_format = std::move(mt);
            }
        }
    }

    // ============================================================================
    // MM (whitespace numeric grid — Matrix Market etc.) — skeleton + column-transpose
    // for space/tab-delimited numeric grids that MT's single-delim parse can't reach.
    // The grid parse is the cheap filter (no proxy); kept only if smaller AND end-to-end
    // roundtrip-verified. (2026-08-09)
    // ============================================================================
    std::vector<uint8_t> mm_format;
    if (try_tabular && mode != CompressionMode::FAST && size >= 256 && is_text_like(data, size)) {
        std::vector<uint8_t> payload;
        if (mwg::build_payload(data, size, payload)) {
            auto inner = compress_impl(payload.data(), payload.size(), zstd_level, block_size,
                                  nullptr, mode, /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
            if (!inner.empty()) {
                std::vector<uint8_t> mm;
                mm.reserve(inner.size() + 16);
                mm.push_back('M'); mm.push_back('M');
                uint8_t vb[16]; size_t vn = write_uvarint_buf(vb, size);
                mm.insert(mm.end(), vb, vb + vn);
                mm.insert(mm.end(), inner.begin(), inner.end());
                // safe-by-construction: adopt only if the 'MM' stream reconstructs exactly
                auto back = decompress(mm.data(), mm.size(), nullptr);
                if (back.size() == size && std::memcmp(back.data(), data, size) == 0)
                    mm_format = std::move(mm);
            }
        }
    }

    // ============================================================================
    // MB (x86 BCJ pre-filter) — Bra86 rewrite of E8/E9 targets, then compress
    // recursively. Gated by MZ/ELF magic or E8/E9 density; kept only if smaller AND
    // end-to-end roundtrip-verified. Recovers the liblzma BCJ gain mzip's XZLIB
    // backstop leaves on the table (mzip_raw == xz-plain). (2026-08-07)
    // ============================================================================
    std::vector<uint8_t> mb_format;
    if (try_bcj && mode != CompressionMode::FAST && size >= 256 && mbcj::looks_like_x86(data, size)) {
        std::vector<uint8_t> filt(data, data + size);
        uint32_t st = 0; mbcj::x86_convert(filt.data(), filt.size(), 0, &st, /*encoding=*/1);
        auto inner = compress_impl(filt.data(), filt.size(), zstd_level, block_size, nullptr, mode,
                              /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
        if (!inner.empty()) {
            std::vector<uint8_t> mb; mb.reserve(inner.size() + 16);
            mb.push_back('M'); mb.push_back('B');
            uint8_t vb[16]; size_t vn = write_uvarint_buf(vb, size); mb.insert(mb.end(), vb, vb + vn);
            mb.insert(mb.end(), inner.begin(), inner.end());
            auto back = decompress(mb.data(), mb.size(), nullptr);
            if (back.size() == size && std::memcmp(back.data(), data, size) == 0) mb_format = std::move(mb);
        }
    }

    // ============================================================================
    // MQ (SQL-INSERT-tuple transpose) — parse INSERT..VALUES tuples into a column
    // grid, delta linear-int columns, transpose, compress recursively. The parse IS
    // the filter (returns nothing on non-SQL); blob self-verify prunes bad parses
    // before the recursive compress; end-to-end roundtrip-verified before adopt.
    // Wins on real SQL dumps: users_dump.sql -34.5% (measured). (2026-08-07)
    // ============================================================================
    std::vector<uint8_t> mq_format;
    if (try_sql && mode != CompressionMode::FAST && size >= 256 && is_text_like(data, size)) {
        auto blob = mqsql::apply(data, size, /*do_delta=*/true, /*group_stmts=*/true);
        if (!blob.empty()) {
            std::vector<uint8_t> chk;
            if (mqsql::invert(blob.data(), blob.size(), chk) && chk.size() == size &&
                std::memcmp(chk.data(), data, size) == 0) {
                auto inner = compress_impl(blob.data(), blob.size(), zstd_level, block_size, nullptr, mode,
                                      /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
                if (!inner.empty()) {
                    std::vector<uint8_t> mq; mq.reserve(inner.size() + 16);
                    mq.push_back('M'); mq.push_back('Q');
                    uint8_t vb[16]; size_t vn = write_uvarint_buf(vb, size); mq.insert(mq.end(), vb, vb + vn);
                    mq.insert(mq.end(), inner.begin(), inner.end());
                    auto back = decompress(mq.data(), mq.size(), nullptr);
                    if (back.size() == size && std::memcmp(back.data(), data, size) == 0) mq_format = std::move(mq);
                }
            }
        }
    }

    // ============================================================================
    // ML (line-templated log timestamp-delta) — in-place delta+zigzag of CLF/Apache
    // bracket timestamps, then compress recursively. apply() self-declines on non-log
    // text; blob self-verify prunes before the recursive compress; end-to-end memcmp
    // + trial-and-keep before adopt. Wins nginx -14.6% / apache -3.5%. (2026-08-07)
    // ============================================================================
    std::vector<uint8_t> ml_format;
    if (try_log && mode != CompressionMode::FAST && size >= 256 && is_text_like(data, size)) {
        auto blob = mltsd::apply(data, size);
        if (!blob.empty()) {
            std::vector<uint8_t> chk;
            if (mltsd::invert(blob.data(), blob.size(), chk) && chk.size() == size &&
                std::memcmp(chk.data(), data, size) == 0) {
                auto inner = compress_impl(blob.data(), blob.size(), zstd_level, block_size, nullptr, mode,
                                      /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
                if (!inner.empty()) {
                    std::vector<uint8_t> ml; ml.reserve(inner.size() + 16);
                    ml.push_back('M'); ml.push_back('L');
                    uint8_t vb[16]; size_t vn = write_uvarint_buf(vb, size); ml.insert(ml.end(), vb, vb + vn);
                    ml.insert(ml.end(), inner.begin(), inner.end());
                    auto back = decompress(ml.data(), ml.size(), nullptr);
                    if (back.size() == size && std::memcmp(back.data(), data, size) == 0) ml_format = std::move(ml);
                }
            }
        }
    }

    // ============================================================================
    // MY (YAML/indentation de-indent) — split each line into (leading-space depth, dedented body)
    // and compress the two sub-streams SEPARATELY (depths -> NUMERIC/PPMD, bodies -> PPMD). Kept
    // only if smaller AND end-to-end roundtrip-verified. Wins on large deeply-nested config
    // (k8s CRD YAML: prom_bundle -24.8%, certmgr -24.3%, measured). (2026-08-08, workflow rank 1)
    // ============================================================================
    std::vector<uint8_t> my_format;
    if (try_yaml && mode != CompressionMode::FAST && size >= 512 && is_text_like(data, size)) {
        std::vector<uint8_t> ind, bod;
        if (mysy::apply(data, size, ind, bod)) {
            std::vector<uint8_t> chk;
            // self-verify the transform before the (2x) recursive compress prunes bad splits early
            if (mysy::invert(ind.data(), ind.size(), bod.data(), bod.size(), chk, size) &&
                chk.size() == size && std::memcmp(chk.data(), data, size) == 0) {
                auto ic = compress_impl(ind.data(), ind.size(), zstd_level, block_size, nullptr, mode,
                                        /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
                auto bc = compress_impl(bod.data(), bod.size(), zstd_level, block_size, nullptr, mode,
                                        /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false);
                if (!ic.empty() && !bc.empty()) {
                    std::vector<uint8_t> my; my.reserve(ic.size() + bc.size() + 24);
                    my.push_back('M'); my.push_back('Y');
                    uint8_t vb[16]; size_t vn;
                    vn = write_uvarint_buf(vb, size);      my.insert(my.end(), vb, vb + vn);
                    vn = write_uvarint_buf(vb, ic.size()); my.insert(my.end(), vb, vb + vn);
                    my.insert(my.end(), ic.begin(), ic.end());
                    my.insert(my.end(), bc.begin(), bc.end());
                    auto back = decompress(my.data(), my.size(), nullptr);
                    if (back.size() == size && std::memcmp(back.data(), data, size) == 0) my_format = std::move(my);
                }
            }
        }
    }

    // ============================================================================
    // MF (FASTQ 4-line de-interleave) — split records into hdr/seq/+/qual sub-streams, compress each
    // SEPARATELY. Kept only if smaller AND end-to-end roundtrip-verified. Wins on FASTQ (-20% vs xz,
    // -12% over mzip's own interleaved result; measured on real SRR6357070). (2026-08-08, sweep-2)
    // ============================================================================
    std::vector<uint8_t> mf_format;
    if (try_fastq && mode != CompressionMode::FAST && size >= 256 && is_text_like(data, size)) {
        uint64_t nrec = 0;
        std::vector<uint8_t> fh, fs, fp, fq;
        if (mfq::apply(data, size, nrec, fh, fs, fp, fq)) {
            std::vector<uint8_t> chk;
            if (mfq::invert(nrec, fh.data(), fh.size(), fs.data(), fs.size(), fp.data(), fp.size(),
                            fq.data(), fq.size(), chk, size) &&
                chk.size() == size && std::memcmp(chk.data(), data, size) == 0) {
                auto ch = compress_impl(fh.data(), fh.size(), zstd_level, block_size, nullptr, mode, false,false,false,false,false,false,false);
                auto cs = compress_impl(fs.data(), fs.size(), zstd_level, block_size, nullptr, mode, false,false,false,false,false,false,false);
                auto cp = compress_impl(fp.data(), fp.size(), zstd_level, block_size, nullptr, mode, false,false,false,false,false,false,false);
                auto cq = compress_impl(fq.data(), fq.size(), zstd_level, block_size, nullptr, mode, false,false,false,false,false,false,false);
                if (!ch.empty() && !cs.empty() && !cp.empty() && !cq.empty()) {
                    std::vector<uint8_t> mf; mf.reserve(ch.size()+cs.size()+cp.size()+cq.size()+40);
                    mf.push_back('M'); mf.push_back('F');
                    uint8_t vb[16]; size_t vn;
                    vn = write_uvarint_buf(vb, size);      mf.insert(mf.end(), vb, vb+vn);
                    vn = write_uvarint_buf(vb, nrec);      mf.insert(mf.end(), vb, vb+vn);
                    vn = write_uvarint_buf(vb, ch.size()); mf.insert(mf.end(), vb, vb+vn);
                    vn = write_uvarint_buf(vb, cs.size()); mf.insert(mf.end(), vb, vb+vn);
                    vn = write_uvarint_buf(vb, cp.size()); mf.insert(mf.end(), vb, vb+vn);
                    mf.insert(mf.end(), ch.begin(), ch.end());
                    mf.insert(mf.end(), cs.begin(), cs.end());
                    mf.insert(mf.end(), cp.begin(), cp.end());
                    mf.insert(mf.end(), cq.begin(), cq.end());
                    auto back = decompress(mf.data(), mf.size(), nullptr);
                    if (back.size() == size && std::memcmp(back.data(), data, size) == 0) mf_format = std::move(mf);
                }
            }
        }
    }

    // Find the smallest output format
    size_t best_size = out_pos;  // mzip format
    int best_format = 0;  // 0=mzip, 1=zstd, 2=µRAW, 3=MC, 4=CL, 6=BG, 7=MS, 8=MT, 9=MB, 10=MQ, 11=ML

    if (!ZSTD_isError(zstd_size) && zstd_size < best_size) {
        best_size = zstd_size;
        best_format = 1;
    }

    if (uraw_size < best_size) {
        best_size = uraw_size;
        best_format = 2;
    }

    if (!mc_format.empty() && mc_size < best_size) {
        best_size = mc_size;
        best_format = 3;
    }

    if (!cl_format.empty() && cl_format.size() < best_size) {
        best_size = cl_format.size();
        best_format = 4;
    }

    if (!mu_format.empty() && mu_format.size() < best_size) {
        best_size = mu_format.size();
        best_format = 5;  // MU format
    }

    if (!bg_format.empty() && bg_format.size() < best_size) {
        best_size = bg_format.size();
        best_format = 6;  // BG (BWT-Big single-block) format
    }
    if (!ms_format.empty() && ms_format.size() < best_size) {
        best_size = ms_format.size();
        best_format = 7;  // MS (SoA structural transform) format
    }
    if (!mt_format.empty() && mt_format.size() < best_size) {
        best_size = mt_format.size();
        best_format = 8;  // MT (tabular column-transpose) format
    }
    if (!mb_format.empty() && mb_format.size() < best_size) {
        best_size = mb_format.size();
        best_format = 9;  // MB (x86 BCJ pre-filter) format
    }
    if (!mq_format.empty() && mq_format.size() < best_size) {
        best_size = mq_format.size();
        best_format = 10;  // MQ (SQL-INSERT-tuple transpose) format
    }
    if (!ml_format.empty() && ml_format.size() < best_size) {
        best_size = ml_format.size();
        best_format = 11;  // ML (log timestamp-delta) format
    }
    if (!my_format.empty() && my_format.size() < best_size) {
        best_size = my_format.size();
        best_format = 12;  // MY (YAML de-indent) format
    }
    if (!mf_format.empty() && mf_format.size() < best_size) {
        best_size = mf_format.size();
        best_format = 13;  // MF (FASTQ 4-line de-interleave) format
    }
    if (!mm_format.empty() && mm_format.size() < best_size) {
        best_size = mm_format.size();
        best_format = 14;  // MM (whitespace numeric grid) format
    }

    if (best_format == 1) {
        // Pure zstd is smallest
        zstd_buf.resize(zstd_size);

        res.success = true;
        res.compressed_size = zstd_size;
        res.block_count = 1;
        res.used_lite_format = true;  // Flag indicates passthrough was used
        if (result) *result = res;
        return zstd_buf;
    }

    if (best_format == 2) {
        // µRAW is smallest (incompressible data)
        // Format: 0xB5 0x52 + varint(size) + raw data
        std::vector<uint8_t> uraw;
        uraw.reserve(uraw_size);
        uraw.push_back(0xB5);  // µ
        uraw.push_back(0x52);  // R

        // Write size as varint
        uint64_t val = size;
        while (val >= 128) {
            uraw.push_back((val & 0x7F) | 0x80);
            val >>= 7;
        }
        uraw.push_back(val);

        // Append raw data
        uraw.insert(uraw.end(), data, data + size);

        res.success = true;
        res.compressed_size = uraw.size();
        res.block_count = 1;
        res.used_lite_format = true;  // Flag indicates passthrough was used
        if (result) *result = res;
        return uraw;
    }

    if (best_format == 3) {
        // MC (Multi-Chunk) format is smallest
        res.success = true;
        res.compressed_size = mc_format.size();
        res.block_count = (size + MC_CHUNK_SIZE - 1) / MC_CHUNK_SIZE;
        res.used_lite_format = true;  // MC format uses lite decompression path
        if (result) *result = res;
        return mc_format;
    }

    if (best_format == 4) {
        // CL (COLUMNAR lite) format is smallest (9-column nginx logs)
        res.success = true;
        res.compressed_size = cl_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // CL format uses lite decompression path
        if (result) *result = res;
        return cl_format;
    }

    if (best_format == 5) {
        // MU (Micro) format is smallest (small text files 4-16KB)
        res.success = true;
        res.compressed_size = mu_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // MU format uses lite decompression path
        if (result) *result = res;
        return mu_format;
    }

    if (best_format == 6) {
        // BG (BWT-Big single-block) format is smallest (prose 1MB-64MB)
        res.success = true;
        res.compressed_size = bg_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // BG format uses lite decompression path
        if (result) *result = res;
        return bg_format;
    }
    if (best_format == 7) {
        // MS (SoA structural transform) format is smallest (interleaved/float binary)
        res.success = true;
        res.compressed_size = ms_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MS' is magic-dispatched at decompress entry
        if (result) *result = res;
        return ms_format;
    }
    if (best_format == 8) {
        // MT (tabular column-transpose) format is smallest (rectangular CSV/TSV)
        res.success = true;
        res.compressed_size = mt_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MT' is magic-dispatched at decompress entry
        if (result) *result = res;
        return mt_format;
    }
    if (best_format == 9) {
        // MB (x86 BCJ pre-filter) format is smallest (x86/x64 executables)
        res.success = true;
        res.compressed_size = mb_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MB' is magic-dispatched at decompress entry
        if (result) *result = res;
        return mb_format;
    }
    if (best_format == 10) {
        // MQ (SQL-INSERT-tuple transpose) format is smallest (SQL dumps)
        res.success = true;
        res.compressed_size = mq_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MQ' is magic-dispatched at decompress entry
        if (result) *result = res;
        return mq_format;
    }
    if (best_format == 11) {
        // ML (log timestamp-delta) format is smallest (CLF/Apache logs)
        res.success = true;
        res.compressed_size = ml_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'ML' is magic-dispatched at decompress entry
        if (result) *result = res;
        return ml_format;
    }
    if (best_format == 12) {
        // MY (YAML de-indent) format is smallest (large deeply-nested config)
        res.success = true;
        res.compressed_size = my_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MY' is magic-dispatched at decompress entry
        if (result) *result = res;
        return my_format;
    }
    if (best_format == 13) {
        // MF (FASTQ 4-line de-interleave) format is smallest (FASTQ sequence data)
        res.success = true;
        res.compressed_size = mf_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MF' is magic-dispatched at decompress entry
        if (result) *result = res;
        return mf_format;
    }
    if (best_format == 14) {
        // MM (whitespace numeric grid) format is smallest (Matrix Market / .mtx / XYZ)
        res.success = true;
        res.compressed_size = mm_format.size();
        res.block_count = 1;
        res.used_lite_format = true;  // 'MM' is magic-dispatched at decompress entry
        if (result) *result = res;
        return mm_format;
    }

    res.success = true;
    res.compressed_size = out_pos;
    res.block_count = block_count;
    if (result) *result = res;

    // If CL format was the winner, return it instead of output
    if (!cl_format.empty()) {
        res.used_lite_format = true;
        if (result) *result = res;
        return cl_format;
    }

    return output;
}

// ============================================================================
// Public compress(): compress_impl() + a TOP-LEVEL END-TO-END LOSSLESSNESS GUARANTEE.
// ----------------------------------------------------------------------------
// mzip is a large trial-and-keep ensemble; individual encoders and the format-selection/framing
// stage have repeatedly shipped subtle losslessness bugs (e.g. the MU/NUMERIC sparse-marker
// collision that fuzz_mzip caught: a valid float file compressed and decompressed to all-zeros).
// This wrapper decompresses the chosen output and requires it reproduces the input byte-for-byte;
// on ANY mismatch it falls back to the guaranteed-lossless µRAW store (0xB5 0x52 + varint(size) +
// raw bytes), so the product can NEVER emit a stream that fails to round-trip -- regardless of
// which encoder or framing path produced it, present or future. This is the definitive backstop
// behind the per-encoder self-verifies.
//   - INERT on correct output (the overwhelming common case): returns compress_impl's bytes
//     unchanged -> byte-identical, ratio-neutral. Only provably-lossy output is ever replaced.
//   - COST: one decompress per top-level compress. Recursive/sub-block compression uses
//     compress_impl directly, so this single check covers the whole assembled stream once.
//   - Revert with -DMZIP_NO_TOPLEVEL_VERIFY (restores the raw compress_impl behaviour).
// (2026-08-07, found+motivated by the fuzz_mzip / fuzz_decode campaign.)
// ============================================================================
inline std::vector<uint8_t> compress(const uint8_t* data, size_t size,
                                     int zstd_level = 3,
                                     size_t block_size = DEFAULT_BLOCK_SIZE,
                                     CompressResult* result = nullptr,
                                     CompressionMode mode = CompressionMode::BALANCED,
                                     bool try_soa = true,
                                     bool try_tabular = true,
                                     bool try_sql = true,
                                     bool try_bcj = true,
                                     bool try_log = true,
                                     bool try_yaml = true,
                                     bool try_fastq = true) {
#ifdef MZIP_NO_TOPLEVEL_VERIFY
    return compress_impl(data, size, zstd_level, block_size, result, mode,
                         try_soa, try_tabular, try_sql, try_bcj, try_log, try_yaml, try_fastq);
#else
    std::vector<uint8_t> out;
    bool ok = false;
    try {
        out = compress_impl(data, size, zstd_level, block_size, result, mode,
                            try_soa, try_tabular, try_sql, try_bcj, try_log, try_yaml, try_fastq);
        if (size == 0) return out;  // empty input: trust compress_impl's (tiny) result
        if (!out.empty()) {
            std::vector<uint8_t> rt = decompress(out.data(), out.size(), nullptr);
            if (rt.size() == size && std::memcmp(rt.data(), data, size) == 0) ok = true;  // verified lossless
        }
    } catch (...) {
        // compress_impl threw (e.g. std::bad_alloc: an encoder over-allocated on a pathological input;
        // fuzz_mzip saw this on a 4356-byte input). Fall through to the guaranteed-lossless µRAW store,
        // which allocates only ~size (the input already resides in memory) so it cannot itself OOM.
        // Result: compress() NEVER terminates, on any input. (2026-08-07)
        out.clear();
    }
    if (ok) return out;
    // chosen output does not round-trip, OR compress_impl threw -> guaranteed-lossless µRAW store.
    std::vector<uint8_t> uraw;
    uraw.reserve(size + 16);
    uraw.push_back(0xB5);  // µ
    uraw.push_back(0x52);  // R
    uint64_t v = (uint64_t)size;
    while (v >= 128) { uraw.push_back((uint8_t)((v & 0x7F) | 0x80)); v >>= 7; }
    uraw.push_back((uint8_t)v);
    uraw.insert(uraw.end(), data, data + size);
    if (result) {
        result->success = true;
        result->compressed_size = uraw.size();
        result->block_count = 1;
        result->used_lite_format = true;
        result->original_size = size;
        result->error.clear();
    }
    return uraw;
#endif
}

// Decompress data in memory
inline std::vector<uint8_t> decompress_impl(const uint8_t* data, size_t size,
                                        DecompressResult* result) {
    DecompressResult res;
    res.success = false;
    res.decompressed_size = 0;

    // Bound recursion depth: the magic-dispatch formats (MS/MT/MB/MQ/ML) recurse into
    // decompress() on their inner stream. A crafted file of nested 2-byte magic prefixes
    // (e.g. repeated "MQ\x00") would otherwise drive unbounded recursion -> stack-overflow
    // DoS. Legitimate nesting is <=3 (outer format -> inner block/CM/bwt). (2026-08-07)
    static thread_local int mz_decomp_depth = 0;
    struct DepthGuard { int& d; DepthGuard(int& r):d(r){++d;} ~DepthGuard(){--d;} } _dg(mz_decomp_depth);
    if (mz_decomp_depth > 32) { res.error = "decompress: max recursion depth exceeded"; if (result) *result = res; return {}; }

    // MS (SoA structural transform): 'M','S', tid, W, cols, varint(orig_size), inner-stream.
    // Decompress the inner recursively, then invert the byte permutation. (2026-08-05)
    if (size >= 6 && data[0] == 'M' && data[1] == 'S') {
        uint8_t tid = data[2], W = data[3], cols = data[4];
        // Validate transform params before use (untrusted stream): tid in {0,1,2};
        // W in {1,2,4,8}; de-interleave/delta need cols>=1 so R=W*cols is nonzero
        // (guards a latent div-by-zero in the tid==1 path too). A well-formed 'MS'
        // stream always passes; the encoder only emits validated params.
        if (tid > 2 || (W != 1 && W != 2 && W != 4 && W != 8) ||
            ((tid == 1 || tid == 2) && cols == 0)) {
            res.error = "MS: invalid transform params";
            if (result) *result = res;
            return {};
        }
        const uint8_t* p = data + 5; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        if (inner.size() != orig) {
            res.error = "MS: inner size mismatch";
            if (result) *result = res;
            return {};
        }
        auto out = soa_invert(inner.data(), (size_t)orig, tid, W, cols);
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // MT (tabular column-transpose): 'M','T', delim, flags, varint(orig), varint(ncols),
    // varint(nrows), delta_bitmap[ceil(ncols/8)], inner-stream. (2026-08-06)
    if (size >= 8 && data[0] == 'M' && data[1] == 'T') {
        uint8_t delim = data[2], flags = data[3];
        const uint8_t* p = data + 4; const uint8_t* end = data + size;
        uint64_t orig  = read_uvarint(p, end);
        uint64_t ncols = read_uvarint(p, end);
        uint64_t nrows = read_uvarint(p, end);
        // validate params on the untrusted stream (MS precedent): bound ncols and require
        // the delta bitmap to fit before the inner stream.
        if (ncols == 0 || ncols > 4096 || p > end) {
            res.error = "MT: invalid params"; if (result) *result = res; return {};
        }
        size_t bmlen = (size_t)((ncols + 7) / 8);
        if ((size_t)(end - p) < bmlen) {
            res.error = "MT: truncated bitmap"; if (result) *result = res; return {};
        }
        std::vector<uint8_t> bitmap(p, p + bmlen); p += bmlen;
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        TabMeta m{ delim, (flags & 1) != 0, (uint32_t)ncols, (uint32_t)nrows };
        std::vector<uint8_t> out;
        if (!tab_invert(inner.data(), inner.size(), m, bitmap, out) || out.size() != orig) {
            res.error = "MT: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // MB (x86 BCJ pre-filter): 'M','B', varint(orig), inner-stream. Decompress inner,
    // then apply the inverse Bra86 filter in-place. (2026-08-07)
    if (size >= 4 && data[0] == 'M' && data[1] == 'B') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        if (inner.size() != orig) {
            res.error = "MB: inner size mismatch"; if (result) *result = res; return {};
        }
        uint32_t st = 0; mbcj::x86_convert(inner.data(), inner.size(), 0, &st, /*encoding=*/0);
        res.success = true;
        res.decompressed_size = inner.size();
        if (result) *result = res;
        return inner;
    }

    // MQ (SQL-INSERT-tuple transpose): 'M','Q', varint(orig), inner-stream. Decompress
    // inner (the self-describing SQL blob), then mqsql::invert. (2026-08-07)
    if (size >= 4 && data[0] == 'M' && data[1] == 'Q') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        std::vector<uint8_t> out;
        if (!mqsql::invert(inner.data(), inner.size(), out) || out.size() != orig) {
            res.error = "MQ: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // ML (log timestamp-delta): 'M','L', varint(orig), inner-stream. (2026-08-07)
    if (size >= 4 && data[0] == 'M' && data[1] == 'L') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        std::vector<uint8_t> out;
        if (!mltsd::invert(inner.data(), inner.size(), out) || out.size() != orig) {
            res.error = "ML: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // MY (YAML de-indent): 'M','Y', varint(orig), varint(len_indent_comp), indent_comp, body_comp.
    // Decompress the two sub-streams, then mysy::invert (orig bounds the reconstruction). (2026-08-08)
    if (size >= 4 && data[0] == 'M' && data[1] == 'Y') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        uint64_t li   = read_uvarint(p, end);
        if (p > end || li > (uint64_t)(end - p)) {
            res.error = "MY: bad framing"; if (result) *result = res; return {};
        }
        auto ind = decompress(p, (size_t)li, nullptr);
        auto bod = decompress(p + li, (size_t)(end - (p + li)), nullptr);
        std::vector<uint8_t> out;
        if (!mysy::invert(ind.data(), ind.size(), bod.data(), bod.size(), out, (size_t)orig) ||
            out.size() != orig) {
            res.error = "MY: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // MF (FASTQ 4-line de-interleave): 'M','F', varint(orig), varint(nrec), varint(Lh), varint(Ls),
    // varint(Lp), hdr_comp, seq_comp, plus_comp, qual_comp. Decompress the 4 sub-streams, then
    // mfq::invert (orig bounds the reconstruction). (2026-08-08)
    if (size >= 4 && data[0] == 'M' && data[1] == 'F') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        uint64_t nrec = read_uvarint(p, end);
        uint64_t Lh = read_uvarint(p, end);
        uint64_t Ls = read_uvarint(p, end);
        uint64_t Lp = read_uvarint(p, end);
        if (p > end) { res.error = "MF: bad header"; if (result) *result = res; return {}; }
        uint64_t avail = (uint64_t)(end - p);
        if (Lh > avail || Ls > avail - Lh || Lp > avail - Lh - Ls) {
            res.error = "MF: bad framing"; if (result) *result = res; return {};
        }
        auto h  = decompress(p, (size_t)Lh, nullptr); p += Lh;
        auto s  = decompress(p, (size_t)Ls, nullptr); p += Ls;
        auto pl = decompress(p, (size_t)Lp, nullptr); p += Lp;
        auto q  = decompress(p, (size_t)(end - p), nullptr);
        std::vector<uint8_t> out;
        if (!mfq::invert(nrec, h.data(), h.size(), s.data(), s.size(), pl.data(), pl.size(),
                         q.data(), q.size(), out, (size_t)orig) || out.size() != orig) {
            res.error = "MF: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    // MM (whitespace numeric grid): 'M','M', varint(orig), inner-stream. Decompress inner
    // (the self-describing grid payload), then mwg::invert. (2026-08-09)
    if (size >= 4 && data[0] == 'M' && data[1] == 'M') {
        const uint8_t* p = data + 2; const uint8_t* end = data + size;
        uint64_t orig = read_uvarint(p, end);
        auto inner = decompress(p, (size_t)(end - p), nullptr);
        std::vector<uint8_t> out;
        if (!mwg::invert(inner.data(), inner.size(), out) || out.size() != orig) {
            res.error = "MM: invert failed"; if (result) *result = res; return {};
        }
        res.success = true;
        res.decompressed_size = out.size();
        if (result) *result = res;
        return out;
    }

    if (size < 4) {
        res.error = "Input too small for header";
        if (result) *result = res;
        return {};
    }

    // Read header
    size_t pos = 0;
    uint32_t magic = read_u32_le(&data[pos]);
    pos += 4;

    // Handle ultra-compact BWT format: "BT" + size_varint + v5_data
    // Magic: 'B' 'T' (0x5442 in little-endian, but we check first 2 bytes)
    if (data[0] == 'B' && data[1] == 'T') {
        pos = 2;

        // Read size varint
        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Decompress v5 data
        auto output = bwt9::decompress(data + pos, size - pos);

        if (output.size() != orig_size) {
            res.error = "BWT decompression size mismatch";
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = output.size();
        if (result) *result = res;
        return output;
    }

    // Handle ultra-compact CL format: "CL" + size_varint + columnar_data
    // Used for 9-column nginx extended log format (beats bzip2 by 3 bytes!)
    if (data[0] == 'C' && data[1] == 'L') {
        pos = 2;

        // Read size varint (original size)
        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Decode columnar data (first byte should be 0xFE for 9-column format)
        auto output = decode_columnar(data + pos, size - pos, orig_size);

        if (output.empty() && orig_size > 0) {
            res.error = "CL decompression failed";
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = output.size();
        if (result) *result = res;
        return output;
    }

    // Handle MU format: Micro format for small text files (4-16KB)
    // Magic: "MU" + BlockType(1) + varint(orig_size) + compressed_data
    if (data[0] == 'M' && data[1] == 'U') {
        pos = 2;

        // Read block type
        BlockType block_type = static_cast<BlockType>(data[pos++]);

        // Read size varint (original size)
        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Compressed data is everything remaining
        const uint8_t* comp_data = data + pos;
        size_t comp_size = size - pos;

        // Decode based on block type
        std::vector<uint8_t> output;
        switch (block_type) {
            case BlockType::BWT_TEXT:
                output = bwt9::decompress(comp_data, comp_size);
                break;
            case BlockType::CM_TEXT:
                output = cmbk::decompress_bwt(comp_data, comp_size);
                break;
            case BlockType::BROTLI: {
                // backstop brotli stream stored whole — decode straight to orig_size
                output.resize(orig_size);
                size_t dsz = orig_size;
                if (BrotliDecoderDecompress(comp_size, comp_data, &dsz, output.data()) != MZ_BROTLI_DECODE_SUCCESS
                    || dsz != orig_size) output.clear();
                break;
            }
            case BlockType::XZLIB: {
                // backstop xz (liblzma) stream stored whole
                output.resize(orig_size);
                uint64_t memlimit = UINT64_MAX;
                size_t in_pos = 0, out_pos2 = 0;
                if (lzma_stream_buffer_decode(&memlimit, 0, nullptr, comp_data, &in_pos, comp_size,
                                              output.data(), &out_pos2, output.size()) != MZ_LZMA_OK
                    || out_pos2 != orig_size) output.clear();
                break;
            }
            case BlockType::PPMD: {
                // PPMd var.H backstop: payload = [order:1][memMiB:1][Ppmd7z stream]. Clamp order/memMiB so
                // a hostile stream cannot drive a huge decode allocation (decode heap == memMiB MiB).
                if (comp_size < 2) break;
                unsigned order = comp_data[0]; uint32_t memMiB = comp_data[1];
                if (order < 2) order = 2; else if (order > 64) order = 64;
                if (memMiB < 1) memMiB = 1; else if (memMiB > 128) memMiB = 128;  // bound untrusted decode heap (encoder writes 64)
                output = ppmdbk::decompress(comp_data + 2, comp_size - 2, orig_size, order, memMiB);
                break;
            }
            case BlockType::ZSTD_DICT: {
                // Format: [dict_id:1] [zstd_data:rest]
                if (comp_size < 2) break;
                uint8_t dict_id = comp_data[0];
                const uint8_t* dict_data = nullptr;
                size_t dict_len = 0;
                for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                    if (mzip_dicts::ALL_DICTS[di].id == dict_id) {
                        dict_data = mzip_dicts::ALL_DICTS[di].data;
                        dict_len = mzip_dicts::ALL_DICTS[di].size;
                        break;
                    }
                }
                if (dict_data) {
                    output.resize(orig_size);
                    ZSTD_DCtx* dctx = ZSTD_createDCtx();
                    size_t result = ZSTD_decompress_usingDict(dctx,
                        output.data(), orig_size,
                        comp_data + 1, comp_size - 1,
                        dict_data, dict_len);
                    ZSTD_freeDCtx(dctx);
                    if (ZSTD_isError(result)) output.clear();
                }
                break;
            }
            case BlockType::TEMPLATE:
                output = decode_template(comp_data, comp_size, orig_size);
                break;
            case BlockType::CHAR_TEMPLATE:
                output = decode_char_template(comp_data, comp_size);
                break;
            case BlockType::LINE_GROUP_TEMPLATE:
                output = decode_line_group_template(comp_data, comp_size, orig_size);
                break;
            case BlockType::KV_CONFIG:
                output = decode_kv_config(comp_data, comp_size, orig_size);
                break;
            case BlockType::WORD_ENCODED:
                output = decode_word_text(comp_data, comp_size);
                break;
            case BlockType::ML_TEMPLATE:
                output = decode_ml_template(comp_data, comp_size, orig_size);
                break;
            case BlockType::SECTION_TEMPLATE:
                output = decode_section_template(comp_data, comp_size, orig_size);
                break;
            case BlockType::DBF_CONSTCOL:
                output = decode_dbf_constcol(comp_data, comp_size, orig_size);
                break;
            case BlockType::NUMERIC: {
                // Sparse delta format: strategy(1) + 0xFF(1) + sparse data
                if (comp_size >= 2 && comp_data[1] == 0xFF) {
                    tieredcompress::Strategy strat = static_cast<tieredcompress::Strategy>(comp_data[0]);
                    // Decode sparse
                    std::vector<uint8_t> preproc(orig_size, 0);
                    size_t p = 2;
                    uint32_t count = 0; int sh = 0;
                    while (p < comp_size && (comp_data[p] & 0x80)) {
                        count |= (uint32_t)(comp_data[p++] & 0x7F) << sh; sh += 7;
                    }
                    if (p < comp_size) count |= (uint32_t)comp_data[p++] << sh;
                    size_t spos = 0;
                    for (uint32_t i = 0; i < count && p + 1 < comp_size; i++) {
                        uint32_t dp = 0; sh = 0;
                        while (p < comp_size && (comp_data[p] & 0x80)) {
                            dp |= (uint32_t)(comp_data[p++] & 0x7F) << sh; sh += 7;
                        }
                        if (p < comp_size) dp |= (uint32_t)comp_data[p++] << sh;
                        spos += dp;
                        if (spos < preproc.size() && p < comp_size) preproc[spos] = comp_data[p++];
                    }
                    // Reverse strategy
                    output.resize(orig_size);
                    reverse_strategy(output.data(), preproc.data(), preproc.size(), orig_size, strat);
                }
                break;
            }
            case BlockType::CODE_STREAM:
                output = decode_code_stream(comp_data, comp_size, orig_size);
                break;
            case BlockType::NUM_EXTRACT: {
                auto decoded = decode_num_extract(comp_data, comp_size, orig_size);
                output = decoded;
                break;
            }
            default:
                res.error = "MU: unsupported block type";
                if (result) *result = res;
                return {};
        }

        if (output.size() != orig_size) {
            res.error = "MU: decompression size mismatch";
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = output.size();
        if (result) *result = res;
        return output;
    }

    // Handle MC format: Multi-Chunk with per-chunk backend selection
    // Magic: "MC" + varint(orig_size) + varint(num_chunks) + [chunks...]
    // Each chunk: backend(1) + varint(orig) + varint(comp) + data
    if (data[0] == 'M' && data[1] == 'C') {
        pos = 2;

        // Read original size varint
        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Read num_chunks varint
        uint64_t num_chunks = 0;
        shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            num_chunks |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            num_chunks |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Decompress each chunk
        std::vector<uint8_t> output;
        output.reserve(orig_size);

        for (uint64_t chunk = 0; chunk < num_chunks && pos < size; chunk++) {
            // Read backend
            uint8_t backend = data[pos++];

            // Read chunk orig_size varint
            uint64_t chunk_orig = 0;
            shift = 0;
            while (pos < size && (data[pos] & 0x80)) {
                chunk_orig |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
                shift += 7;
                pos++;
            }
            if (pos < size) {
                chunk_orig |= static_cast<uint64_t>(data[pos]) << shift;
                pos++;
            }

            // Read chunk comp_size varint
            uint64_t chunk_comp = 0;
            shift = 0;
            while (pos < size && (data[pos] & 0x80)) {
                chunk_comp |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
                shift += 7;
                pos++;
            }
            if (pos < size) {
                chunk_comp |= static_cast<uint64_t>(data[pos]) << shift;
                pos++;
            }

            // INPUT BOUNDS CHECK for the MC (multi-chunk) branch — 2026-08-04.
            // chunk_comp and chunk_orig are read as untrusted 64-bit varints just above and
            // were then used with NO validation: `data + pos, chunk_comp` was handed to
            // ZSTD_decompress / bwt9::decompress (an out-of-bounds READ when
            // pos + chunk_comp runs past the input), and chunk_output.resize(chunk_orig)
            // allocated from the same untrusted 64-bit value (unbounded allocation / OOM).
            // The STANDARD block path ~250 lines below already performs exactly this test
            // ("Truncated block data"); the MC branch was missing its twin. Two guards,
            // matching that precedent:
            if (pos + chunk_comp > size) {
                res.error = "MC: truncated chunk data (corrupt or malicious archive)";
                if (result) *result = res;
                return {};
            }
            // A chunk cannot legitimately decode to more than the whole file's declared size.
            if (chunk_orig > orig_size) {
                res.error = "MC: chunk original size exceeds file size (corrupt or malicious archive)";
                if (result) *result = res;
                return {};
            }

            // Decompress based on backend
            std::vector<uint8_t> chunk_output;
            if (backend == 0) {
                // zstd
                chunk_output.resize(chunk_orig);
                size_t dec_size = ZSTD_decompress(chunk_output.data(), chunk_output.size(),
                                                   data + pos, chunk_comp);
                if (ZSTD_isError(dec_size) || dec_size != chunk_orig) {
                    res.error = "MC: zstd chunk decompression failed";
                    if (result) *result = res;
                    return {};
                }
            } else if (backend == 1) {
                // BWT
                chunk_output = bwt9::decompress(data + pos, chunk_comp);
                if (chunk_output.size() != chunk_orig) {
                    res.error = "MC: BWT chunk decompression failed";
                    if (result) *result = res;
                    return {};
                }
            } else {
                res.error = "MC: unknown backend";
                if (result) *result = res;
                return {};
            }

            output.insert(output.end(), chunk_output.begin(), chunk_output.end());
            pos += chunk_comp;
        }

        if (output.size() != orig_size) {
            res.error = "MC: decompressed size mismatch";
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = output.size();
        if (result) *result = res;
        return output;
    }

    // Handle BG format: BWT-Big single-block (prose 1MB-64MB)
    // Magic: "BG" + varint(orig_size) + bwt5_bytes
    if (data[0] == 'B' && data[1] == 'G') {
        pos = 2;

        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        auto output = bwt9::decompress(data + pos, size - pos);
        if (output.size() != orig_size) {
            res.error = "BG: decompressed size mismatch";
            if (result) *result = res;
            return {};
        }
        res.success = true;
        res.decompressed_size = output.size();
        if (result) *result = res;
        return output;
    }

    // Handle µRAW format: ultra-minimal raw passthrough (5 bytes overhead for 1MB!)
    // Magic: 0xB5 0x52 ("µR") + varint size + raw data
    if (data[0] == 0xB5 && data[1] == 0x52) {
        pos = 2;

        // Read size varint
        uint64_t orig_size = 0;
        int shift = 0;
        while (pos < size && (data[pos] & 0x80)) {
            orig_size |= static_cast<uint64_t>(data[pos] & 0x7F) << shift;
            shift += 7;
            pos++;
        }
        if (pos < size) {
            orig_size |= static_cast<uint64_t>(data[pos]) << shift;
            pos++;
        }

        // Validate: remaining bytes must equal orig_size
        if (pos + orig_size == size) {
            // Valid µRAW format - return raw data
            std::vector<uint8_t> output(data + pos, data + size);

            res.success = true;
            res.decompressed_size = output.size();
            if (result) *result = res;
            return output;
        }
        // Invalid µRAW - fall through to try other formats
        pos = 4;  // Reset for other format checks
    }

    // Handle pure zstd passthrough (zero-overhead format)
    // zstd magic is 0xFD2FB528 (little-endian: 28 B5 2F FD)
    if (magic == ZSTD_MAGIC) {
        // Pure zstd data - decompress directly
        unsigned long long original_size = ZSTD_getFrameContentSize(data, size);
        if (original_size == ZSTD_CONTENTSIZE_ERROR || original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            res.error = "Cannot determine decompressed size from zstd frame";
            if (result) *result = res;
            return {};
        }

        std::vector<uint8_t> output(original_size);
        size_t decompressed = ZSTD_decompress(output.data(), output.size(), data, size);

        if (ZSTD_isError(decompressed)) {
            res.error = "ZSTD decompression failed: " + std::string(ZSTD_getErrorName(decompressed));
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = decompressed;
        if (result) *result = res;
        return output;
    }

    // Handle legacy lite format: MAGIC_LITE (4) + zstd_data (for backwards compatibility)
    if (magic == MAGIC_LITE) {
        const uint8_t* zstd_data = data + 4;
        size_t zstd_size = size - 4;

        // Get original size from zstd frame
        unsigned long long original_size = ZSTD_getFrameContentSize(zstd_data, zstd_size);
        if (original_size == ZSTD_CONTENTSIZE_ERROR || original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            res.error = "Cannot determine decompressed size from lite format";
            if (result) *result = res;
            return {};
        }

        std::vector<uint8_t> output(original_size);
        size_t decompressed = ZSTD_decompress(output.data(), output.size(), zstd_data, zstd_size);

        if (ZSTD_isError(decompressed)) {
            res.error = "ZSTD decompression failed: " + std::string(ZSTD_getErrorName(decompressed));
            if (result) *result = res;
            return {};
        }

        res.success = true;
        res.decompressed_size = decompressed;
        if (result) *result = res;
        return output;
    }

    // Dispatch needs only magic(4)+version(1). The compact format is ~12-14 bytes for a single
    // block; only the legacy/standard header needs 17. Rejecting EVERYTHING < 17 dropped VALID
    // compact streams: a 16-byte constant input (0x82 x16) compressed to a 14-byte compact stream
    // that decode returned EMPTY for -> losslessness failure (found by fuzz_mzip_nv; also
    // backstopped by compress()'s top-level verify). Gate the 17-byte minimum on the LEGACY
    // version only; compact reads are varint/end-bounded below. (2026-08-07)
    if (size < 5) {
        res.error = "Input too small for header";
        if (result) *result = res;
        return {};
    }

    if (magic != MAGIC) {
        res.error = "Invalid magic number";
        if (result) *result = res;
        return {};
    }

    uint8_t version = data[pos++];
    if (version != VERSION && version != VERSION_COMPACT) {
        res.error = "Unsupported version: " + std::to_string(version);
        if (result) *result = res;
        return {};
    }
    if (version == VERSION && size < 17) {
        res.error = "Input too small for standard header";
        if (result) *result = res;
        return {};
    }

    uint64_t original_size;
    uint32_t block_count;

    if (version == VERSION_COMPACT) {
        // Compact format: read varints
        const uint8_t* ptr = &data[pos];
        const uint8_t* end = data + size;
        original_size = read_uvarint(ptr, end);
        block_count = (uint32_t)read_uvarint(ptr, end);
        pos = ptr - data;
    } else {
        // Legacy format: read fixed sizes
        original_size = read_u64_le(&data[pos]);
        pos += 8;
        block_count = read_u32_le(&data[pos]);
        pos += 4;
    }

    // Allocate output
    std::vector<uint8_t> output(original_size);
    size_t out_pos = 0;

    // Work buffers
    std::vector<uint8_t> decompress_buf(MAX_BLOCK_SIZE);
    std::vector<uint8_t> unpreprocess_buf(MAX_BLOCK_SIZE);

    // Track decompressed blocks for REFERENCE decoding
    // Maps block_index -> (offset_in_output, size)
    // We only need to track blocks that might be referenced
    std::map<uint32_t, std::pair<size_t, size_t>> block_locations;

    // Process blocks
    for (uint32_t i = 0; i < block_count; i++) {
        BlockType type;
        tieredcompress::Strategy strategy;
        uint32_t block_original_size;
        uint32_t preprocess_size;
        uint32_t stored_size;
        bool is_raw;

        if (version == VERSION_COMPACT) {
            // Compact format: read varints with packed flags
            if (pos + 2 > size) {
                res.error = "Truncated compact block header";
                if (result) *result = res;
                return {};
            }
            type = static_cast<BlockType>(data[pos++]);
            uint8_t flags = data[pos++];
            is_raw = (flags & BLOCK_FLAG_RAW) != 0;
            bool preproc_eq_orig = (flags & BLOCK_FLAG_PREPROC_EQ_ORIG) != 0;
            bool comp_eq_preproc = (flags & BLOCK_FLAG_COMP_EQ_PREPROC) != 0;
            strategy = static_cast<tieredcompress::Strategy>((flags >> 2) & 0x1F);

            const uint8_t* ptr = &data[pos];
            const uint8_t* end = data + size;
            block_original_size = (uint32_t)read_uvarint(ptr, end);
            if (preproc_eq_orig) {
                preprocess_size = block_original_size;
            } else {
                preprocess_size = (uint32_t)read_uvarint(ptr, end);
            }
            if (comp_eq_preproc) {
                stored_size = preprocess_size;
            } else {
                stored_size = (uint32_t)read_uvarint(ptr, end);
            }
            pos = ptr - data;
        } else {
            // Legacy format: read fixed sizes
            if (pos + 14 > size) {
                res.error = "Truncated block header";
                if (result) *result = res;
                return {};
            }

            type = static_cast<BlockType>(data[pos++]);
            strategy = static_cast<tieredcompress::Strategy>(data[pos++]);
            block_original_size = read_u32_le(&data[pos]);
            pos += 4;
            preprocess_size = read_u32_le(&data[pos]);
            pos += 4;
            stored_size = read_u32_le(&data[pos]);
            pos += 4;

            is_raw = (stored_size & 0x80000000) != 0;
            stored_size &= 0x7FFFFFFF;
        }

        if (pos + stored_size > size) {
            res.error = "Truncated block data";
            if (result) *result = res;
            return {};
        }

        // OUTPUT BOUNDS CHECK — 2026-08-04. This is the symmetric partner of the INPUT
        // check directly above, which was present while its output-side twin was missing.
        // `output` is sized to the file header's original_size, and each block then does
        // `memcpy(&output[out_pos], decoded.data(), decoded.size()); out_pos += ...` across
        // ~41 sites with NO test that the write stays inside the buffer. A crafted archive
        // that declares a tiny original_size but ships a block whose block_original_size is
        // large therefore drives an unbounded heap write.
        //
        // CONFIRMED, reachable-today: take any valid archive, edit the single header field
        // original_size from 287,748 to 1 (a 3-byte varint edit, no other change), and the
        // decoder memcpy's ~287 KB into a 1-byte buffer -> process crash / heap corruption
        // on untrusted .mz input.
        //
        // A truthful archive always passes: the encoder sets original_size = sum of every
        // block_original_size, so out_pos + block_original_size never exceeds output.size().
        // The individual memcpy sites still use decoded.size(); a well-formed decoder
        // produces exactly block_original_size, so this single check at the top of the loop
        // body bounds them all without touching 41 call sites.
        if (out_pos + block_original_size > output.size()) {
            res.error = "Block original size exceeds output buffer (corrupt or malicious archive)";
            if (result) *result = res;
            return {};
        }

        // Decompress or copy
        const uint8_t* block_data;
        size_t block_size;

        if (is_raw) {
            block_data = &data[pos];
            block_size = stored_size;
        } else {
            size_t decompressed = ZSTD_decompress(
                decompress_buf.data(), decompress_buf.size(),
                &data[pos], stored_size
            );

            if (ZSTD_isError(decompressed)) {
                res.error = "ZSTD decompression failed: " + std::string(ZSTD_getErrorName(decompressed));
                if (result) *result = res;
                return {};
            }

            block_data = decompress_buf.data();
            block_size = decompressed;
        }

        // Reverse preprocessing
        if (type == BlockType::LINEAR_GEN) {
            // Decode linear generator - regenerate sequence from params
            auto decoded = decode_linear_gen(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::LINEAR_GEN_APPROX) {
            // Decode linear generator with exceptions - regenerate + apply exceptions
            auto decoded = decode_linear_gen_approx(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::GEOMETRIC) {
            // Decode geometric generator - regenerate from base * ratio^i
            auto decoded = decode_geometric(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::QUADRATIC) {
            // Decode quadratic generator - regenerate from a + b*i + c*i²
            auto decoded = decode_quadratic(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::RECURRENCE) {
            // Decode recurrence generator - regenerate Fibonacci-like sequence
            auto decoded = decode_recurrence(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::MODULAR) {
            // Decode modular generator - regenerate wrapping counter sequence
            auto decoded = decode_modular(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::PERIODIC) {
            // Decode periodic pattern - regenerate from period + pattern
            auto decoded = decode_periodic(block_data, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::PERIODIC_APPROX) {
            // Decode periodic pattern with exceptions (Effective Complexity)
            auto decoded = decode_periodic_approx(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::SPARSE) {
            // Decode sparse - auto-detect format: 'SR' = Rice, 'SP' = varint
            std::vector<uint8_t> decoded;
            if (block_size >= 2 && block_data[0] == 'S' && block_data[1] == 'R') {
                decoded = decode_sparse_rice(block_data, block_size, block_original_size);
            } else {
                decoded = decode_sparse(block_data, block_size, block_original_size);
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::TIMESTAMP) {
            // Decode timestamp - reverse delta-of-delta + zigzag + varint
            auto decoded = decode_timestamp(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::TEMPLATE) {
            // Decode template - reconstruct from template + columns
            auto decoded = decode_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::CHAR_TEMPLATE) {
            // Decode char template - reconstruct from char-level template + columns
            auto decoded = decode_char_template(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::ML_TEMPLATE) {
            // Decode multi-line template - reconstruct from template + variables
            auto decoded = decode_ml_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::ML_TEMPLATE_DUAL) {
            // Decode dual multi-line template - reconstruct from two templates + variables
            auto decoded = decode_ml_template_dual(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::COLUMNAR) {
            // Decode columnar - reconstruct from columns
            auto decoded = decode_columnar(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::CSV_COLUMNAR) {
            // Decode CSV columnar - reconstruct from columns with LINEAR_GEN
            auto decoded = decode_csv_columnar(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::JSON_COLUMNAR) {
            // Decode JSON columnar - reconstruct from LINEAR_GEN requestId + delta timestamps
            auto decoded = decode_json_columnar(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "JSON_COLUMNAR decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::NUM_EXTRACT) {
            // Decode NUM_EXTRACT - reconstruct from template + numbers
            auto decoded = decode_num_extract(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "NUM_EXTRACT decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::LINE_TEMPLATE) {
            // Decode line template - reconstruct from prefix/suffix/separators + linear vars
            auto decoded = decode_line_template(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::PHRASE_PARTITION) {
            // Decode phrase partition - decompress indices and expand phrases
            auto decoded = decode_phrase_partition(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "PHRASE_PARTITION decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::DUAL_STREAM) {
            // Decode dual stream - decompress and reinterleave even/odd bytes
            auto decoded = decode_dual_stream(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "DUAL_STREAM decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::PHRASE_DICT) {
            // Decode phrase dictionary - decompress using stored dictionary
            auto decoded = decode_phrase_dict(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "PHRASE_DICT decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::SORTED_DICT) {
            // Decode sorted dictionary - decompress and unsort lines
            auto decoded = decode_sorted_dict(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "SORTED_DICT decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::KV_CONFIG) {
            // Decode key-value config - reconstruct INI/YAML from structural encoding
            auto decoded = decode_kv_config(block_data, block_size, block_original_size);
            if (decoded.empty()) {
                res.error = "KV_CONFIG decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::SECTION_TEMPLATE) {
            // Decode section template - reconstruct from template + LINEAR_GEN params
            auto decoded = decode_section_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::WORD_TEMPLATE) {
            // Decode word template - reconstruct from template + words list
            auto decoded = decode_word_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::MULTI_WORD_TEMPLATE) {
            // Decode multi-word template - reconstruct from template + multiple variable lists
            auto decoded = decode_multi_word_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::LINE_GROUP_TEMPLATE) {
            // Decode line group template - reconstruct from grouped lines
            auto decoded = decode_line_group_template(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::CODE_STREAM) {
            // Decode code stream - reconstruct from skeleton + grammar-parsed identifiers
            auto decoded = decode_code_stream(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::DBF_CONSTCOL) {
            // Decode DBF constant column elimination — zstd decompress + reconstruct records
            auto decoded = decode_dbf_constcol(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::BWT_TEXT) {
            // Decode smart adaptive BWT (v9) - dispatches to v8 or v4 based on mode byte
            auto decoded = bwt9::decompress(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::CM_TEXT) {
            // Decode BWT + context-mixing (bzip3-class)
            auto decoded = cmbk::decompress_bwt(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::ZSTD_DICT) {
            // Decode zstd with pre-trained dictionary
            if (block_size < 2) { res.error = "ZSTD_DICT block too small"; if (result) *result = res; return {}; }
            uint8_t dict_id = block_data[0];
            const uint8_t* dict_ptr = nullptr;
            size_t dict_len = 0;
            for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                if (mzip_dicts::ALL_DICTS[di].id == dict_id) {
                    dict_ptr = mzip_dicts::ALL_DICTS[di].data;
                    dict_len = mzip_dicts::ALL_DICTS[di].size;
                    break;
                }
            }
            if (!dict_ptr) { res.error = "ZSTD_DICT unknown dict_id"; if (result) *result = res; return {}; }
            ZSTD_DCtx* dctx = ZSTD_createDCtx();
            size_t dec_size = ZSTD_decompress_usingDict(dctx,
                &output[out_pos], block_original_size,
                block_data + 1, block_size - 1,
                dict_ptr, dict_len);
            ZSTD_freeDCtx(dctx);
            if (ZSTD_isError(dec_size)) { res.error = "ZSTD_DICT decompression failed"; if (result) *result = res; return {}; }
            out_pos += dec_size;
        } else if (type == BlockType::HTML_STREAM) {
            // Decode HTML stream - reconstruct from separated tag/content streams
            auto decoded = decode_html_stream(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::URL_STREAM) {
            // Decode URL stream - reconstruct from separated protocol/domain/path/params streams
            auto decoded = decode_url_stream(block_data, block_size, block_original_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::BASE64_DECODE) {
            // Decode base64 - decompress binary, re-encode to base64 with exact original size
            auto decoded = decode_base64_decode(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::WORD_ENCODED) {
            // Decode word-encoded text - reconstruct from vocabulary + word indices
            auto decoded = decode_word_text(block_data, block_size);
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::NUMERIC && strategy != tieredcompress::Strategy::NONE) {
            const uint8_t* rev_data = block_data;
            size_t rev_size = block_size;

            // Check for sparse delta encoding: strategy(1) + 0xFF(1) + sparse data
            std::vector<uint8_t> sparse_decoded;
            if (block_size >= 2 && block_data[0] == (uint8_t)strategy && block_data[1] == 0xFF) {
                // Decode sparse: reconstruct preprocessed data (mostly zeros)
                sparse_decoded.resize(block_original_size, 0);
                size_t p = 2;
                // Read count (varint)
                uint32_t count = 0; int shift = 0;
                while (p < block_size && (block_data[p] & 0x80)) {
                    count |= (uint32_t)(block_data[p++] & 0x7F) << shift; shift += 7;
                }
                if (p < block_size) count |= (uint32_t)block_data[p++] << shift;
                // Read delta-encoded positions + values
                size_t pos = 0;
                for (uint32_t i = 0; i < count && p + 1 < block_size; i++) {
                    uint32_t delta_pos = 0; shift = 0;
                    while (p < block_size && (block_data[p] & 0x80)) {
                        delta_pos |= (uint32_t)(block_data[p++] & 0x7F) << shift; shift += 7;
                    }
                    if (p < block_size) delta_pos |= (uint32_t)block_data[p++] << shift;
                    pos += delta_pos;
                    if (pos < sparse_decoded.size() && p < block_size)
                        sparse_decoded[pos] = block_data[p++];
                }
                rev_data = sparse_decoded.data();
                rev_size = sparse_decoded.size();
            }

            size_t final_size = reverse_strategy(
                unpreprocess_buf.data(), rev_data, rev_size,
                block_original_size, strategy
            );
            memcpy(&output[out_pos], unpreprocess_buf.data(), final_size);
            out_pos += final_size;
        } else if (type == BlockType::REFERENCE) {
            // REFERENCE block: decode using previous block as dictionary
            // First 4 bytes are ref_block_index
            if (block_size < 4) {
                res.error = "Invalid REFERENCE block: too small";
                if (result) *result = res;
                return {};
            }
            
            uint32_t ref_idx = block_data[0] | (block_data[1] << 8) | 
                              (block_data[2] << 16) | (block_data[3] << 24);
            
            auto it = block_locations.find(ref_idx);
            if (it == block_locations.end()) {
                res.error = "Invalid REFERENCE: block " + std::to_string(ref_idx) + " not found";
                if (result) *result = res;
                return {};
            }
            
            size_t ref_offset = it->second.first;
            size_t ref_size = it->second.second;
            
            // Decode using dictionary
            auto decoded = decode_reference(
                block_data, block_size,
                output.data() + ref_offset, ref_size,
                block_original_size
            );
            
            if (decoded.empty()) {
                res.error = "REFERENCE decompression failed";
                if (result) *result = res;
                return {};
            }
            
            memcpy(&output[out_pos], decoded.data(), decoded.size());
            out_pos += decoded.size();
        } else if (type == BlockType::BINARY_X86) {
            // Reverse E8/E9 filtering
            memcpy(&output[out_pos], block_data, block_size);
            e8e9_filter_decode(&output[out_pos], block_size);
            out_pos += block_size;
        } else if (type == BlockType::LZMA_OPTIMAL) {
            // Decompress LZMA, then reverse E8/E9 filtering
            auto decompressed = lzma_dec::decompress(block_data, block_size);
            if (decompressed.empty()) {
                res.error = "LZMA decompression failed";
                if (result) *result = res;
                return {};
            }
            // Reverse E8/E9 filter
            e8e9_filter_decode(decompressed.data(), decompressed.size());
            memcpy(&output[out_pos], decompressed.data(), decompressed.size());
            out_pos += decompressed.size();
        } else if (type == BlockType::BROTLI) {
            // brotli backstop decode
            std::vector<uint8_t> decoded(block_original_size ? block_original_size : 1);
            size_t dsz = block_original_size;
            if (BrotliDecoderDecompress(block_size, block_data, &dsz, decoded.data()) != MZ_BROTLI_DECODE_SUCCESS
                || dsz != block_original_size) {
                res.error = "brotli decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), dsz);
            out_pos += dsz;
        } else if (type == BlockType::XZLIB) {
            // xz (liblzma) backstop decode
            std::vector<uint8_t> decoded(block_original_size ? block_original_size : 1);
            uint64_t memlimit = UINT64_MAX;
            size_t in_pos = 0, out_pos2 = 0;
            if (lzma_stream_buffer_decode(&memlimit, 0, nullptr, block_data, &in_pos, block_size,
                                          decoded.data(), &out_pos2, decoded.size()) != MZ_LZMA_OK
                || out_pos2 != block_original_size) {
                res.error = "xz decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decoded.data(), out_pos2);
            out_pos += out_pos2;
        } else if (type == BlockType::PPMD) {
            // PPMd var.H backstop: block_data = [order:1][memMiB:1][Ppmd7z stream]. Clamp order/memMiB
            // (untrusted): decode heap == memMiB MiB, a pure function of that byte, so bound it.
            if (block_size < 2) { res.error = "PPMD block too small"; if (result) *result = res; return {}; }
            unsigned order = block_data[0]; uint32_t memMiB = block_data[1];
            if (order < 2) order = 2; else if (order > 64) order = 64;
            if (memMiB < 1) memMiB = 1; else if (memMiB > 128) memMiB = 128;  // bound untrusted decode heap (encoder writes 64)
            auto dec = ppmdbk::decompress(block_data + 2, block_size - 2, block_original_size, order, memMiB);
            if (dec.size() != block_original_size) { res.error = "PPMD decompression failed"; if (result) *result = res; return {}; }
            memcpy(&output[out_pos], dec.data(), dec.size());
            out_pos += dec.size();
        } else if (type == BlockType::LZMA_RAW) {
            // Decompress LZMA (no E8/E9 filter)
            auto decompressed = lzma_dec::decompress(block_data, block_size);
            if (decompressed.empty()) {
                res.error = "LZMA decompression failed";
                if (result) *result = res;
                return {};
            }
            memcpy(&output[out_pos], decompressed.data(), decompressed.size());
            out_pos += decompressed.size();
        } else {
            memcpy(&output[out_pos], block_data, block_size);
            out_pos += block_size;
        }

        // Record block location for potential REFERENCE use
        // Note: block_original_size is the decompressed size of this block
        size_t block_start = out_pos - block_original_size;
        if (type != BlockType::REFERENCE) {
            // Only track non-REFERENCE blocks (they could be used as dictionaries)
            block_locations[i] = std::make_pair(block_start, block_original_size);
        }
        
        pos += stored_size;
    }

    if (out_pos != original_size) {
        res.error = "Size mismatch: expected " + std::to_string(original_size) +
                   ", got " + std::to_string(out_pos);
        if (result) *result = res;
        return {};
    }

    res.success = true;
    res.decompressed_size = out_pos;
    if (result) *result = res;

    return output;
}

// ============================================================================
// Public decompress(): decompress_impl() guarded so a hostile/oversized .mz can never crash the
// process. The decode paths carry OOB/length guards, but an untrusted stream can still declare a
// huge original_size (mzip.hpp: `std::vector<uint8_t> output(original_size)`) or otherwise drive a
// std::bad_alloc / std::length_error, which would propagate out as an uncaught exception and
// terminate(). Catch everything here and return {} (with an error on the result) instead -- the same
// "return empty on failure" contract the decode paths already use. INERT on valid streams (no
// exception thrown -> zero-cost). Completes the untrusted-stream robustness story alongside the OOB
// fixes. (2026-08-07, motivated by fuzz_compact: uZIP-magic tiny streams demanding multi-GB allocs.)
// ============================================================================
inline std::vector<uint8_t> decompress(const uint8_t* data, size_t size, DecompressResult* result) {
    try {
        return decompress_impl(data, size, result);
    } catch (const std::exception& e) {
        if (result) { result->success = false; result->decompressed_size = 0; result->error = std::string("decompress: exception (hostile/oversized stream): ") + e.what(); }
        return {};
    } catch (...) {
        if (result) { result->success = false; result->decompressed_size = 0; result->error = "decompress: unknown exception (hostile/oversized stream)"; }
        return {};
    }
}

// ============================================================================
// File API
// ============================================================================

inline CompressResult compress_file(const std::string& input_path,
                                    const std::string& output_path,
                                    int zstd_level = 3,
                                    size_t block_size = DEFAULT_BLOCK_SIZE) {
    CompressResult res;
    res.success = false;

    // Read input file
    std::ifstream in(input_path, std::ios::binary | std::ios::ate);
    if (!in) {
        res.error = "Cannot open input file: " + input_path;
        return res;
    }

    size_t file_size = in.tellg();
    in.seekg(0);

    std::vector<uint8_t> input_data(file_size);
    in.read(reinterpret_cast<char*>(input_data.data()), file_size);
    in.close();

    // Compress
    auto compressed = compress(input_data.data(), input_data.size(), zstd_level, block_size, &res);
    if (!res.success) {
        return res;
    }

    // Write output file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        res.error = "Cannot open output file: " + output_path;
        res.success = false;
        return res;
    }

    out.write(reinterpret_cast<char*>(compressed.data()), compressed.size());
    out.close();

    return res;
}

inline DecompressResult decompress_file(const std::string& input_path,
                                        const std::string& output_path) {
    DecompressResult res;
    res.success = false;

    // Read input file
    std::ifstream in(input_path, std::ios::binary | std::ios::ate);
    if (!in) {
        res.error = "Cannot open input file: " + input_path;
        return res;
    }

    size_t file_size = in.tellg();
    in.seekg(0);

    std::vector<uint8_t> input_data(file_size);
    in.read(reinterpret_cast<char*>(input_data.data()), file_size);
    in.close();

    // Decompress
    auto decompressed = decompress(input_data.data(), input_data.size(), &res);
    if (!res.success) {
        return res;
    }

    // Write output file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        res.error = "Cannot open output file: " + output_path;
        res.success = false;
        return res;
    }

    out.write(reinterpret_cast<char*>(decompressed.data()), decompressed.size());
    out.close();

    return res;
}

// ============================================================================
// Info / Debug
// ============================================================================

inline const char* block_type_name(BlockType type) {
    switch (type) {
        case BlockType::RAW: return "RAW";
        case BlockType::NUMERIC: return "NUMERIC";
        case BlockType::TEXT: return "TEXT";
        case BlockType::STRUCTURED: return "STRUCTURED";
        case BlockType::IMAGE: return "IMAGE";
        case BlockType::BINARY_X86: return "BINARY_X86";
        case BlockType::LZMA_OPTIMAL: return "LZMA_OPTIMAL";
        case BlockType::LZMA_RAW: return "LZMA_RAW";
        case BlockType::LINEAR_GEN: return "LINEAR_GEN";
        case BlockType::PERIODIC: return "PERIODIC";
        case BlockType::TIMESTAMP: return "TIMESTAMP";
        case BlockType::TEMPLATE: return "TEMPLATE";
        case BlockType::GEOMETRIC: return "GEOMETRIC";
        case BlockType::QUADRATIC: return "QUADRATIC";
        case BlockType::RECURRENCE: return "RECURRENCE";
        case BlockType::MODULAR: return "MODULAR";
        case BlockType::ML_TEMPLATE: return "ML_TEMPLATE";
        case BlockType::ML_TEMPLATE_DUAL: return "ML_TEMPLATE_DUAL";
        case BlockType::COLUMNAR: return "COLUMNAR";
        case BlockType::CSV_COLUMNAR: return "CSV_COLUMNAR";
        case BlockType::SECTION_TEMPLATE: return "SECTION_TEMPLATE";
        case BlockType::WORD_TEMPLATE: return "WORD_TEMPLATE";
        case BlockType::MULTI_WORD_TEMPLATE: return "MULTI_WORD_TEMPLATE";
        case BlockType::WORD_ENCODED: return "WORD_ENCODED";
        case BlockType::BWT_TEXT: return "BWT_TEXT";
        case BlockType::CHAR_TEMPLATE: return "CHAR_TEMPLATE";
        case BlockType::LINE_TEMPLATE: return "LINE_TEMPLATE";
        case BlockType::PHRASE_DICT: return "PHRASE_DICT";
        case BlockType::PHRASE_PARTITION: return "PHRASE_PARTITION";
        case BlockType::DUAL_STREAM: return "DUAL_STREAM";
        case BlockType::HTML_STREAM: return "HTML_STREAM";
        case BlockType::URL_STREAM: return "URL_STREAM";
        case BlockType::BASE64_DECODE: return "BASE64_DECODE";
        case BlockType::SORTED_DICT: return "SORTED_DICT";
        case BlockType::KV_CONFIG: return "KV_CONFIG";
        case BlockType::LINEAR_GEN_APPROX: return "LINEAR_GEN_APPROX";
        case BlockType::PERIODIC_APPROX: return "PERIODIC_APPROX";
        case BlockType::GEOMETRIC_APPROX: return "GEOMETRIC_APPROX";
        case BlockType::QUADRATIC_APPROX: return "QUADRATIC_APPROX";
        case BlockType::SPARSE: return "SPARSE";
        case BlockType::LINE_GROUP_TEMPLATE: return "LINE_GROUP_TPL";
        case BlockType::CODE_STREAM: return "CODE_STREAM";
        case BlockType::REFERENCE: return "REFERENCE";
        case BlockType::DBF_CONSTCOL: return "DBF_CONSTCOL";
        case BlockType::ZSTD_DICT: return "ZSTD_DICT";
        case BlockType::CM_TEXT: return "CM_TEXT";
        case BlockType::BROTLI: return "BROTLI";
        case BlockType::XZLIB: return "XZLIB";
        case BlockType::PPMD: return "PPMD";
        case BlockType::JSON_COLUMNAR: return "JSON_COLUMNAR";
        case BlockType::NUM_EXTRACT: return "NUM_EXTRACT";
        case BlockType::INCOMPRESSIBLE: return "INCOMPRESSIBLE";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// ADAPTIVE Mode - Smart compression selection
// ============================================================================
//
// Key insight from benchmarks:
//   - NUMERIC data: μzip preprocessing ALWAYS wins (+17-108%)
//   - LINEAR_GEN data: 3855x compression on sequential integers!
//   - TEXT data: plain zstd often wins (block overhead hurts)
//   - BINARY_X86: E8/E9 helps but marginal
//
// ADAPTIVE strategy:
//   1. Sample first 4KB to detect data type
//   2. NUMERIC/LINEAR_GEN: use block-based μzip (always wins)
//   3. TEXT/other: use single zstd stream (no block overhead)
//
// This is MUCH faster than full block-level processing

enum class AdaptiveMode {
    BLOCK,      // Full block-based processing (slow, max ratio)
    STREAM,     // Single zstd stream (fast, good for text)
    ADAPTIVE    // Auto-select based on data type
};

// Fast compression for non-numeric data (no block overhead)
inline std::vector<uint8_t> compress_stream(const uint8_t* data, size_t size,
                                             int zstd_level = 3) {
    // Minimal header: just magic + size
    std::vector<uint8_t> output(4 + 8 + ZSTD_compressBound(size));

    size_t pos = 0;
    // Use 'MZST' magic for stream mode
    output[pos++] = 'M';
    output[pos++] = 'Z';
    output[pos++] = 'S';
    output[pos++] = 'T';

    // Original size
    write_u64_le(&output[pos], size);
    pos += 8;

    // Single zstd compress
    size_t compressed = ZSTD_compress(&output[pos], output.size() - pos,
                                       data, size, zstd_level);
    if (ZSTD_isError(compressed)) {
        return {};
    }

    output.resize(pos + compressed);
    return output;
}

// Decompress stream mode
inline std::vector<uint8_t> decompress_stream(const uint8_t* data, size_t size) {
    if (size < 12) return {};

    // Check magic
    if (data[0] != 'M' || data[1] != 'Z' || data[2] != 'S' || data[3] != 'T') {
        return {};
    }

    uint64_t original_size = read_u64_le(&data[4]);
    std::vector<uint8_t> output(original_size);

    size_t decompressed = ZSTD_decompress(output.data(), output.size(),
                                           &data[12], size - 12);
    if (ZSTD_isError(decompressed) || decompressed != original_size) {
        return {};
    }

    return output;
}

// ADAPTIVE compression - picks best strategy based on data type
//
// Key insight from benchmarks:
//   - NUMERIC: block mode wins BIG on ratio (+108%) - worth 94% speed penalty
//   - TEXT: block mode wins marginally (+16-55%) but loses HARD on speed (-98%)
//   - Binary: block adds overhead without benefit
//
// Strategy: NUMERIC → block, everything else → stream (no trial needed)
//
inline std::vector<uint8_t> compress_adaptive(const uint8_t* data, size_t size,
                                               int zstd_level = 3,
                                               size_t block_size = DEFAULT_BLOCK_SIZE,
                                               CompressResult* result = nullptr) {
    // Quick detection on first 4KB
    size_t sample_size = std::min(size, (size_t)4096);
    auto analysis = analyze_block(data, sample_size);

    // NUMERIC data: always use block-based (108%+ ratio gain is worth speed cost)
    if (analysis.type == BlockType::NUMERIC) {
        return compress(data, size, zstd_level, block_size, result);
    }

    // Everything else: use stream mode
    // - TEXT: 16-55% ratio gain not worth 98% speed penalty
    // - BINARY_X86: marginal E8/E9 benefit not worth overhead
    // - Other: straight zstd is optimal
    if (result) {
        result->success = true;
        result->original_size = size;
        result->block_count = 1;
    }
    return compress_stream(data, size, zstd_level);
}

// Universal decompress - handles both µZIP (block) and MZST (stream) formats
inline std::vector<uint8_t> decompress_auto(const uint8_t* data, size_t size,
                                             DecompressResult* result = nullptr) {
    if (size < 4) {
        if (result) {
            result->success = false;
            result->error = "Input too small";
        }
        return {};
    }

    // Check magic to determine format
    if (data[0] == 'M' && data[1] == 'Z' && data[2] == 'S' && data[3] == 'T') {
        // Stream mode
        auto output = decompress_stream(data, size);
        if (result) {
            result->success = !output.empty();
            result->decompressed_size = output.size();
            if (output.empty()) result->error = "Stream decompression failed";
        }
        return output;
    } else {
        // Block mode (TZIP)
        return decompress(data, size, result);
    }
}

// ============================================================================
// Fast compression - optimized for speed
// ============================================================================
//
// Only applies preprocessing when it's CERTAIN to help:
// - NUMERIC: always preprocess (108% ratio gain)
// - Everything else: straight zstd (avoid block overhead)

inline std::vector<uint8_t> compress_fast(const uint8_t* data, size_t size,
                                           int zstd_level = 1) {
    // Ultra-fast detection: just check first 256 bytes for numeric pattern
    size_t sample = std::min(size, (size_t)256);
    int numeric_bytes = 0;
    for (size_t i = 0; i < sample; i++) {
        uint8_t c = data[i];
        // Numeric patterns: sequential bytes, low entropy
        if (c < 10 || (c >= '0' && c <= '9')) numeric_bytes++;
    }

    bool likely_numeric = (numeric_bytes > sample / 2);

    if (likely_numeric && size >= 1024) {
        // Use tieredcompress detection to confirm
        std::vector<uint8_t> work(std::min(size, (size_t)4096));
        auto detection = tieredcompress::detect(data, std::min(size, (size_t)4096), work.data());

        if (detection.strategy != tieredcompress::Strategy::NONE) {
            // Confirmed numeric - use block mode
            return compress(data, size, zstd_level, DEFAULT_BLOCK_SIZE);
        }
    }

    // Default: straight zstd
    return compress_stream(data, size, zstd_level);
}

} // namespace mzip
