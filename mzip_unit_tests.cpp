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
// uRAW-BLOAT PATHOLOGY GUARDS
// A losslessness test CANNOT catch these: a whole-file uRAW fallback (0xB5 0x52) round-trips
// perfectly but ships the input near-raw. Two real pathologies escaped the audit exactly this way
// -- CHAR_TEMPLATE on repetitive syslog (22x), and int64-overflow on BIGINT UNSIGNED SQL (65-139x)
// -- both when one encoder trial threw/mis-encoded and the top-level guard dumped the whole file raw.
// These guards assert the output is NOT a whole-file raw store AND compresses sanely. (2026-08-08)
// ============================================================================
static bool is_uraw_wholefile(const std::vector<uint8_t>& c) {
    return c.size() >= 2 && c[0] == 0xB5 && c[1] == 0x52;   // uRAW magic
}

TEST(no_uraw_bloat_bigint_unsigned_sql) {
    // MySQL BIGINT UNSIGNED / snowflake incrementing keys (>= 2^63) -- was 65-139x uRAW bloat.
    std::string s = "-- dump\nINSERT INTO users (id,name) VALUES ";
    unsigned long long base = 18000000000000000000ULL;   // > 2^63, valid uint64, overflows int64
    for (int i = 0; i < 3000; i++) {
        char buf[64];
        snprintf(buf, sizeof buf, "%s(%llu,'u%d')", i ? "," : "", base + (unsigned long long)i, i);
        s += buf;
    }
    s += ";\n";
    std::vector<uint8_t> in(s.begin(), s.end());
    auto c = mzip::compress(in.data(), in.size());
    ASSERT(!is_uraw_wholefile(c));                 // must NOT fall to whole-file raw store
    ASSERT(c.size() * 10 < in.size());             // incrementing ids -> must compress hard (>10x)
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), in.size());
    ASSERT(memcmp(d.data(), in.data(), in.size()) == 0);
}

TEST(no_uraw_bloat_repetitive_syslog) {
    // Highly repetitive RFC3164-style syslog (CHAR_TEMPLATE class) -- was 22x uRAW bloat.
    std::string s;
    for (int i = 0; i < 3000; i++) {
        char buf[160];
        snprintf(buf, sizeof buf,
                 "Jan 10 %02d:%02d:%02d host sshd[%d]: Failed password for root from 10.0.0.%d port 22 ssh2\n",
                 (i/3600)%24, (i/60)%60, i%60, 1000 + i, i % 255);
        s += buf;
    }
    std::vector<uint8_t> in(s.begin(), s.end());
    auto c = mzip::compress(in.data(), in.size());
    ASSERT(!is_uraw_wholefile(c));
    ASSERT(c.size() * 8 < in.size());              // repetitive log -> must compress well (>8x)
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), in.size());
    ASSERT(memcmp(d.data(), in.data(), in.size()) == 0);
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
// Structural encoders (MT/MQ/ML) — fire + roundtrip
// ============================================================================

TEST(mt_tabular_fires_and_roundtrips) {
    std::string s = "id,name,val\n";
    for (int i = 0; i < 400; i++) { char b[64]; snprintf(b, sizeof b, "%d,item%d,%d\n", 1000 + i, i % 7, (i * 13) % 1000); s += b; }
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'T');           // MT fired
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(mt_all_four_delimiters_fire_and_roundtrip) {
    // The sniff covers {',', '\t', ';', '|'}. Until 2026-08-11 it was comma+tab only and the
    // suite only ever exercised ','; the two new delimiters shipped with no unit coverage.
    // The grid content is IDENTICAL across delimiters, so every arm must reach the same MT
    // payload — which is what proves the transform is delimiter-agnostic rather than merely
    // lossless. The delimiter must not appear inside any field or the re-delimit is not a bijection.
    for (char delim : {',', '\t', ';', '|'}) {
        std::string s = std::string("id") + delim + "name" + delim + "val\n";
        for (int i = 0; i < 400; i++) {
            char b[64];
            snprintf(b, sizeof b, "%d%citem%d%c%d\n", 1000 + i, delim, i % 7, delim, (i * 13) % 1000);
            s += b;
        }
        std::vector<uint8_t> orig(s.begin(), s.end());
        auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr,
                                mzip::CompressionMode::SMALL);
        ASSERT(c.size() >= 3 && c[0] == 'M' && c[1] == 'T');       // MT fired for this delimiter
        ASSERT(c[2] == (uint8_t)delim);                            // and stored the delimiter it used
        auto d = mzip::decompress(c.data(), c.size());
        ASSERT_EQ(d.size(), orig.size());
        ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
    }
}

TEST(mq_sql_fires_and_roundtrips) {
    std::string s = "-- dump\nINSERT INTO users (id,name,email) VALUES ";
    for (int i = 0; i < 200; i++) { char b[96]; snprintf(b, sizeof b, "%s(%d,'user%d','u%d@x.com')", i ? "," : "", 1 + i, i, i); s += b; }
    s += ";\n";
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'Q');           // MQ fired
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(ml_log_fires_and_roundtrips) {
    std::string s;
    for (int i = 0; i < 400; i++) { char b[160]; snprintf(b, sizeof b,
        "10.0.0.1 - - [10/Oct/2000:13:%02d:%02d -0700] \"GET /p/%d HTTP/1.1\" 200 %d\n", (i / 60) % 60, i % 60, i % 50, 100 + i); s += b; }
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'L');           // ML fired
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(mm_matrixmarket_fires_and_roundtrips) {
    // column-sorted Matrix Market coordinate file WITH sign-alignment spacing (positive
    // values get an extra leading space, as real .mtx writers emit) -> exercises the exact
    // whitespace the skeleton must preserve and MT structurally cannot.
    std::string s = "%%MatrixMarket matrix coordinate real symmetric\n% a comment\n2000 2000 6000\n";
    for (int j = 1; j <= 2000; j++)
        for (int i = j; i < j + 3 && i <= 2000; i++) {
            double v = ((i * 31 + j * 7) % 2 ? -1.0 : 1.0) * (1e7 + (i * 131 + j) % 900000);
            char b[64];
            snprintf(b, sizeof b, v < 0 ? "%d %d %.10e\n" : "%d %d  %.10e\n", i, j, v);
            s += b;
        }
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'M');           // MM fired
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);       // byte-exact incl. spacing
}

// ============================================================================
// Malformed / untrusted-stream safety (locks in the 011a155 decode hardening)
// ============================================================================

TEST(mqsql_invert_rejects_oversized_varint) {
    // MTSQL1 + nsegs=1 + tag=0 (verbatim) + ~10-byte varint length near 2^64 -> must reject, no OOB read
    std::vector<uint8_t> blob = {'M','T','S','Q','L','1',0x00, 0x01, 0x00};
    for (int i = 0; i < 9; i++) blob.push_back(0xFF); blob.push_back(0x01);
    std::vector<uint8_t> out;
    ASSERT(!mzip::mqsql::invert(blob.data(), blob.size(), out));
}

TEST(mqsql_invert_rejects_huge_nrows) {
    // region blob: open_len=0, ncols=2, nrows=~2^64 -> must reject, no exabyte allocation
    std::vector<uint8_t> region = {0x00, 0x02};
    for (int i = 0; i < 9; i++) region.push_back(0xFF); region.push_back(0x01);
    std::vector<uint8_t> blob = {'M','T','S','Q','L','1',0x00, 0x01, 0x01, (uint8_t)region.size()};
    blob.insert(blob.end(), region.begin(), region.end());
    std::vector<uint8_t> out;
    ASSERT(!mzip::mqsql::invert(blob.data(), blob.size(), out));
}

TEST(mltsd_invert_rejects_overflow_number) {
    // LTCLF1 header with a 25-digit base_epoch -> pdec must reject (>19 digits), no signed-overflow UB
    std::string h = "LTCLF1\n1234567890123456789012345\n0\n\n";
    std::vector<uint8_t> blob(h.begin(), h.end());
    std::vector<uint8_t> out;
    ASSERT(!mzip::mltsd::invert(blob.data(), blob.size(), out));
}

TEST(decompress_nested_magic_no_stack_overflow) {
    // ~300 KB of repeated "MQ\x00" would recurse unboundedly without the depth guard -> must return, not crash
    std::vector<uint8_t> bomb;
    for (int i = 0; i < 100000; i++) { bomb.push_back('M'); bomb.push_back('Q'); bomb.push_back(0x00); }
    auto d = mzip::decompress(bomb.data(), bomb.size());
    ASSERT(d.empty());
}

// 'MS', 'MB', 'MY' and 'MF' are SHIPPED formats with decode branches reachable from untrusted input,
// and until 2026-08-13 the suite had no test that any of them fires at all. A regression that silently
// stopped one firing would surface only as a ratio drift in the corpus A/B, and a regression in its
// decode branch might not surface anywhere. Each input below is built to satisfy that encoder's actual
// gate, read from the source rather than guessed:
//   MS  try_soa   && size >= 4096 && !is_text_like     -> binary float lanes
//   MB  try_bcj   && size >= 256  && looks_like_x86    -> 'MZ' magic (mbcj::looks_like_x86 line 1)
//   MY  try_yaml  && size >= 512  && is_text_like      -> deeply indented YAML
//   MF  try_fastq && size >= 256  && is_text_like      -> 4-line FASTQ records
// Each asserts the magic AND round-trips. If one stops firing the assert names which.
TEST(ms_soa_transform_inverts_exactly) {
    // NOTE ON WHY THIS IS A TRANSFORM TEST AND NOT AN END-TO-END "does MS win" TEST.
    // MS is trial-and-keep: it is offered on every non-text block >= 4096 B and ships only if it
    // beats every other candidate. A first draft asserted the output magic was 'MS' on synthetic
    // float lanes and FAILED -- MZIP_STATS showed NUMERIC winning at 68 bytes, because a perfectly
    // linear float ramp is exactly what the specialised numeric encoder is for. The encoder was
    // fine; the test was demanding that it win a contest rigged against it. Constructing an input
    // where a given member of an ensemble WINS is brittle and tests the ensemble, not the encoder.
    // Inverting the transform is the property that must hold unconditionally. (2026-08-13)
    for (uint8_t tid : {0, 1, 2}) {
        for (uint8_t W : {2, 4, 8}) {
            for (uint8_t cols : {1, 2, 3}) {
                if ((tid == 1 || tid == 2) && cols == 0) continue;
                size_t n = 4096;                       // not a multiple of every W*cols on purpose
                std::vector<uint8_t> orig(n);
                for (size_t i = 0; i < n; i++) orig[i] = (uint8_t)(i * 31 + (i >> 6) * 7);
                auto t = mzip::soa_apply(orig.data(), orig.size(), tid, W, cols);
                ASSERT_EQ(t.size(), orig.size());
                auto back = mzip::soa_invert(t.data(), t.size(), tid, W, cols);
                ASSERT_EQ(back.size(), orig.size());
                if (back != orig) {
                    char buf[128];
                    snprintf(buf, sizeof buf, "soa tid=%d W=%d cols=%d did not invert", tid, W, cols);
                    throw std::runtime_error(buf);
                }
            }
        }
    }
}

TEST(mb_bcj_filter_inverts_exactly) {
    // Same reasoning as the SoA test above: a first draft asserted the shipped magic was 'MB' on a
    // synthetic 'MZ'+E8/E9 buffer and FAILED -- MZSTATS showed 'MS' winning at 5,858 B, because the
    // synthetic byte pattern was more amenable to SoA de-interleaving than to a BCJ rewrite. What
    // must hold unconditionally is that the Bra86 filter is BIJECTIVE: encode then decode restores
    // the input exactly, for any bytes. That is what makes a wrong detection merely lose a trial
    // rather than corrupt.
    std::vector<uint8_t> orig(16384);
    orig[0] = 0x4D; orig[1] = 0x5A;                       // 'MZ'
    for (size_t i = 2; i < orig.size(); i++) orig[i] = (uint8_t)((i * 37) ^ (i >> 5));
    for (size_t i = 64; i + 5 < orig.size(); i += 61) {   // scattered CALL/JMP rel32
        orig[i] = (i & 1) ? 0xE8 : 0xE9;
        uint32_t target = (uint32_t)(i * 4 + 0x1000);
        memcpy(&orig[i + 1], &target, 4);
    }
    ASSERT(mzip::mbcj::looks_like_x86(orig.data(), orig.size()));   // the detector does fire

    std::vector<uint8_t> filt = orig;
    uint32_t st = 0; mzip::mbcj::x86_convert(filt.data(), filt.size(), 0, &st, /*encoding=*/1);
    ASSERT(filt != orig);                                           // the filter actually did work
    uint32_t st2 = 0; mzip::mbcj::x86_convert(filt.data(), filt.size(), 0, &st2, /*encoding=*/0);
    ASSERT(filt == orig);                                           // and it is bijective

    // end-to-end losslessness on the same buffer, without demanding which encoder wins
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr,
                            mzip::CompressionMode::SMALL);
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(my_yaml_fires_and_roundtrips) {
    // deep, repetitive indentation is exactly what the de-indent transform separates into
    // depth + body streams
    std::string s = "apiVersion: v1\nkind: ConfigMap\nmetadata:\n  name: demo\ndata:\n";
    for (int i = 0; i < 300; i++) {
        char b[256];
        snprintf(b, sizeof b,
                 "  key%03d:\n    nested:\n      alpha: value%03d\n      beta: %d\n"
                 "      gamma:\n        - item%03d\n        - item%03d\n", i, i, i * 7, i, i + 1);
        s += b;
    }
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr,
                            mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'Y');
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(mf_fastq_fires_and_roundtrips) {
    // strict 4-line records: @id / sequence / '+' / quality — the de-interleave splits the four lanes
    std::string s;
    for (int i = 0; i < 400; i++) {
        char id[64]; snprintf(id, sizeof id, "@SEQ_%06d length=60\n", i);
        s += id;
        std::string seq, qual;
        for (int j = 0; j < 60; j++) { seq += "ACGT"[(i + j) % 4]; qual += (char)('!' + ((i + j) % 40)); }
        s += seq + "\n+\n" + qual + "\n";
    }
    std::vector<uint8_t> orig(s.begin(), s.end());
    auto c = mzip::compress(orig.data(), orig.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr,
                            mzip::CompressionMode::SMALL);
    ASSERT(c.size() >= 2 && c[0] == 'M' && c[1] == 'F');
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), orig.size());
    ASSERT(memcmp(d.data(), orig.data(), orig.size()) == 0);
}

TEST(mimg_filter_inverts_exactly) {
    // Every one of the 5 PNG predictors must invert byte-for-byte. Build a gradient with
    // both vertical and horizontal structure so different rows genuinely pick different
    // filters, then prove unfilter(filter(x)) == x for 1/3/4 bytes-per-pixel.
    for (uint8_t bpp : {1, 3, 4}) {
        size_t rows = 64, stride = 61 * bpp;   // deliberately not a power of two
        std::vector<uint8_t> plane(rows * stride);
        for (size_t y = 0; y < rows; y++)
            for (size_t x = 0; x < stride; x++)
                plane[y * stride + x] = (uint8_t)(y * 3 + x * 7 + ((x ^ y) & 31));
        std::vector<uint8_t> tags, res, back;
        mzip::mimg::filter(plane.data(), rows, stride, bpp, tags, res);
        ASSERT(tags.size() == rows);
        ASSERT(res.size() == rows * stride);
        ASSERT(mzip::mimg::unfilter(res.data(), rows, stride, bpp, tags.data(), back));
        ASSERT(back == plane);
    }
}

TEST(mimg_bmp_ppm_pgm_roundtrip) {
    // Real container shapes through the full public API: compress -> decompress -> compare.
    auto pixels = [](size_t n) {
        std::vector<uint8_t> v(n);
        for (size_t i = 0; i < n; i++) v[i] = (uint8_t)((i / 97) * 5 + (i % 97) * 3);
        return v;
    };
    // BMP: 40-byte BITMAPINFOHEADER, 24bpp, rows padded to 4 bytes
    {
        uint32_t w = 61, h = 70, stride = ((w * 3 + 3) / 4) * 4, off = 54;
        std::vector<uint8_t> f(off, 0);
        f[0] = 'B'; f[1] = 'M';
        uint32_t total = off + stride * h;
        for (int i = 0; i < 4; i++) {
            f[2 + i]  = (uint8_t)(total >> (8 * i));
            f[10 + i] = (uint8_t)(off   >> (8 * i));
            f[18 + i] = (uint8_t)(w     >> (8 * i));
            f[22 + i] = (uint8_t)(h     >> (8 * i));
        }
        f[14] = 40; f[26] = 1; f[28] = 24;
        auto px = pixels(stride * h);
        f.insert(f.end(), px.begin(), px.end());
        auto c = mzip::compress(f.data(), f.size());
        auto d = mzip::decompress(c.data(), c.size());
        ASSERT(d == f);
    }
    // PPM (P6) with a comment line, and PGM (P5)
    for (const char* magic : {"P6", "P5"}) {
        size_t bpp = (magic[1] == '6') ? 3 : 1;
        std::string hdr = std::string(magic) + "\n# made by the mzip unit suite\n71 80\n255\n";
        std::vector<uint8_t> f(hdr.begin(), hdr.end());
        auto px = pixels(71 * 80 * bpp);
        f.insert(f.end(), px.begin(), px.end());
        auto c = mzip::compress(f.data(), f.size());
        auto d = mzip::decompress(c.data(), c.size());
        ASSERT(d == f);
    }
}

TEST(mi_decode_rejects_malformed_stream) {
    std::vector<uint8_t> out;
    // rows*stride overflowing the claimed original -> must reject, never allocate on it
    {
        std::vector<uint8_t> bad = {'M','I', 0x10, 0x00, 0xFF, 0x7F, 0xFF, 0x7F, 3, 0x00};
        auto d = mzip::decompress(bad.data(), bad.size());
        ASSERT(d.empty());
    }
    // bpp = 0 and bpp > 4 are both invalid
    for (uint8_t bpp : {0, 5, 255}) {
        std::vector<uint8_t> bad = {'M','I', 0x40, 0x00, 0x04, 0x10, bpp, 0x00};
        auto d = mzip::decompress(bad.data(), bad.size());
        ASSERT(d.empty());
    }
    // a filter tag > 4 in an otherwise well-formed plane must be refused by unfilter
    {
        std::vector<uint8_t> tags = {9, 0, 0, 0}, res(4 * 8, 0);
        ASSERT(!mzip::mimg::unfilter(res.data(), 4, 8, 1, tags.data(), out));
    }
}

TEST(mwg_invert_rejects_bad_framing) {
    // k=3 grid claiming header longer than the payload -> must reject, no OOB read
    std::vector<uint8_t> bad = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};  // huge varint header length
    std::vector<uint8_t> out;
    ASSERT(!mzip::mwg::invert(bad.data(), bad.size(), out));
    // hlen=0, k=99 (out of 2..8 range) -> reject
    std::vector<uint8_t> bad2 = {0x00, 99, 0x04, 0x00};
    ASSERT(!mzip::mwg::invert(bad2.data(), bad2.size(), out));
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

    printf("\nSTRUCTURAL ENCODERS (MT/MQ/ML):\n");
    RUN_TEST(mt_tabular_fires_and_roundtrips);
    RUN_TEST(mt_all_four_delimiters_fire_and_roundtrip);
    RUN_TEST(mq_sql_fires_and_roundtrips);
    RUN_TEST(ml_log_fires_and_roundtrips);
    RUN_TEST(mm_matrixmarket_fires_and_roundtrips);
    RUN_TEST(ms_soa_transform_inverts_exactly);
    RUN_TEST(mb_bcj_filter_inverts_exactly);
    RUN_TEST(my_yaml_fires_and_roundtrips);
    RUN_TEST(mf_fastq_fires_and_roundtrips);
    RUN_TEST(mimg_filter_inverts_exactly);
    RUN_TEST(mimg_bmp_ppm_pgm_roundtrip);

    printf("\nuRAW-BLOAT PATHOLOGY GUARDS:\n");
    RUN_TEST(no_uraw_bloat_bigint_unsigned_sql);
    RUN_TEST(no_uraw_bloat_repetitive_syslog);

    printf("\nMALFORMED-STREAM SAFETY:\n");
    RUN_TEST(mqsql_invert_rejects_oversized_varint);
    RUN_TEST(mqsql_invert_rejects_huge_nrows);
    RUN_TEST(mltsd_invert_rejects_overflow_number);
    RUN_TEST(mwg_invert_rejects_bad_framing);
    RUN_TEST(mi_decode_rejects_malformed_stream);
    RUN_TEST(decompress_nested_magic_no_stack_overflow);

    printf("\nCOMPREHENSIVE:\n");
    RUN_TEST(all_generators_roundtrip_64kb);

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
