#pragma once
// tieredcompress: Detection-first compression preprocessing
// Detect data characteristics, apply optimal preprocessing before compression

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace tieredcompress {

// ============================================================================
// Portable Utilities
// ============================================================================

// ZigZag encoding: maps signed integers to unsigned
// -1 → 1, 1 → 2, -2 → 3, 2 → 4, etc.
// This clusters small magnitudes near zero regardless of sign
inline uint32_t zigzag_encode32(int32_t n) {
    return (static_cast<uint32_t>(n) << 1) ^ static_cast<uint32_t>(n >> 31);
}

inline int32_t zigzag_decode32(uint32_t n) {
    return static_cast<int32_t>((n >> 1) ^ -(n & 1));
}

inline uint64_t zigzag_encode64(int64_t n) {
    return (static_cast<uint64_t>(n) << 1) ^ static_cast<uint64_t>(n >> 63);
}

inline int64_t zigzag_decode64(uint64_t n) {
    return static_cast<int64_t>((n >> 1) ^ -(n & 1));
}

// Count bits needed to represent a value
inline int bits_needed(uint64_t max_val) {
    if (max_val == 0) return 1;
    int bits = 0;
    while (max_val > 0) {
        bits++;
        max_val >>= 1;
    }
    return bits;
}

// ============================================================================
// Bit-Packing: Pack N-bit values into byte stream
// ============================================================================

// Pack 32-bit values using exactly 'bits' bits per value
// Returns number of bytes written
// Format: [bits_per_value (1 byte)] [count (4 bytes)] [packed data...]
inline size_t bitpack32_encode(uint8_t* out, const uint32_t* values, size_t count, int bits) {
    if (count == 0 || bits == 0 || bits > 32) return 0;

    out[0] = static_cast<uint8_t>(bits);
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;

    size_t out_pos = 5;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 0; i < count; i++) {
        buffer |= (static_cast<uint64_t>(values[i]) << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    // Flush remaining bits
    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

// Unpack bit-packed 32-bit values
// Returns number of values unpacked
inline size_t bitpack32_decode(uint32_t* out, const uint8_t* in, size_t in_size) {
    if (in_size < 5) return 0;

    int bits = in[0];
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);

    if (bits == 0 || bits > 32) return 0;

    size_t in_pos = 5;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint32_t mask = (1ULL << bits) - 1;

    for (size_t i = 0; i < count; i++) {
        while (buf_bits < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits);
            buf_bits += 8;
        }
        out[i] = buffer & mask;
        buffer >>= bits;
        buf_bits -= bits;
    }

    return count;
}

// ============================================================================
// Delta + ZigZag + BitPack combined (TurboPFor-style)
// ============================================================================

// Delta + ZigZag encode 32-bit values, return max value for bit-packing decision
inline uint32_t delta_zigzag32_encode(uint32_t* out, const uint32_t* in, size_t count) {
    if (count == 0) return 0;

    out[0] = in[0];  // First value stored as-is
    uint32_t max_val = in[0];
    uint32_t prev = in[0];

    for (size_t i = 1; i < count; i++) {
        int32_t delta = static_cast<int32_t>(in[i] - prev);
        uint32_t zz = zigzag_encode32(delta);
        out[i] = zz;
        if (zz > max_val) max_val = zz;
        prev = in[i];
    }

    return max_val;
}

// Decode ZigZag + Delta for 32-bit values
inline void delta_zigzag32_decode(uint32_t* out, const uint32_t* in, size_t count) {
    if (count == 0) return;

    out[0] = in[0];
    uint32_t prev = in[0];

    for (size_t i = 1; i < count; i++) {
        int32_t delta = zigzag_decode32(in[i]);
        out[i] = prev + delta;
        prev = out[i];
    }
}

// Full pipeline: Delta + ZigZag + BitPack for 32-bit data
// Format: [header: 1 byte bits + 4 bytes count] [first value: 4 bytes] [packed deltas]
inline size_t delta_zigzag_bitpack32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 4;

    // Convert bytes to 32-bit values
    std::vector<uint32_t> values(count);
    for (size_t i = 0; i < count; i++) {
        values[i] = in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | (static_cast<uint32_t>(in[i*4+3]) << 24);
    }

    // Delta + ZigZag encode
    std::vector<uint32_t> encoded(count);
    uint32_t max_val = delta_zigzag32_encode(encoded.data(), values.data(), count);

    // Determine bits needed (excluding first value which is stored raw)
    uint32_t max_delta = 0;
    for (size_t i = 1; i < count; i++) {
        if (encoded[i] > max_delta) max_delta = encoded[i];
    }
    int bits = bits_needed(max_delta);

    // If bit-packing doesn't help, skip it
    size_t packed_size = 5 + 4 + ((count - 1) * bits + 7) / 8;
    if (packed_size >= n) {
        memcpy(out, in, n);
        return n;
    }

    // Write header
    out[0] = static_cast<uint8_t>(bits);
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;

    // Write first value raw
    out[5] = encoded[0] & 0xFF;
    out[6] = (encoded[0] >> 8) & 0xFF;
    out[7] = (encoded[0] >> 16) & 0xFF;
    out[8] = (encoded[0] >> 24) & 0xFF;

    // Bit-pack remaining values
    size_t out_pos = 9;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 1; i < count; i++) {
        buffer |= (static_cast<uint64_t>(encoded[i]) << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

// Delta + FOR (Frame-of-Reference) + BitPack for 32-bit data
// Better than ZigZag when deltas cluster around a non-zero value (like jittery timestamps)
// Format: [1 byte bits] [4 bytes count] [4 bytes reference] [4 bytes first_value] [packed residuals]
inline size_t delta_for_bitpack32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 4;

    // Convert bytes to 32-bit values
    std::vector<int32_t> values(count);
    for (size_t i = 0; i < count; i++) {
        values[i] = static_cast<int32_t>(in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | (static_cast<uint32_t>(in[i*4+3]) << 24));
    }

    // Compute deltas
    std::vector<int32_t> deltas(count);
    deltas[0] = values[0];
    int32_t min_delta = INT32_MAX;
    int32_t max_delta = INT32_MIN;
    for (size_t i = 1; i < count; i++) {
        deltas[i] = values[i] - values[i-1];
        if (deltas[i] < min_delta) min_delta = deltas[i];
        if (deltas[i] > max_delta) max_delta = deltas[i];
    }

    // Compute range and bits needed
    int64_t range = static_cast<int64_t>(max_delta) - static_cast<int64_t>(min_delta);
    if (range < 0 || range > UINT32_MAX) {
        memcpy(out, in, n);
        return n;
    }
    int bits = bits_needed(static_cast<uint64_t>(range));

    // Calculate packed size: header(1) + count(4) + reference(4) + first_value(4) + packed data
    size_t packed_size = 1 + 4 + 4 + 4 + ((count - 1) * bits + 7) / 8;
    if (packed_size >= n) {
        memcpy(out, in, n);
        return n;
    }

    // Write header
    out[0] = static_cast<uint8_t>(bits);
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;

    // Write reference (min_delta) as signed 32-bit
    uint32_t ref_unsigned = static_cast<uint32_t>(min_delta);
    out[5] = (ref_unsigned >> 0) & 0xFF;
    out[6] = (ref_unsigned >> 8) & 0xFF;
    out[7] = (ref_unsigned >> 16) & 0xFF;
    out[8] = (ref_unsigned >> 24) & 0xFF;

    // Write first value
    uint32_t first_unsigned = static_cast<uint32_t>(deltas[0]);
    out[9] = (first_unsigned >> 0) & 0xFF;
    out[10] = (first_unsigned >> 8) & 0xFF;
    out[11] = (first_unsigned >> 16) & 0xFF;
    out[12] = (first_unsigned >> 24) & 0xFF;

    // Bit-pack residuals (delta - reference)
    size_t out_pos = 13;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 1; i < count; i++) {
        uint32_t residual = static_cast<uint32_t>(deltas[i] - min_delta);
        buffer |= (static_cast<uint64_t>(residual) << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

inline size_t delta_for_bitpack32_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 13) {
        memcpy(out, in, std::min(in_size, expected_out_size));
        return std::min(in_size, expected_out_size);
    }

    int bits = in[0];
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);

    int32_t min_delta = static_cast<int32_t>(in[5] | (in[6] << 8) | (in[7] << 16) | (static_cast<uint32_t>(in[8]) << 24));
    int32_t first_value = static_cast<int32_t>(in[9] | (in[10] << 8) | (in[11] << 16) | (static_cast<uint32_t>(in[12]) << 24));

    std::vector<int32_t> deltas(count);
    deltas[0] = first_value;

    size_t in_pos = 13;
    uint64_t buffer = 0;
    int buf_bits_val = 0;
    uint32_t mask = (bits == 32) ? ~0U : ((1U << bits) - 1);

    for (size_t i = 1; i < count; i++) {
        while (buf_bits_val < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits_val);
            buf_bits_val += 8;
        }
        uint32_t residual = buffer & mask;
        buffer >>= bits;
        buf_bits_val -= bits;

        deltas[i] = static_cast<int32_t>(residual) + min_delta;
    }

    std::vector<int32_t> values(count);
    values[0] = deltas[0];
    for (size_t i = 1; i < count; i++) {
        values[i] = values[i-1] + deltas[i];
    }

    for (size_t i = 0; i < count && i * 4 < expected_out_size; i++) {
        uint32_t v = static_cast<uint32_t>(values[i]);
        out[i*4 + 0] = (v >> 0) & 0xFF;
        out[i*4 + 1] = (v >> 8) & 0xFF;
        out[i*4 + 2] = (v >> 16) & 0xFF;
        out[i*4 + 3] = (v >> 24) & 0xFF;
    }

    return count * 4;
}

// Decode: BitPack + ZigZag + Delta
inline size_t delta_zigzag_bitpack32_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 9) {
        memcpy(out, in, in_size);
        return in_size;
    }

    int bits = in[0];
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);

    if (bits == 0 || bits > 32 || count * 4 != expected_out_size) {
        memcpy(out, in, in_size);
        return in_size;
    }

    // Read first value
    uint32_t first = in[5] | (in[6] << 8) | (in[7] << 16) | (static_cast<uint32_t>(in[8]) << 24);

    // Unpack deltas
    std::vector<uint32_t> encoded(count);
    encoded[0] = first;

    size_t in_pos = 9;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint32_t mask = (1ULL << bits) - 1;

    for (size_t i = 1; i < count; i++) {
        while (buf_bits < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits);
            buf_bits += 8;
        }
        encoded[i] = buffer & mask;
        buffer >>= bits;
        buf_bits -= bits;
    }

    // Decode ZigZag + Delta
    std::vector<uint32_t> values(count);
    delta_zigzag32_decode(values.data(), encoded.data(), count);

    // Write output
    for (size_t i = 0; i < count; i++) {
        out[i*4] = values[i] & 0xFF;
        out[i*4+1] = (values[i] >> 8) & 0xFF;
        out[i*4+2] = (values[i] >> 16) & 0xFF;
        out[i*4+3] = (values[i] >> 24) & 0xFF;
    }

    return count * 4;
}

// ============================================================================
// ALP-style Decimal Float Encoding
// Key insight: Most real-world doubles are decimals like 20.01 = 2001 * 10^-2
// ============================================================================

// Powers of 10 lookup table
static const double POW10[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18
};

// Check if a double is a "decimal" that can be represented as int * 10^-exp
// Returns the exponent (0-18) or -1 if not decimal
inline int detect_decimal_exponent(double val, int64_t& factor) {
    if (!std::isfinite(val)) return -1;

    double abs_val = std::fabs(val);
    if (abs_val > 1e18) return -1;  // Too large

    // Try each exponent from 0 to 15
    for (int exp = 0; exp <= 15; exp++) {
        double scaled = val * POW10[exp];
        double rounded = std::round(scaled);

        // Check if rounded value is close enough to original
        if (std::fabs(scaled - rounded) < 1e-9 &&
            std::fabs(rounded) <= static_cast<double>(INT64_MAX)) {
            factor = static_cast<int64_t>(rounded);
            return exp;
        }
    }
    return -1;
}

// Check if a 64-bit pattern looks like a valid IEEE 754 double
// (not an integer being misinterpreted as a float)
inline bool looks_like_double(uint64_t bits) {
    // Extract exponent (bits 52-62)
    int exponent = (bits >> 52) & 0x7FF;

    // Exponent of 0 means denormalized (usually integer 0 or very small)
    // Exponent of 2047 means inf/nan
    // Most real-world doubles have exponent between 1000 and 1030 (values ~1e-4 to ~1e4)
    // Small integers (like 1000000) have exponent around 1072 (for values near 1e6)

    // Reject if exponent looks like a small integer (< 512 or > 1075)
    // Small integers like 1, 2, 1000000 have very specific bit patterns
    if (exponent < 512 || exponent > 1075) return false;

    return true;
}

// Detect if a block of doubles can be encoded as decimals
// Returns common exponent or -1 if not applicable
inline int is_decimal_float64_candidate(const uint8_t* data, size_t n, double threshold = 0.90) {
    if (n < 64 || (n % 8) != 0) return -1;

    size_t count = std::min(n / 8, (size_t)64);
    int exp_counts[16] = {0};
    int total_valid = 0;
    int looks_like_float = 0;

    for (size_t i = 0; i < count; i++) {
        uint64_t bits = 0;
        for (int b = 0; b < 8; b++) {
            bits |= (static_cast<uint64_t>(data[i*8 + b])) << (b*8);
        }

        // First check if this looks like a float at all
        if (!looks_like_double(bits)) continue;
        looks_like_float++;

        double val;
        memcpy(&val, &bits, sizeof(double));

        int64_t factor;
        int exp = detect_decimal_exponent(val, factor);
        if (exp >= 0 && exp < 16) {
            exp_counts[exp]++;
            total_valid++;
        }
    }

    // At least 80% must look like floats (not integers)
    if (looks_like_float < count * 0.80) return -1;

    if (total_valid < count * threshold) return -1;

    // Find most common exponent
    int best_exp = -1;
    int best_count = 0;
    for (int i = 0; i < 16; i++) {
        if (exp_counts[i] > best_count) {
            best_count = exp_counts[i];
            best_exp = i;
        }
    }

    // At least 80% should share the same exponent
    if (best_count < count * 0.80) return -1;

    return best_exp;
}

// ALP-style encode: doubles -> exponent + delta-encoded integers
// Format: [exponent: 1 byte] [count: 4 bytes] [first factor: 8 bytes] [packed delta factors]
inline size_t alp_float64_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 8;

    // First pass: determine exponent and check all values
    int exp = is_decimal_float64_candidate(in, n);
    if (exp < 0) {
        memcpy(out, in, n);
        return n;
    }

    // Convert all doubles to integer factors
    std::vector<int64_t> factors(count);
    double multiplier = POW10[exp];

    for (size_t i = 0; i < count; i++) {
        uint64_t bits = 0;
        for (int b = 0; b < 8; b++) {
            bits |= (static_cast<uint64_t>(in[i*8 + b])) << (b*8);
        }
        double val;
        memcpy(&val, &bits, sizeof(double));

        factors[i] = static_cast<int64_t>(std::round(val * multiplier));
    }

    // Delta encode the factors
    std::vector<int64_t> deltas(count);
    deltas[0] = factors[0];
    int64_t max_delta = 0;
    for (size_t i = 1; i < count; i++) {
        deltas[i] = factors[i] - factors[i-1];
        int64_t abs_delta = deltas[i] < 0 ? -deltas[i] : deltas[i];
        if (abs_delta > max_delta) max_delta = abs_delta;
    }

    // ZigZag encode deltas
    std::vector<uint64_t> zz_deltas(count);
    zz_deltas[0] = static_cast<uint64_t>(factors[0]);  // First stored raw
    uint64_t max_zz = 0;
    for (size_t i = 1; i < count; i++) {
        zz_deltas[i] = zigzag_encode64(deltas[i]);
        if (zz_deltas[i] > max_zz) max_zz = zz_deltas[i];
    }

    int bits = bits_needed(max_zz);

    // Calculate output size
    size_t packed_size = 1 + 4 + 8 + ((count - 1) * bits + 7) / 8;  // exp + count + first + packed
    if (packed_size >= n) {
        memcpy(out, in, n);
        return n;
    }

    // Write header
    out[0] = static_cast<uint8_t>(exp | (bits << 4));  // exp in low 4 bits, bits info in high 4
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;

    // Write bits needed for deltas
    out[5] = static_cast<uint8_t>(bits);

    // Write first factor raw (8 bytes)
    int64_t first = factors[0];
    for (int b = 0; b < 8; b++) {
        out[6 + b] = (first >> (b*8)) & 0xFF;
    }

    // Bit-pack remaining deltas
    size_t out_pos = 14;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 1; i < count; i++) {
        buffer |= (zz_deltas[i] << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

// ALP-style decode
inline size_t alp_float64_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 14) {
        memcpy(out, in, in_size);
        return in_size;
    }

    int exp = in[0] & 0x0F;
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);
    int bits = in[5];

    if (exp > 15 || bits == 0 || bits > 64 || count * 8 != expected_out_size) {
        memcpy(out, in, in_size);
        return in_size;
    }

    // Read first factor
    int64_t first = 0;
    for (int b = 0; b < 8; b++) {
        first |= (static_cast<int64_t>(in[6 + b])) << (b*8);
    }

    // Unpack deltas
    std::vector<int64_t> factors(count);
    factors[0] = first;

    size_t in_pos = 14;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);

    for (size_t i = 1; i < count; i++) {
        while (buf_bits < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits);
            buf_bits += 8;
        }
        uint64_t zz = buffer & mask;
        buffer >>= bits;
        buf_bits -= bits;

        int64_t delta = zigzag_decode64(zz);
        factors[i] = factors[i-1] + delta;
    }

    // Convert factors back to doubles
    double divisor = POW10[exp];
    for (size_t i = 0; i < count; i++) {
        double val = static_cast<double>(factors[i]) / divisor;
        uint64_t bits_val;
        memcpy(&bits_val, &val, sizeof(double));

        for (int b = 0; b < 8; b++) {
            out[i*8 + b] = (bits_val >> (b*8)) & 0xFF;
        }
    }

    return count * 8;
}

// ============================================================================
// ALP-style Decimal Float32 Encoding (for FCBench compatibility)
// Key insight: Most float32 in time-series are decimals like 20.01f = 2001 * 10^-2
// ============================================================================

// Powers of 10 for float32 (smaller range than float64)
static const float POW10F[] = {
    1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f
};

// Check if a float32 pattern looks like a valid IEEE 754 float (not misinterpreted integer)
inline bool looks_like_float32(uint32_t bits) {
    // Extract exponent (bits 23-30)
    int exponent = (bits >> 23) & 0xFF;

    // Exponent of 0 means denormalized (usually integer 0 or very small)
    // Exponent of 255 means inf/nan
    // Most real-world floats have exponent between 100 and 150 (values ~1e-8 to ~1e10)
    // Common temperature range (-50 to 50C): exponent around 124-132
    // Common sensor values: exponent around 110-145

    if (exponent < 100 || exponent > 160) return false;

    return true;
}

// Check if a float is a "decimal" that can be represented as int * 10^-exp
// Returns the exponent (0-9) or -1 if not decimal
inline int detect_decimal_exponent_f32(float val, int32_t& factor) {
    if (!std::isfinite(val)) return -1;

    float abs_val = std::fabs(val);
    if (abs_val > 1e9f) return -1;  // Too large for int32

    // Try each exponent from 0 to 7 (float32 has less precision than float64)
    for (int exp = 0; exp <= 7; exp++) {
        float scaled = val * POW10F[exp];
        float rounded = std::round(scaled);

        // Check if rounded value is close enough to original
        // Float32 has ~7 digits of precision, so use looser tolerance
        if (std::fabs(scaled - rounded) < 1e-4f &&
            std::fabs(rounded) <= static_cast<float>(INT32_MAX)) {
            int32_t f = static_cast<int32_t>(rounded);
            // CRITICAL: Verify exact bit-level reconstruction
            // ALP is only lossless if factor/10^exp reconstructs EXACT original bits
            float reconstructed = static_cast<float>(f) / POW10F[exp];
            uint32_t orig_bits, rec_bits;
            memcpy(&orig_bits, &val, sizeof(float));
            memcpy(&rec_bits, &reconstructed, sizeof(float));
            if (orig_bits == rec_bits) {
                factor = f;
                return exp;
            }
        }
    }
    return -1;
}

// Detect if a block of float32s can be encoded as decimals
// Returns common exponent or -1 if not applicable
inline int is_decimal_float32_candidate(const uint8_t* data, size_t n, double threshold = 0.85) {
    if (n < 32 || (n % 4) != 0) return -1;

    size_t count = std::min(n / 4, (size_t)128);
    int exp_counts[10] = {0};
    int total_valid = 0;
    int looks_like_float = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t bits = data[i*4] | (data[i*4+1] << 8) |
                        (data[i*4+2] << 16) | (static_cast<uint32_t>(data[i*4+3]) << 24);

        // First check if this looks like a float at all
        if (!looks_like_float32(bits)) continue;
        looks_like_float++;

        float val;
        memcpy(&val, &bits, sizeof(float));

        int32_t factor;
        int exp = detect_decimal_exponent_f32(val, factor);
        if (exp >= 0 && exp < 10) {
            exp_counts[exp]++;
            total_valid++;
        }
    }

    // At least 70% must look like floats (not integers)
    if (looks_like_float < count * 0.70) return -1;

    if (total_valid < count * threshold) return -1;

    // Find most common exponent
    int best_exp = -1;
    int best_count = 0;
    for (int i = 0; i < 10; i++) {
        if (exp_counts[i] > best_count) {
            best_count = exp_counts[i];
            best_exp = i;
        }
    }

    // At least 70% should share the same exponent
    if (best_count < count * 0.70) return -1;

    return best_exp;
}

// ALP-style encode for float32: floats -> exponent + delta-encoded integers
// Format: [exponent: 1 byte] [count: 4 bytes] [bits: 1 byte] [first factor: 4 bytes] [packed delta factors]
inline size_t alp_float32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 4;

    // First pass: determine exponent and check all values
    int exp = is_decimal_float32_candidate(in, n);
    if (exp < 0) {
        memcpy(out, in, n);
        return n;
    }

    // Convert all floats to integer factors, verifying exact reconstruction
    std::vector<int32_t> factors(count);
    float multiplier = POW10F[exp];
    float divisor = POW10F[exp];

    for (size_t i = 0; i < count; i++) {
        uint32_t bits = in[i*4] | (in[i*4+1] << 8) |
                        (in[i*4+2] << 16) | (static_cast<uint32_t>(in[i*4+3]) << 24);
        float val;
        memcpy(&val, &bits, sizeof(float));

        int32_t f = static_cast<int32_t>(std::round(val * multiplier));

        // CRITICAL: Verify exact bit-level reconstruction for EVERY float
        float reconstructed = static_cast<float>(f) / divisor;
        uint32_t rec_bits;
        memcpy(&rec_bits, &reconstructed, sizeof(float));
        if (bits != rec_bits) {
            // This float cannot be exactly reconstructed - fall back to raw copy
            memcpy(out, in, n);
            return n;
        }

        factors[i] = f;
    }

    // Delta encode the factors
    std::vector<int32_t> deltas(count);
    deltas[0] = factors[0];
    int32_t max_delta = 0;
    for (size_t i = 1; i < count; i++) {
        deltas[i] = factors[i] - factors[i-1];
        int32_t abs_delta = deltas[i] < 0 ? -deltas[i] : deltas[i];
        if (abs_delta > max_delta) max_delta = abs_delta;
    }

    // ZigZag encode deltas
    std::vector<uint32_t> zz_deltas(count);
    zz_deltas[0] = static_cast<uint32_t>(factors[0]);  // First stored raw (as unsigned)
    uint32_t max_zz = 0;
    for (size_t i = 1; i < count; i++) {
        zz_deltas[i] = zigzag_encode32(deltas[i]);
        if (zz_deltas[i] > max_zz) max_zz = zz_deltas[i];
    }

    int bits = bits_needed(max_zz);

    // Calculate output size: exp(1) + count(4) + bits(1) + first(4) + packed
    size_t packed_size = 1 + 4 + 1 + 4 + ((count - 1) * bits + 7) / 8;
    if (packed_size >= n) {
        memcpy(out, in, n);
        return n;
    }

    // Write header
    out[0] = static_cast<uint8_t>(exp);
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;
    out[5] = static_cast<uint8_t>(bits);

    // Write first factor raw (4 bytes, as signed)
    int32_t first = factors[0];
    out[6] = (first >> 0) & 0xFF;
    out[7] = (first >> 8) & 0xFF;
    out[8] = (first >> 16) & 0xFF;
    out[9] = (first >> 24) & 0xFF;

    // Bit-pack remaining deltas
    size_t out_pos = 10;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 1; i < count; i++) {
        buffer |= (static_cast<uint64_t>(zz_deltas[i]) << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

// ALP-style decode for float32
inline size_t alp_float32_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 10) {
        memcpy(out, in, in_size);
        return in_size;
    }

    int exp = in[0];
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);
    int bits = in[5];

    if (exp > 9 || bits == 0 || bits > 32 || count * 4 != expected_out_size) {
        memcpy(out, in, in_size);
        return in_size;
    }

    // Read first factor (signed 32-bit)
    int32_t first = static_cast<int32_t>(in[6] | (in[7] << 8) | (in[8] << 16) | (static_cast<uint32_t>(in[9]) << 24));

    // Unpack deltas
    std::vector<int32_t> factors(count);
    factors[0] = first;

    size_t in_pos = 10;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint32_t mask = (bits == 32) ? ~0U : ((1U << bits) - 1);

    for (size_t i = 1; i < count; i++) {
        while (buf_bits < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits);
            buf_bits += 8;
        }
        uint32_t zz = buffer & mask;
        buffer >>= bits;
        buf_bits -= bits;

        int32_t delta = zigzag_decode32(zz);
        factors[i] = factors[i-1] + delta;
    }

    // Convert factors back to floats
    float divisor = POW10F[exp];
    for (size_t i = 0; i < count; i++) {
        float val = static_cast<float>(factors[i]) / divisor;
        uint32_t bits_val;
        memcpy(&bits_val, &val, sizeof(float));

        out[i*4 + 0] = (bits_val >> 0) & 0xFF;
        out[i*4 + 1] = (bits_val >> 8) & 0xFF;
        out[i*4 + 2] = (bits_val >> 16) & 0xFF;
        out[i*4 + 3] = (bits_val >> 24) & 0xFF;
    }

    return count * 4;
}

// XOR encode for 32-bit floating point (Gorilla-style)
inline void xor_float32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;

    // First value unchanged
    for (int b = 0; b < 4; b++) out[b] = in[b];

    uint32_t prev = in[0] | (in[1] << 8) | (in[2] << 16) | (static_cast<uint32_t>(in[3]) << 24);

    for (size_t i = 1; i < count; i++) {
        uint32_t curr = in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | (static_cast<uint32_t>(in[i*4+3]) << 24);
        uint32_t xored = curr ^ prev;

        out[i*4 + 0] = (xored >> 0) & 0xFF;
        out[i*4 + 1] = (xored >> 8) & 0xFF;
        out[i*4 + 2] = (xored >> 16) & 0xFF;
        out[i*4 + 3] = (xored >> 24) & 0xFF;
        prev = curr;
    }
}

// XOR decode for 32-bit floating point
inline void xor_float32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;

    // First value unchanged
    for (int b = 0; b < 4; b++) out[b] = in[b];

    uint32_t prev = in[0] | (in[1] << 8) | (in[2] << 16) | (static_cast<uint32_t>(in[3]) << 24);

    for (size_t i = 1; i < count; i++) {
        uint32_t xored = in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | (static_cast<uint32_t>(in[i*4+3]) << 24);
        uint32_t curr = xored ^ prev;

        out[i*4 + 0] = (curr >> 0) & 0xFF;
        out[i*4 + 1] = (curr >> 8) & 0xFF;
        out[i*4 + 2] = (curr >> 16) & 0xFF;
        out[i*4 + 3] = (curr >> 24) & 0xFF;
        prev = curr;
    }
}

// Check for XOR-encodable 32-bit floating point data
inline bool is_xor_float32_candidate(const uint8_t* data, size_t n, double threshold = 0.60) {
    if (n < 64 || (n % 4) != 0) return false;

    size_t count = std::min(n / 4 - 1, (size_t)128);
    size_t high_leading_zeros = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t v0 = data[i*4] | (data[i*4+1] << 8) | (data[i*4+2] << 16) | (static_cast<uint32_t>(data[i*4+3]) << 24);
        uint32_t v1 = data[i*4+4] | (data[i*4+5] << 8) | (data[i*4+6] << 16) | (static_cast<uint32_t>(data[i*4+7]) << 24);

        // First check both look like floats
        if (!looks_like_float32(v0) || !looks_like_float32(v1)) continue;

        uint32_t xored = v0 ^ v1;

        // Count leading zeros (similar values have many)
        int lz = 0;
        if (xored == 0) {
            lz = 32;
        } else {
#if defined(__GNUC__) || defined(__clang__)
            lz = __builtin_clz(xored);
#elif defined(_MSC_VER)
            unsigned long idx;
            if (_BitScanReverse(&idx, xored)) lz = 31 - idx;
#else
            uint32_t x = xored;
            if ((x & 0xFFFF0000) == 0) { lz += 16; x <<= 16; }
            if ((x & 0xFF000000) == 0) { lz += 8;  x <<= 8; }
            if ((x & 0xF0000000) == 0) { lz += 4;  x <<= 4; }
            if ((x & 0xC0000000) == 0) { lz += 2;  x <<= 2; }
            if ((x & 0x80000000) == 0) { lz += 1; }
#endif
        }
        if (lz >= 8) {  // At least 8 leading zero bits
            high_leading_zeros++;
        }
    }

    return (double)high_leading_zeros / count >= threshold;
}

// Portable count leading zeros for 64-bit
inline int clz64(uint64_t x) {
    if (x == 0) return 64;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(x);
#elif defined(_MSC_VER)
    unsigned long idx;
    if (_BitScanReverse64(&idx, x)) return 63 - idx;
    return 64;
#else
    // Portable fallback
    int n = 0;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000ULL) == 0) { n += 8;  x <<= 8; }
    if ((x & 0xF000000000000000ULL) == 0) { n += 4;  x <<= 4; }
    if ((x & 0xC000000000000000ULL) == 0) { n += 2;  x <<= 2; }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
#endif
}

// ============================================================================
// Detection Features
// ============================================================================

// Calculate byte entropy (bits per byte) from a sample
// Lower entropy = more compressible
inline double byte_entropy(const uint8_t* data, size_t n) {
    if (n == 0) return 0.0;

    size_t counts[256] = {0};
    for (size_t i = 0; i < n; i++) {
        counts[data[i]]++;
    }

    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / n;
            entropy -= p * log2(p);
        }
    }
    return entropy;  // 0-8 bits per byte
}

// Check if data is delta-encodable (sequential with small differences)
// Sample head region, check what fraction has small deltas
inline bool is_delta_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 64) return false;

    size_t sample_size = std::min(n - 1, (size_t)256);
    size_t small_deltas = 0;

    for (size_t i = 0; i < sample_size; i++) {
        int delta = (int)data[i+1] - (int)data[i];
        // Small delta: -16 to +16, or wrapping (255->0, 0->255)
        if (delta >= -16 && delta <= 16) {
            small_deltas++;
        }
    }

    return (double)small_deltas / sample_size >= threshold;
}

// Check for 16-bit word-level delta pattern (little-endian)
inline bool is_delta16_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 128 || (n % 2) != 0) return false;

    size_t count = std::min(n / 2 - 1, (size_t)128);
    size_t small_deltas = 0;

    for (size_t i = 0; i < count; i++) {
        int16_t val0 = (int16_t)(data[i*2] | (data[i*2 + 1] << 8));
        int16_t val1 = (int16_t)(data[i*2 + 2] | (data[i*2 + 3] << 8));
        int delta = val1 - val0;
        if (delta >= -1000 && delta <= 1000) {
            small_deltas++;
        }
    }

    return (double)small_deltas / count >= threshold;
}

// Check for 32-bit word-level delta pattern (little-endian)
// Handles timestamps, IDs, AND IEEE float32 (which have larger bit-level deltas)
// IEEE float32: consecutive similar values like 20.0, 20.1 have deltas ~100k-500k
inline bool is_delta32_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 256 || (n % 4) != 0) return false;

    size_t count = std::min(n / 4 - 1, (size_t)64);
    size_t small_deltas = 0;
    size_t medium_deltas = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t val0 = data[i*4] | (data[i*4+1] << 8) | (data[i*4+2] << 16) | ((uint32_t)data[i*4+3] << 24);
        uint32_t val1 = data[i*4+4] | (data[i*4+5] << 8) | (data[i*4+6] << 16) | ((uint32_t)data[i*4+7] << 24);
        int64_t delta = (int64_t)val1 - (int64_t)val0;
        // Timestamps differ by <10000ms typically, IDs by <1000
        if (delta >= -100000 && delta <= 100000) {
            small_deltas++;
        }
        // IEEE float32: similar values have larger deltas (100k-500k)
        if (delta >= -500000 && delta <= 500000) {
            medium_deltas++;
        }
    }

    // Either most deltas are small, OR most deltas are medium (float-like)
    return (double)small_deltas / count >= threshold ||
           (double)medium_deltas / count >= 0.90;  // Stricter for medium range
}

// Check for 64-bit word-level delta pattern (little-endian)
inline bool is_delta64_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 512 || (n % 8) != 0) return false;

    size_t count = std::min(n / 8 - 1, (size_t)64);
    size_t small_deltas = 0;

    for (size_t i = 0; i < count; i++) {
        uint64_t val0 = 0, val1 = 0;
        for (int b = 0; b < 8; b++) {
            val0 |= ((uint64_t)data[i*8 + b]) << (b*8);
            val1 |= ((uint64_t)data[i*8 + 8 + b]) << (b*8);
        }
        int64_t delta = (int64_t)(val1 - val0);
        // Database IDs typically increment by 1-100
        if (delta >= -10000 && delta <= 10000) {
            small_deltas++;
        }
    }

    return (double)small_deltas / count >= threshold;
}
// Check for slow-moving float64 sequence (temperature sensors, GPS altitude, etc.)
// These have similar consecutive values when interpreted as doubles, but wildly different
// bit patterns when interpreted as int64. Delta encoding on bits still compresses well.
inline bool is_slow_float64_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 512 || (n % 8) != 0) return false;

    size_t count = std::min(n / 8 - 1, (size_t)64);
    size_t slow_moving = 0;

    for (size_t i = 0; i < count; i++) {
        // Read as little-endian double
        uint64_t bits0 = 0, bits1 = 0;
        for (int b = 0; b < 8; b++) {
            bits0 |= ((uint64_t)data[i*8 + b]) << (b*8);
            bits1 |= ((uint64_t)data[i*8 + 8 + b]) << (b*8);
        }

        // Interpret as doubles
        double v0, v1;
        memcpy(&v0, &bits0, 8);
        memcpy(&v1, &bits1, 8);

        // Check for valid floats (not NaN, not Inf)
        if (v0 != v0 || v1 != v1) continue;  // NaN check
        if (v0 == 1.0/0.0 || v0 == -1.0/0.0) continue;
        if (v1 == 1.0/0.0 || v1 == -1.0/0.0) continue;

        // Check if values are close (within 1% of magnitude or within +/-1.0)
        double diff = (v1 > v0) ? (v1 - v0) : (v0 - v1);
        double magnitude = (v0 > 0 ? v0 : -v0);
        if (magnitude < 1.0) magnitude = 1.0;

        if (diff < magnitude * 0.01 || diff < 1.0) {
            slow_moving++;
        }
    }

    return (double)slow_moving / count >= threshold;
}

// Check for delta-of-delta pattern (32-bit, for timestamps with regular intervals)
// Delta-of-delta: values[i] - 2*values[i-1] + values[i-2]
// If timestamps are evenly spaced, delta-of-delta ≈ 0
inline bool is_delta_of_delta32_candidate(const uint8_t* data, size_t n, double threshold = 0.75) {
    if (n < 256 || (n % 4) != 0) return false;

    size_t count = std::min(n / 4 - 2, (size_t)64);
    size_t near_zero = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t v0 = data[i*4] | (data[i*4+1] << 8) | (data[i*4+2] << 16) | ((uint32_t)data[i*4+3] << 24);
        uint32_t v1 = data[i*4+4] | (data[i*4+5] << 8) | (data[i*4+6] << 16) | ((uint32_t)data[i*4+7] << 24);
        uint32_t v2 = data[i*4+8] | (data[i*4+9] << 8) | (data[i*4+10] << 16) | ((uint32_t)data[i*4+11] << 24);

        // Delta-of-delta = (v2 - v1) - (v1 - v0) = v2 - 2*v1 + v0
        int64_t d1 = (int64_t)v1 - (int64_t)v0;
        int64_t d2 = (int64_t)v2 - (int64_t)v1;
        int64_t dd = d2 - d1;

        // Near-zero delta-of-delta indicates regular intervals
        if (dd >= -1000 && dd <= 1000) {
            near_zero++;
        }
    }

    return (double)near_zero / count >= threshold;
}

// Check for XOR-encodable 64-bit floating point data (Gorilla-style)
// Consecutive floats with similar values have many common bits
// XOR produces values with many leading zeros
inline bool is_xor_float64_candidate(const uint8_t* data, size_t n, double threshold = 0.60) {
    if (n < 128 || (n % 8) != 0) return false;

    size_t count = std::min(n / 8 - 1, (size_t)64);
    size_t high_leading_zeros = 0;

    for (size_t i = 0; i < count; i++) {
        uint64_t v0 = 0, v1 = 0;
        for (int b = 0; b < 8; b++) {
            v0 |= ((uint64_t)data[i*8 + b]) << (b*8);
            v1 |= ((uint64_t)data[i*8 + 8 + b]) << (b*8);
        }
        uint64_t xored = v0 ^ v1;

        // Count leading zeros (similar values have many)
        int lz = clz64(xored);
        if (lz >= 16) {  // At least 16 leading zero bits
            high_leading_zeros++;
        }
    }

    return (double)high_leading_zeros / count >= threshold;
}

// Check if data has high run lengths (repeated consecutive bytes)
inline double avg_run_length(const uint8_t* data, size_t n) {
    if (n < 2) return 1.0;

    size_t sample_size = std::min(n, (size_t)1024);
    size_t runs = 1;

    for (size_t i = 1; i < sample_size; i++) {
        if (data[i] != data[i-1]) runs++;
    }

    return (double)sample_size / runs;
}

// Check for template-like structure (repeating line prefixes)
// Returns estimated template coverage (0.0 - 1.0)
inline double template_coverage(const uint8_t* data, size_t n) {
    if (n < 100) return 0.0;

    // Find newlines, extract first N line prefixes
    const size_t PREFIX_LEN = 16;
    const size_t MAX_LINES = 32;

    uint8_t prefixes[MAX_LINES][PREFIX_LEN];
    size_t prefix_counts[MAX_LINES] = {0};
    size_t num_prefixes = 0;
    size_t total_lines = 0;

    size_t line_start = 0;
    for (size_t i = 0; i < n && num_prefixes < MAX_LINES; i++) {
        if (data[i] == '\n' || i == n - 1) {
            size_t line_len = i - line_start;
            if (line_len >= PREFIX_LEN) {
                total_lines++;

                // Check if this prefix matches any existing
                bool found = false;
                for (size_t p = 0; p < num_prefixes; p++) {
                    if (memcmp(&data[line_start], prefixes[p], PREFIX_LEN) == 0) {
                        prefix_counts[p]++;
                        found = true;
                        break;
                    }
                }

                if (!found && num_prefixes < MAX_LINES) {
                    memcpy(prefixes[num_prefixes], &data[line_start], PREFIX_LEN);
                    prefix_counts[num_prefixes] = 1;
                    num_prefixes++;
                }
            }
            line_start = i + 1;
        }
    }

    if (total_lines < 4) return 0.0;

    // Find most common prefix
    size_t max_count = 0;
    for (size_t p = 0; p < num_prefixes; p++) {
        max_count = std::max(max_count, prefix_counts[p]);
    }

    return (double)max_count / total_lines;
}

// ============================================================================
// Preprocessing Strategies
// ============================================================================

enum class Strategy {
    NONE,           // No preprocessing, raw data
    DELTA,          // Delta encoding for sequential byte data
    DELTA16,        // Delta encoding for 16-bit word data (sensors, audio)
    DELTA32,        // Delta encoding for 32-bit word data (timestamps, prices)
    DELTA64,        // Delta encoding for 64-bit word data (database IDs)
    DELTA_OF_DELTA32, // Delta-of-delta for regular interval timestamps
    XOR_FLOAT64,    // XOR encoding for 64-bit floats (Gorilla-style)
    XOR_FLOAT32,    // XOR encoding for 32-bit floats (Gorilla-style)
    DELTA_ZIGZAG_BITPACK32, // Delta + ZigZag + BitPack for 32-bit (TurboPFor-style)
    DELTA_FOR_BITPACK32,    // Delta + FOR + BitPack for 32-bit (better for jittery timestamps)
    ALP_FLOAT64,    // ALP-style decimal float encoding (doubles)
    ALP_FLOAT32,    // ALP-style decimal float encoding (floats) - for FCBench
    FOR16,          // Frame-of-Reference for 16-bit values (sensor data around mean)
    STRIDED32,      // Deinterleave + delta32 for interleaved 32-bit data (2-channel)
    STRIDED32_FOR,  // STRIDED32 + FOR bit-packing for tight delta ranges (GPS int32)
    STRIDED64,      // Deinterleave + delta64 for interleaved 64-bit data (GPS lat/lon doubles)
    STRIDED3_32,    // Deinterleave + delta32 for 3-channel 32-bit data (XYZ vertex, accelerometer, RGB)
    STRIDED4_32,    // Deinterleave + delta32 for 4-channel 32-bit data (RGBA, quaternions)
    BYTE_SHUFFLE32, // Blosc2-style byte shuffle for 32-bit data (floats, ints)
    BYTE_SHUFFLE64, // Blosc2-style byte shuffle for 64-bit data (doubles)
    BYTE_SHUFFLE2,  // 2-byte stride shuffle for alternating patterns (protobuf tag/value pairs)
    E8E9_X86,       // x86 CALL/JMP relative-to-absolute conversion (+15-25% on executables)
    LINEAR_PRED16,  // Linear prediction for 16-bit smooth signals (audio ramps)
    LINEAR_PRED32,  // Linear prediction for 32-bit smooth signals
    PAETH_RGB,      // PNG Paeth predictor for RGB image data (3 bytes/pixel, known row width)
    RLE,            // Run-length encoding for repetitive data
    BLOCK_COLUMNAR, // Fixed-width record columnar encoding (+52-78% on logs/records/HTML)
    NARROW16,       // 16-bit values with range ≤255: pack to 8-bit (sensor data around mean)
    // Future: TEMPLATE, BWT, etc.
    //
    // ⚠ 5-BIT CEILING (2026-08-04). mzip's compact single-block header packs Strategy into a
    // 5-bit field: `((strategy & 0x1F) << 2)` on encode and `(flags >> 2) & 0x1F` on decode
    // (mzip.hpp). Values 0..31 fit; a 33rd strategy (index 32) would SILENTLY wrap to NONE on
    // that path — no error, wrong-but-right-sized output. Keep new strategies before the
    // sentinel; the static_assert below is the tripwire.
    _STRATEGY_COUNT
};
// If this fires, the compact header (mzip.hpp, `& 0x1F`) can no longer distinguish all
// strategies. Widen that field (a format change: both encode and decode) before adding more.
static_assert(static_cast<unsigned>(Strategy::_STRATEGY_COUNT) <= 32,
              "Strategy no longer fits mzip's 5-bit compact-header field");

// Delta encode: out[i] = in[i] - in[i-1]
inline void delta_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n == 0) return;
    out[0] = in[0];
    for (size_t i = 1; i < n; i++) {
        out[i] = in[i] - in[i-1];  // Wrapping subtraction
    }
}

// Delta decode: out[i] = out[i-1] + in[i]
inline void delta_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n == 0) return;
    out[0] = in[0];
    for (size_t i = 1; i < n; i++) {
        out[i] = out[i-1] + in[i];  // Wrapping addition
    }
}

// 16-bit delta encode (little-endian)
inline void delta16_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 2 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 2;
    // First value unchanged
    out[0] = in[0];
    out[1] = in[1];

    int16_t prev = (int16_t)(in[0] | (in[1] << 8));
    for (size_t i = 1; i < count; i++) {
        int16_t curr = (int16_t)(in[i*2] | (in[i*2 + 1] << 8));
        int16_t delta = curr - prev;
        out[i*2] = delta & 0xFF;
        out[i*2 + 1] = (delta >> 8) & 0xFF;
        prev = curr;
    }
}

// 16-bit delta decode (little-endian)
inline void delta16_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 2 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 2;
    out[0] = in[0];
    out[1] = in[1];

    int16_t prev = (int16_t)(in[0] | (in[1] << 8));
    for (size_t i = 1; i < count; i++) {
        int16_t delta = (int16_t)(in[i*2] | (in[i*2 + 1] << 8));
        int16_t curr = prev + delta;
        out[i*2] = curr & 0xFF;
        out[i*2 + 1] = (curr >> 8) & 0xFF;
        prev = curr;
    }
}

// 32-bit delta encode (little-endian)
inline void delta32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;
    // First value unchanged
    for (int b = 0; b < 4; b++) out[b] = in[b];

    uint32_t prev = in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
    for (size_t i = 1; i < count; i++) {
        uint32_t curr = in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | ((uint32_t)in[i*4+3] << 24);
        int32_t delta = (int32_t)(curr - prev);
        out[i*4] = delta & 0xFF;
        out[i*4 + 1] = (delta >> 8) & 0xFF;
        out[i*4 + 2] = (delta >> 16) & 0xFF;
        out[i*4 + 3] = (delta >> 24) & 0xFF;
        prev = curr;
    }
}

// 32-bit delta decode (little-endian)
inline void delta32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;
    for (int b = 0; b < 4; b++) out[b] = in[b];

    uint32_t prev = in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
    for (size_t i = 1; i < count; i++) {
        int32_t delta = (int32_t)(in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | ((uint32_t)in[i*4+3] << 24));
        uint32_t curr = prev + delta;
        out[i*4] = curr & 0xFF;
        out[i*4 + 1] = (curr >> 8) & 0xFF;
        out[i*4 + 2] = (curr >> 16) & 0xFF;
        out[i*4 + 3] = (curr >> 24) & 0xFF;
        prev = curr;
    }
}

// 64-bit delta encode (little-endian)
inline void delta64_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 8;
    for (int b = 0; b < 8; b++) out[b] = in[b];

    uint64_t prev = 0;
    for (int b = 0; b < 8; b++) prev |= ((uint64_t)in[b]) << (b*8);

    for (size_t i = 1; i < count; i++) {
        uint64_t curr = 0;
        for (int b = 0; b < 8; b++) curr |= ((uint64_t)in[i*8 + b]) << (b*8);
        int64_t delta = (int64_t)(curr - prev);
        for (int b = 0; b < 8; b++) out[i*8 + b] = (delta >> (b*8)) & 0xFF;
        prev = curr;
    }
}

// 64-bit delta decode (little-endian)
inline void delta64_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 8;
    for (int b = 0; b < 8; b++) out[b] = in[b];

    uint64_t prev = 0;
    for (int b = 0; b < 8; b++) prev |= ((uint64_t)in[b]) << (b*8);

    for (size_t i = 1; i < count; i++) {
        int64_t delta = 0;
        for (int b = 0; b < 8; b++) delta |= ((int64_t)(uint8_t)in[i*8 + b]) << (b*8);
        // Sign-extend if needed (for negative deltas)
        if (delta & 0x8000000000000000ULL) {
            // Already correct due to int64_t storage
        }
        uint64_t curr = prev + delta;
        for (int b = 0; b < 8; b++) out[i*8 + b] = (curr >> (b*8)) & 0xFF;
        prev = curr;
    }
}

// Delta-of-delta 32-bit encode (for timestamps with regular intervals)
// Stores: first value, first delta, then delta-of-deltas
// This produces near-zero values for evenly-spaced timestamps
inline void delta_of_delta32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 12 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;

    // Copy first two values as-is (we need them for decode)
    for (int b = 0; b < 8; b++) out[b] = in[b];

    // Read first two values
    uint32_t v0 = in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
    uint32_t v1 = in[4] | (in[5] << 8) | (in[6] << 16) | ((uint32_t)in[7] << 24);
    int32_t prev_delta = (int32_t)(v1 - v0);
    uint32_t prev = v1;

    // Encode delta-of-deltas starting from third value
    for (size_t i = 2; i < count; i++) {
        uint32_t curr = in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | ((uint32_t)in[i*4+3] << 24);
        int32_t curr_delta = (int32_t)(curr - prev);
        int32_t dd = curr_delta - prev_delta;  // delta-of-delta

        // Store delta-of-delta (typically very small, often zero)
        out[i*4] = dd & 0xFF;
        out[i*4 + 1] = (dd >> 8) & 0xFF;
        out[i*4 + 2] = (dd >> 16) & 0xFF;
        out[i*4 + 3] = (dd >> 24) & 0xFF;

        prev_delta = curr_delta;
        prev = curr;
    }
}

// Delta-of-delta 32-bit decode
inline void delta_of_delta32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 12 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 4;

    // Copy first two values
    for (int b = 0; b < 8; b++) out[b] = in[b];

    // Read first two values
    uint32_t v0 = in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
    uint32_t v1 = in[4] | (in[5] << 8) | (in[6] << 16) | ((uint32_t)in[7] << 24);
    int32_t prev_delta = (int32_t)(v1 - v0);
    uint32_t prev = v1;

    // Decode from delta-of-deltas
    for (size_t i = 2; i < count; i++) {
        int32_t dd = (int32_t)(in[i*4] | (in[i*4+1] << 8) | (in[i*4+2] << 16) | ((uint32_t)in[i*4+3] << 24));
        int32_t curr_delta = prev_delta + dd;
        uint32_t curr = prev + curr_delta;

        out[i*4] = curr & 0xFF;
        out[i*4 + 1] = (curr >> 8) & 0xFF;
        out[i*4 + 2] = (curr >> 16) & 0xFF;
        out[i*4 + 3] = (curr >> 24) & 0xFF;

        prev_delta = curr_delta;
        prev = curr;
    }
}

// XOR encode for 64-bit floating point (Gorilla-style)
// Stores: first value as-is, then XOR with previous value
// Similar floats produce values with many leading zeros
inline void xor_float64_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 8;

    // First value unchanged
    for (int b = 0; b < 8; b++) out[b] = in[b];

    uint64_t prev = 0;
    for (int b = 0; b < 8; b++) prev |= ((uint64_t)in[b]) << (b*8);

    for (size_t i = 1; i < count; i++) {
        uint64_t curr = 0;
        for (int b = 0; b < 8; b++) curr |= ((uint64_t)in[i*8 + b]) << (b*8);

        uint64_t xored = curr ^ prev;

        for (int b = 0; b < 8; b++) out[i*8 + b] = (xored >> (b*8)) & 0xFF;
        prev = curr;
    }
}

// XOR decode for 64-bit floating point
inline void xor_float64_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }
    size_t count = n / 8;

    // First value unchanged
    for (int b = 0; b < 8; b++) out[b] = in[b];

    uint64_t prev = 0;
    for (int b = 0; b < 8; b++) prev |= ((uint64_t)in[b]) << (b*8);

    for (size_t i = 1; i < count; i++) {
        uint64_t xored = 0;
        for (int b = 0; b < 8; b++) xored |= ((uint64_t)in[i*8 + b]) << (b*8);

        uint64_t curr = xored ^ prev;

        for (int b = 0; b < 8; b++) out[i*8 + b] = (curr >> (b*8)) & 0xFF;
        prev = curr;
    }
}

// ============================================================================
// FOR16: Frame-of-Reference for 16-bit values
// For sensor data that clusters around a mean (e.g., 2000±50)
// Format: [1 byte bits] [4 bytes count] [2 bytes min_value] [packed residuals]
// ============================================================================

// Detect if 16-bit data would benefit from FOR (clustered around mean)
// Returns true if values have limited range (for bit-packing)
inline bool is_for16_candidate(const uint8_t* data, size_t n) {
    if (n < 128 || (n % 2) != 0) return false;

    size_t count = std::min(n / 2, (size_t)256);

    // Find min/max
    uint16_t min_val = UINT16_MAX;
    uint16_t max_val = 0;

    for (size_t i = 0; i < count; i++) {
        uint16_t val = data[i*2] | (data[i*2 + 1] << 8);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    // Calculate range and bits needed
    uint32_t range = max_val - min_val;
    int bits = bits_needed(range);

    // FOR is beneficial if we can pack into fewer than 14 bits
    // (16 bits original - 2 bits overhead for header amortized)
    // More realistically: beneficial if bits <= 12 (saves 25%+)
    return bits <= 12;
}

// FOR16 encode: subtract minimum, bit-pack residuals
inline size_t for16_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 2) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 2;

    // Find min/max
    uint16_t min_val = UINT16_MAX;
    uint16_t max_val = 0;

    for (size_t i = 0; i < count; i++) {
        uint16_t val = in[i*2] | (in[i*2 + 1] << 8);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    uint32_t range = max_val - min_val;
    int bits = bits_needed(range);

    // Calculate packed size: header(1) + count(4) + min_value(2) + packed data
    size_t packed_size = 1 + 4 + 2 + (count * bits + 7) / 8;
    if (packed_size >= n) {
        memcpy(out, in, n);
        return n;
    }

    // Write header
    out[0] = static_cast<uint8_t>(bits);
    out[1] = (count >> 0) & 0xFF;
    out[2] = (count >> 8) & 0xFF;
    out[3] = (count >> 16) & 0xFF;
    out[4] = (count >> 24) & 0xFF;

    // Write min_value (2 bytes)
    out[5] = min_val & 0xFF;
    out[6] = (min_val >> 8) & 0xFF;

    // Bit-pack residuals
    size_t out_pos = 7;
    uint64_t buffer = 0;
    int buf_bits = 0;

    for (size_t i = 0; i < count; i++) {
        uint16_t val = in[i*2] | (in[i*2 + 1] << 8);
        uint16_t residual = val - min_val;

        buffer |= (static_cast<uint64_t>(residual) << buf_bits);
        buf_bits += bits;

        while (buf_bits >= 8) {
            out[out_pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }

    if (buf_bits > 0) {
        out[out_pos++] = buffer & 0xFF;
    }

    return out_pos;
}

// FOR16 decode
inline size_t for16_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 7) {
        memcpy(out, in, std::min(in_size, expected_out_size));
        return std::min(in_size, expected_out_size);
    }

    int bits = in[0];
    size_t count = in[1] | (in[2] << 8) | (in[3] << 16) | (static_cast<size_t>(in[4]) << 24);
    uint16_t min_val = in[5] | (in[6] << 8);

    if (bits == 0 || bits > 16 || count * 2 != expected_out_size) {
        memcpy(out, in, std::min(in_size, expected_out_size));
        return std::min(in_size, expected_out_size);
    }

    size_t in_pos = 7;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint32_t mask = (bits == 16) ? 0xFFFF : ((1U << bits) - 1);

    for (size_t i = 0; i < count; i++) {
        while (buf_bits < bits && in_pos < in_size) {
            buffer |= (static_cast<uint64_t>(in[in_pos++]) << buf_bits);
            buf_bits += 8;
        }
        uint16_t residual = buffer & mask;
        buffer >>= bits;
        buf_bits -= bits;

        uint16_t val = min_val + residual;
        out[i*2] = val & 0xFF;
        out[i*2 + 1] = (val >> 8) & 0xFF;
    }

    return count * 2;
}


// ============================================================================
// NARROW16: 16-bit values with range ≤255 packed to 8-bit
// For sensor data clustered around a mean (e.g., ADC readings 2000±50)
// Format: [2 bytes min_value] [n/2 bytes: 8-bit residuals]
// Better than FOR16 because byte-aligned data compresses better with zstd
// ============================================================================

// Detect if 16-bit data would benefit from NARROW16 (range fits in 8 bits)
inline bool is_narrow16_candidate(const uint8_t* data, size_t n) {
    if (n < 128 || (n % 2) != 0) return false;

    size_t count = n / 2;
    size_t sample_count = std::min(count, (size_t)512);

    // Find min/max in sample
    uint16_t min_val = UINT16_MAX;
    uint16_t max_val = 0;

    for (size_t i = 0; i < sample_count; i++) {
        uint16_t val = data[i*2] | (data[i*2 + 1] << 8);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    // NARROW16 is beneficial if range fits in 8 bits (saves 50% raw size)
    return (max_val - min_val) <= 255;
}

// NARROW16 encode: subtract minimum, store as 8-bit
// Returns encoded size (2 + n/2 bytes) or n if range doesn't fit
inline size_t narrow16_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 2) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t count = n / 2;

    // Find min/max
    uint16_t min_val = UINT16_MAX;
    uint16_t max_val = 0;

    for (size_t i = 0; i < count; i++) {
        uint16_t val = in[i*2] | (in[i*2 + 1] << 8);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    // Check if range fits in 8 bits
    if (max_val - min_val > 255) {
        memcpy(out, in, n);
        return n;
    }

    // Write header: min_value (2 bytes)
    out[0] = min_val & 0xFF;
    out[1] = (min_val >> 8) & 0xFF;

    // Write 8-bit residuals
    for (size_t i = 0; i < count; i++) {
        uint16_t val = in[i*2] | (in[i*2 + 1] << 8);
        out[2 + i] = (uint8_t)(val - min_val);
    }

    return 2 + count;
}

// NARROW16 decode: add minimum back to 8-bit residuals
inline size_t narrow16_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t expected_out_size) {
    if (in_size < 3 || expected_out_size < 2) {
        memcpy(out, in, std::min(in_size, expected_out_size));
        return std::min(in_size, expected_out_size);
    }

    uint16_t min_val = in[0] | (in[1] << 8);
    size_t count = in_size - 2;

    if (count * 2 != expected_out_size) {
        memcpy(out, in, std::min(in_size, expected_out_size));
        return std::min(in_size, expected_out_size);
    }

    for (size_t i = 0; i < count; i++) {
        uint16_t val = min_val + in[2 + i];
        out[i*2] = val & 0xFF;
        out[i*2 + 1] = (val >> 8) & 0xFF;
    }

    return count * 2;
}
// ============================================================================
// STRIDED32: Deinterleave + Delta32 for interleaved data (GPS lat/lon)
// Input: [A0, B0, A1, B1, ...] where A and B are 32-bit sequences
// Output: delta([A0, A1, ...]) || delta([B0, B1, ...])
// ============================================================================

// Detect if data is 2-channel interleaved 32-bit with sequential pattern in each channel
inline bool is_strided32_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 8) != 0) return false;  // Need pairs of 32-bit values

    size_t pairs = n / 8;
    size_t sample_count = std::min(pairs - 1, (size_t)64);
    size_t channel_a_sequential = 0;
    size_t channel_b_sequential = 0;

    for (size_t i = 0; i < sample_count; i++) {
        // Channel A: words at positions 0, 2, 4, ... (bytes 0-3, 8-11, 16-19, ...)
        uint32_t a0 = data[i*8] | (data[i*8+1] << 8) | (data[i*8+2] << 16) | ((uint32_t)data[i*8+3] << 24);
        uint32_t a1 = data[i*8+8] | (data[i*8+9] << 8) | (data[i*8+10] << 16) | ((uint32_t)data[i*8+11] << 24);
        int64_t delta_a = (int64_t)a1 - (int64_t)a0;
        if (delta_a >= -10000 && delta_a <= 10000) channel_a_sequential++;

        // Channel B: words at positions 1, 3, 5, ... (bytes 4-7, 12-15, 20-23, ...)
        uint32_t b0 = data[i*8+4] | (data[i*8+5] << 8) | (data[i*8+6] << 16) | ((uint32_t)data[i*8+7] << 24);
        uint32_t b1 = data[i*8+12] | (data[i*8+13] << 8) | (data[i*8+14] << 16) | ((uint32_t)data[i*8+15] << 24);
        int64_t delta_b = (int64_t)b1 - (int64_t)b0;
        if (delta_b >= -10000 && delta_b <= 10000) channel_b_sequential++;
    }

    // Both channels must be sequential
    return (double)channel_a_sequential / sample_count >= 0.60 &&
           (double)channel_b_sequential / sample_count >= 0.60;
}

// Strided32 encode: deinterleave, delta encode each half
inline void strided32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t pairs = n / 8;

    // Deinterleave to two halves
    std::vector<uint8_t> channel_a(pairs * 4);
    std::vector<uint8_t> channel_b(pairs * 4);

    for (size_t i = 0; i < pairs; i++) {
        for (int b = 0; b < 4; b++) {
            channel_a[i*4 + b] = in[i*8 + b];      // First 32-bit value
            channel_b[i*4 + b] = in[i*8 + 4 + b];  // Second 32-bit value
        }
    }

    // Delta encode each channel
    std::vector<uint8_t> delta_a(pairs * 4);
    std::vector<uint8_t> delta_b(pairs * 4);
    delta32_encode(delta_a.data(), channel_a.data(), pairs * 4);
    delta32_encode(delta_b.data(), channel_b.data(), pairs * 4);

    // Output: channel A deltas followed by channel B deltas
    memcpy(out, delta_a.data(), pairs * 4);
    memcpy(out + pairs * 4, delta_b.data(), pairs * 4);
}

// Strided32 decode: delta decode each half, reinterleave
inline void strided32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t pairs = n / 8;

    // Delta decode each half
    std::vector<uint8_t> channel_a(pairs * 4);
    std::vector<uint8_t> channel_b(pairs * 4);
    delta32_decode(channel_a.data(), in, pairs * 4);
    delta32_decode(channel_b.data(), in + pairs * 4, pairs * 4);

    // Reinterleave
    for (size_t i = 0; i < pairs; i++) {
        for (int b = 0; b < 4; b++) {
            out[i*8 + b] = channel_a[i*4 + b];
            out[i*8 + 4 + b] = channel_b[i*4 + b];
        }
    }
}

// ============================================================================
// STRIDED32_FOR: Deinterleave + Delta32 + FOR bit-packing for GPS int32 data
// When delta values have small range, bit-pack to save space before zstd
// Format: [flag:1][base_a:4][base_b:4][min_a:4][min_b:4][bits_a:1][bits_b:1]
//         [packed_deltas_a][packed_deltas_b]
// ============================================================================

// Detect if STRIDED32 data would benefit from FOR bit-packing
// Returns true if delta range is small enough that bit-packing beats raw 32-bit
inline bool is_strided32_for_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 8) != 0) return false;

    // First check if it's a STRIDED32 candidate
    if (!is_strided32_candidate(data, n)) return false;

    size_t pairs = n / 8;
    size_t sample_count = std::min(pairs - 1, (size_t)64);

    // Sample delta ranges for both channels
    int32_t a_min = INT32_MAX, a_max = INT32_MIN;
    int32_t b_min = INT32_MAX, b_max = INT32_MIN;

    int32_t prev_a = 0, prev_b = 0;
    for (size_t i = 0; i <= sample_count; i++) {
        int32_t a = data[i*8] | (data[i*8+1] << 8) | (data[i*8+2] << 16) | ((int32_t)data[i*8+3] << 24);
        int32_t b = data[i*8+4] | (data[i*8+5] << 8) | (data[i*8+6] << 16) | ((int32_t)data[i*8+7] << 24);

        if (i > 0) {
            int32_t delta_a = a - prev_a;
            int32_t delta_b = b - prev_b;
            if (delta_a < a_min) a_min = delta_a;
            if (delta_a > a_max) a_max = delta_a;
            if (delta_b < b_min) b_min = delta_b;
            if (delta_b > b_max) b_max = delta_b;
        }
        prev_a = a;
        prev_b = b;
    }

    // Calculate bits needed for each channel
    int64_t a_range = (int64_t)a_max - a_min + 1;
    int64_t b_range = (int64_t)b_max - b_min + 1;
    int a_bits = 0; while ((1LL << a_bits) < a_range) a_bits++;
    int b_bits = 0; while ((1LL << b_bits) < b_range) b_bits++;

    // Worth it if bit-packing saves significant space
    // Header: 19 bytes + packed data
    // vs current: 8 bytes per pair * pairs = n bytes
    size_t for_size = 19 + ((pairs - 1) * a_bits + 7) / 8 + ((pairs - 1) * b_bits + 7) / 8;
    return for_size < n * 0.75;  // Must save at least 25%
}

// STRIDED32_FOR encode: deinterleave, delta, FOR bit-pack
// Returns encoded size (may be smaller than n)
inline size_t strided32_for_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return n;
    }

    size_t pairs = n / 8;

    // Deinterleave and delta encode
    std::vector<int32_t> delta_a(pairs);
    std::vector<int32_t> delta_b(pairs);

    int32_t prev_a = 0, prev_b = 0;
    for (size_t i = 0; i < pairs; i++) {
        int32_t a = in[i*8] | (in[i*8+1] << 8) | (in[i*8+2] << 16) | ((int32_t)in[i*8+3] << 24);
        int32_t b = in[i*8+4] | (in[i*8+5] << 8) | (in[i*8+6] << 16) | ((int32_t)in[i*8+7] << 24);

        if (i == 0) {
            delta_a[0] = a;  // First value stored as-is
            delta_b[0] = b;
        } else {
            delta_a[i] = a - prev_a;
            delta_b[i] = b - prev_b;
        }
        prev_a = a;
        prev_b = b;
    }

    // Find min/max of deltas (excluding first values)
    int32_t a_min = delta_a[1], a_max = delta_a[1];
    int32_t b_min = delta_b[1], b_max = delta_b[1];
    for (size_t i = 2; i < pairs; i++) {
        if (delta_a[i] < a_min) a_min = delta_a[i];
        if (delta_a[i] > a_max) a_max = delta_a[i];
        if (delta_b[i] < b_min) b_min = delta_b[i];
        if (delta_b[i] > b_max) b_max = delta_b[i];
    }

    // Calculate bits needed
    int64_t a_range = (int64_t)a_max - a_min + 1;
    int64_t b_range = (int64_t)b_max - b_min + 1;
    int a_bits = 0; while ((1LL << a_bits) < a_range) a_bits++;
    int b_bits = 0; while ((1LL << b_bits) < b_range) b_bits++;

    // Check if FOR encoding is beneficial
    size_t for_size = 19 + ((pairs - 1) * a_bits + 7) / 8 + ((pairs - 1) * b_bits + 7) / 8;
    if (for_size >= n) {
        // Fall back to regular STRIDED32
        strided32_encode(out, in, n);
        return n;
    }

    // Write header
    size_t pos = 0;
    out[pos++] = 0x46;  // 'F' for FOR encoding marker

    // Write bases (first delta values = original first values)
    memcpy(out + pos, &delta_a[0], 4); pos += 4;
    memcpy(out + pos, &delta_b[0], 4); pos += 4;

    // Write min values
    memcpy(out + pos, &a_min, 4); pos += 4;
    memcpy(out + pos, &b_min, 4); pos += 4;

    // Write bits per channel
    out[pos++] = (uint8_t)a_bits;
    out[pos++] = (uint8_t)b_bits;

    // Bit-pack channel A deltas
    uint64_t buffer = 0;
    int buf_bits = 0;
    for (size_t i = 1; i < pairs; i++) {
        uint32_t val = (uint32_t)(delta_a[i] - a_min);
        buffer |= ((uint64_t)val << buf_bits);
        buf_bits += a_bits;
        while (buf_bits >= 8) {
            out[pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }
    if (buf_bits > 0) { out[pos++] = buffer & 0xFF; buffer = 0; buf_bits = 0; }

    // Bit-pack channel B deltas
    for (size_t i = 1; i < pairs; i++) {
        uint32_t val = (uint32_t)(delta_b[i] - b_min);
        buffer |= ((uint64_t)val << buf_bits);
        buf_bits += b_bits;
        while (buf_bits >= 8) {
            out[pos++] = buffer & 0xFF;
            buffer >>= 8;
            buf_bits -= 8;
        }
    }
    if (buf_bits > 0) { out[pos++] = buffer & 0xFF; }

    return pos;
}

// STRIDED32_FOR decode: unpack, delta decode, reinterleave
inline void strided32_for_decode(uint8_t* out, const uint8_t* in, size_t in_size, size_t original_n) {
    if (in_size < 19 || in[0] != 0x46) {
        // Not FOR encoded, use regular STRIDED32 decode
        strided32_decode(out, in, original_n);
        return;
    }

    size_t pairs = original_n / 8;
    size_t pos = 1;

    // Read bases
    int32_t base_a = in[pos] | (in[pos+1] << 8) | (in[pos+2] << 16) | ((int32_t)in[pos+3] << 24); pos += 4;
    int32_t base_b = in[pos] | (in[pos+1] << 8) | (in[pos+2] << 16) | ((int32_t)in[pos+3] << 24); pos += 4;

    // Read mins
    int32_t a_min = in[pos] | (in[pos+1] << 8) | (in[pos+2] << 16) | ((int32_t)in[pos+3] << 24); pos += 4;
    int32_t b_min = in[pos] | (in[pos+1] << 8) | (in[pos+2] << 16) | ((int32_t)in[pos+3] << 24); pos += 4;

    // Read bits per channel
    int a_bits = in[pos++];
    int b_bits = in[pos++];

    // Unpack channel A deltas
    std::vector<int32_t> delta_a(pairs);
    delta_a[0] = base_a;
    uint64_t buffer = 0;
    int buf_bits = 0;
    uint32_t a_mask = (1ULL << a_bits) - 1;
    for (size_t i = 1; i < pairs; i++) {
        while (buf_bits < a_bits && pos < in_size) {
            buffer |= ((uint64_t)in[pos++] << buf_bits);
            buf_bits += 8;
        }
        delta_a[i] = (int32_t)(buffer & a_mask) + a_min;
        buffer >>= a_bits;
        buf_bits -= a_bits;
    }

    // Unpack channel B deltas
    std::vector<int32_t> delta_b(pairs);
    delta_b[0] = base_b;
    buffer = 0;
    buf_bits = 0;
    uint32_t b_mask = (1ULL << b_bits) - 1;
    for (size_t i = 1; i < pairs; i++) {
        while (buf_bits < b_bits && pos < in_size) {
            buffer |= ((uint64_t)in[pos++] << buf_bits);
            buf_bits += 8;
        }
        delta_b[i] = (int32_t)(buffer & b_mask) + b_min;
        buffer >>= b_bits;
        buf_bits -= b_bits;
    }

    // Delta decode and reinterleave
    int32_t a = delta_a[0], b = delta_b[0];
    for (size_t i = 0; i < pairs; i++) {
        if (i > 0) {
            a += delta_a[i];
            b += delta_b[i];
        }
        out[i*8+0] = a & 0xFF;
        out[i*8+1] = (a >> 8) & 0xFF;
        out[i*8+2] = (a >> 16) & 0xFF;
        out[i*8+3] = (a >> 24) & 0xFF;
        out[i*8+4] = b & 0xFF;
        out[i*8+5] = (b >> 8) & 0xFF;
        out[i*8+6] = (b >> 16) & 0xFF;
        out[i*8+7] = (b >> 24) & 0xFF;
    }
}

// ============================================================================
// STRIDED64: Deinterleave + Delta64 for interleaved 64-bit data (GPS doubles)
// Input: [A0, B0, A1, B1, ...] where A and B are 64-bit sequences
// Output: delta([A0, A1, ...]) || delta([B0, B1, ...])
// ============================================================================

// Detect if data is 2-channel interleaved 64-bit with sequential pattern
// GPS lat/lon: lat changes slowly, lon changes slowly, but lat vs lon differ
inline bool is_strided64_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 16) != 0) return false;  // Need pairs of 64-bit values

    size_t pairs = n / 16;
    size_t sample_count = std::min(pairs - 1, (size_t)32);

    size_t channel_a_sequential = 0;
    size_t channel_b_sequential = 0;

    for (size_t i = 0; i < sample_count; i++) {
        // Read two consecutive pairs (A0,B0) and (A1,B1)
        uint64_t a0 = 0, b0 = 0, a1 = 0, b1 = 0;
        for (int k = 0; k < 8; k++) {
            a0 |= ((uint64_t)data[i*16 + k]) << (k*8);
            b0 |= ((uint64_t)data[i*16 + 8 + k]) << (k*8);
            a1 |= ((uint64_t)data[i*16 + 16 + k]) << (k*8);
            b1 |= ((uint64_t)data[i*16 + 24 + k]) << (k*8);
        }

        // Check if channel A has sequential delta
        int64_t delta_a = (int64_t)(a1 - a0);
        // For IEEE 754 doubles: small value changes cause large ULP differences.
        // At magnitude ~37 (latitude), delta of 0.005 = ~700 billion ULPs.
        // Use 1 trillion threshold to capture GPS-like doubles.
        constexpr int64_t DOUBLE_DELTA_THRESHOLD = 1000000000000LL;  // 10^12
        if (delta_a >= -DOUBLE_DELTA_THRESHOLD && delta_a <= DOUBLE_DELTA_THRESHOLD) channel_a_sequential++;

        // Check if channel B has sequential delta
        int64_t delta_b = (int64_t)(b1 - b0);
        if (delta_b >= -DOUBLE_DELTA_THRESHOLD && delta_b <= DOUBLE_DELTA_THRESHOLD) channel_b_sequential++;
    }

    // Both channels must be sequential
    return (double)channel_a_sequential / sample_count >= 0.60 &&
           (double)channel_b_sequential / sample_count >= 0.60;
}

// Strided64 encode: deinterleave, delta encode each half
inline void strided64_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 16 || (n % 16) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t pairs = n / 16;

    // Deinterleave to two halves
    std::vector<uint8_t> channel_a(pairs * 8);
    std::vector<uint8_t> channel_b(pairs * 8);

    for (size_t i = 0; i < pairs; i++) {
        for (int b = 0; b < 8; b++) {
            channel_a[i*8 + b] = in[i*16 + b];
            channel_b[i*8 + b] = in[i*16 + 8 + b];
        }
    }

    // Delta64 encode each channel
    std::vector<uint8_t> delta_a(pairs * 8);
    std::vector<uint8_t> delta_b(pairs * 8);
    delta64_encode(delta_a.data(), channel_a.data(), pairs * 8);
    delta64_encode(delta_b.data(), channel_b.data(), pairs * 8);

    // Output: channel A deltas followed by channel B deltas
    memcpy(out, delta_a.data(), pairs * 8);
    memcpy(out + pairs * 8, delta_b.data(), pairs * 8);
}

// Strided64 decode: delta decode each half, reinterleave
inline void strided64_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 16 || (n % 16) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t pairs = n / 16;

    // Delta decode each half
    std::vector<uint8_t> channel_a(pairs * 8);
    std::vector<uint8_t> channel_b(pairs * 8);
    delta64_decode(channel_a.data(), in, pairs * 8);
    delta64_decode(channel_b.data(), in + pairs * 8, pairs * 8);

    // Reinterleave
    for (size_t i = 0; i < pairs; i++) {
        for (int b = 0; b < 8; b++) {
            out[i*16 + b] = channel_a[i*8 + b];
            out[i*16 + 8 + b] = channel_b[i*8 + b];
        }
    }
}

// ============================================================================
// STRIDED3_32: Deinterleave + Delta32 for 3-channel 32-bit data
// Input: [A0, B0, C0, A1, B1, C1, ...] where A, B, C are 32-bit sequences
// Output: delta([A0, A1, ...]) || delta([B0, B1, ...]) || delta([C0, C1, ...])
// Use cases: XYZ vertex positions, XYZ accelerometer, RGB float images
// ============================================================================

// Detect if data is 3-channel interleaved 32-bit with sequential pattern per channel
inline bool is_strided3_32_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 12) != 0) return false;  // Need triples of 32-bit values

    size_t triples = n / 12;
    size_t sample_count = std::min(triples - 1, (size_t)32);

    size_t channel_a_sequential = 0;
    size_t channel_b_sequential = 0;
    size_t channel_c_sequential = 0;

    // For float32 vertex data, deltas can be larger than raw integers
    // At magnitude ~100, delta 0.1 = ~10^5 ULP
    constexpr int64_t FLOAT32_DELTA_THRESHOLD = 1000000;  // 10^6

    for (size_t i = 0; i < sample_count; i++) {
        // Read two consecutive triples (A0,B0,C0) and (A1,B1,C1)
        size_t base0 = i * 12;
        size_t base1 = (i + 1) * 12;

        uint32_t a0 = data[base0] | (data[base0+1] << 8) | (data[base0+2] << 16) | ((uint32_t)data[base0+3] << 24);
        uint32_t b0 = data[base0+4] | (data[base0+5] << 8) | (data[base0+6] << 16) | ((uint32_t)data[base0+7] << 24);
        uint32_t c0 = data[base0+8] | (data[base0+9] << 8) | (data[base0+10] << 16) | ((uint32_t)data[base0+11] << 24);

        uint32_t a1 = data[base1] | (data[base1+1] << 8) | (data[base1+2] << 16) | ((uint32_t)data[base1+3] << 24);
        uint32_t b1 = data[base1+4] | (data[base1+5] << 8) | (data[base1+6] << 16) | ((uint32_t)data[base1+7] << 24);
        uint32_t c1 = data[base1+8] | (data[base1+9] << 8) | (data[base1+10] << 16) | ((uint32_t)data[base1+11] << 24);

        // Check deltas for each channel
        int64_t delta_a = (int64_t)a1 - (int64_t)a0;
        int64_t delta_b = (int64_t)b1 - (int64_t)b0;
        int64_t delta_c = (int64_t)c1 - (int64_t)c0;

        if (delta_a >= -FLOAT32_DELTA_THRESHOLD && delta_a <= FLOAT32_DELTA_THRESHOLD) channel_a_sequential++;
        if (delta_b >= -FLOAT32_DELTA_THRESHOLD && delta_b <= FLOAT32_DELTA_THRESHOLD) channel_b_sequential++;
        if (delta_c >= -FLOAT32_DELTA_THRESHOLD && delta_c <= FLOAT32_DELTA_THRESHOLD) channel_c_sequential++;
    }

    // All three channels must be sequential
    return (double)channel_a_sequential / sample_count >= 0.60 &&
           (double)channel_b_sequential / sample_count >= 0.60 &&
           (double)channel_c_sequential / sample_count >= 0.60;
}

// Strided3_32 encode: deinterleave, delta32 encode each channel
inline void strided3_32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 12 || (n % 12) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t triples = n / 12;

    // Deinterleave to three channels
    std::vector<uint8_t> channel_a(triples * 4);
    std::vector<uint8_t> channel_b(triples * 4);
    std::vector<uint8_t> channel_c(triples * 4);

    for (size_t i = 0; i < triples; i++) {
        for (int b = 0; b < 4; b++) {
            channel_a[i*4 + b] = in[i*12 + b];
            channel_b[i*4 + b] = in[i*12 + 4 + b];
            channel_c[i*4 + b] = in[i*12 + 8 + b];
        }
    }

    // Delta32 encode each channel into output
    delta32_encode(out, channel_a.data(), triples * 4);
    delta32_encode(out + triples * 4, channel_b.data(), triples * 4);
    delta32_encode(out + triples * 8, channel_c.data(), triples * 4);
}

// Strided3_32 decode: delta32 decode each channel, reinterleave
inline void strided3_32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 12 || (n % 12) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t triples = n / 12;

    // Delta decode each channel
    std::vector<uint8_t> channel_a(triples * 4);
    std::vector<uint8_t> channel_b(triples * 4);
    std::vector<uint8_t> channel_c(triples * 4);
    delta32_decode(channel_a.data(), in, triples * 4);
    delta32_decode(channel_b.data(), in + triples * 4, triples * 4);
    delta32_decode(channel_c.data(), in + triples * 8, triples * 4);

    // Reinterleave
    for (size_t i = 0; i < triples; i++) {
        for (int b = 0; b < 4; b++) {
            out[i*12 + b] = channel_a[i*4 + b];
            out[i*12 + 4 + b] = channel_b[i*4 + b];
            out[i*12 + 8 + b] = channel_c[i*4 + b];
        }
    }
}

// ============================================================================
// STRIDED4_32: Deinterleave + Delta32 for 4-channel 32-bit data
// Input: [A0, B0, C0, D0, A1, B1, C1, D1, ...] where A, B, C, D are 32-bit sequences
// Output: delta([A0, A1, ...]) || delta([B0, B1, ...]) || delta([C0, C1, ...]) || delta([D0, D1, ...])
// Use cases: RGBA float images, quaternion animations
// ============================================================================

inline bool is_strided4_32_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 16) != 0) return false;  // Need quads of 32-bit values

    size_t quads = n / 16;
    size_t sample_count = std::min(quads - 1, (size_t)32);

    size_t channel_a_sequential = 0;
    size_t channel_b_sequential = 0;
    size_t channel_c_sequential = 0;
    size_t channel_d_sequential = 0;

    constexpr int64_t FLOAT32_DELTA_THRESHOLD = 1000000;  // 10^6

    for (size_t i = 0; i < sample_count; i++) {
        size_t base0 = i * 16;
        size_t base1 = (i + 1) * 16;

        uint32_t a0 = data[base0] | (data[base0+1] << 8) | (data[base0+2] << 16) | ((uint32_t)data[base0+3] << 24);
        uint32_t b0 = data[base0+4] | (data[base0+5] << 8) | (data[base0+6] << 16) | ((uint32_t)data[base0+7] << 24);
        uint32_t c0 = data[base0+8] | (data[base0+9] << 8) | (data[base0+10] << 16) | ((uint32_t)data[base0+11] << 24);
        uint32_t d0 = data[base0+12] | (data[base0+13] << 8) | (data[base0+14] << 16) | ((uint32_t)data[base0+15] << 24);

        uint32_t a1 = data[base1] | (data[base1+1] << 8) | (data[base1+2] << 16) | ((uint32_t)data[base1+3] << 24);
        uint32_t b1 = data[base1+4] | (data[base1+5] << 8) | (data[base1+6] << 16) | ((uint32_t)data[base1+7] << 24);
        uint32_t c1 = data[base1+8] | (data[base1+9] << 8) | (data[base1+10] << 16) | ((uint32_t)data[base1+11] << 24);
        uint32_t d1 = data[base1+12] | (data[base1+13] << 8) | (data[base1+14] << 16) | ((uint32_t)data[base1+15] << 24);

        int64_t delta_a = (int64_t)a1 - (int64_t)a0;
        int64_t delta_b = (int64_t)b1 - (int64_t)b0;
        int64_t delta_c = (int64_t)c1 - (int64_t)c0;
        int64_t delta_d = (int64_t)d1 - (int64_t)d0;

        if (delta_a >= -FLOAT32_DELTA_THRESHOLD && delta_a <= FLOAT32_DELTA_THRESHOLD) channel_a_sequential++;
        if (delta_b >= -FLOAT32_DELTA_THRESHOLD && delta_b <= FLOAT32_DELTA_THRESHOLD) channel_b_sequential++;
        if (delta_c >= -FLOAT32_DELTA_THRESHOLD && delta_c <= FLOAT32_DELTA_THRESHOLD) channel_c_sequential++;
        if (delta_d >= -FLOAT32_DELTA_THRESHOLD && delta_d <= FLOAT32_DELTA_THRESHOLD) channel_d_sequential++;
    }

    // All four channels must be sequential
    return (double)channel_a_sequential / sample_count >= 0.60 &&
           (double)channel_b_sequential / sample_count >= 0.60 &&
           (double)channel_c_sequential / sample_count >= 0.60 &&
           (double)channel_d_sequential / sample_count >= 0.60;
}

inline void strided4_32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 16 || (n % 16) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t quads = n / 16;

    std::vector<uint8_t> channel_a(quads * 4);
    std::vector<uint8_t> channel_b(quads * 4);
    std::vector<uint8_t> channel_c(quads * 4);
    std::vector<uint8_t> channel_d(quads * 4);

    for (size_t i = 0; i < quads; i++) {
        for (int b = 0; b < 4; b++) {
            channel_a[i*4 + b] = in[i*16 + b];
            channel_b[i*4 + b] = in[i*16 + 4 + b];
            channel_c[i*4 + b] = in[i*16 + 8 + b];
            channel_d[i*4 + b] = in[i*16 + 12 + b];
        }
    }

    delta32_encode(out, channel_a.data(), quads * 4);
    delta32_encode(out + quads * 4, channel_b.data(), quads * 4);
    delta32_encode(out + quads * 8, channel_c.data(), quads * 4);
    delta32_encode(out + quads * 12, channel_d.data(), quads * 4);
}

inline void strided4_32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 16 || (n % 16) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t quads = n / 16;

    std::vector<uint8_t> channel_a(quads * 4);
    std::vector<uint8_t> channel_b(quads * 4);
    std::vector<uint8_t> channel_c(quads * 4);
    std::vector<uint8_t> channel_d(quads * 4);

    delta32_decode(channel_a.data(), in, quads * 4);
    delta32_decode(channel_b.data(), in + quads * 4, quads * 4);
    delta32_decode(channel_c.data(), in + quads * 8, quads * 4);
    delta32_decode(channel_d.data(), in + quads * 12, quads * 4);

    for (size_t i = 0; i < quads; i++) {
        for (int b = 0; b < 4; b++) {
            out[i*16 + b] = channel_a[i*4 + b];
            out[i*16 + 4 + b] = channel_b[i*4 + b];
            out[i*16 + 8 + b] = channel_c[i*4 + b];
            out[i*16 + 12 + b] = channel_d[i*4 + b];
        }
    }
}

// ============================================================================
// Byte Shuffle (Blosc2-style)
// Transpose bytes: cluster byte[0]s together, byte[1]s together, etc.
// For 4-byte floats: [b0 b1 b2 b3][b0 b1 b2 b3]... -> [b0 b0...][b1 b1...][b2 b2...][b3 b3...]
// This clusters similar exponent bytes together, improving compression
// ============================================================================

inline void byte_shuffle32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t count = n / 4;

    // Transpose: gather all byte[0]s, then byte[1]s, etc.
    for (size_t i = 0; i < count; i++) {
        out[i] = in[i*4];                // byte 0s
        out[count + i] = in[i*4 + 1];    // byte 1s
        out[count*2 + i] = in[i*4 + 2];  // byte 2s
        out[count*3 + i] = in[i*4 + 3];  // byte 3s (MSB - exponent for floats)
    }
}

inline void byte_shuffle32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t count = n / 4;

    // Reverse transpose
    for (size_t i = 0; i < count; i++) {
        out[i*4] = in[i];                // byte 0s
        out[i*4 + 1] = in[count + i];    // byte 1s
        out[i*4 + 2] = in[count*2 + i];  // byte 2s
        out[i*4 + 3] = in[count*3 + i];  // byte 3s
    }
}

inline void byte_shuffle64_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t count = n / 8;

    // Transpose: gather all byte[0]s, then byte[1]s, etc.
    for (size_t i = 0; i < count; i++) {
        out[i] = in[i*8];
        out[count + i] = in[i*8 + 1];
        out[count*2 + i] = in[i*8 + 2];
        out[count*3 + i] = in[i*8 + 3];
        out[count*4 + i] = in[i*8 + 4];
        out[count*5 + i] = in[i*8 + 5];
        out[count*6 + i] = in[i*8 + 6];
        out[count*7 + i] = in[i*8 + 7];
    }
}

inline void byte_shuffle64_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 8) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t count = n / 8;

    // Reverse transpose
    for (size_t i = 0; i < count; i++) {
        out[i*8] = in[i];
        out[i*8 + 1] = in[count + i];
        out[i*8 + 2] = in[count*2 + i];
        out[i*8 + 3] = in[count*3 + i];
        out[i*8 + 4] = in[count*4 + i];
        out[i*8 + 5] = in[count*5 + i];
        out[i*8 + 6] = in[count*6 + i];
        out[i*8 + 7] = in[count*7 + i];
    }
}

// 2-byte stride shuffle: split alternating bytes into two streams
// Key insight: protobuf tag/value pairs have different entropy (3 bits vs 7 bits)
// Splitting them allows zstd to compress each stream more efficiently
inline void byte_shuffle2_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 2 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t half = n / 2;
    for (size_t i = 0; i < half; i++) {
        out[i] = in[i * 2];          // Even bytes (stream 0)
        out[half + i] = in[i * 2 + 1]; // Odd bytes (stream 1)
    }
}

inline void byte_shuffle2_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 2 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }

    size_t half = n / 2;
    for (size_t i = 0; i < half; i++) {
        out[i * 2] = in[i];          // Even bytes (stream 0)
        out[i * 2 + 1] = in[half + i]; // Odd bytes (stream 1)
    }
}

// Detect if 2-byte shuffle would improve compression
// Key insight: if even/odd bytes have different entropy, splitting helps
inline bool is_byte_shuffle2_candidate(const uint8_t* data, size_t n) {
    if (n < 256 || (n % 2) != 0) return false;

    // Sample first 4KB
    size_t sample = std::min(n, (size_t)4096);
    size_t half = sample / 2;

    // Count byte frequencies for even/odd positions
    int even_counts[256] = {0};
    int odd_counts[256] = {0};

    for (size_t i = 0; i < sample; i += 2) {
        even_counts[data[i]]++;
        if (i + 1 < sample) odd_counts[data[i + 1]]++;
    }

    // Calculate entropy for each stream
    double even_entropy = 0, odd_entropy = 0;
    for (int i = 0; i < 256; i++) {
        if (even_counts[i] > 0) {
            double p = (double)even_counts[i] / half;
            even_entropy -= p * log2(p);
        }
        if (odd_counts[i] > 0) {
            double p = (double)odd_counts[i] / half;
            odd_entropy -= p * log2(p);
        }
    }

    // Calculate original entropy
    double original_entropy = byte_entropy(data, sample);

    // Splitting helps if:
    // 1. The two streams have DIFFERENT entropy (one is more compressible)
    // 2. The average is at least a bit lower than original
    double entropy_diff = (even_entropy > odd_entropy) ?
                          (even_entropy - odd_entropy) :
                          (odd_entropy - even_entropy);

    // If entropy differs by >1 bit, splitting likely helps
    // Also check that at least one stream is low entropy (<5 bits)
    return (entropy_diff > 1.0 && (even_entropy < 5.0 || odd_entropy < 5.0));
}

// ============================================================================
// E8/E9 Filter for x86 Executables
// Converts CALL (E8) and JMP (E9) relative addresses to absolute
// This clusters similar targets together, improving LZ77 compression by 15-25%
// ============================================================================

inline bool is_e8e9_candidate(const uint8_t* data, size_t n) {
    if (n < 64) return false;

    // Check for PE/ELF header signatures
    bool has_pe = (n >= 2 && data[0] == 0x4D && data[1] == 0x5A);  // MZ
    bool has_elf = (n >= 4 && data[0] == 0x7F && data[1] == 'E' &&
                    data[2] == 'L' && data[3] == 'F');

    if (!has_pe && !has_elf) return false;

    // Count E8/E9 bytes (rough heuristic)
    size_t sample_size = std::min(n, (size_t)4096);
    size_t e8e9_count = 0;
    for (size_t i = 0; i < sample_size; i++) {
        if (data[i] == 0xE8 || data[i] == 0xE9) e8e9_count++;
    }

    // Expect at least 0.5% CALL/JMP instructions
    return e8e9_count >= sample_size / 200;
}

inline void e8e9_encode(uint8_t* out, const uint8_t* in, size_t n) {
    memcpy(out, in, n);

    // Process E8 (CALL) and E9 (JMP) - they have 4-byte relative offsets
    for (size_t i = 0; i + 5 <= n; i++) {
        if (out[i] == 0xE8 || out[i] == 0xE9) {
            // Read little-endian 32-bit relative offset
            int32_t rel = out[i+1] | (out[i+2] << 8) | (out[i+3] << 16) | (out[i+4] << 24);
            // Convert to absolute (current position + offset + 5)
            int32_t abs = rel + (int32_t)(i + 5);
            // Write back as little-endian
            out[i+1] = abs & 0xFF;
            out[i+2] = (abs >> 8) & 0xFF;
            out[i+3] = (abs >> 16) & 0xFF;
            out[i+4] = (abs >> 24) & 0xFF;
            i += 4;  // Skip the address bytes
        }
    }
}

inline void e8e9_decode(uint8_t* out, const uint8_t* in, size_t n) {
    memcpy(out, in, n);

    // Reverse the transformation
    for (size_t i = 0; i + 5 <= n; i++) {
        if (out[i] == 0xE8 || out[i] == 0xE9) {
            // Read little-endian 32-bit absolute address
            int32_t abs = out[i+1] | (out[i+2] << 8) | (out[i+3] << 16) | (out[i+4] << 24);
            // Convert to relative (absolute - current position - 5)
            int32_t rel = abs - (int32_t)(i + 5);
            // Write back as little-endian
            out[i+1] = rel & 0xFF;
            out[i+2] = (rel >> 8) & 0xFF;
            out[i+3] = (rel >> 16) & 0xFF;
            out[i+4] = (rel >> 24) & 0xFF;
            i += 4;  // Skip the address bytes
        }
    }
}

// ============================================================================
// Linear Prediction
// Predicts x[i] = 2*x[i-1] - x[i-2] (linear extrapolation)
// Stores prediction error. Better than delta for smooth ramps/curves.
// ============================================================================

inline bool is_linear_pred16_candidate(const uint8_t* data, size_t n) {
    if (n < 64 || (n % 2) != 0) return false;

    const int16_t* p = (const int16_t*)data;
    size_t count = n / 2;
    size_t samples = std::min(count - 2, (size_t)32);

    // Count how often linear prediction is better than delta
    size_t linear_wins = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t pred = 2 * (int32_t)p[i+1] - (int32_t)p[i];
        int32_t actual = p[i+2];
        int32_t linear_err = abs(actual - pred);
        int32_t delta_err = abs(actual - p[i+1]);
        if (linear_err < delta_err) linear_wins++;
    }

    // Linear prediction should win at least 60% of the time
    return linear_wins >= samples * 6 / 10;
}

inline void linear_pred16_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }

    const int16_t* inp = (const int16_t*)in;
    int16_t* outp = (int16_t*)out;
    size_t count = n / 2;

    // Store first two values raw
    outp[0] = inp[0];
    outp[1] = inp[1];

    // Store prediction errors
    for (size_t i = 2; i < count; i++) {
        int32_t pred = 2 * (int32_t)inp[i-1] - (int32_t)inp[i-2];
        int16_t err = (int16_t)(inp[i] - pred);
        outp[i] = err;
    }
}

inline void linear_pred16_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4 || (n % 2) != 0) {
        memcpy(out, in, n);
        return;
    }

    const int16_t* inp = (const int16_t*)in;
    int16_t* outp = (int16_t*)out;
    size_t count = n / 2;

    // Recover first two values
    outp[0] = inp[0];
    outp[1] = inp[1];

    // Recover from prediction errors
    for (size_t i = 2; i < count; i++) {
        int32_t pred = 2 * (int32_t)outp[i-1] - (int32_t)outp[i-2];
        outp[i] = (int16_t)(pred + inp[i]);
    }
}

inline bool is_linear_pred32_candidate(const uint8_t* data, size_t n) {
    if (n < 128 || (n % 4) != 0) return false;

    const int32_t* p = (const int32_t*)data;
    size_t count = n / 4;
    size_t samples = std::min(count - 2, (size_t)32);

    size_t linear_wins = 0;
    for (size_t i = 0; i < samples; i++) {
        int64_t pred = 2 * (int64_t)p[i+1] - (int64_t)p[i];
        int64_t actual = p[i+2];
        int64_t linear_err = llabs(actual - pred);
        int64_t delta_err = llabs(actual - p[i+1]);
        if (linear_err < delta_err) linear_wins++;
    }

    return linear_wins >= samples * 6 / 10;
}

inline void linear_pred32_encode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }

    const int32_t* inp = (const int32_t*)in;
    int32_t* outp = (int32_t*)out;
    size_t count = n / 4;

    outp[0] = inp[0];
    outp[1] = inp[1];

    for (size_t i = 2; i < count; i++) {
        int64_t pred = 2 * (int64_t)inp[i-1] - (int64_t)inp[i-2];
        int32_t err = (int32_t)(inp[i] - pred);
        outp[i] = err;
    }
}

inline void linear_pred32_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 8 || (n % 4) != 0) {
        memcpy(out, in, n);
        return;
    }

    const int32_t* inp = (const int32_t*)in;
    int32_t* outp = (int32_t*)out;
    size_t count = n / 4;

    outp[0] = inp[0];
    outp[1] = inp[1];

    for (size_t i = 2; i < count; i++) {
        int64_t pred = 2 * (int64_t)outp[i-1] - (int64_t)outp[i-2];
        outp[i] = (int32_t)(pred + inp[i]);
    }
}

// ============================================================================
// PAETH PREDICTOR (PNG-style for image data)
// ============================================================================
// Paeth predictor: predict from left(a), top(b), top-left(c) neighbors
// Choose prediction closest to a + b - c

inline uint8_t paeth_predict(uint8_t a, uint8_t b, uint8_t c) {
    int p = (int)a + (int)b - (int)c;
    int pa = abs(p - (int)a);
    int pb = abs(p - (int)b);
    int pc = abs(p - (int)c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Detect best row width for Paeth by trying common widths
// Returns 0 if no good width found
inline size_t detect_paeth_row_width(const uint8_t* data, size_t n) {
    if (n < 1024) return 0;

    // Try common RGB row widths (width * 3 bytes)
    // 256*3=768, 512*3=1536, 1024*3=3072, etc.
    size_t best_width = 0;
    double best_score = 0.0;

    const size_t widths[] = {768, 1536, 2304, 3072, 4608, 6144};  // 256,512,768,1024,1536,2048 pixels * 3

    for (size_t w : widths) {
        if (w > n / 4) continue;  // Need at least 4 rows

        // Sample correlation: how similar is row[i] to row[i+width]?
        size_t matches = 0;
        size_t samples = std::min((size_t)512, n - w);
        for (size_t i = 0; i < samples; i++) {
            int diff = abs((int)data[i] - (int)data[i + w]);
            if (diff < 16) matches++;  // Close values
        }

        double score = (double)matches / samples;
        if (score > 0.5 && score > best_score) {  // At least 50% correlation
            best_score = score;
            best_width = w;
        }
    }

    return best_width;
}

// Check if data looks like RGB image (rows of correlated pixels)
inline bool is_paeth_rgb_candidate(const uint8_t* data, size_t n) {
    return detect_paeth_row_width(data, n) > 0;
}

// Encode with Paeth predictor (explicit row width)
// Format: [row_width: 2 bytes] [paeth residuals: n-2 bytes]
inline void paeth_rgb_encode_with_width(uint8_t* out, const uint8_t* in, size_t n, size_t row_width) {
    if (row_width == 0 || row_width > 65535 || n < row_width + 2) {
        memcpy(out, in, n);
        return;
    }

    // Store row width in first 2 bytes
    out[0] = (uint8_t)(row_width & 0xFF);
    out[1] = (uint8_t)((row_width >> 8) & 0xFF);

    // First row: just delta from left (a=left, b=0, c=0)
    out[2] = in[0];  // First pixel raw
    for (size_t i = 1; i < row_width; i++) {
        out[2 + i] = in[i] - in[i-1];
    }

    // Remaining rows: use full Paeth
    for (size_t row_start = row_width; row_start < n; row_start += row_width) {
        size_t row_len = std::min(row_width, n - row_start);

        // First pixel of row: predict from top (a=0, b=top, c=0)
        out[2 + row_start] = in[row_start] - in[row_start - row_width];

        for (size_t i = 1; i < row_len; i++) {
            size_t pos = row_start + i;
            uint8_t a = in[pos - 1];              // Left
            uint8_t b = in[pos - row_width];       // Top
            uint8_t c = in[pos - row_width - 1];   // Top-left
            uint8_t pred = paeth_predict(a, b, c);
            out[2 + pos] = in[pos] - pred;
        }
    }
}

// Encode with Paeth predictor (auto-detect row width)
inline void paeth_rgb_encode(uint8_t* out, const uint8_t* in, size_t n) {
    size_t row_width = detect_paeth_row_width(in, n);
    if (row_width == 0) {
        memcpy(out, in, n);
        return;
    }
    paeth_rgb_encode_with_width(out, in, n, row_width);
}

// Decode Paeth predictor
inline void paeth_rgb_decode(uint8_t* out, const uint8_t* in, size_t n) {
    if (n < 4) {
        memcpy(out, in, n);
        return;
    }

    size_t row_width = (size_t)in[0] | ((size_t)in[1] << 8);
    size_t data_size = n - 2;

    if (row_width == 0 || row_width > data_size) {
        memcpy(out, in, n);
        return;
    }

    // First row: delta decode from left
    out[0] = in[2];
    for (size_t i = 1; i < row_width && i < data_size; i++) {
        out[i] = out[i-1] + in[2 + i];
    }

    // Remaining rows: Paeth decode
    for (size_t row_start = row_width; row_start < data_size; row_start += row_width) {
        size_t row_len = std::min(row_width, data_size - row_start);

        // First pixel: predict from top
        out[row_start] = out[row_start - row_width] + in[2 + row_start];

        for (size_t i = 1; i < row_len; i++) {
            size_t pos = row_start + i;
            uint8_t a = out[pos - 1];
            uint8_t b = out[pos - row_width];
            uint8_t c = out[pos - row_width - 1];
            uint8_t pred = paeth_predict(a, b, c);
            out[pos] = pred + in[2 + pos];
        }
    }
}

// ============================================================================
// BLOCK_COLUMNAR: Fixed-width record columnar encoding
// Detects fixed-length blocks, separates into columns, encodes each optimally
// +52-78% vs zstd on logs, records, HTML with sequential IDs
// ============================================================================

// Column encoding types
enum class ColumnType : uint8_t {
    CONSTANT = 0,  // All values same, store once
    DELTA = 1,     // Small deltas between values
    LINEAR = 2,    // Perfect arithmetic sequence (base + i*delta)
    RAW = 3        // No pattern, store all values
};

// Detect block length by looking for repeating byte patterns
// Returns 0 if no regular structure found
// out_header_offset: if non-null, set to the byte offset where records start (e.g. DBF header)
inline size_t detect_block_length(const uint8_t* data, size_t n, size_t* out_header_offset = nullptr) {
    if (out_header_offset) *out_header_offset = 0;
    if (n < 60) return 0;  // Need at least 3 blocks of minimum size

    // === DBF format detection (FIRST - header tells us exact record size) ===
    // dBASE/FoxPro files have fixed-width records after a variable-length header.
    // Must run before heuristic scan: DBF field descriptors (32-byte padding) cause
    // false matches at len=32/64 in the short-record scan.
    if (n > 32) {
        uint8_t ver = data[0];
        bool is_dbf = (ver == 0x02 || ver == 0x03 || ver == 0x04 || ver == 0x05 ||
                       ver == 0x30 || ver == 0x31 || ver == 0x43 || ver == 0x63 ||
                       ver == 0x83 || ver == 0x8B || ver == 0x8E || ver == 0xCB ||
                       ver == 0xF5 || ver == 0xFB);
        if (is_dbf) {
            uint16_t hdr_size = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
            uint16_t rec_size = (uint16_t)data[10] | ((uint16_t)data[11] << 8);
            if (rec_size >= 2 && rec_size <= 32768 && hdr_size >= 32 &&
                hdr_size < n && hdr_size + (size_t)rec_size * 3 <= n) {
                // Validate: first field name at offset 32 should be printable ASCII or null
                bool valid = true;
                for (int i = 0; i < 4; i++) {
                    uint8_t c = data[32 + i];
                    if (c != 0 && (c < 0x20 || c > 0x7E)) { valid = false; break; }
                }
                if (valid) {
                    // Verify record structure: check 3 consecutive records match
                    const uint8_t* recs = data + hdr_size;
                    size_t avail = n - hdr_size;
                    size_t nblocks = avail / rec_size;
                    if (nblocks >= 3) {
                        size_t same = 0;
                        for (size_t pos = 0; pos < (size_t)rec_size; pos++) {
                            if (recs[pos] == recs[rec_size + pos] &&
                                recs[pos] == recs[2 * (size_t)rec_size + pos])
                                same++;
                        }
                        // Lower threshold (33%) since DBF header already confirmed format
                        if (same > (size_t)rec_size / 3) {
                            if (out_header_offset) *out_header_offset = hdr_size;
                            return rec_size;
                        }
                    }
                }
            }
        }
    }

    // === Short records (20-200), heuristic scan ===
    for (size_t len = 20; len <= 200 && len <= n / 3; len++) {
        size_t num_blocks = n / len;
        if (num_blocks < 3) continue;

        // Count positions that are same across first 3 blocks
        size_t same_count = 0;
        for (size_t pos = 0; pos < len; pos++) {
            if (data[pos] == data[len + pos] && data[pos] == data[2 * len + pos]) {
                same_count++;
            }
        }

        // If >50% positions are same, this might be the block length
        if (same_count > len / 2) {
            return len;
        }
    }

    return 0;
}

// Check if a column is constant (all same value)
inline bool is_constant_column(const uint8_t* data, size_t block_len, size_t num_blocks, size_t pos) {
    uint8_t ref = data[pos];
    for (size_t b = 1; b < num_blocks; b++) {
        if (data[b * block_len + pos] != ref) return false;
    }
    return true;
}

// Check if a column follows a linear pattern (base + i*delta)
inline bool is_linear_column(const uint8_t* data, size_t block_len, size_t num_blocks, size_t pos,
                              int& base, int& delta) {
    if (num_blocks < 3) return false;
    base = data[pos];
    delta = (int)data[block_len + pos] - base;
    for (size_t b = 2; b < num_blocks; b++) {
        int expected = base + delta * (int)b;
        // Handle wraparound for byte values
        expected = ((expected % 256) + 256) % 256;
        if ((int)data[b * block_len + pos] != expected) return false;
    }
    return true;
}

// Check if a column has small deltas (average |delta| < 5)
inline bool is_delta_column(const uint8_t* data, size_t block_len, size_t num_blocks, size_t pos) {
    if (num_blocks < 2) return false;
    int64_t sum_abs_delta = 0;
    for (size_t b = 1; b < num_blocks; b++) {
        int d = (int)data[b * block_len + pos] - (int)data[(b-1) * block_len + pos];
        sum_abs_delta += std::abs(d);
    }
    double avg_delta = (double)sum_abs_delta / (num_blocks - 1);
    return avg_delta < 5.0;
}

// Detect if data is a good BLOCK_COLUMNAR candidate
inline bool is_block_columnar_candidate(const uint8_t* data, size_t n,
                                         size_t& out_block_len, size_t& out_header_offset) {
    out_header_offset = 0;
    out_block_len = detect_block_length(data, n, &out_header_offset);
    if (out_block_len == 0) return false;

    // Work on record data (after header)
    const uint8_t* recs = data + out_header_offset;
    size_t recs_n = n - out_header_offset;
    size_t num_blocks = recs_n / out_block_len;
    if (num_blocks < 3) return false;

    // Count how many columns are CONSTANT or DELTA (not RAW)
    size_t good_cols = 0;
    for (size_t pos = 0; pos < out_block_len; pos++) {
        int base, delta;
        if (is_constant_column(recs, out_block_len, num_blocks, pos)) {
            good_cols++;
        } else if (is_linear_column(recs, out_block_len, num_blocks, pos, base, delta)) {
            good_cols++;
        } else if (is_delta_column(recs, out_block_len, num_blocks, pos)) {
            good_cols++;
        }
    }

    // Need at least 50% columns to be structured for this to be worthwhile
    return good_cols > out_block_len / 2;
}

// BLOCK_COLUMNAR encode
// Format:
//   [2 bytes] block_length
//   [2 bytes] num_blocks
//   [2 bytes] header_offset (bytes before records start, e.g. DBF header)
//   [header_offset bytes] raw header/prefix data
//   [block_len bytes] column_types (one byte per column)
//   [variable] column data:
//     CONSTANT: 1 byte (the constant value)
//     LINEAR: 2 bytes (base, delta+128 for signed)
//     DELTA: 1 + (num_blocks-1) bytes (first value + deltas+128)
//     RAW: num_blocks bytes
//   [variable] remainder bytes (data after last complete block)
inline size_t block_columnar_encode(uint8_t* out, const uint8_t* in, size_t n,
                                     size_t block_len, size_t header_offset = 0) {
    if (block_len == 0 || block_len > 65535 || header_offset > 65535) {
        memcpy(out, in, n);
        return n;
    }

    // Records start after the header
    const uint8_t* recs = in + header_offset;
    size_t recs_n = n - header_offset;
    size_t num_blocks = recs_n / block_len;
    if (num_blocks < 2 || num_blocks > 65535) {
        memcpy(out, in, n);
        return n;
    }

    size_t out_pos = 0;

    // Header: block_length (2 bytes), num_blocks (2 bytes), header_offset (2 bytes)
    out[out_pos++] = block_len & 0xFF;
    out[out_pos++] = (block_len >> 8) & 0xFF;
    out[out_pos++] = num_blocks & 0xFF;
    out[out_pos++] = (num_blocks >> 8) & 0xFF;
    out[out_pos++] = header_offset & 0xFF;
    out[out_pos++] = (header_offset >> 8) & 0xFF;

    // Write raw header/prefix
    for (size_t i = 0; i < header_offset; i++) {
        out[out_pos++] = in[i];
    }

    // Analyze and write column types
    std::vector<ColumnType> col_types(block_len);
    std::vector<int> col_base(block_len);
    std::vector<int> col_delta(block_len);

    for (size_t pos = 0; pos < block_len; pos++) {
        if (is_constant_column(recs, block_len, num_blocks, pos)) {
            col_types[pos] = ColumnType::CONSTANT;
        } else if (is_linear_column(recs, block_len, num_blocks, pos, col_base[pos], col_delta[pos])) {
            col_types[pos] = ColumnType::LINEAR;
        } else if (is_delta_column(recs, block_len, num_blocks, pos)) {
            col_types[pos] = ColumnType::DELTA;
        } else {
            col_types[pos] = ColumnType::RAW;
        }
        out[out_pos++] = static_cast<uint8_t>(col_types[pos]);
    }

    // Write column data
    for (size_t pos = 0; pos < block_len; pos++) {
        switch (col_types[pos]) {
            case ColumnType::CONSTANT:
                out[out_pos++] = recs[pos];
                break;

            case ColumnType::LINEAR:
                out[out_pos++] = static_cast<uint8_t>(col_base[pos]);
                out[out_pos++] = static_cast<uint8_t>(col_delta[pos] + 128);  // Signed delta
                break;

            case ColumnType::DELTA:
                out[out_pos++] = recs[pos];  // First value
                for (size_t b = 1; b < num_blocks; b++) {
                    int d = (int)recs[b * block_len + pos] - (int)recs[(b-1) * block_len + pos];
                    out[out_pos++] = static_cast<uint8_t>(d + 128);  // Signed delta
                }
                break;

            case ColumnType::RAW:
                for (size_t b = 0; b < num_blocks; b++) {
                    out[out_pos++] = recs[b * block_len + pos];
                }
                break;
        }
    }

    // Write remainder (bytes after last complete block in records section)
    size_t remainder = recs_n - num_blocks * block_len;
    for (size_t i = 0; i < remainder; i++) {
        out[out_pos++] = recs[num_blocks * block_len + i];
    }

    return out_pos;
}

// BLOCK_COLUMNAR decode
inline size_t block_columnar_decode(uint8_t* out, const uint8_t* in, size_t encoded_size, size_t original_size) {
    if (encoded_size < 6) {
        memcpy(out, in, encoded_size);
        return encoded_size;
    }

    // Read header
    size_t block_len = (size_t)in[0] | ((size_t)in[1] << 8);
    size_t num_blocks = (size_t)in[2] | ((size_t)in[3] << 8);
    size_t header_offset = (size_t)in[4] | ((size_t)in[5] << 8);

    if (block_len == 0 || num_blocks == 0 || block_len > 65535 || num_blocks > 65535) {
        memcpy(out, in, encoded_size);
        return encoded_size;
    }

    size_t in_pos = 6;

    // Read raw header/prefix
    if (header_offset > 0) {
        if (in_pos + header_offset > encoded_size) {
            memcpy(out, in, encoded_size);
            return encoded_size;
        }
        memcpy(out, in + in_pos, header_offset);
        in_pos += header_offset;
    }

    // Output records start after header
    uint8_t* out_recs = out + header_offset;

    // Read column types
    if (in_pos + block_len > encoded_size) {
        memcpy(out, in, encoded_size);
        return encoded_size;
    }

    std::vector<ColumnType> col_types(block_len);
    for (size_t pos = 0; pos < block_len; pos++) {
        col_types[pos] = static_cast<ColumnType>(in[in_pos++]);
    }

    // Decode column data back to original layout
    for (size_t pos = 0; pos < block_len; pos++) {
        switch (col_types[pos]) {
            case ColumnType::CONSTANT: {
                uint8_t val = in[in_pos++];
                for (size_t b = 0; b < num_blocks; b++) {
                    out_recs[b * block_len + pos] = val;
                }
                break;
            }

            case ColumnType::LINEAR: {
                int base = in[in_pos++];
                int delta = (int)in[in_pos++] - 128;
                for (size_t b = 0; b < num_blocks; b++) {
                    int val = base + delta * (int)b;
                    out_recs[b * block_len + pos] = static_cast<uint8_t>(((val % 256) + 256) % 256);
                }
                break;
            }

            case ColumnType::DELTA: {
                uint8_t prev = in[in_pos++];
                out_recs[pos] = prev;
                for (size_t b = 1; b < num_blocks; b++) {
                    int d = (int)in[in_pos++] - 128;
                    prev = static_cast<uint8_t>(prev + d);
                    out_recs[b * block_len + pos] = prev;
                }
                break;
            }

            case ColumnType::RAW: {
                for (size_t b = 0; b < num_blocks; b++) {
                    out_recs[b * block_len + pos] = in[in_pos++];
                }
                break;
            }
        }
    }

    // Read remainder (after last complete block in records section)
    size_t recs_total = original_size - header_offset;
    size_t remainder = recs_total - num_blocks * block_len;
    for (size_t i = 0; i < remainder; i++) {
        out_recs[num_blocks * block_len + i] = in[in_pos++];
    }

    return original_size;
}

// ============================================================================
// Detection + Dispatch
// ============================================================================

struct DetectionResult {
    Strategy strategy;
    double confidence;
    double original_entropy;
    double preprocessed_entropy;
};

// Detect optimal strategy for data block
// Returns the strategy and entropy measurements
// Order: 64-bit → 32-bit → 16-bit → byte → RLE (most specific first)
inline DetectionResult detect(const uint8_t* data, size_t n, uint8_t* work_buffer = nullptr) {
    DetectionResult result;
    result.original_entropy = byte_entropy(data, std::min(n, (size_t)4096));
    result.strategy = Strategy::NONE;
    result.confidence = 0.0;
    result.preprocessed_entropy = result.original_entropy;

    uint8_t* temp = work_buffer;
    bool allocated = false;
    if (!temp) {
        temp = new uint8_t[n + 65542];  // Extra space for block_columnar (block_len + 6 header bytes)
        allocated = true;
    }

    double best_entropy = result.original_entropy;
    Strategy best_strategy = Strategy::NONE;

    // Check E8/E9 filter for x86 executables (PE/ELF headers)
    // This is VERY specific - only applies to machine code
    if (is_e8e9_candidate(data, n)) {
        e8e9_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 10% entropy reduction
        if (entropy < best_entropy * 0.90) {
            best_entropy = entropy;
            best_strategy = Strategy::E8E9_X86;
        }
    }

    // Check BLOCK_COLUMNAR for fixed-width text records (logs, CSV, DBF, HTML items)
    // This is very specific - only applies to data with repeating fixed-length structures
    {
        size_t block_len = 0;
        size_t hdr_offset = 0;
        if (is_block_columnar_candidate(data, n, block_len, hdr_offset)) {
            size_t encoded_size = block_columnar_encode(temp, data, n, block_len, hdr_offset);
            // Skip the format header (6 bytes) and raw prefix when measuring entropy
            // Otherwise the raw DBF header dominates the entropy sample
            size_t skip = 6 + hdr_offset;
            size_t measure_start = std::min(skip, encoded_size);
            size_t measure_len = encoded_size > measure_start ? encoded_size - measure_start : 0;
            double entropy = measure_len > 0
                ? byte_entropy(temp + measure_start, std::min(measure_len, (size_t)4096))
                : best_entropy;
            // Require significant entropy reduction (at least 30%)
            if (entropy < best_entropy * 0.70) {
                best_entropy = entropy;
                best_strategy = Strategy::BLOCK_COLUMNAR;
            }
        }
    }

    // Check 64-bit delta (database IDs) - most specific
    if (is_delta64_candidate(data, n, 0.60)) {
        delta64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy * 0.8) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA64;
        }
    }

    // Check 32-bit delta (timestamps, prices)
    if (is_delta32_candidate(data, n, 0.60)) {
        delta32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA32;
        }
    }

    // Check delta-of-delta for regular interval timestamps
    // Only try if data looks like 32-bit sequential and entropy is still high
    if (is_delta_of_delta32_candidate(data, n, 0.60)) {
        delta_of_delta32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA_OF_DELTA32;
        }
    }

    // Check 16-bit delta (sensors, audio)
    if (is_delta16_candidate(data, n, 0.60)) {
        delta16_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA16;
        }
    }

    // Check LINEAR_PRED16 for smooth 16-bit signals (audio ramps, gradients)
    // Linear prediction: pred[i] = 2*x[i-1] - x[i-2], better than delta for smooth curves
    if (is_linear_pred16_candidate(data, n)) {
        linear_pred16_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::LINEAR_PRED16;
        }
    }

    // Check LINEAR_PRED32 for smooth 32-bit signals (sensor data with ramps)
    if (is_linear_pred32_candidate(data, n)) {
        linear_pred32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::LINEAR_PRED32;
        }
    }

    // Check byte-level delta
    if (is_delta_candidate(data, n, 0.60)) {
        delta_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA;
        }
    }

    // Check for slow-moving float64 sequences (temperature, altitude, etc.)
    // These have similar consecutive values as doubles but different bit patterns.
    // Delta encoding on raw bits compresses extremely well (2.4x better than xz).
    // Detection: if values change slowly (within 1% or 1.0), delta will help.
    // Note: entropy doesn't capture the benefit well, but slow-moving floats
    // are a strong enough signal that we can trust the detection directly.
    if (is_slow_float64_candidate(data, n, 0.70)) {
        delta64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Slow-moving floats: accept even small entropy reduction (5%+)
        // because zstd exploits the structure of delta-encoded floats well
        if (entropy < best_entropy * 0.95) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA64;
        }
    }

    // Check XOR encoding for 64-bit floats (Gorilla-style)
    // IMPORTANT: XOR can hurt zstd's LZ77 matching. Require significant entropy
    // reduction (25%+) to justify using XOR preprocessing.
    if (is_xor_float64_candidate(data, n, 0.70)) {
        xor_float64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy * 0.75) {
            best_entropy = entropy;
            best_strategy = Strategy::XOR_FLOAT64;
        }
    }

    // Check ALP-style decimal float encoding for float64 (better than XOR for decimal floats)
    int alp_exp64 = is_decimal_float64_candidate(data, n);
    if (alp_exp64 >= 0) {
        size_t alp_size = alp_float64_encode(temp, data, n);
        // ALP wins if it compresses by at least 20%
        if (alp_size < n * 0.80) {
            double effective_entropy = 8.0 * alp_size / n;  // Simulated entropy from size
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::ALP_FLOAT64;
            }
        }
    }

    // Check ALP-style decimal float encoding for float32 (FCBench data)
    // This is critical for benchmarking against SOTA - FCBench uses float32
    int alp_exp32 = is_decimal_float32_candidate(data, n);
    if (alp_exp32 >= 0) {
        size_t alp_size = alp_float32_encode(temp, data, n);
        // ALP wins if it compresses by at least 20%
        if (alp_size < n * 0.80) {
            double effective_entropy = 8.0 * alp_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::ALP_FLOAT32;
            }
        }
    }

    // Check XOR encoding for 32-bit floats (Gorilla-style)
    // Only if ALP didn't win - XOR is fallback for non-decimal floats
    // IMPORTANT: XOR often has low byte entropy but produces patterns that zstd
    // doesn't compress well (weak dictionary matches). Require significant entropy
    // reduction (25%+) to justify using XOR, otherwise it hurts overall compression.
    if (best_strategy != Strategy::ALP_FLOAT32 && is_xor_float32_candidate(data, n, 0.70)) {
        xor_float32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 25% entropy reduction - XOR can hurt zstd's LZ77 matching
        if (entropy < best_entropy * 0.75) {
            best_entropy = entropy;
            best_strategy = Strategy::XOR_FLOAT32;
        }
    }

    // Check Delta + ZigZag + BitPack for 32-bit data (TurboPFor-style)
    // Try this for 32-bit data that might benefit from bit-packing
    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.50)) {
        size_t packed_size = delta_zigzag_bitpack32_encode(temp, data, n);
        // Win if it compresses significantly
        if (packed_size < n * 0.70) {
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::DELTA_ZIGZAG_BITPACK32;
            }
        }
    }

    // Check Delta + FOR + BitPack for 32-bit data (better for jittery timestamps)
    // FOR is better when deltas are clustered around a non-zero value
    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.50)) {
        size_t packed_size = delta_for_bitpack32_encode(temp, data, n);
        if (packed_size < n * 0.70) {
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::DELTA_FOR_BITPACK32;
            }
        }
    }


    // Check NARROW16 for 16-bit data with range ≤255 (sensor data around mean)
    // Preferred over FOR16: byte-aligned data compresses better with zstd
    if ((n % 2) == 0 && n >= 128 && is_narrow16_candidate(data, n)) {
        size_t packed_size = narrow16_encode(temp, data, n);
        if (packed_size < n * 0.60) {  // NARROW16 should get ~50% reduction
            double effective_entropy = 8.0 * packed_size / n * 0.85;  // Bonus for byte-aligned
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::NARROW16;
            }
        }
    }

    // Check FOR16 for 16-bit data clustered around a mean (sensor data)
    // Try before delta16 since FOR works better for non-sequential clustered values
    if ((n % 2) == 0 && n >= 128 && is_for16_candidate(data, n)) {
        size_t packed_size = for16_encode(temp, data, n);
        if (packed_size < n * 0.80) {  // Need 20% compression to justify overhead
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::FOR16;
            }
        }
    }

    // Check STRIDED32 for interleaved 2-channel 32-bit data (GPS int32 lat/lon)
    // Always check unconditionally - GPS data won't trigger XOR_FLOAT64 but benefits greatly
    // Strided produces more regular patterns that zstd compresses better
    if ((n % 8) == 0 && n >= 256 && is_strided32_candidate(data, n)) {
        strided32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy && entropy < result.original_entropy * 0.65) {
            best_entropy = entropy;
            best_strategy = Strategy::STRIDED32;
        }

        // Check if STRIDED32_FOR (with bit-packing) is even better
        if (is_strided32_for_candidate(data, n)) {
            size_t for_size = strided32_for_encode(temp, data, n);
            // FOR produces smaller output - estimate equivalent entropy
            double effective_entropy = 8.0 * (double)for_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::STRIDED32_FOR;
            }
        }
    }

    // Check STRIDED64 for interleaved 2-channel 64-bit data (GPS lat/lon doubles)
    // This is the primary case for GPS coordinates
    if ((n % 16) == 0 && n >= 256 && is_strided64_candidate(data, n)) {
        strided64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::STRIDED64;
        }
    }

    // Check STRIDED3_32 for 3-channel interleaved 32-bit data (XYZ vertex, accelerometer, RGB)
    if ((n % 12) == 0 && n >= 256 && is_strided3_32_candidate(data, n)) {
        strided3_32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::STRIDED3_32;
        }
    }

    // Check STRIDED4_32 for 4-channel interleaved 32-bit data (RGBA, quaternions)
    if ((n % 16) == 0 && n >= 256 && is_strided4_32_candidate(data, n)) {
        strided4_32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::STRIDED4_32;
        }
    }

    // Check BYTE_SHUFFLE32 for generic 32-bit data (floats, ints)
    // Blosc2-style: cluster similar bytes together, helps with uniform-magnitude data
    if ((n % 4) == 0 && n >= 64) {
        byte_shuffle32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 10% entropy reduction to justify byte shuffle
        if (entropy < best_entropy * 0.90) {
            best_entropy = entropy;
            best_strategy = Strategy::BYTE_SHUFFLE32;
        }
    }

    // Check BYTE_SHUFFLE64 for generic 64-bit data (doubles)
    if ((n % 8) == 0 && n >= 64) {
        byte_shuffle64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 10% entropy reduction to justify byte shuffle
        if (entropy < best_entropy * 0.90) {
            best_entropy = entropy;
            best_strategy = Strategy::BYTE_SHUFFLE64;
        }
    }

    // Check BYTE_SHUFFLE2 for 2-byte alternating patterns (protobuf tag/value)
    // Key insight: if even/odd bytes have very different entropy, splitting helps
    if ((n % 2) == 0 && n >= 64 && is_byte_shuffle2_candidate(data, n)) {
        byte_shuffle2_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 5% entropy reduction (splitting is very cheap)
        if (entropy < best_entropy * 0.95) {
            best_entropy = entropy;
            best_strategy = Strategy::BYTE_SHUFFLE2;
        }
    }

    // Check PAETH_RGB for image-like data (RGB rows with correlation)
    size_t paeth_width = detect_paeth_row_width(data, n);
    if (paeth_width > 0) {
        paeth_rgb_encode_with_width(temp, data, n, paeth_width);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        // Require 15% entropy reduction to justify Paeth (+2 byte overhead)
        if (entropy < best_entropy * 0.85) {
            best_entropy = entropy;
            best_strategy = Strategy::PAETH_RGB;
        }
    }

    if (best_strategy != Strategy::NONE) {
        result.strategy = best_strategy;
        result.confidence = 1.0 - (best_entropy / result.original_entropy);
        result.preprocessed_entropy = best_entropy;
    }

    if (allocated) delete[] temp;

    // Check RLE candidacy (only if delta didn't win)
    if (result.strategy == Strategy::NONE) {
        double avg_run = avg_run_length(data, n);
        if (avg_run > 8.0) {
            result.strategy = Strategy::RLE;
            result.confidence = std::min(1.0, (avg_run - 8.0) / 16.0);
        }
    }

    return result;
}

// Strategy name for debugging
inline const char* strategy_name(Strategy s) {
    switch (s) {
        case Strategy::NONE: return "NONE";
        case Strategy::DELTA: return "DELTA";
        case Strategy::DELTA16: return "DELTA16";
        case Strategy::DELTA32: return "DELTA32";
        case Strategy::DELTA64: return "DELTA64";
        case Strategy::DELTA_OF_DELTA32: return "DOD32";
        case Strategy::XOR_FLOAT64: return "XOR64";
        case Strategy::XOR_FLOAT32: return "XOR32";
        case Strategy::DELTA_ZIGZAG_BITPACK32: return "DZBP32";
        case Strategy::DELTA_FOR_BITPACK32: return "DFBP32";
        case Strategy::ALP_FLOAT64: return "ALP64";
        case Strategy::ALP_FLOAT32: return "ALP32";
        case Strategy::FOR16: return "FOR16";
        case Strategy::NARROW16: return "NARR16";
        case Strategy::STRIDED32: return "STRD32";
        case Strategy::STRIDED32_FOR: return "STRD32F";
        case Strategy::STRIDED64: return "STRD64";
        case Strategy::STRIDED3_32: return "STR3_32";
        case Strategy::STRIDED4_32: return "STR4_32";
        case Strategy::BYTE_SHUFFLE32: return "BSHF32";
        case Strategy::BYTE_SHUFFLE64: return "BSHF64";
        case Strategy::BYTE_SHUFFLE2: return "BSHF2";
        case Strategy::RLE: return "RLE";
        case Strategy::BLOCK_COLUMNAR: return "BLKCOL";
        case Strategy::E8E9_X86: return "E8E9";
        case Strategy::LINEAR_PRED16: return "LPRED16";
        case Strategy::LINEAR_PRED32: return "LPRED32";
        case Strategy::PAETH_RGB: return "PAETH";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// EXPERIMENTAL DETECTION (Dec 2025)
//
// RESULT: Original detect() wins. These experiments performed worse.
//
// detect_v2: Tested actual zstd compression instead of entropy.
//   Problem: Sample-based compression testing less reliable than entropy.
//   Result: 8% worse total compression vs original detect().
//
// detect_strict: Raised thresholds from 0.60 to 0.75.
//   Problem: Stricter thresholds cause false negatives (DOD32 missed).
//   Result: 1.3% worse total compression vs original detect().
//
// LEARNINGS (added to LOGIC.md/LOGICS.md):
// - "Strict beats fuzzy" applies to PRESENCE detection, not COMPARATIVE detection
// - "Entropy != compression" is XOR-specific; Delta/ALP entropy is valid
// - Original detect() already handles XOR correctly (25% entropy reduction required)
// ============================================================================

#ifdef TIEREDCOMPRESS_ZSTD_AWARE
// Only available when zstd.h is included before this header
// Usage: #include "zstd.h"
//        #define TIEREDCOMPRESS_ZSTD_AWARE
//        #include "tieredcompress.hpp"

// Test actual zstd compressed size on a sample (4KB)
// Returns compressed size
inline size_t test_compression_size(const uint8_t* data, size_t n, uint8_t* work, int level = 1) {
    size_t sample_size = std::min(n, (size_t)4096);
    size_t bound = ZSTD_compressBound(sample_size);
    return ZSTD_compress(work, bound, data, sample_size, level);
}

// Detect optimal strategy using ACTUAL compression, not entropy
// This is the improved detection that follows LOGIC.md:
// - "Measure final output" not entropy
// - "Strict beats fuzzy" - higher thresholds
inline DetectionResult detect_v2(const uint8_t* data, size_t n, uint8_t* work_buffer = nullptr, int zstd_level = 1) {
    DetectionResult result;
    result.strategy = Strategy::NONE;
    result.confidence = 0.0;
    result.original_entropy = byte_entropy(data, std::min(n, (size_t)4096));
    result.preprocessed_entropy = result.original_entropy;

    // Allocate working memory: preprocessed data + compression output
    size_t work_size = n + ZSTD_compressBound(std::min(n, (size_t)4096));
    uint8_t* temp = work_buffer;
    uint8_t* comp_buf = nullptr;
    bool allocated = false;
    if (!temp) {
        temp = new uint8_t[work_size];
        allocated = true;
    }
    comp_buf = temp + n;  // Second half for compression testing

    // Get baseline: raw data compressed size
    size_t sample_size = std::min(n, (size_t)4096);
    size_t baseline_size = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                          data, sample_size, zstd_level);

    size_t best_size = baseline_size;
    Strategy best_strategy = Strategy::NONE;

    // STRICT THRESHOLD: 0.75 (must reduce compressed size by 25%+)
    // This prevents false positives that hurt compression
    const double STRICT_THRESHOLD = 0.75;

    // === 64-bit patterns (most specific first) ===

    // Check 64-bit delta (database IDs)
    if (is_delta64_candidate(data, n, STRICT_THRESHOLD)) {
        delta64_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size * 0.85) {  // Need 15% improvement
            best_size = csize;
            best_strategy = Strategy::DELTA64;
        }
    }

    // Check ALP for 64-bit decimal floats
    int alp_exp64 = is_decimal_float64_candidate(data, n);
    if (alp_exp64 >= 0) {
        size_t alp_size = alp_float64_encode(temp, data, n);
        if (alp_size < n * 0.80) {
            // Test compressed size of ALP output
            size_t alp_sample = std::min(alp_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(alp_sample),
                                          temp, alp_sample, zstd_level);
            // Scale to compare fairly
            double effective_size = (double)csize * n / alp_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::ALP_FLOAT64;
            }
        }
    }

    // Check XOR for 64-bit floats - ONLY if passes strict test
    // XOR is the main source of false positives (entropy ↓ but compression ↑)
    if (is_xor_float64_candidate(data, n, 0.80)) {  // Even stricter: 80% threshold
        xor_float64_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        // CRITICAL: Must actually reduce compressed size, not just entropy
        if (csize < best_size * 0.85) {
            best_size = csize;
            best_strategy = Strategy::XOR_FLOAT64;
        }
    }

    // === 32-bit patterns ===

    // Check delta-of-delta first (most specific 32-bit pattern)
    if (is_delta_of_delta32_candidate(data, n, STRICT_THRESHOLD)) {
        delta_of_delta32_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size) {
            best_size = csize;
            best_strategy = Strategy::DELTA_OF_DELTA32;
        }
    }

    // Check 32-bit delta
    if (is_delta32_candidate(data, n, STRICT_THRESHOLD)) {
        delta32_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size) {
            best_size = csize;
            best_strategy = Strategy::DELTA32;
        }
    }

    // Check Delta + ZigZag + BitPack
    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.60)) {
        size_t packed_size = delta_zigzag_bitpack32_encode(temp, data, n);
        if (packed_size < n * 0.70) {
            size_t pack_sample = std::min(packed_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(pack_sample),
                                          temp, pack_sample, zstd_level);
            double effective_size = (double)csize * n / packed_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::DELTA_ZIGZAG_BITPACK32;
            }
        }
    }

    // Check Delta + FOR + BitPack
    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.60)) {
        size_t packed_size = delta_for_bitpack32_encode(temp, data, n);
        if (packed_size < n * 0.70) {
            size_t pack_sample = std::min(packed_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(pack_sample),
                                          temp, pack_sample, zstd_level);
            double effective_size = (double)csize * n / packed_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::DELTA_FOR_BITPACK32;
            }
        }
    }

    // Check ALP for 32-bit decimal floats
    int alp_exp32 = is_decimal_float32_candidate(data, n);
    if (alp_exp32 >= 0) {
        size_t alp_size = alp_float32_encode(temp, data, n);
        if (alp_size < n * 0.80) {
            size_t alp_sample = std::min(alp_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(alp_sample),
                                          temp, alp_sample, zstd_level);
            double effective_size = (double)csize * n / alp_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::ALP_FLOAT32;
            }
        }
    }

    // Check XOR for 32-bit floats - strict threshold
    if (best_strategy != Strategy::ALP_FLOAT32 && is_xor_float32_candidate(data, n, 0.80)) {
        xor_float32_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size * 0.85) {
            best_size = csize;
            best_strategy = Strategy::XOR_FLOAT32;
        }
    }

    // Check STRIDED32 for interleaved data
    if ((n % 8) == 0 && n >= 256 && is_strided32_candidate(data, n)) {
        strided32_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size) {
            best_size = csize;
            best_strategy = Strategy::STRIDED32;
        }

        // Check if STRIDED32_FOR (with bit-packing) is even better
        if (is_strided32_for_candidate(data, n)) {
            size_t for_size = strided32_for_encode(temp, data, n);
            // FOR encoding produces smaller output, compress that
            size_t for_csize = ZSTD_compress(comp_buf, ZSTD_compressBound(for_size),
                                              temp, for_size, zstd_level);
            if (for_csize < best_size) {
                best_size = for_csize;
                best_strategy = Strategy::STRIDED32_FOR;
            }
        }
    }

    // === 16-bit patterns ===

    // Check 16-bit delta
    if (is_delta16_candidate(data, n, STRICT_THRESHOLD)) {
        delta16_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size) {
            best_size = csize;
            best_strategy = Strategy::DELTA16;
        }
    }


    // Check NARROW16 for 16-bit data with range ≤255
    if ((n % 2) == 0 && n >= 128 && is_narrow16_candidate(data, n)) {
        size_t packed_size = narrow16_encode(temp, data, n);
        if (packed_size < n * 0.60) {
            size_t pack_sample = std::min(packed_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(pack_sample),
                                          temp, pack_sample, zstd_level);
            double effective_size = (double)csize * n / packed_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::NARROW16;
            }
        }
    }

    // Check FOR16 for clustered 16-bit values
    if ((n % 2) == 0 && n >= 128 && is_for16_candidate(data, n)) {
        size_t packed_size = for16_encode(temp, data, n);
        if (packed_size < n * 0.80) {
            size_t pack_sample = std::min(packed_size, (size_t)4096);
            size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(pack_sample),
                                          temp, pack_sample, zstd_level);
            double effective_size = (double)csize * n / packed_size;
            if (effective_size < best_size) {
                best_size = (size_t)effective_size;
                best_strategy = Strategy::FOR16;
            }
        }
    }

    // === Byte-level patterns ===

    // Check byte-level delta
    if (is_delta_candidate(data, n, STRICT_THRESHOLD)) {
        delta_encode(temp, data, n);
        size_t csize = ZSTD_compress(comp_buf, ZSTD_compressBound(sample_size),
                                      temp, sample_size, zstd_level);
        if (csize < best_size) {
            best_size = csize;
            best_strategy = Strategy::DELTA;
        }
    }

    // Set result
    if (best_strategy != Strategy::NONE) {
        result.strategy = best_strategy;
        result.confidence = 1.0 - (double)best_size / baseline_size;
        result.preprocessed_entropy = 8.0 * best_size / sample_size;  // Approximation
    }

    // Check RLE as last resort
    if (result.strategy == Strategy::NONE) {
        double avg_run = avg_run_length(data, n);
        if (avg_run > 8.0) {
            result.strategy = Strategy::RLE;
            result.confidence = std::min(1.0, (avg_run - 8.0) / 16.0);
        }
    }

    if (allocated) delete[] temp;
    return result;
}

#endif // TIEREDCOMPRESS_ZSTD_AWARE

// ============================================================================
// Improved detection WITHOUT zstd dependency
// Uses stricter thresholds based on LOGIC.md learnings
// ============================================================================

inline DetectionResult detect_strict(const uint8_t* data, size_t n, uint8_t* work_buffer = nullptr) {
    DetectionResult result;
    result.original_entropy = byte_entropy(data, std::min(n, (size_t)4096));
    result.strategy = Strategy::NONE;
    result.confidence = 0.0;
    result.preprocessed_entropy = result.original_entropy;

    uint8_t* temp = work_buffer;
    bool allocated = false;
    if (!temp) {
        temp = new uint8_t[n + 65542];  // Extra space for block_columnar (block_len + 6 header bytes)
        allocated = true;
    }

    double best_entropy = result.original_entropy;
    Strategy best_strategy = Strategy::NONE;

    // STRICT THRESHOLD: 0.75 instead of 0.60
    // "Strict beats fuzzy" - LOGIC.md
    const double STRICT_THRESHOLD = 0.75;

    // Check 64-bit delta (database IDs) - most specific
    if (is_delta64_candidate(data, n, STRICT_THRESHOLD)) {
        delta64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy * 0.75) {  // Require 25% reduction
            best_entropy = entropy;
            best_strategy = Strategy::DELTA64;
        }
    }

    // Check delta-of-delta for regular interval timestamps
    if (is_delta_of_delta32_candidate(data, n, STRICT_THRESHOLD)) {
        delta_of_delta32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA_OF_DELTA32;
        }
    }

    // Check 32-bit delta (timestamps, prices)
    if (is_delta32_candidate(data, n, STRICT_THRESHOLD)) {
        delta32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA32;
        }
    }

    // Check 16-bit delta (sensors, audio)
    if (is_delta16_candidate(data, n, STRICT_THRESHOLD)) {
        delta16_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA16;
        }
    }

    // Check byte-level delta
    if (is_delta_candidate(data, n, STRICT_THRESHOLD)) {
        delta_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy) {
            best_entropy = entropy;
            best_strategy = Strategy::DELTA;
        }
    }

    // XOR encoding: VERY STRICT because entropy is misleading here
    // "entropy ↓ = compression ↑" is FALSE for XOR - LOGIC.md Surprise #1
    // Require 40% entropy reduction (was 25%) to compensate for unreliable proxy
    if (is_xor_float64_candidate(data, n, 0.85)) {  // Raised from 0.70
        xor_float64_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy * 0.60) {  // Require 40% reduction (was 25%)
            best_entropy = entropy;
            best_strategy = Strategy::XOR_FLOAT64;
        }
    }

    // ALP strategies use actual size, not entropy - these are good
    int alp_exp64 = is_decimal_float64_candidate(data, n);
    if (alp_exp64 >= 0) {
        size_t alp_size = alp_float64_encode(temp, data, n);
        if (alp_size < n * 0.75) {  // Stricter: 25% reduction (was 20%)
            double effective_entropy = 8.0 * alp_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::ALP_FLOAT64;
            }
        }
    }

    int alp_exp32 = is_decimal_float32_candidate(data, n);
    if (alp_exp32 >= 0) {
        size_t alp_size = alp_float32_encode(temp, data, n);
        if (alp_size < n * 0.75) {
            double effective_entropy = 8.0 * alp_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::ALP_FLOAT32;
            }
        }
    }

    // XOR32: same strict treatment
    if (best_strategy != Strategy::ALP_FLOAT32 && is_xor_float32_candidate(data, n, 0.85)) {
        xor_float32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy * 0.60) {
            best_entropy = entropy;
            best_strategy = Strategy::XOR_FLOAT32;
        }
    }

    // BitPack strategies use actual size - keep reasonable thresholds
    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.60)) {
        size_t packed_size = delta_zigzag_bitpack32_encode(temp, data, n);
        if (packed_size < n * 0.65) {  // Stricter: 35% reduction (was 30%)
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::DELTA_ZIGZAG_BITPACK32;
            }
        }
    }

    if ((n % 4) == 0 && n >= 256 && is_delta32_candidate(data, n, 0.60)) {
        size_t packed_size = delta_for_bitpack32_encode(temp, data, n);
        if (packed_size < n * 0.65) {
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::DELTA_FOR_BITPACK32;
            }
        }
    }

    // FOR16
    if ((n % 2) == 0 && n >= 128 && is_for16_candidate(data, n)) {
        size_t packed_size = for16_encode(temp, data, n);
        if (packed_size < n * 0.75) {
            double effective_entropy = 8.0 * packed_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::FOR16;
            }
        }
    }

    // STRIDED32 - check if beats current best
    if ((n % 8) == 0 && n >= 256 && is_strided32_candidate(data, n)) {
        strided32_encode(temp, data, n);
        double entropy = byte_entropy(temp, std::min(n, (size_t)4096));
        if (entropy < best_entropy && entropy < result.original_entropy * 0.60) {
            best_entropy = entropy;
            best_strategy = Strategy::STRIDED32;
        }

        // Check if STRIDED32_FOR (with bit-packing) is even better
        if (is_strided32_for_candidate(data, n)) {
            size_t for_size = strided32_for_encode(temp, data, n);
            // FOR produces smaller output - estimate equivalent entropy
            double effective_entropy = 8.0 * (double)for_size / n;
            if (effective_entropy < best_entropy) {
                best_entropy = effective_entropy;
                best_strategy = Strategy::STRIDED32_FOR;
            }
        }
    }

    if (best_strategy != Strategy::NONE) {
        result.strategy = best_strategy;
        result.confidence = 1.0 - (best_entropy / result.original_entropy);
        result.preprocessed_entropy = best_entropy;
    }

    if (allocated) delete[] temp;

    // Check RLE candidacy (only if nothing else won)
    if (result.strategy == Strategy::NONE) {
        double avg_run = avg_run_length(data, n);
        if (avg_run > 8.0) {
            result.strategy = Strategy::RLE;
            result.confidence = std::min(1.0, (avg_run - 8.0) / 16.0);
        }
    }

    return result;
}

} // namespace tieredcompress
