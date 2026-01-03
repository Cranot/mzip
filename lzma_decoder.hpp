// LZMA Decoder - Simple standalone decoder for LZMA streams
// Compatible with output from lzma_optimal2.hpp encoder
#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace lzma_dec {

constexpr uint32_t kNumStates = 12;
constexpr uint32_t kNumPosSlotBits = 6;
constexpr uint32_t kNumLenToPosStates = 4;
constexpr uint32_t kEndPosModelIndex = 14;
constexpr uint32_t kNumFullDistances = 1 << (kEndPosModelIndex / 2);
constexpr uint32_t kMatchMinLen = 2;
constexpr uint32_t kNumAlignBits = 4;

constexpr uint32_t kTopValue = 1 << 24;
constexpr uint32_t kNumBitModelTotalBits = 11;
constexpr uint32_t kBitModelTotal = 1 << kNumBitModelTotalBits;
constexpr uint32_t kNumMoveBits = 5;
constexpr uint32_t kProbInitValue = kBitModelTotal / 2;

constexpr uint8_t kLiteralNextStates[kNumStates] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5};
constexpr uint8_t kMatchNextStates[kNumStates] = {7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10};
constexpr uint8_t kRepNextStates[kNumStates] = {8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11};
constexpr uint8_t kShortRepNextStates[kNumStates] = {9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11};

struct Prob {
    uint16_t p = kProbInitValue;
    void update0() { p += (kBitModelTotal - p) >> kNumMoveBits; }
    void update1() { p -= p >> kNumMoveBits; }
};

class RangeDecoder {
public:
    const uint8_t* in_;
    size_t in_pos_;
    size_t in_size_;
    uint32_t range_;
    uint32_t code_;

    RangeDecoder(const uint8_t* data, size_t size)
        : in_(data), in_pos_(0), in_size_(size), range_(0xFFFFFFFF), code_(0) {
        // Initialize: read 5 bytes (first byte is ignored per LZMA spec)
        if (in_size_ < 5) throw std::runtime_error("LZMA: input too short");
        in_pos_++;  // Skip first byte
        for (int i = 0; i < 4; i++) {
            code_ = (code_ << 8) | in_[in_pos_++];
        }
    }

    void normalize() {
        while (range_ < kTopValue) {
            range_ <<= 8;
            code_ = (code_ << 8) | (in_pos_ < in_size_ ? in_[in_pos_++] : 0);
        }
    }

    uint32_t decode_bit(Prob& prob) {
        uint32_t bound = (range_ >> kNumBitModelTotalBits) * prob.p;
        uint32_t bit;
        if (code_ < bound) {
            range_ = bound;
            prob.update0();
            bit = 0;
        } else {
            code_ -= bound;
            range_ -= bound;
            prob.update1();
            bit = 1;
        }
        normalize();
        return bit;
    }

    uint32_t decode_direct_bits(int num_bits) {
        uint32_t result = 0;
        for (int i = 0; i < num_bits; i++) {
            range_ >>= 1;
            code_ -= range_;
            uint32_t t = 0 - ((uint32_t)code_ >> 31);  // Must be uint32_t, not int32_t!
            code_ += range_ & t;
            result = (result << 1) | (1 & ~t);  // ~t is 0xFFFFFFFF or 0
            normalize();
        }
        return result;
    }
};

template<int N>
struct BitTree {
    Prob probs[1 << N];

    uint32_t decode(RangeDecoder& dec) {
        uint32_t m = 1;
        for (int i = 0; i < N; i++) {
            m = (m << 1) | dec.decode_bit(probs[m]);
        }
        return m - (1 << N);
    }

    uint32_t decode_reverse(RangeDecoder& dec) {
        uint32_t m = 1;
        uint32_t symbol = 0;
        for (int i = 0; i < N; i++) {
            uint32_t bit = dec.decode_bit(probs[m]);
            m = (m << 1) | bit;
            symbol |= bit << i;
        }
        return symbol;
    }

    static uint32_t decode_reverse_raw(RangeDecoder& dec, Prob* probs_base, int num_bits) {
        uint32_t m = 1;
        uint32_t symbol = 0;
        for (int i = 0; i < num_bits; i++) {
            uint32_t bit = dec.decode_bit(probs_base[m]);
            m = (m << 1) | bit;
            symbol |= bit << i;
        }
        return symbol;
    }
};

struct MatchedLiteralDecoder {
    Prob probs[0x300];

    uint8_t decode(RangeDecoder& dec) {
        uint32_t m = 1;
        for (int i = 0; i < 8; i++) {
            m = (m << 1) | dec.decode_bit(probs[m]);
        }
        return m & 0xFF;
    }

    uint8_t decode_matched(RangeDecoder& dec, uint8_t match_byte) {
        uint32_t m = 1;
        for (int i = 7; i >= 0; i--) {
            uint32_t match_bit = (match_byte >> i) & 1;
            uint32_t ctx = 0x100 + (match_bit << 8) + m;
            uint32_t bit = dec.decode_bit(probs[ctx]);
            m = (m << 1) | bit;
            if (match_bit != bit) {
                // Switch to normal decoding
                while (i-- > 0) {
                    m = (m << 1) | dec.decode_bit(probs[m]);
                }
                break;
            }
        }
        return m & 0xFF;
    }
};

struct LenDecoder {
    Prob choice;
    Prob choice2;
    BitTree<3> low[16];
    BitTree<3> mid[16];
    BitTree<8> high;

    uint32_t decode(RangeDecoder& dec, uint32_t pos_state) {
        if (dec.decode_bit(choice) == 0) {
            return low[pos_state].decode(dec);
        } else if (dec.decode_bit(choice2) == 0) {
            return 8 + mid[pos_state].decode(dec);
        } else {
            return 16 + high.decode(dec);
        }
    }
};

struct LzmaProbs {
    Prob is_match[kNumStates][16];
    Prob is_rep[kNumStates];
    Prob is_rep0[kNumStates];
    Prob is_rep0_long[kNumStates][16];
    Prob is_rep1[kNumStates];
    Prob is_rep2[kNumStates];
    BitTree<kNumPosSlotBits> pos_slot[kNumLenToPosStates];
    Prob pos_decoders[kNumFullDistances - kEndPosModelIndex];
    BitTree<kNumAlignBits> align_decoder;
    LenDecoder len_decoder;
    LenDecoder rep_len_decoder;
    MatchedLiteralDecoder literal[8];
};

// Decompress LZMA stream
// Input: raw LZMA data (13-byte header + compressed data)
// Output: decompressed data
inline std::vector<uint8_t> decompress(const uint8_t* data, size_t size) {
    if (size < 13) throw std::runtime_error("LZMA: input too short for header");

    // Parse header
    uint8_t props = data[0];
    uint8_t lc = props % 9;
    uint8_t remainder = props / 9;
    uint8_t lp = remainder % 5;
    uint8_t pb = remainder / 5;

    if (pb > 4) throw std::runtime_error("LZMA: invalid pb");

    uint32_t dict_size = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
    uint64_t uncompressed_size = 0;
    for (int i = 0; i < 8; i++) {
        uncompressed_size |= (uint64_t)data[5 + i] << (i * 8);
    }

    uint32_t pos_state_mask = (1 << pb) - 1;

    // Initialize decoder
    RangeDecoder dec(data + 13, size - 13);
    LzmaProbs probs;

    std::vector<uint8_t> out;
    if (uncompressed_size != UINT64_MAX) {
        out.reserve(uncompressed_size);
    }

    uint32_t state = 0;
    uint32_t reps[4] = {1, 1, 1, 1};  // 1-based internally

    while (out.size() < uncompressed_size) {
        size_t pos = out.size();
        uint32_t pos_state = pos & pos_state_mask;

        if (dec.decode_bit(probs.is_match[state][pos_state]) == 0) {
            // Literal
            uint8_t prev = pos > 0 ? out[pos - 1] : 0;
            uint8_t ctx = prev >> (8 - lc);  // Use lc bits from previous byte

            uint8_t byte;
            if (state >= 7 && reps[0] > 0 && pos >= reps[0]) {
                uint8_t match_byte = out[pos - reps[0]];
                byte = probs.literal[ctx].decode_matched(dec, match_byte);
            } else {
                byte = probs.literal[ctx].decode(dec);
            }
            out.push_back(byte);
            state = kLiteralNextStates[state];
        } else {
            // Match
            uint32_t len;
            uint32_t dist;

            if (dec.decode_bit(probs.is_rep[state]) == 0) {
                // New match
                len = probs.len_decoder.decode(dec, pos_state) + kMatchMinLen;

                uint32_t len_state = std::min(len - 2, kNumLenToPosStates - 1);
                uint32_t pos_slot = probs.pos_slot[len_state].decode(dec);

                if (pos_slot < 4) {
                    dist = pos_slot;
                } else {
                    uint32_t num_direct = (pos_slot >> 1) - 1;
                    uint32_t base = (2 | (pos_slot & 1)) << num_direct;

                    if (pos_slot < kEndPosModelIndex) {
                        dist = base + BitTree<8>::decode_reverse_raw(
                            dec, probs.pos_decoders + base - pos_slot - 1, num_direct);
                    } else {
                        uint32_t direct = dec.decode_direct_bits(num_direct - kNumAlignBits);
                        uint32_t align = probs.align_decoder.decode_reverse(dec);
                        dist = base + (direct << kNumAlignBits) + align;
                    }
                }
                dist++;  // Convert to 1-based

                // Update reps
                reps[3] = reps[2];
                reps[2] = reps[1];
                reps[1] = reps[0];
                reps[0] = dist;
                state = kMatchNextStates[state];
            } else {
                // Rep match
                if (dec.decode_bit(probs.is_rep0[state]) == 0) {
                    if (dec.decode_bit(probs.is_rep0_long[state][pos_state]) == 0) {
                        // Short rep0
                        if (reps[0] > pos) throw std::runtime_error("LZMA: invalid rep distance");
                        out.push_back(out[pos - reps[0]]);
                        state = kShortRepNextStates[state];
                        continue;
                    }
                    // Long rep0
                    dist = reps[0];
                } else if (dec.decode_bit(probs.is_rep1[state]) == 0) {
                    dist = reps[1];
                    reps[1] = reps[0];
                    reps[0] = dist;
                } else if (dec.decode_bit(probs.is_rep2[state]) == 0) {
                    dist = reps[2];
                    reps[2] = reps[1];
                    reps[1] = reps[0];
                    reps[0] = dist;
                } else {
                    dist = reps[3];
                    reps[3] = reps[2];
                    reps[2] = reps[1];
                    reps[1] = reps[0];
                    reps[0] = dist;
                }

                len = probs.rep_len_decoder.decode(dec, pos_state) + kMatchMinLen;
                state = kRepNextStates[state];
            }

            // Copy match
            if (reps[0] > out.size()) {
                throw std::runtime_error("LZMA: rep distance exceeds output size");
            }
            for (uint32_t i = 0; i < len; i++) {
                out.push_back(out[out.size() - reps[0]]);
            }
        }
    }

    return out;
}

// Convenience: decompress with size check
inline std::vector<uint8_t> decompress_safe(const uint8_t* data, size_t size) {
    try {
        return decompress(data, size);
    } catch (const std::exception& e) {
        return {};  // Return empty on error
    }
}

} // namespace lzma_dec
