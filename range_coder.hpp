// Range Coder - LZMA-style adaptive binary entropy coding
// Based on LZMA specification and TinyZZZ implementation
#pragma once
#include <cstdint>
#include <vector>

namespace rc {

constexpr uint32_t kTopValue = 1 << 24;
constexpr uint32_t kNumBitModelTotalBits = 11;
constexpr uint32_t kBitModelTotal = 1 << kNumBitModelTotalBits;  // 2048
constexpr uint32_t kNumMoveBits = 5;
constexpr uint32_t kProbInitValue = kBitModelTotal / 2;  // 1024 = 50%

// Probability model for a single bit
struct Prob {
    uint16_t p = kProbInitValue;

    void update0() { p += (kBitModelTotal - p) >> kNumMoveBits; }  // Saw 0
    void update1() { p -= p >> kNumMoveBits; }                      // Saw 1
};

// Range Encoder
class Encoder {
private:
    std::vector<uint8_t>& out_;
    uint64_t low_ = 0;
    uint32_t range_ = 0xFFFFFFFF;
    uint8_t cache_ = 0;
    uint32_t cache_size_ = 1;

    void shift_low() {
        if ((uint32_t)low_ < 0xFF000000 || (low_ >> 32) != 0) {
            uint8_t temp = cache_;
            do {
                uint8_t byte = temp + (uint8_t)(low_ >> 32);
#ifdef DEBUG_RC
                if (out_.size() < 20) {
                    fprintf(stderr, "RC: push 0x%02x (temp=%02x, carry=%02x)\n",
                            byte, temp, (uint8_t)(low_ >> 32));
                }
#endif
                out_.push_back(byte);
                temp = 0xFF;
            } while (--cache_size_ != 0);
            cache_ = (uint8_t)(low_ >> 24);
        }
        cache_size_++;
        low_ = (uint32_t)(low_ << 8);
    }

public:
    Encoder(std::vector<uint8_t>& output) : out_(output) {}

    // Encode a bit with probability model
    void encode_bit(Prob& prob, uint32_t bit) {
        uint32_t bound = (range_ >> kNumBitModelTotalBits) * prob.p;
        if (bit == 0) {
            range_ = bound;
            prob.update0();
        } else {
            low_ += bound;
            range_ -= bound;
            prob.update1();
        }

        // Normalize
        while (range_ < kTopValue) {
            range_ <<= 8;
            shift_low();
        }
    }

    // Encode a bit with fixed probability (0.5)
    void encode_bit_equal(uint32_t bit) {
        range_ >>= 1;
        if (bit) {
            low_ += range_;
        }
        while (range_ < kTopValue) {
            range_ <<= 8;
            shift_low();
        }
    }

    // Encode N bits directly (no probability model)
    void encode_direct_bits(uint32_t value, int num_bits) {
        for (int i = num_bits - 1; i >= 0; i--) {
            range_ >>= 1;
            if ((value >> i) & 1) {
                low_ += range_;
            }
            while (range_ < kTopValue) {
                range_ <<= 8;
                shift_low();
            }
        }
    }

    // Flush remaining data
    void finish() {
        #ifdef DEBUG_RC_FINISH
        fprintf(stderr, "finish() start: low_=0x%llX range_=0x%08X cache_=0x%02X cache_size_=%u out_size=%zu\n",
                (unsigned long long)low_, range_, cache_, cache_size_, out_.size());
        #endif
        for (int i = 0; i < 5; i++) {
            shift_low();
            #ifdef DEBUG_RC_FINISH
            fprintf(stderr, "  after shift %d: low_=0x%llX out_size=%zu\n",
                    i, (unsigned long long)low_, out_.size());
            #endif
        }
        #ifdef DEBUG_RC_FINISH
        fprintf(stderr, "finish() end: out_size=%zu\n", out_.size());
        #endif
    }
};

// Range Decoder
class Decoder {
private:
    const uint8_t* in_;
    size_t pos_;
    size_t size_;
    uint32_t range_ = 0xFFFFFFFF;
    uint32_t code_ = 0;

    uint8_t read_byte() {
        return (pos_ < size_) ? in_[pos_++] : 0;
    }

public:
    Decoder(const uint8_t* data, size_t size) : in_(data), pos_(0), size_(size) {
        // Initialize code from first 5 bytes
        code_ = 0;
        for (int i = 0; i < 5; i++) {
            code_ = (code_ << 8) | read_byte();
        }
    }

    // Decode a bit with probability model
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

        // Normalize
        while (range_ < kTopValue) {
            range_ <<= 8;
            code_ = (code_ << 8) | read_byte();
        }

        return bit;
    }

    // Decode a bit with fixed probability (0.5)
    uint32_t decode_bit_equal() {
        range_ >>= 1;
        uint32_t bit = (code_ >= range_) ? 1 : 0;
        if (bit) {
            code_ -= range_;
        }
        while (range_ < kTopValue) {
            range_ <<= 8;
            code_ = (code_ << 8) | read_byte();
        }
        return bit;
    }

    // Decode N bits directly
    uint32_t decode_direct_bits(int num_bits) {
        uint32_t result = 0;
        for (int i = 0; i < num_bits; i++) {
            range_ >>= 1;
            code_ -= range_;
            uint32_t t = 0 - ((uint32_t)code_ >> 31);
            code_ += range_ & t;
            result = (result << 1) | (1 - t);
            while (range_ < kTopValue) {
                range_ <<= 8;
                code_ = (code_ << 8) | read_byte();
            }
        }
        return result;
    }

    bool is_finished() const { return pos_ >= size_; }
};

// Bit tree encoder/decoder for multi-bit symbols
template<int NumBits>
struct BitTree {
    Prob probs[1 << NumBits];  // Will be 0x300 for matched literals

    // Helper to access probs with range checking in debug
    Prob& prob_at(size_t idx) { return probs[idx]; }

    void encode(Encoder& enc, uint32_t symbol) {
        uint32_t m = 1;
        for (int i = NumBits - 1; i >= 0; i--) {
            uint32_t bit = (symbol >> i) & 1;
            enc.encode_bit(probs[m], bit);
            m = (m << 1) | bit;
        }
    }

    uint32_t decode(Decoder& dec) {
        uint32_t m = 1;
        for (int i = 0; i < NumBits; i++) {
            m = (m << 1) | dec.decode_bit(probs[m]);
        }
        return m - (1 << NumBits);
    }

    void encode_reverse(Encoder& enc, uint32_t symbol) {
        uint32_t m = 1;
        for (int i = 0; i < NumBits; i++) {
            uint32_t bit = symbol & 1;
            enc.encode_bit(probs[m], bit);
            m = (m << 1) | bit;
            symbol >>= 1;
        }
    }

    // Static helper: encode using a raw Prob array as a reverse bit tree
    // probs points to the base of the tree (probs[1] is root, probs[0] unused)
    static void encode_reverse_raw(Encoder& enc, Prob* probs, uint32_t num_bits, uint32_t symbol) {
        uint32_t m = 1;
        for (uint32_t i = 0; i < num_bits; i++) {
            uint32_t bit = symbol & 1;
            enc.encode_bit(probs[m], bit);
            m = (m << 1) | bit;
            symbol >>= 1;
        }
    }

    // LZMA matched literal encoding - proper bit-by-bit context encoding
    // Uses match_byte bits to select probability contexts
    // Requires probs array to have 0x300 (768) entries for full matched literal support
    void encode_matched(Encoder& enc, uint32_t symbol, uint32_t match_byte) {
        uint32_t m = 1;
        uint32_t match = match_byte;
        for (int i = 7; i >= 0; i--) {
            uint32_t match_bit = (match >> i) & 1;
            uint32_t bit = (symbol >> i) & 1;
            // Context includes match_bit: 0x100 + (match_bit << 8) + m
            uint32_t ctx = 0x100 + (match_bit << 8) + m;
            enc.encode_bit(probs[ctx], bit);
            m = (m << 1) | bit;
            if (match_bit != bit) {
                // After mismatch, continue with normal encoding
                for (i--; i >= 0; i--) {
                    bit = (symbol >> i) & 1;
                    enc.encode_bit(probs[m], bit);
                    m = (m << 1) | bit;
                }
                break;
            }
        }
    }

    uint32_t decode_reverse(Decoder& dec) {
        uint32_t m = 1;
        uint32_t result = 0;
        for (int i = 0; i < NumBits; i++) {
            uint32_t bit = dec.decode_bit(probs[m]);
            m = (m << 1) | bit;
            result |= bit << i;
        }
        return result;
    }
};

// Matched literal coder for LZMA - needs 0x300 (768) probabilities
// Normal literals use probs[1..255], matched use probs[0x100..(0x2FF)]
struct MatchedLiteralCoder {
    Prob probs[0x300];

    void encode(Encoder& enc, uint32_t symbol) {
        uint32_t m = 1;
        for (int i = 7; i >= 0; i--) {
            uint32_t bit = (symbol >> i) & 1;
            enc.encode_bit(probs[m], bit);
            m = (m << 1) | bit;
        }
    }

    void encode_matched(Encoder& enc, uint32_t symbol, uint32_t match_byte) {
        uint32_t m = 1;
        for (int i = 7; i >= 0; i--) {
            uint32_t match_bit = (match_byte >> i) & 1;
            uint32_t bit = (symbol >> i) & 1;
            // Context: 0x100 + (match_bit << 8) + m
            uint32_t ctx = 0x100 + (match_bit << 8) + m;
            enc.encode_bit(probs[ctx], bit);
            m = (m << 1) | bit;
            if (match_bit != bit) {
                // After mismatch, continue with normal encoding
                for (i--; i >= 0; i--) {
                    bit = (symbol >> i) & 1;
                    enc.encode_bit(probs[m], bit);
                    m = (m << 1) | bit;
                }
                break;
            }
        }
    }
};

// Length encoder/decoder (LZMA style)
struct LenCoder {
    Prob choice;      // 0 = short (0-7), 1 = longer
    Prob choice2;     // 0 = mid (8-15), 1 = long (16-271)
    BitTree<3> low[16];   // Short lengths by pos_state
    BitTree<3> mid[16];   // Mid lengths by pos_state
    BitTree<8> high;      // Long lengths

    void encode(Encoder& enc, uint32_t len, uint32_t pos_state) {
        if (len < 8) {
            enc.encode_bit(choice, 0);
            low[pos_state].encode(enc, len);
        } else if (len < 16) {
            enc.encode_bit(choice, 1);
            enc.encode_bit(choice2, 0);
            mid[pos_state].encode(enc, len - 8);
        } else {
            enc.encode_bit(choice, 1);
            enc.encode_bit(choice2, 1);
            high.encode(enc, len - 16);
        }
    }

    uint32_t decode(Decoder& dec, uint32_t pos_state) {
        if (dec.decode_bit(choice) == 0) {
            return low[pos_state].decode(dec);
        }
        if (dec.decode_bit(choice2) == 0) {
            return 8 + mid[pos_state].decode(dec);
        }
        return 16 + high.decode(dec);
    }
};

} // namespace rc
