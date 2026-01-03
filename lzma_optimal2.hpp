// LZMA Optimal Parser v2 - Multi-arrival dynamic programming
// Based on LZMA/Bulat forward parse with N arrivals per position
#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>
#include "range_coder.hpp"

namespace lzma_opt2 {

constexpr uint32_t kNumStates = 12;
constexpr uint32_t kNumPosSlotBits = 6;
constexpr uint32_t kNumLenToPosStates = 4;
constexpr uint32_t kEndPosModelIndex = 14;
constexpr uint32_t kNumFullDistances = 1 << (kEndPosModelIndex / 2);
constexpr uint32_t kMatchMinLen = 2;
constexpr uint32_t kNumAlignBits = 4;

constexpr uint8_t kLiteralNextStates[kNumStates] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5};
constexpr uint8_t kMatchNextStates[kNumStates] = {7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10};
constexpr uint8_t kRepNextStates[kNumStates] = {8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11};
constexpr uint8_t kShortRepNextStates[kNumStates] = {9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11};

// Price constants (1/8 bits) - tuned for optimal compression
// Key insight: prices must balance literal vs match decisions correctly.
// Too-cheap literals = miss matches. Too-expensive literals = take bad matches.
constexpr uint32_t kInfPrice = UINT32_MAX / 2;

// Literal pricing:
// - Text has ~4-5 bits entropy, binary can be 6-8 bits
// - After match (state >= 7), matched literal encoding is very efficient
// - Balance: favor matches for longer patterns, literals for short/poor matches
constexpr uint32_t kLitPrice = 52;         // ~6.5 bits (middle ground)
constexpr uint32_t kLitMatchedPrice = 36;  // ~4.5 bits (efficient after match)

// Rep match: very cheap since no distance encoding
// These prices should strongly favor rep matches
constexpr uint32_t kShortRep0Price = 16;   // ~2 bits: is_match + is_rep + is_rep0 + is_rep0_long
constexpr uint32_t kRep0Price = 20;        // ~2.5 bits base + len
constexpr uint32_t kRep1Price = 28;        // ~3.5 bits base + len
constexpr uint32_t kRep2Price = 36;        // ~4.5 bits base + len
constexpr uint32_t kRep3Price = 44;        // ~5.5 bits base + len

inline uint32_t len_price(uint32_t len) {
    // LZMA length encoding: choice bits + tree encoding
    // Calibrated to actual LZMA SDK encoding costs
    if (len <= 9) return 24 + (len - 2) * 3;   // 24-45 (low range: 8 lengths)
    if (len <= 17) return 36 + (len - 10) * 3; // 36-57 (mid range: 8 lengths)
    return 48 + (len - 18) / 4;  // 48+ (high range, slow growth for long matches)
}

inline uint32_t dist_price(uint32_t dist) {
    // LZMA distance encoding: pos_slot + extra bits
    if (dist < 4) return 24;       // pos_slot only (4 slots, 6 bits tree)
    if (dist < 8) return 32;       // 7 bits
    if (dist < 16) return 40;      // 8 bits
    if (dist < 32) return 48;      // 9 bits
    if (dist < 64) return 56;      // 10 bits
    if (dist < 128) return 64;     // 11 bits
    if (dist < 512) return 80;     // 13 bits
    if (dist < 2048) return 96;    // 15 bits
    if (dist < 8192) return 112;   // 17 bits
    if (dist < 32768) return 128;  // 19 bits
    if (dist < 131072) return 144; // 21 bits
    if (dist < 524288) return 160; // 23 bits
    if (dist < 2097152) return 176; // 25 bits
    return 208;  // max ~26 bits
}

inline uint32_t get_pos_slot(uint32_t dist) {
    if (dist < 4) return dist;
    uint32_t bits = 31 - __builtin_clz(dist);
    return (bits * 2) + ((dist >> (bits - 1)) & 1);
}

// Probability tables
struct LzmaProbs {
    rc::Prob is_match[kNumStates][16];
    rc::Prob is_rep[kNumStates];
    rc::Prob is_rep0[kNumStates];
    rc::Prob is_rep0_long[kNumStates][16];
    rc::Prob is_rep1[kNumStates];
    rc::Prob is_rep2[kNumStates];
    rc::BitTree<kNumPosSlotBits> pos_slot[kNumLenToPosStates];
    rc::Prob pos_encoders[kNumFullDistances - kEndPosModelIndex];
    rc::BitTree<kNumAlignBits> align_encoder;
    rc::LenCoder len_encoder;
    rc::LenCoder rep_len_encoder;
    // Combined literal coder with matched literal support (768 probs each)
    rc::MatchedLiteralCoder literal[8];
};

// Binary tree match finder (BT4) - finds ALL matches efficiently
// Key insight: new position becomes ROOT, tree splits into left/right subtrees
class MatchFinder {
public:
    const uint8_t* data_;
    size_t size_;
    uint32_t dict_size_;

    std::vector<uint32_t> hash_;      // Hash4 -> tree root position
    std::vector<uint32_t> hash3_;     // Hash3 -> position (for short matches)
    std::vector<uint32_t> left_;      // Left child (lexicographically smaller)
    std::vector<uint32_t> right_;     // Right child (lexicographically larger)

    static constexpr int HASH_BITS = 20;  // 1M entries - good balance
    static constexpr int HASH3_BITS = 16;  // 64K entries for short matches
    static constexpr int MAX_DEPTH = 64;
    static constexpr uint32_t kEmpty = UINT32_MAX;

    MatchFinder(const uint8_t* data, size_t size, uint32_t dict_size)
        : data_(data), size_(size), dict_size_(dict_size) {
        hash_.resize(1 << HASH_BITS, kEmpty);
        hash3_.resize(1 << HASH3_BITS, kEmpty);
        left_.resize(size, kEmpty);
        right_.resize(size, kEmpty);
    }

    uint32_t hash4(size_t pos) const {
        if (pos + 3 >= size_) return 0;
        uint32_t h = data_[pos] | (data_[pos+1] << 8) | (data_[pos+2] << 16) | (data_[pos+3] << 24);
        return (h * 0x9E3779B9) >> (32 - HASH_BITS);
    }

    uint32_t hash3_func(size_t pos) const {
        if (pos + 2 >= size_) return 0;
        uint32_t h = data_[pos] | (data_[pos+1] << 8) | (data_[pos+2] << 16);
        return (h * 0x1E35A7BD) >> (32 - HASH3_BITS);
    }

    struct Match { uint32_t dist; uint32_t len; };

    // Check rep matches first (REPDIST optimization)
    // Returns best rep length found - caller can use this to limit tree search
    uint32_t check_reps(size_t pos, const uint32_t* reps, std::vector<Match>& rep_matches) {
        rep_matches.clear();
        uint32_t best_rep_len = 0;

        for (int r = 0; r < 4; r++) {
            if (reps[r] == 0 || reps[r] > pos) continue;
            size_t mp = pos - reps[r];
            uint32_t len = 0;
            while (pos + len < size_ && len < 273 && data_[pos + len] == data_[mp + len]) {
                len++;
            }
            if (len >= 2) {
                rep_matches.push_back({reps[r], len});
                if (len > best_rep_len) best_rep_len = len;
            }
            // Also add short rep0 (length 1) for rep[0]
            if (r == 0 && len >= 1) {
                rep_matches.push_back({reps[0], 1});
            }
        }
        return best_rep_len;
    }

    // Core BT4: Find matches AND insert pos as new root (combined operation)
    // nice_len can be reduced if good rep match was found (REPDIST optimization)
    void find_matches_and_update(size_t pos, std::vector<Match>& matches, uint32_t nice_len = 273) {
        matches.clear();
        if (pos + 3 >= size_) return;

        // Check hash3 for short match first
        uint32_t h3 = hash3_func(pos);
        uint32_t match3 = hash3_[h3];
        if (match3 != kEmpty && pos > match3 && pos - match3 <= dict_size_) {
            uint32_t len = 0;
            while (pos + len < size_ && len < 3 && data_[pos + len] == data_[match3 + len]) len++;
            if (len >= 2) {
                matches.push_back({(uint32_t)(pos - match3), len});
            }
        }
        hash3_[h3] = pos;  // Update hash3

        uint32_t h = hash4(pos);
        uint32_t cur = hash_[h];

        // ptr0 = where to attach nodes GREATER than pos (goes to right_[pos])
        // ptr1 = where to attach nodes LESS than pos (goes to left_[pos])
        uint32_t* ptr0 = &right_[pos];
        uint32_t* ptr1 = &left_[pos];
        *ptr0 = kEmpty;
        *ptr1 = kEmpty;

        uint32_t best_len = 1;

        for (int depth = 0; depth < MAX_DEPTH && cur != kEmpty; depth++) {
            if (pos - cur > dict_size_) {
                // Out of dictionary - cut subtree
                *ptr0 = kEmpty;
                *ptr1 = kEmpty;
                break;
            }

            uint32_t dist = pos - cur;

            // Compare strings from the beginning
            // Note: The len0/len1 optimization is disabled because it was causing
            // incorrect match lengths. The optimization assumes tree structural
            // properties that aren't guaranteed when nodes have colliding hashes.
            uint32_t len = 0;
            while (pos + len < size_ && len < 273 && data_[pos + len] == data_[cur + len]) {
                len++;
            }

            // Record match if better
            if (len >= 2 && len > best_len) {
                matches.push_back({dist, len});
                best_len = len;
            }

            // Early exit if good enough
            if (len >= nice_len) {
                // Attach cur's children and exit
                *ptr0 = right_[cur];
                *ptr1 = left_[cur];
                break;
            }

            // Decide direction based on lexicographic comparison
            if (pos + len >= size_ || (len < 273 && data_[pos + len] < data_[cur + len])) {
                // pos < cur lexicographically
                // cur and its RIGHT subtree are GREATER than pos -> attach to ptr0
                *ptr0 = cur;
                ptr0 = &left_[cur];
                cur = left_[cur];
            } else {
                // pos >= cur lexicographically
                // cur and its LEFT subtree are LESS than pos -> attach to ptr1
                *ptr1 = cur;
                ptr1 = &right_[cur];
                cur = right_[cur];
            }
        }

        // New position becomes root
        hash_[h] = pos;
    }

    // Find matches with rep checking (uses cached tree matches)
    void find_matches(size_t pos, const uint32_t* reps,
                      std::vector<Match>& matches, uint32_t nice_len = 273) {
        // First get rep matches
        std::vector<Match> result;
        for (int r = 0; r < 4; r++) {
            // Validate: rep distance must not exceed current position
            if (reps[r] == 0 || reps[r] > pos) continue;
            size_t mp = pos - reps[r];
            uint32_t max_len = 0;
            while (pos + max_len < size_ && max_len < 273 &&
                   data_[pos + max_len] == data_[mp + max_len]) max_len++;
            if (max_len >= 2) {
                result.push_back({reps[r], max_len});
            }
            if (r == 0 && max_len >= 1) {
                result.push_back({reps[0], 1});
            }
        }

        // Then add tree matches (filtering out reps)
        for (const auto& m : matches) {
            bool is_rep = false;
            for (int r = 0; r < 4; r++) {
                if (reps[r] == m.dist) { is_rep = true; break; }
            }
            if (!is_rep) {
                result.push_back(m);
            }
        }

        matches = std::move(result);
    }

    // Update tree for skipped positions (must still maintain structure)
    void update(size_t pos) {
        if (pos + 3 >= size_) return;

        // Update hash3
        uint32_t h3 = hash3_func(pos);
        hash3_[h3] = pos;

        uint32_t h = hash4(pos);
        uint32_t cur = hash_[h];

        uint32_t* ptr0 = &right_[pos];
        uint32_t* ptr1 = &left_[pos];
        *ptr0 = kEmpty;
        *ptr1 = kEmpty;

        for (int depth = 0; depth < MAX_DEPTH && cur != kEmpty; depth++) {
            if (pos - cur > dict_size_) {
                *ptr0 = kEmpty;
                *ptr1 = kEmpty;
                break;
            }

            // Compare from the beginning (same fix as find_matches_and_update)
            uint32_t len = 0;
            while (pos + len < size_ && len < 273 && data_[pos + len] == data_[cur + len]) {
                len++;
            }

            if (pos + len >= size_ || (len < 273 && data_[pos + len] < data_[cur + len])) {
                *ptr0 = cur;
                ptr0 = &left_[cur];
                cur = left_[cur];
            } else {
                *ptr1 = cur;
                ptr1 = &right_[cur];
                cur = right_[cur];
            }
        }

        hash_[h] = pos;
    }
};

// Multi-arrival node
constexpr int kNumArrivals = 16;  // 16 is a good balance of compression vs speed

struct Arrival {
    uint32_t price;
    uint32_t back_len;     // 0 = literal
    uint32_t back_dist;
    int8_t back_rep;       // -1 if not rep
    uint8_t state;         // State AFTER this decision
    uint32_t reps[4];      // Reps AFTER this decision
    uint8_t prev_state;    // State BEFORE this decision (for trace matching)
    uint32_t prev_reps[4]; // Reps BEFORE this decision (for trace matching)

    void reset() {
        price = kInfPrice;
        back_len = 0;
        back_dist = 0;
        back_rep = -1;
        state = 0;
        prev_state = 0;
        for (int i = 0; i < 4; i++) {
            reps[i] = 1;
            prev_reps[i] = 1;
        }
    }
};

struct OptNode {
    Arrival arrivals[kNumArrivals];
    int num_arrivals;

    void reset() {
        for (int i = 0; i < kNumArrivals; i++) arrivals[i].reset();
        num_arrivals = 0;
    }

    // Try to add a new arrival, keep only N best
    // Since we store prev_state/prev_reps, we don't rely on index stability
    void add_arrival(const Arrival& arr) {
        // If we have room, just append
        if (num_arrivals < kNumArrivals) {
            arrivals[num_arrivals++] = arr;
            return;
        }

        // Array full - find worst and replace if new is better
        int worst_idx = 0;
        uint32_t worst_price = arrivals[0].price;
        for (int i = 1; i < kNumArrivals; i++) {
            if (arrivals[i].price > worst_price) {
                worst_price = arrivals[i].price;
                worst_idx = i;
            }
        }
        if (arr.price < worst_price) {
            arrivals[worst_idx] = arr;
        }
    }

    // Get index of best (lowest price) arrival
    int best_arrival_idx() const {
        if (num_arrivals == 0) return 0;
        int best = 0;
        for (int i = 1; i < num_arrivals; i++) {
            if (arrivals[i].price < arrivals[best].price) {
                best = i;
            }
        }
        return best;
    }
};

// Compression level settings
struct LzmaSettings {
    size_t opt_window;
    int num_arrivals;
    int max_depth;

    static LzmaSettings for_size(size_t size, int level = 2) {
        // Level 0: No optimal parsing (use lazy encoder instead)
        // Level 1: Fast - moderate window
        // Level 2: Normal - larger window for better compression
        // Level 3: Best - maximum window for best compression
        // Empirically: larger window = better compression, similar speed

        if (level == 0) {
            return {0, 1, 32};  // No optimal parsing
        }

        // Level-specific base settings
        size_t window, arrivals, depth;
        switch (level) {
            case 1:  // Fast
                window = 8192;
                arrivals = 16;
                depth = 64;
                break;
            case 2:  // Normal (good balance)
            default:
                window = 32768;
                arrivals = 16;
                depth = 64;
                break;
            case 3:  // Best compression
                window = 262144;  // 256K - best balance of compression vs memory
                arrivals = 16;
                depth = 64;
                break;
        }

        // For small files, optimize the entire file
        if (size < 32 * 1024) {
            window = size;
            arrivals = std::max(arrivals, (size_t)16);  // More arrivals for small files
        }

        return {window, (int)arrivals, (int)depth};
    }
};

std::vector<uint8_t> compress(const uint8_t* data, size_t size, uint32_t dict_size = 1 << 22, int level = 2) {
    std::vector<uint8_t> out;

    // LZMA Header (13 bytes):
    // Byte 0: Properties = (pb * 5 + lp) * 9 + lc
    //         Default: lc=3, lp=0, pb=2 -> (2*5+0)*9+3 = 93 = 0x5D
    // Bytes 1-4: Dictionary size (little-endian)
    // Bytes 5-12: Uncompressed size (little-endian, -1 for unknown)
    uint8_t lc = 3, lp = 0, pb = 2;
    uint32_t pos_state_mask = (1 << pb) - 1;  // = 3 for pb=2
    uint8_t props = (pb * 5 + lp) * 9 + lc;
    out.push_back(props);
    for (int i = 0; i < 4; i++) out.push_back((dict_size >> (i * 8)) & 0xFF);
    for (int i = 0; i < 8; i++) out.push_back((size >> (i * 8)) & 0xFF);

    // Get adaptive settings based on size and level
    LzmaSettings settings = LzmaSettings::for_size(size, level);

    rc::Encoder enc(out);
    LzmaProbs probs;
    MatchFinder mf(data, size, dict_size);

    uint32_t state = 0;
    uint32_t reps[4] = {1, 1, 1, 1};

    // Use adaptive window size
    size_t kOptLen = settings.opt_window > 0 ? settings.opt_window : 8192;
    std::vector<OptNode> opt(kOptLen + 273);
    std::vector<MatchFinder::Match> matches;

    size_t pos = 0;
    while (pos < size) {
        size_t chunk_start = pos;
        size_t chunk_end = std::min(pos + kOptLen, size);
        size_t chunk_len = chunk_end - chunk_start;

        // Reset DP
        for (size_t i = 0; i <= chunk_len + 272; i++) opt[i].reset();

        // Initialize first position with current state
        opt[0].arrivals[0].price = 0;
        opt[0].arrivals[0].state = state;
        memcpy(opt[0].arrivals[0].reps, reps, sizeof(reps));
        opt[0].num_arrivals = 1;

        // Forward pass - fill optimal prices
        std::vector<MatchFinder::Match> tree_matches;  // Cache tree matches per position
        std::vector<MatchFinder::Match> rep_matches;   // Rep matches (REPDIST)

        for (size_t i = 0; i < chunk_len; i++) {
            size_t cur_pos = chunk_start + i;

            if (opt[i].num_arrivals == 0) {
                mf.update(cur_pos);  // Still need to update tree
                continue;
            }

            // REPDIST optimization: Check reps first using best arrival's reps
            // If we find a long rep match, we can use shorter nice_len for tree search
            int best_arr_idx = opt[i].best_arrival_idx();
            const uint32_t* best_reps = opt[i].arrivals[best_arr_idx].reps;
            uint32_t best_rep_len = mf.check_reps(cur_pos, best_reps, rep_matches);

            // Use best rep length to limit tree search (if rep >= 32, skip deep search)
            uint32_t nice_len = (best_rep_len >= 32) ? std::min(best_rep_len, 64u) : 273;

            // Get tree matches AND update tree (combined BT4 operation)
            mf.find_matches_and_update(cur_pos, tree_matches, nice_len);

            // Try each arrival at this position
            for (int a = 0; a < opt[i].num_arrivals; a++) {
                const Arrival& arr = opt[i].arrivals[a];
                uint32_t cur_state = arr.state;
                const uint32_t* cur_reps = arr.reps;

                // Option 1: Literal
                // Cheaper if after match (state >= 7) due to XOR with match_byte
                uint32_t lit_price = arr.price + (cur_state >= 7 ? kLitMatchedPrice : kLitPrice);
                if (i + 1 <= chunk_len + 272) {
                    Arrival new_arr;
                    new_arr.price = lit_price;
                    new_arr.back_len = 0;
                    new_arr.back_dist = 0;
                    new_arr.back_rep = -1;
                    new_arr.state = kLiteralNextStates[cur_state];
                    memcpy(new_arr.reps, cur_reps, sizeof(new_arr.reps));
                    new_arr.prev_state = cur_state;
                    memcpy(new_arr.prev_reps, cur_reps, sizeof(new_arr.prev_reps));
                    opt[i + 1].add_arrival(new_arr);
                }

                // Option 2: Matches - REPDIST: process reps first, they're cheaper
                // Re-check reps for this specific arrival's rep set
                std::vector<MatchFinder::Match> arrival_rep_matches;
                mf.check_reps(cur_pos, cur_reps, arrival_rep_matches);

                // Combine: reps first (cheaper), then tree matches (filter out reps)
                matches.clear();
                matches.insert(matches.end(), arrival_rep_matches.begin(), arrival_rep_matches.end());
                for (const auto& tm : tree_matches) {
                    bool is_rep = false;
                    for (int r = 0; r < 4; r++) {
                        if (cur_reps[r] == tm.dist) { is_rep = true; break; }
                    }
                    if (!is_rep) matches.push_back(tm);
                }

                for (const auto& m : matches) {
                    // Check if rep match
                    int rep_idx = -1;
                    for (int r = 0; r < 4; r++) {
                        if (cur_reps[r] == m.dist) { rep_idx = r; break; }
                    }

                    uint32_t min_len = (rep_idx >= 0) ? ((rep_idx == 0) ? 1 : 2) : 2;
                    uint32_t max_len = m.len;

                    // For short matches (common in text), try all lengths
                    // For longer matches, sample key points
                    uint32_t lens_to_try[32];
                    int num_lens = 0;

                    if (max_len <= 12) {
                        // Short match: try all lengths for better text compression
                        for (uint32_t l = min_len; l <= max_len; l++) {
                            lens_to_try[num_lens++] = l;
                        }
                    } else {
                        // Long match: sample key points
                        lens_to_try[num_lens++] = min_len;
                        lens_to_try[num_lens++] = min_len + 1;
                        if (min_len + 2 <= max_len) lens_to_try[num_lens++] = min_len + 2;
                        // Quarter, half, three-quarters
                        uint32_t q1 = min_len + (max_len - min_len) / 4;
                        uint32_t q2 = min_len + (max_len - min_len) / 2;
                        uint32_t q3 = min_len + 3 * (max_len - min_len) / 4;
                        if (q1 > min_len + 2) lens_to_try[num_lens++] = q1;
                        if (q2 > q1) lens_to_try[num_lens++] = q2;
                        if (q3 > q2) lens_to_try[num_lens++] = q3;
                        if (max_len - 1 > q3) lens_to_try[num_lens++] = max_len - 1;
                        lens_to_try[num_lens++] = max_len;
                    }

                    for (int li = 0; li < num_lens; li++) {
                        uint32_t try_len = lens_to_try[li];
                        if (i + try_len > chunk_len + 272) continue;

                        uint32_t match_price;
                        uint8_t new_state;
                        uint32_t new_reps[4];

                        if (rep_idx >= 0) {
                            if (rep_idx == 0 && try_len == 1) {
                                // Short rep0: cheapest possible match
                                match_price = arr.price + kShortRep0Price;
                                new_state = kShortRepNextStates[cur_state];
                            } else {
                                // Rep match with length
                                uint32_t rep_base = (rep_idx == 0) ? kRep0Price :
                                                   (rep_idx == 1) ? kRep1Price :
                                                   (rep_idx == 2) ? kRep2Price : kRep3Price;
                                match_price = arr.price + rep_base + len_price(try_len);
                                new_state = kRepNextStates[cur_state];
                            }
                            new_reps[0] = cur_reps[rep_idx];
                            for (int r = 1; r <= rep_idx; r++) new_reps[r] = cur_reps[r - 1];
                            for (int r = rep_idx + 1; r < 4; r++) new_reps[r] = cur_reps[r];
                        } else {
                            // New match: is_match + is_rep(0) + len + dist
                            match_price = arr.price + 16 + len_price(try_len) + dist_price(m.dist);
                            new_state = kMatchNextStates[cur_state];
                            new_reps[0] = m.dist;
                            new_reps[1] = cur_reps[0];
                            new_reps[2] = cur_reps[1];
                            new_reps[3] = cur_reps[2];
                        }

                        // Validate match before adding
                        if (cur_pos < m.dist) {
                            // Invalid: match distance exceeds position
                            continue;
                        }

                        Arrival new_arr;
                        new_arr.price = match_price;
                        new_arr.back_len = try_len;
                        new_arr.back_dist = m.dist;
                        new_arr.back_rep = rep_idx;
                        new_arr.state = new_state;
                        memcpy(new_arr.reps, new_reps, sizeof(new_reps));
                        new_arr.prev_state = cur_state;
                        memcpy(new_arr.prev_reps, cur_reps, sizeof(new_arr.prev_reps));
                        opt[i + try_len].add_arrival(new_arr);
                    }
                }
            }
            // Note: tree already updated by find_matches_and_update()
        }

        // Find best arrival at chunk end (we MUST process the FULL chunk)
        // Priority: Use chunk_len if it has arrivals. Only search backwards if not.
        size_t best_end = chunk_len;
        int best_arrival = 0;

        if (opt[chunk_len].num_arrivals > 0) {
            // Chunk_len is reachable - use it (we must encode all bytes)
            best_arrival = opt[chunk_len].best_arrival_idx();
        } else {
            // No arrivals at chunk_len - find closest reachable position
            // This shouldn't happen with correct DP, but handle gracefully
            for (size_t i = chunk_len - 1; i > 0; i--) {
                if (opt[i].num_arrivals > 0) {
                    best_end = i;
                    best_arrival = opt[i].best_arrival_idx();
                    break;
                }
            }
        }

        #ifdef DEBUG_OPTIMAL
        if (best_end != chunk_len) {
            fprintf(stderr, "Chunk %zu: best_end=%zu < chunk_len=%zu (gap of %zu)\n",
                    chunk_start, best_end, chunk_len, chunk_len - best_end);
        }
        #endif

        // Backward trace - collect decisions
        // We match by prev_state/prev_reps since indices can become stale
        struct Decision {
            uint32_t back_len;
            uint32_t back_dist;
            int8_t back_rep;
        };
        std::vector<Decision> decisions;

        size_t cur = best_end;
        int cur_arr = best_arrival;
        while (cur > 0) {
            const Arrival& arr = opt[cur].arrivals[cur_arr];
            Decision d = {arr.back_len, arr.back_dist, arr.back_rep};
            decisions.push_back(d);

            // Get source state/reps to match at previous position
            uint8_t target_state = arr.prev_state;
            const uint32_t* target_reps = arr.prev_reps;

            // Move cur back
            if (arr.back_len > 0) {
                cur -= arr.back_len;
            } else {
                cur--;
            }

            // Find matching arrival at source position by state/reps
            cur_arr = 0;  // Default fallback
            if (cur > 0) {
                for (int i = 0; i < opt[cur].num_arrivals; i++) {
                    const Arrival& prev = opt[cur].arrivals[i];
                    if (prev.state == target_state &&
                        prev.reps[0] == target_reps[0] && prev.reps[1] == target_reps[1] &&
                        prev.reps[2] == target_reps[2] && prev.reps[3] == target_reps[3]) {
                        cur_arr = i;
                        break;
                    }
                }
            }
        }
        std::reverse(decisions.begin(), decisions.end());

        // Encode the decisions
        size_t enc_pos = 0;
        for (const auto& d : decisions) {
            size_t data_pos = chunk_start + enc_pos;
            uint32_t pos_state = data_pos & pos_state_mask;

            if (d.back_len == 0) {
                // Literal
                enc.encode_bit(probs.is_match[state][pos_state], 0);
                uint8_t byte = data[data_pos];
                uint8_t prev = (data_pos > 0) ? data[data_pos - 1] : 0;
                uint8_t ctx = prev >> 5;

                // Use matched context if after a match (state >= 7)
                if (state >= 7 && reps[0] > 0 && data_pos >= reps[0]) {
                    // XOR with match byte for better context separation
                    uint8_t match_byte = data[data_pos - reps[0]];
                    #ifdef DEBUG_POS_1970
                    if (data_pos == 1970) {
                        fprintf(stderr, "OPT ENCODE pos=1970: byte=0x%02X match_byte=0x%02X ctx=%u state=%u reps=[%u,%u,%u,%u]\n",
                                byte, match_byte, ctx, state, reps[0], reps[1], reps[2], reps[3]);
                        fprintf(stderr, "  0x41 indices: 0x101=%u 0x102=%u 5=%u 10=%u 20=%u 40=%u 80=%u 160=%u\n",
                                probs.literal[ctx].probs[0x101].p, probs.literal[ctx].probs[0x102].p,
                                probs.literal[ctx].probs[5].p, probs.literal[ctx].probs[10].p,
                                probs.literal[ctx].probs[20].p, probs.literal[ctx].probs[40].p,
                                probs.literal[ctx].probs[80].p, probs.literal[ctx].probs[160].p);
                        fprintf(stderr, "  0x80 indices: 3=%u 6=%u 12=%u 24=%u 48=%u 96=%u 192=%u\n",
                                probs.literal[ctx].probs[3].p, probs.literal[ctx].probs[6].p,
                                probs.literal[ctx].probs[12].p, probs.literal[ctx].probs[24].p,
                                probs.literal[ctx].probs[48].p, probs.literal[ctx].probs[96].p,
                                probs.literal[ctx].probs[192].p);
                    }
                    #endif
                    probs.literal[ctx].encode_matched(enc, byte, match_byte);
                } else {
                    probs.literal[ctx].encode(enc, byte);
                }
                state = kLiteralNextStates[state];
                enc_pos++;
            } else {
                // Match
                enc.encode_bit(probs.is_match[state][pos_state], 1);

                if (d.back_rep >= 0) {
                    // Rep match
                    enc.encode_bit(probs.is_rep[state], 1);
                    if (d.back_rep == 0) {
                        enc.encode_bit(probs.is_rep0[state], 0);
                        if (d.back_len == 1) {
                            // Short rep0 - no length encoding needed
                            enc.encode_bit(probs.is_rep0_long[state][pos_state], 0);
                            state = kShortRepNextStates[state];
                        } else {
                            enc.encode_bit(probs.is_rep0_long[state][pos_state], 1);
                            probs.rep_len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);
                            state = kRepNextStates[state];
                        }
                    } else {
                        enc.encode_bit(probs.is_rep0[state], 1);
                        if (d.back_rep == 1) {
                            enc.encode_bit(probs.is_rep1[state], 0);
                        } else {
                            enc.encode_bit(probs.is_rep1[state], 1);
                            enc.encode_bit(probs.is_rep2[state], d.back_rep == 2 ? 0 : 1);
                        }
                        probs.rep_len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);
                        state = kRepNextStates[state];
                    }
                    // Update reps
                    uint32_t dist = reps[d.back_rep];
                    for (int r = d.back_rep; r > 0; r--) reps[r] = reps[r - 1];
                    reps[0] = dist;
                } else {
                    // Normal match - encode distance as LZMA format (0-based)
                    enc.encode_bit(probs.is_rep[state], 0);
                    probs.len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);

                    uint32_t len_state = std::min(d.back_len - 2, kNumLenToPosStates - 1);
                    uint32_t lzma_dist = d.back_dist - 1;  // LZMA uses 0-based distances
                    uint32_t pos_slot = get_pos_slot(lzma_dist);
                    probs.pos_slot[len_state].encode(enc, pos_slot);

                    if (pos_slot >= 4) {
                        uint32_t num_direct = (pos_slot >> 1) - 1;
                        uint32_t base = (2 | (pos_slot & 1)) << num_direct;
                        uint32_t pos_reduced = lzma_dist - base;

                        if (pos_slot < kEndPosModelIndex) {
                            // Use reverse bit tree encoding (LZMA SDK style)
                            rc::BitTree<8>::encode_reverse_raw(enc, probs.pos_encoders + base - pos_slot - 1, num_direct, pos_reduced);
                        } else {
                            enc.encode_direct_bits(pos_reduced >> kNumAlignBits, num_direct - kNumAlignBits);
                            probs.align_encoder.encode_reverse(enc, pos_reduced & ((1 << kNumAlignBits) - 1));
                        }
                    }

                    reps[3] = reps[2];
                    reps[2] = reps[1];
                    reps[1] = reps[0];
                    reps[0] = d.back_dist;  // Keep internal format as 1-based
                    state = kMatchNextStates[state];
                }
                enc_pos += d.back_len;
            }
        }

        // Fill any gap between best_end and chunk_len with literals
        // This happens when the DP can't find a path to the exact chunk end
        while (enc_pos < chunk_len && chunk_start + enc_pos < size) {
            size_t data_pos = chunk_start + enc_pos;
            uint32_t pos_state = data_pos & pos_state_mask;

            enc.encode_bit(probs.is_match[state][pos_state], 0);
            uint8_t byte = data[data_pos];
            uint8_t prev = (data_pos > 0) ? data[data_pos - 1] : 0;
            uint8_t ctx = prev >> 5;

            if (state >= 7 && reps[0] > 0 && data_pos >= reps[0]) {
                uint8_t match_byte = data[data_pos - reps[0]];
                probs.literal[ctx].encode_matched(enc, byte, match_byte);
            } else {
                probs.literal[ctx].encode(enc, byte);
            }
            state = kLiteralNextStates[state];
            enc_pos++;
        }

        pos = chunk_start + enc_pos;
        // State and reps are already updated during encoding
        // (both DP path and gap-filling literals update state)
    }

    // Sanity check: we should have processed all bytes
    // (pos != size would indicate a bug in the encoder)

    enc.finish();
    return out;
}

// Custom version with explicit window size for tuning
std::vector<uint8_t> compress_custom(const uint8_t* data, size_t size, uint32_t dict_size,
                                      size_t opt_window, int max_arrivals, int depth) {
    std::vector<uint8_t> out;

    // LZMA Header (13 bytes)
    uint8_t lc = 3, lp = 0, pb = 2;
    uint32_t pos_state_mask = (1 << pb) - 1;  // = 3 for pb=2
    uint8_t props = (pb * 5 + lp) * 9 + lc;
    out.push_back(props);
    for (int i = 0; i < 4; i++) out.push_back((dict_size >> (i * 8)) & 0xFF);
    for (int i = 0; i < 8; i++) out.push_back((size >> (i * 8)) & 0xFF);

    rc::Encoder enc(out);
    LzmaProbs probs;
    MatchFinder mf(data, size, dict_size);

    uint32_t state = 0;
    uint32_t reps[4] = {1, 1, 1, 1};

    size_t kOptLen = opt_window > 0 ? opt_window : 8192;
    std::vector<OptNode> opt(kOptLen + 273);
    std::vector<MatchFinder::Match> matches;

    size_t pos = 0;
    while (pos < size) {
        size_t chunk_start = pos;
        size_t chunk_end = std::min(pos + kOptLen, size);
        size_t chunk_len = chunk_end - chunk_start;

        // Reset DP
        for (size_t i = 0; i <= chunk_len + 272; i++) opt[i].reset();

        // Initialize first position with current state
        opt[0].arrivals[0].price = 0;
        opt[0].arrivals[0].state = state;
        memcpy(opt[0].arrivals[0].reps, reps, sizeof(reps));
        opt[0].num_arrivals = 1;

        // Forward pass - fill optimal prices
        std::vector<MatchFinder::Match> tree_matches;
        std::vector<MatchFinder::Match> rep_matches;   // Rep matches (REPDIST)

        for (size_t i = 0; i < chunk_len; i++) {
            size_t cur_pos = chunk_start + i;

            if (opt[i].num_arrivals == 0) {
                mf.update(cur_pos);
                continue;
            }

            // REPDIST optimization: Check reps first using best arrival's reps
            int best_arr_idx = opt[i].best_arrival_idx();
            const uint32_t* best_reps = opt[i].arrivals[best_arr_idx].reps;
            uint32_t best_rep_len = mf.check_reps(cur_pos, best_reps, rep_matches);

            // Use best rep length to limit tree search
            uint32_t nice_len = (best_rep_len >= 32) ? std::min(best_rep_len, 64u) : 273;

            // Get tree matches AND update tree
            mf.find_matches_and_update(cur_pos, tree_matches, nice_len);

            // Process each arrival at this position
            for (int a = 0; a < opt[i].num_arrivals; a++) {
                const Arrival& arr = opt[i].arrivals[a];
                uint32_t cur_state = arr.state;
                const uint32_t* cur_reps = arr.reps;

                // Option 1: Literal
                uint32_t lit_price = arr.price + (cur_state >= 7 ? kLitMatchedPrice : kLitPrice);
                if (i + 1 <= chunk_len + 272) {
                    Arrival new_arr;
                    new_arr.price = lit_price;
                    new_arr.back_len = 0;
                    new_arr.back_dist = 0;
                    new_arr.back_rep = -1;
                    new_arr.state = kLiteralNextStates[cur_state];
                    memcpy(new_arr.reps, cur_reps, sizeof(new_arr.reps));
                    new_arr.prev_state = cur_state;
                    memcpy(new_arr.prev_reps, cur_reps, sizeof(new_arr.prev_reps));
                    opt[i + 1].add_arrival(new_arr);
                }

                // Option 2: Matches - REPDIST: process reps first, they're cheaper
                std::vector<MatchFinder::Match> arrival_rep_matches;
                mf.check_reps(cur_pos, cur_reps, arrival_rep_matches);

                // Combine: reps first (cheaper), then tree matches (filter out reps)
                matches.clear();
                matches.insert(matches.end(), arrival_rep_matches.begin(), arrival_rep_matches.end());
                for (const auto& tm : tree_matches) {
                    bool is_rep = false;
                    for (int r = 0; r < 4; r++) {
                        if (cur_reps[r] == tm.dist) { is_rep = true; break; }
                    }
                    if (!is_rep) matches.push_back(tm);
                }

                for (const auto& m : matches) {
                    // Check if rep match
                    int rep_idx = -1;
                    for (int r = 0; r < 4; r++) {
                        if (cur_reps[r] == m.dist) { rep_idx = r; break; }
                    }

                    uint32_t min_len = (rep_idx >= 0) ? ((rep_idx == 0) ? 1 : 2) : 2;
                    uint32_t max_len = m.len;

                    // For short matches, try all lengths; for long matches, sample
                    uint32_t lens_to_try[32];
                    int num_lens = 0;

                    if (max_len <= 12) {
                        for (uint32_t l = min_len; l <= max_len; l++) {
                            lens_to_try[num_lens++] = l;
                        }
                    } else {
                        lens_to_try[num_lens++] = min_len;
                        lens_to_try[num_lens++] = min_len + 1;
                        if (min_len + 2 <= max_len) lens_to_try[num_lens++] = min_len + 2;
                        uint32_t q1 = min_len + (max_len - min_len) / 4;
                        uint32_t q2 = min_len + (max_len - min_len) / 2;
                        uint32_t q3 = min_len + 3 * (max_len - min_len) / 4;
                        if (q1 > min_len + 2) lens_to_try[num_lens++] = q1;
                        if (q2 > q1) lens_to_try[num_lens++] = q2;
                        if (q3 > q2) lens_to_try[num_lens++] = q3;
                        if (max_len - 1 > q3) lens_to_try[num_lens++] = max_len - 1;
                        lens_to_try[num_lens++] = max_len;
                    }

                    for (int li = 0; li < num_lens; li++) {
                        uint32_t try_len = lens_to_try[li];
                        if (i + try_len > chunk_len + 272) continue;

                        uint32_t match_price;
                        uint8_t new_state;
                        uint32_t new_reps[4];

                        if (rep_idx >= 0) {
                            if (rep_idx == 0 && try_len == 1) {
                                match_price = arr.price + kShortRep0Price;
                                new_state = kShortRepNextStates[cur_state];
                            } else {
                                uint32_t rep_base = (rep_idx == 0) ? kRep0Price :
                                                   (rep_idx == 1) ? kRep1Price :
                                                   (rep_idx == 2) ? kRep2Price : kRep3Price;
                                match_price = arr.price + rep_base + len_price(try_len);
                                new_state = kRepNextStates[cur_state];
                            }
                            new_reps[0] = cur_reps[rep_idx];
                            for (int r = 1; r <= rep_idx; r++) new_reps[r] = cur_reps[r - 1];
                            for (int r = rep_idx + 1; r < 4; r++) new_reps[r] = cur_reps[r];
                        } else {
                            match_price = arr.price + 16 + len_price(try_len) + dist_price(m.dist);
                            new_state = kMatchNextStates[cur_state];
                            new_reps[0] = m.dist;
                            new_reps[1] = cur_reps[0];
                            new_reps[2] = cur_reps[1];
                            new_reps[3] = cur_reps[2];
                        }

                        if (cur_pos < m.dist) continue;

                        Arrival new_arr;
                        new_arr.price = match_price;
                        new_arr.back_len = try_len;
                        new_arr.back_dist = m.dist;
                        new_arr.back_rep = rep_idx;
                        new_arr.state = new_state;
                        memcpy(new_arr.reps, new_reps, sizeof(new_reps));
                        new_arr.prev_state = cur_state;
                        memcpy(new_arr.prev_reps, cur_reps, sizeof(new_arr.prev_reps));
                        opt[i + try_len].add_arrival(new_arr);
                    }
                }
            }
        }

        // Find best arrival at chunk end (we MUST process the FULL chunk)
        size_t best_end = chunk_len;
        int best_arrival = 0;

        if (opt[chunk_len].num_arrivals > 0) {
            // Chunk_len is reachable - use it
            best_arrival = opt[chunk_len].best_arrival_idx();
        } else {
            // No arrivals at chunk_len - find closest position
            for (size_t i = chunk_len - 1; i > 0; i--) {
                if (opt[i].num_arrivals > 0) {
                    best_end = i;
                    best_arrival = opt[i].best_arrival_idx();
                    break;
                }
            }
        }

        // Backward trace
        struct Decision {
            uint32_t back_len;
            uint32_t back_dist;
            int8_t back_rep;
        };
        std::vector<Decision> decisions;

        size_t cur = best_end;
        int cur_arr = best_arrival;
        while (cur > 0) {
            const Arrival& arr = opt[cur].arrivals[cur_arr];
            Decision d = {arr.back_len, arr.back_dist, arr.back_rep};
            decisions.push_back(d);

            uint8_t target_state = arr.prev_state;
            const uint32_t* target_reps = arr.prev_reps;

            if (arr.back_len > 0) cur -= arr.back_len;
            else cur--;

            cur_arr = 0;
            if (cur > 0) {
                for (int i = 0; i < opt[cur].num_arrivals; i++) {
                    const Arrival& prev = opt[cur].arrivals[i];
                    if (prev.state == target_state &&
                        prev.reps[0] == target_reps[0] && prev.reps[1] == target_reps[1] &&
                        prev.reps[2] == target_reps[2] && prev.reps[3] == target_reps[3]) {
                        cur_arr = i;
                        break;
                    }
                }
            }
        }
        std::reverse(decisions.begin(), decisions.end());

        // Encode the decisions
        size_t enc_pos = 0;
        for (const auto& d : decisions) {
            size_t data_pos = chunk_start + enc_pos;
            uint32_t pos_state = data_pos & pos_state_mask;

            if (d.back_len == 0) {
                enc.encode_bit(probs.is_match[state][pos_state], 0);
                uint8_t byte = data[data_pos];
                uint8_t prev = (data_pos > 0) ? data[data_pos - 1] : 0;
                uint8_t ctx = prev >> 5;

                if (state >= 7 && reps[0] > 0 && data_pos >= reps[0]) {
                    uint8_t match_byte = data[data_pos - reps[0]];
                    probs.literal[ctx].encode_matched(enc, byte, match_byte);
                } else {
                    probs.literal[ctx].encode(enc, byte);
                }
                state = kLiteralNextStates[state];
                enc_pos++;
            } else {
                enc.encode_bit(probs.is_match[state][pos_state], 1);

                if (d.back_rep >= 0) {
                    enc.encode_bit(probs.is_rep[state], 1);
                    if (d.back_rep == 0) {
                        enc.encode_bit(probs.is_rep0[state], 0);
                        if (d.back_len == 1) {
                            enc.encode_bit(probs.is_rep0_long[state][pos_state], 0);
                            state = kShortRepNextStates[state];
                        } else {
                            enc.encode_bit(probs.is_rep0_long[state][pos_state], 1);
                            probs.rep_len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);
                            state = kRepNextStates[state];
                        }
                    } else {
                        enc.encode_bit(probs.is_rep0[state], 1);
                        if (d.back_rep == 1) {
                            enc.encode_bit(probs.is_rep1[state], 0);
                        } else {
                            enc.encode_bit(probs.is_rep1[state], 1);
                            enc.encode_bit(probs.is_rep2[state], d.back_rep == 2 ? 0 : 1);
                        }
                        probs.rep_len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);
                        state = kRepNextStates[state];
                    }
                    uint32_t dist = reps[d.back_rep];
                    for (int r = d.back_rep; r > 0; r--) reps[r] = reps[r - 1];
                    reps[0] = dist;
                } else {
                    // Normal match - encode distance as LZMA format (0-based)
                    enc.encode_bit(probs.is_rep[state], 0);
                    probs.len_encoder.encode(enc, d.back_len - kMatchMinLen, pos_state);

                    uint32_t len_state = std::min(d.back_len - 2, kNumLenToPosStates - 1);
                    uint32_t lzma_dist = d.back_dist - 1;  // LZMA uses 0-based distances
                    uint32_t pos_slot = get_pos_slot(lzma_dist);
                    probs.pos_slot[len_state].encode(enc, pos_slot);

                    if (pos_slot >= 4) {
                        uint32_t num_direct = (pos_slot >> 1) - 1;
                        uint32_t base = (2 | (pos_slot & 1)) << num_direct;
                        uint32_t pos_reduced = lzma_dist - base;

                        if (pos_slot < kEndPosModelIndex) {
                            // Use reverse bit tree encoding (LZMA SDK style)
                            rc::BitTree<8>::encode_reverse_raw(enc, probs.pos_encoders + base - pos_slot - 1, num_direct, pos_reduced);
                        } else {
                            enc.encode_direct_bits(pos_reduced >> kNumAlignBits, num_direct - kNumAlignBits);
                            probs.align_encoder.encode_reverse(enc, pos_reduced & ((1 << kNumAlignBits) - 1));
                        }
                    }

                    reps[3] = reps[2];
                    reps[2] = reps[1];
                    reps[1] = reps[0];
                    reps[0] = d.back_dist;  // Keep internal format as 1-based
                    state = kMatchNextStates[state];
                }
                enc_pos += d.back_len;
            }
        }

        while (enc_pos < chunk_len && chunk_start + enc_pos < size) {
            size_t data_pos = chunk_start + enc_pos;
            uint32_t pos_state = data_pos & pos_state_mask;

            enc.encode_bit(probs.is_match[state][pos_state], 0);
            uint8_t byte = data[data_pos];
            uint8_t prev = (data_pos > 0) ? data[data_pos - 1] : 0;
            uint8_t ctx = prev >> 5;

            if (state >= 7 && reps[0] > 0 && data_pos >= reps[0]) {
                uint8_t match_byte = data[data_pos - reps[0]];
                probs.literal[ctx].encode_matched(enc, byte, match_byte);
            } else {
                probs.literal[ctx].encode(enc, byte);
            }
            state = kLiteralNextStates[state];
            enc_pos++;
        }

        pos = chunk_start + enc_pos;
    }

    enc.finish();
    return out;
}

} // namespace lzma_opt2
