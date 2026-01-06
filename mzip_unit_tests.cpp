// mzip_unit_tests.cpp - Unit tests for mzip compression strategies
// Usage: mzip_unit_tests.exe
// Tests: LINEAR_GEN, CHAR_TEMPLATE, BWT_TEXT, COLUMNAR
//
// Compile:
//   ../mingw64/bin/g++.exe -O3 -march=native -I./zstd_release/zstd-v1.5.6-win64/include \
//     -L./zstd_release/zstd-v1.5.6-win64/dll -o mzip_unit_tests.exe mzip_unit_tests.cpp libsais.c -lzstd

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <functional>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"
#include "generators.hpp"

// Simple test framework
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("  %-50s ", #name); \
    fflush(stdout); \
    try { \
        test_##name(); \
        printf("[PASS]\n"); \
        tests_passed++; \
    } catch (const std::exception& e) { \
        printf("[FAIL] %s\n", e.what()); \
        tests_failed++; \
    } catch (...) { \
        printf("[FAIL] Unknown exception\n"); \
        tests_failed++; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "Expected %lld == %lld", (long long)(a), (long long)(b)); \
        throw std::runtime_error(buf); \
    } \
} while(0)

// ============================================================================
// LINEAR_GEN Tests
// ============================================================================

TEST(linear_gen_detection_32bit_sequential) {
    // Create sequential 32-bit IDs: 1000, 1001, 1002, ...
    std::vector<uint8_t> data(256);
    uint32_t* p = (uint32_t*)data.data();
    for (size_t i = 0; i < 64; i++) p[i] = 1000 + i;

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(detected);
    ASSERT_EQ(params.elem_size, 4);
    ASSERT_EQ(params.base, 1000);
    ASSERT_EQ(params.delta, 1);
    ASSERT_EQ(params.count, 64);
}

TEST(linear_gen_detection_64bit_db_ids) {
    // Create 64-bit database IDs: 1000000000000, 1000000000001, ...
    std::vector<uint8_t> data(512);
    uint64_t* p = (uint64_t*)data.data();
    for (size_t i = 0; i < 64; i++) p[i] = 1000000000000ULL + i;

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(detected);
    ASSERT_EQ(params.elem_size, 8);
    ASSERT_EQ(params.base, 1000000000000LL);
    ASSERT_EQ(params.delta, 1);
    ASSERT_EQ(params.count, 64);
}

TEST(linear_gen_detection_negative_delta) {
    // Create countdown: 100, 99, 98, ...
    std::vector<uint8_t> data(256);
    uint32_t* p = (uint32_t*)data.data();
    for (size_t i = 0; i < 64; i++) p[i] = 100 - i;

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(detected);
    ASSERT_EQ(params.elem_size, 4);
    ASSERT_EQ(params.base, 100);
    ASSERT_EQ(params.delta, -1);
}

TEST(linear_gen_detection_larger_delta) {
    // Create sequence with delta=10: 0, 10, 20, 30, ...
    std::vector<uint8_t> data(256);
    uint32_t* p = (uint32_t*)data.data();
    for (size_t i = 0; i < 64; i++) p[i] = i * 10;

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(detected);
    ASSERT_EQ(params.delta, 10);
}

TEST(linear_gen_rejects_random) {
    // Random data should not be detected as linear
    auto data = gen::random_bytes(256);

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(!detected);
}

TEST(linear_gen_rejects_constant) {
    // Constant fill should be handled by PERIODIC, not LINEAR_GEN
    std::vector<uint8_t> data(256);
    uint32_t* p = (uint32_t*)data.data();
    for (size_t i = 0; i < 64; i++) p[i] = 42;

    mzip::LinearGenParams params;
    bool detected = mzip::detect_linear_gen(data.data(), data.size(), params);

    ASSERT(!detected);  // delta=0 should be rejected
}

TEST(linear_gen_roundtrip_small) {
    // Create sequential 32-bit IDs
    std::vector<uint8_t> original(256);
    uint32_t* p = (uint32_t*)original.data();
    for (size_t i = 0; i < 64; i++) p[i] = 1000 + i;

    // Compress
    auto compressed = mzip::compress(original.data(), original.size());

    // Decompress
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(linear_gen_roundtrip_1mb) {
    // Use generator for 1MB db_ids
    auto original = gen::db_ids(1048576);

    // Compress
    auto compressed = mzip::compress(original.data(), original.size());

    // Decompress
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(linear_gen_compression_ratio) {
    // 256KB of sequential IDs should compress to < 50 bytes
    auto original = gen::db_ids(262144);
    auto compressed = mzip::compress(original.data(), original.size());

    ASSERT(compressed.size() < 50);  // ~17-20 bytes expected

    // Verify roundtrip
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());
    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

// ============================================================================
// CHAR_TEMPLATE Tests
// ============================================================================

TEST(char_template_roundtrip_metrics) {
    // Metrics log has fixed structure per line
    auto original = gen::metrics(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(char_template_roundtrip_access_log) {
    auto original = gen::access_log(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(char_template_roundtrip_various_sizes) {
    const size_t sizes[] = {4096, 16384, 65536, 262144};

    for (size_t size : sizes) {
        auto original = gen::metrics(size);
        auto compressed = mzip::compress(original.data(), original.size());
        auto decompressed = mzip::decompress(compressed.data(), compressed.size());

        ASSERT_EQ(decompressed.size(), original.size());
        if (memcmp(decompressed.data(), original.data(), original.size()) != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Roundtrip failed at size %zu", size);
            throw std::runtime_error(buf);
        }
    }
}

// ============================================================================
// BWT_TEXT Tests
// ============================================================================

TEST(bwt_text_roundtrip_natural) {
    auto original = gen::natural_text(65536);

    auto compressed = mzip::compress(original.data(), original.size(), 19,
                                      mzip::DEFAULT_BLOCK_SIZE, nullptr,
                                      mzip::CompressionMode::SMALL);
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(bwt_text_roundtrip_markdown) {
    auto original = gen::markdown(65536);

    auto compressed = mzip::compress(original.data(), original.size(), 19,
                                      mzip::DEFAULT_BLOCK_SIZE, nullptr,
                                      mzip::CompressionMode::SMALL);
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(bwt_text_roundtrip_python_code) {
    auto original = gen::python(65536);

    auto compressed = mzip::compress(original.data(), original.size(), 19,
                                      mzip::DEFAULT_BLOCK_SIZE, nullptr,
                                      mzip::CompressionMode::SMALL);
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(bwt_text_roundtrip_various_sizes) {
    const size_t sizes[] = {4096, 16384, 65536, 262144, 1048576};

    for (size_t size : sizes) {
        auto original = gen::natural_text(size);
        auto compressed = mzip::compress(original.data(), original.size(), 19,
                                          mzip::DEFAULT_BLOCK_SIZE, nullptr,
                                          mzip::CompressionMode::SMALL);
        auto decompressed = mzip::decompress(compressed.data(), compressed.size());

        ASSERT_EQ(decompressed.size(), original.size());
        if (memcmp(decompressed.data(), original.data(), original.size()) != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "BWT roundtrip failed at size %zu", size);
            throw std::runtime_error(buf);
        }
    }
}

// ============================================================================
// COLUMNAR Tests
// ============================================================================

TEST(columnar_detection_nginx_log) {
    auto data = gen::nginx_log(65536);

    mzip::ColumnarParams params;
    bool detected = mzip::detect_columnar_log(data.data(), data.size(), params);

    ASSERT(detected);
    ASSERT(params.columns.size() >= 8);  // nginx has 9 columns
}

TEST(columnar_roundtrip_nginx) {
    auto original = gen::nginx_log(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(columnar_roundtrip_various_sizes) {
    const size_t sizes[] = {16384, 65536, 262144};

    for (size_t size : sizes) {
        auto original = gen::nginx_log(size);
        auto compressed = mzip::compress(original.data(), original.size());
        auto decompressed = mzip::decompress(compressed.data(), compressed.size());

        ASSERT_EQ(decompressed.size(), original.size());
        if (memcmp(decompressed.data(), original.data(), original.size()) != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "COLUMNAR roundtrip failed at size %zu", size);
            throw std::runtime_error(buf);
        }
    }
}

// ============================================================================
// TEMPLATE (ML_TEMPLATE / SECTION_TEMPLATE) Tests
// ============================================================================

TEST(template_roundtrip_javascript) {
    auto original = gen::javascript(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(template_roundtrip_terraform) {
    auto original = gen::terraform(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

// ============================================================================
// Edge Cases and Regression Tests
// ============================================================================

TEST(empty_input) {
    std::vector<uint8_t> empty;
    auto compressed = mzip::compress(empty.data(), 0);
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), 0);
}

TEST(small_input_1byte) {
    std::vector<uint8_t> data = {0x42};
    auto compressed = mzip::compress(data.data(), data.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), 1);
    ASSERT_EQ(decompressed[0], 0x42);
}

TEST(small_input_10bytes) {
    std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto compressed = mzip::compress(data.data(), data.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), 10);
    ASSERT(memcmp(decompressed.data(), data.data(), data.size()) == 0);
}

TEST(random_data_roundtrip) {
    auto original = gen::random_bytes(65536);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(all_zeros_roundtrip) {
    std::vector<uint8_t> original(65536, 0);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

TEST(all_0xff_roundtrip) {
    std::vector<uint8_t> original(65536, 0xff);

    auto compressed = mzip::compress(original.data(), original.size());
    auto decompressed = mzip::decompress(compressed.data(), compressed.size());

    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT(memcmp(decompressed.data(), original.data(), original.size()) == 0);
}

// ============================================================================
// Comprehensive Roundtrip (all generators)
// ============================================================================

TEST(all_generators_roundtrip_64kb) {
    for (const auto& type : gen::all_types()) {
        auto original = type.generator(65536);
        auto compressed = mzip::compress(original.data(), original.size());
        auto decompressed = mzip::decompress(compressed.data(), compressed.size());

        if (decompressed.size() != original.size() ||
            memcmp(decompressed.data(), original.data(), original.size()) != 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Roundtrip failed for %s at 64KB", type.name.c_str());
            throw std::runtime_error(buf);
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== mzip Unit Tests ===\n\n");

    printf("LINEAR_GEN:\n");
    RUN_TEST(linear_gen_detection_32bit_sequential);
    RUN_TEST(linear_gen_detection_64bit_db_ids);
    RUN_TEST(linear_gen_detection_negative_delta);
    RUN_TEST(linear_gen_detection_larger_delta);
    RUN_TEST(linear_gen_rejects_random);
    RUN_TEST(linear_gen_rejects_constant);
    RUN_TEST(linear_gen_roundtrip_small);
    RUN_TEST(linear_gen_roundtrip_1mb);
    RUN_TEST(linear_gen_compression_ratio);

    printf("\nCHAR_TEMPLATE:\n");
    RUN_TEST(char_template_roundtrip_metrics);
    RUN_TEST(char_template_roundtrip_access_log);
    RUN_TEST(char_template_roundtrip_various_sizes);

    printf("\nBWT_TEXT:\n");
    RUN_TEST(bwt_text_roundtrip_natural);
    RUN_TEST(bwt_text_roundtrip_markdown);
    RUN_TEST(bwt_text_roundtrip_python_code);
    RUN_TEST(bwt_text_roundtrip_various_sizes);

    printf("\nCOLUMNAR:\n");
    RUN_TEST(columnar_detection_nginx_log);
    RUN_TEST(columnar_roundtrip_nginx);
    RUN_TEST(columnar_roundtrip_various_sizes);

    printf("\nTEMPLATE:\n");
    RUN_TEST(template_roundtrip_javascript);
    RUN_TEST(template_roundtrip_terraform);

    printf("\nEDGE CASES:\n");
    RUN_TEST(empty_input);
    RUN_TEST(small_input_1byte);
    RUN_TEST(small_input_10bytes);
    RUN_TEST(random_data_roundtrip);
    RUN_TEST(all_zeros_roundtrip);
    RUN_TEST(all_0xff_roundtrip);

    printf("\nCOMPREHENSIVE:\n");
    RUN_TEST(all_generators_roundtrip_64kb);

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
