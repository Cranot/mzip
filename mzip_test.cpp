// mzip_test.cpp - Quick testing tool using generators.hpp
// Usage:
//   mzip_test                  # Test all types at 64KB
//   mzip_test python           # Test specific type
//   mzip_test python 256k      # Test specific type and size
//   mzip_test --bwt            # Show BWT vs bzip2 gaps
//   mzip_test --list           # List available types

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "generators.hpp"
#include "mzip.hpp"
#include "bwt_compress_v5.hpp"

// External compressor (uses 7-Zip for bzip2)
size_t compress_bzip2(const std::vector<uint8_t>& data) {
    static int counter = 0;
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_test_%d.bin", counter);
    snprintf(out_name, sizeof(out_name), "_test_%d.bz2", counter++);

    FILE* f = fopen(bin_name, "wb");
    if (f) { fwrite(data.data(), 1, data.size(), f); fclose(f); }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "\"C:\\Program Files\\7-Zip\\7z.exe\" a -tbzip2 -mx=9 %s %s >nul 2>&1", out_name, bin_name);
    system(cmd);

    f = fopen(out_name, "rb");
    size_t sz = 0;
    if (f) { fseek(f, 0, SEEK_END); sz = ftell(f); fclose(f); }

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, out_name);
    system(cmd);
    return sz;
}

size_t parse_size(const char* s) {
    if (!s) return 65536;
    if (strcasecmp(s, "4k") == 0 || strcasecmp(s, "4kb") == 0) return 4096;
    if (strcasecmp(s, "16k") == 0 || strcasecmp(s, "16kb") == 0) return 16384;
    if (strcasecmp(s, "64k") == 0 || strcasecmp(s, "64kb") == 0) return 65536;
    if (strcasecmp(s, "256k") == 0 || strcasecmp(s, "256kb") == 0) return 262144;
    if (strcasecmp(s, "1m") == 0 || strcasecmp(s, "1mb") == 0) return 1048576;
    return 65536;
}

const char* size_name(size_t n) {
    if (n == 4096) return "4KB";
    if (n == 16384) return "16KB";
    if (n == 65536) return "64KB";
    if (n == 262144) return "256KB";
    if (n == 1048576) return "1MB";
    return "???";
}

void test_type(const gen::DataType& type, size_t size, bool show_bwt = false) {
    auto data = type.generator(size);
    size_t n = data.size();

    // zstd
    size_t zstd_bound = ZSTD_compressBound(n);
    std::vector<uint8_t> zstd_out(zstd_bound);
    size_t zstd_size = ZSTD_compress(zstd_out.data(), zstd_bound, data.data(), n, 19);

    // BWT raw
    auto bwt_out = bwt5::compress(data.data(), n);

    // mzip SMALL
    auto mzip_out = mzip::compress(data.data(), n, 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);

    // bzip2
    size_t bzip2_size = compress_bzip2(data);

    // Determine winner
    size_t best = std::min({zstd_size, bzip2_size, mzip_out.size()});
    const char* winner = "???";
    if (best == mzip_out.size()) winner = "mzip";
    else if (best == bzip2_size) winner = "bzip2";
    else winner = "zstd";

    double ratio = (double)n / mzip_out.size();

    printf("%-20s %6s: %6zu -> %6zu (%5.1fx) ", type.name.c_str(), size_name(n), n, mzip_out.size(), ratio);

    if (strcmp(winner, "mzip") == 0) {
        printf("[WIN]\n");
    } else {
        printf("[%s %+d]\n", winner, (int)(mzip_out.size() - best));
    }

    if (show_bwt) {
        printf("  -> zstd:19=%zu, BWT=%zu, bzip2=%zu\n", zstd_size, bwt_out.size(), bzip2_size);
        printf("  -> BWT vs bzip2: %+d bytes\n", (int)(bwt_out.size() - bzip2_size));
    }
}

void show_bwt_gaps(size_t size) {
    printf("=== BWT Gap Analysis at %s ===\n\n", size_name(size));
    printf("%-20s %8s %8s %8s %+8s\n", "Type", "BWT", "bzip2", "mzip", "BWT gap");
    printf("%-20s %8s %8s %8s %+8s\n", "----", "---", "-----", "----", "-------");

    for (const auto& type : gen::all_types()) {
        auto data = type.generator(size);
        auto bwt_out = bwt5::compress(data.data(), data.size());
        size_t bzip2_size = compress_bzip2(data);
        auto mzip_out = mzip::compress(data.data(), data.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);

        int gap = (int)(bwt_out.size() - bzip2_size);
        printf("%-20s %8zu %8zu %8zu %+8d%s\n",
               type.name.c_str(), bwt_out.size(), bzip2_size, mzip_out.size(), gap,
               gap > 0 ? " !" : "");
    }
}

int main(int argc, char** argv) {
    // Parse args
    bool list_types = false;
    bool show_bwt = false;
    const char* type_filter = nullptr;
    size_t size = 65536;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) list_types = true;
        else if (strcmp(argv[i], "--bwt") == 0) show_bwt = true;
        else if (argv[i][0] != '-') {
            if (!type_filter) type_filter = argv[i];
            else size = parse_size(argv[i]);
        }
    }

    if (list_types) {
        printf("Available types:\n");
        for (const auto& t : gen::all_types()) {
            printf("  %-20s (%s)\n", t.key.c_str(), t.category.c_str());
        }
        return 0;
    }

    if (show_bwt) {
        show_bwt_gaps(size);
        return 0;
    }

    printf("=== mzip Test at %s ===\n\n", size_name(size));

    if (type_filter) {
        // Single type
        auto gen_fn = gen::get(type_filter);
        if (!gen_fn) {
            printf("Unknown type: %s\n", type_filter);
            return 1;
        }
        for (const auto& t : gen::all_types()) {
            if (t.key == type_filter) {
                test_type(t, size, true);
                break;
            }
        }
    } else {
        // All types
        int wins = 0, losses = 0;
        for (const auto& t : gen::all_types()) {
            auto data = t.generator(size);
            auto mzip_out = mzip::compress(data.data(), data.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);
            size_t bzip2_size = compress_bzip2(data);

            size_t zstd_bound = ZSTD_compressBound(data.size());
            std::vector<uint8_t> zstd_out(zstd_bound);
            size_t zstd_size = ZSTD_compress(zstd_out.data(), zstd_bound, data.data(), data.size(), 19);

            size_t best = std::min({zstd_size, bzip2_size});
            if (mzip_out.size() <= best) wins++;
            else losses++;

            test_type(t, size, false);
        }
        printf("\n%d wins, %d losses (%.1f%%)\n", wins, losses, 100.0 * wins / (wins + losses));
    }

    return 0;
}
