// BWT Bisection Test: Find where mzip diverges from bzip2
//
// Compares our BWT pipeline vs bzip2 at each stage to find the gap.
//
// Build: ../mingw64/bin/g++.exe -O3 -o bwt_bisect.exe bwt_bisect.cpp -I. -lzstd -L./zstd_release/zstd-v1.5.6-win64/dll

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include "bwt_compress_v5.hpp"
#include "bwt_compress_v9.hpp"
#include "generators.hpp"
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"

// External bzip2 - we'll shell out to it
// Use local copy. Note: Windows cmd.exe needs no ./ prefix
#define BZIP2_PATH "bzip2.exe"

size_t run_bzip2_compress(const char* input_file) {
    std::string cmd = BZIP2_PATH;
    cmd += " -9 -k -f ";
    cmd += input_file;
    cmd += " 2>NUL";
    return system(cmd.c_str());
}

bool check_bzip2() {
    // Just check if file exists - use ./ prefix for fopen (POSIX style)
    FILE* f = fopen("./bzip2.exe", "rb");
    if (f) { fclose(f); return true; }
    return false;
}

size_t compress_bzip2(const uint8_t* data, size_t n) {
    // Write to temp file
    FILE* f = fopen("_temp_input.bin", "wb");
    fwrite(data, 1, n, f);
    fclose(f);

    // Run bzip2
    run_bzip2_compress("_temp_input.bin");

    // Read compressed size
    f = fopen("_temp_input.bin.bz2", "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fclose(f);

    // Cleanup
    remove("_temp_input.bin");
    remove("_temp_input.bin.bz2");

    return size;
}

// Analyze entropy after each stage
double entropy(const uint8_t* data, size_t n) {
    if (n == 0) return 0;

    size_t freq[256] = {0};
    for (size_t i = 0; i < n; i++) freq[data[i]]++;

    double H = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / n;
            H -= p * log2(p);
        }
    }
    return H;
}

// Analyze zero percentage (MTF output should have lots of zeros)
double zero_pct(const uint8_t* data, size_t n) {
    if (n == 0) return 0;
    size_t zeros = 0;
    for (size_t i = 0; i < n; i++) if (data[i] == 0) zeros++;
    return 100.0 * zeros / n;
}

// Count runs of zeros (for ZRLE analysis)
void analyze_runs(const uint8_t* data, size_t n) {
    std::vector<size_t> run_lengths;
    size_t i = 0;
    while (i < n) {
        if (data[i] == 0) {
            size_t run = 0;
            while (i < n && data[i] == 0) { run++; i++; }
            run_lengths.push_back(run);
        } else {
            i++;
        }
    }

    if (run_lengths.empty()) {
        printf("  No zero runs found\n");
        return;
    }

    size_t total = 0, max_run = 0;
    for (auto r : run_lengths) {
        total += r;
        if (r > max_run) max_run = r;
    }

    printf("  Zero runs: %zu runs, avg=%.1f, max=%zu\n",
           run_lengths.size(), (double)total / run_lengths.size(), max_run);
}

void bisect_stages(const uint8_t* data, size_t n, const char* name) {
    printf("\n=== Bisecting: %s (%zu bytes) ===\n", name, n);

    // Stage 0: Original
    printf("\nStage 0: Original\n");
    printf("  Size: %zu bytes\n", n);
    printf("  Entropy: %.3f bits/byte\n", entropy(data, n));

    // Stage 1: Pre-RLE
    printf("\nStage 1: Pre-RLE\n");
    auto rle1 = bwt5::pre_rle_encode(data, n);
    printf("  Size: %zu bytes (%.1f%% of original)\n", rle1.size(), 100.0 * rle1.size() / n);
    printf("  Entropy: %.3f bits/byte\n", entropy(rle1.data(), rle1.size()));

    // Stage 2: BWT
    printf("\nStage 2: BWT\n");
    uint32_t primary_idx;
    auto bwt = bwt5::bwt_encode(rle1.data(), rle1.size(), primary_idx);
    printf("  Size: %zu bytes (same as pre-RLE)\n", bwt.size());
    printf("  Primary index: %u\n", primary_idx);
    printf("  Entropy: %.3f bits/byte\n", entropy(bwt.data(), bwt.size()));

    // Stage 3: MTF
    printf("\nStage 3: MTF\n");
    auto mtf = bwt5::mtf_encode(bwt.data(), bwt.size());
    printf("  Size: %zu bytes (same as BWT)\n", mtf.size());
    printf("  Entropy: %.3f bits/byte\n", entropy(mtf.data(), mtf.size()));
    printf("  Zero%%: %.1f%%\n", zero_pct(mtf.data(), mtf.size()));
    analyze_runs(mtf.data(), mtf.size());

    // Count symbol distribution in MTF output
    size_t mtf_hist[256] = {0};
    for (size_t i = 0; i < mtf.size(); i++) mtf_hist[mtf[i]]++;
    printf("  Top symbols: 0=%zu (%.1f%%), 1=%zu (%.1f%%), 2=%zu (%.1f%%)\n",
           mtf_hist[0], 100.0 * mtf_hist[0] / mtf.size(),
           mtf_hist[1], 100.0 * mtf_hist[1] / mtf.size(),
           mtf_hist[2], 100.0 * mtf_hist[2] / mtf.size());

    // Stage 4: ZRLE
    printf("\nStage 4: ZRLE\n");
    auto zrle = bwt5::zrle_encode(mtf.data(), mtf.size());
    printf("  Symbols: %zu (%.1f%% of MTF size)\n", zrle.size(), 100.0 * zrle.size() / mtf.size());

    // Count symbol distribution in ZRLE output
    size_t zrle_hist[512] = {0};
    int max_sym = 0;
    for (auto s : zrle) {
        if (s < 512) zrle_hist[s]++;
        if (s > max_sym) max_sym = s;
    }
    printf("  Max symbol: %d\n", max_sym);
    printf("  RUNA(0): %zu (%.1f%%), RUNB(1): %zu (%.1f%%)\n",
           zrle_hist[0], 100.0 * zrle_hist[0] / zrle.size(),
           zrle_hist[1], 100.0 * zrle_hist[1] / zrle.size());

    // Theoretical minimum (just entropy * symbols)
    double zrle_entropy = 0;
    for (int i = 0; i <= max_sym; i++) {
        if (zrle_hist[i] > 0) {
            double p = (double)zrle_hist[i] / zrle.size();
            zrle_entropy -= p * log2(p);
        }
    }
    size_t theoretical_min = (size_t)(zrle.size() * zrle_entropy / 8);
    printf("  ZRLE entropy: %.3f bits/symbol\n", zrle_entropy);
    printf("  Theoretical minimum: %zu bytes\n", theoretical_min);

    // Stage 5: Multi-tree Huffman (our full compression)
    printf("\nStage 5: Full compression (our BWT v5)\n");
    auto compressed = bwt5::compress(data, n);
    printf("  mzip BWT: %zu bytes\n", compressed.size());

    // Compare with bzip2
    size_t bzip2_size = compress_bzip2(data, n);
    printf("  bzip2 -9: %zu bytes\n", bzip2_size);

    int64_t gap = (int64_t)compressed.size() - (int64_t)bzip2_size;
    double gap_pct = 100.0 * gap / bzip2_size;
    printf("\n  GAP: %+lld bytes (%+.2f%% vs bzip2)\n", (long long)gap, gap_pct);

    // Breakdown
    printf("\nBreakdown:\n");
    printf("  Theoretical min (ZRLE entropy): %zu bytes\n", theoretical_min);
    printf("  Our overhead vs theoretical: %zu bytes (%.1f%%)\n",
           compressed.size() - theoretical_min, 100.0 * (compressed.size() - theoretical_min) / theoretical_min);
    printf("  bzip2 overhead vs theoretical: %zu bytes (%.1f%%)\n",
           bzip2_size - theoretical_min, 100.0 * (bzip2_size - theoretical_min) / theoretical_min);
}

int main() {
    // Check if bzip2 is available
    if (!check_bzip2()) {
        printf("ERROR: bzip2 not found\n");
        printf("Expected at: %s\n", BZIP2_PATH);
        return 1;
    }
    printf("bzip2 found.\n");

    // Test BWT roundtrip on multiple data types
    printf("\n=== BWT Roundtrip Test ===\n");
    printf("%-20s %8s %8s %8s %8s %s\n", "Type", "Original", "mzip", "bzip2", "Ratio", "Roundtrip");
    printf("%-20s %8s %8s %8s %8s %s\n", "----", "--------", "----", "-----", "-----", "---------");

    std::vector<std::pair<std::string, std::function<std::vector<uint8_t>(size_t)>>> types = {
        {"Markdown", gen::markdown},
        {"GraphQL", gen::graphql},
        {"Unicode", gen::unicode_text},
        {"CSS", gen::css},
        {"Python", gen::python},
        {"Natural text", gen::natural_text},
    };

    for (auto& [name, gen_fn] : types) {
        auto data = gen_fn(262144);  // 256KB

        // Compress with mzip
        auto compressed = mzip::compress(data.data(), data.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);

        // Decompress and verify
        auto decompressed = mzip::decompress(compressed.data(), compressed.size());
        bool ok = (decompressed.size() == data.size() && memcmp(decompressed.data(), data.data(), data.size()) == 0);

        // Get bzip2 size for comparison
        size_t bzip2_size = compress_bzip2(data.data(), data.size());

        double ratio = (double)data.size() / compressed.size();
        printf("%-20s %8zu %8zu %8zu %7.1fx %s\n",
               name.c_str(), data.size(), compressed.size(), bzip2_size, ratio,
               ok ? "OK" : "FAIL");
    }

    return 0;
}
