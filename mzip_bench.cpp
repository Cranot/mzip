// Unified mzip benchmark tool
// Version: 0.6
//
// Methodology:
//   - Data generators use seed 42 for reproducibility
//   - mzip/zstd: in-process timing (direct API calls)
//   - External tools: system() calls (includes process spawn overhead)
//   - Timing: median of N iterations (configurable, default=3)
//   - LZ4: uses --best (HC mode) for ratio, not speed
//   - Snappy/LZ4 excluded from "best_other" (speed-focused, not ratio-focused)
//
// Usage:
//   mzip_bench                    # Full benchmark (summary-only by default)
//   mzip_bench --quick            # Quick: 64KB only
//   mzip_bench --type graphql     # Single type, all sizes
//   mzip_bench --size 256k        # All types, single size
//   mzip_bench --list             # List available types
//   mzip_bench -o results.txt     # Save output to file
//   mzip_bench --save             # Auto-save to bench_YYYYMMDD_HHMMSS.txt
//   mzip_bench -v                 # Verbose: show per-test output

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdarg>
#include <ctime>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "generators.hpp"
#include "mzip.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <random>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <chrono>
#include <functional>

// ============================================================================
// OUTPUT HANDLING - Write to both stdout and optional file
// ============================================================================

static FILE* g_output_file = nullptr;

// Printf wrapper that outputs to both stdout and optional file
void out(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (g_output_file) {
        va_list args2;
        va_start(args2, fmt);
        vfprintf(g_output_file, fmt, args2);
        va_end(args2);
    }
}

// Flush both outputs
void out_flush() {
    fflush(stdout);
    if (g_output_file) fflush(g_output_file);
}

// Generate timestamped filename
std::string generate_output_filename() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "bench_%Y%m%d_%H%M%S.txt", t);
    return buf;
}

// ============================================================================
// EXTERNAL COMPRESSION TOOLS
// ============================================================================

static int file_counter = 0;
static int g_iterations = 3;  // Number of iterations for timing (default: 3)
static int g_warmup = 0;      // Warmup iterations before timing (0 = disabled for faster runs)

// Helper to get median of a vector
static double median(std::vector<double>& v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 0) {
        return (v[n/2 - 1] + v[n/2]) / 2.0;
    }
    return v[n/2];
}

struct CompressResult {
    size_t size;
    double time_ms;
};

CompressResult compress_with_tool_timed(const std::vector<uint8_t>& data, const char* ext, const char* cmd_fmt) {
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_temp_%d.bin", file_counter);
    snprintf(out_name, sizeof(out_name), "_temp_%d.%s", file_counter, ext);
    file_counter++;

    std::ofstream out(bin_name, std::ios::binary);
    out.write((char*)data.data(), data.size());
    out.close();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    snprintf(cmd, sizeof(cmd), cmd_fmt, out_name, bin_name);

    auto start = std::chrono::high_resolution_clock::now();
    system(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::ifstream in(out_name, std::ios::binary | std::ios::ate);
    size_t sz = in.good() ? (size_t)in.tellg() : 0;
    in.close();

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, out_name);
    system(cmd);
    return {sz, time_ms};
}

CompressResult compress_7z(const std::vector<uint8_t>& data) {
    return compress_with_tool_timed(data, "7z",
        "\"C:\\Program Files\\7-Zip\\7z.exe\" a -t7z -mx=9 %s %s > nul 2>&1");
}

CompressResult compress_rar(const std::vector<uint8_t>& data) {
    return compress_with_tool_timed(data, "rar",
        "\"C:\\Program Files\\WinRAR\\Rar.exe\" a -m5 -ep %s %s > nul 2>&1");
}

CompressResult compress_gzip(const std::vector<uint8_t>& data) {
    return compress_with_tool_timed(data, "gz",
        "\"C:\\Program Files\\7-Zip\\7z.exe\" a -tgzip -mx=9 %s %s > nul 2>&1");
}

CompressResult compress_bzip2(const std::vector<uint8_t>& data) {
    return compress_with_tool_timed(data, "bz2",
        "\"C:\\Program Files\\7-Zip\\7z.exe\" a -tbzip2 -mx=9 %s %s > nul 2>&1");
}

CompressResult compress_xz(const std::vector<uint8_t>& data) {
    return compress_with_tool_timed(data, "xz",
        "\"C:\\Program Files\\7-Zip\\7z.exe\" a -txz -mx=9 %s %s > nul 2>&1");
}

CompressResult compress_brotli(const std::vector<uint8_t>& data) {
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_temp_%d.bin", file_counter);
    snprintf(out_name, sizeof(out_name), "_temp_%d.br", file_counter);
    file_counter++;

    std::ofstream out(bin_name, std::ios::binary);
    out.write((char*)data.data(), data.size());
    out.close();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "brotli.exe -q 11 -o %s %s > nul 2>&1", out_name, bin_name);

    auto start = std::chrono::high_resolution_clock::now();
    system(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::ifstream in(out_name, std::ios::binary | std::ios::ate);
    size_t sz = in.good() ? (size_t)in.tellg() : 0;
    in.close();

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, out_name);
    system(cmd);
    return {sz, time_ms};
}

CompressResult compress_lz4(const std::vector<uint8_t>& data) {
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_temp_%d.bin", file_counter);
    snprintf(out_name, sizeof(out_name), "_temp_%d.lz4", file_counter);
    file_counter++;

    std::ofstream out(bin_name, std::ios::binary);
    out.write((char*)data.data(), data.size());
    out.close();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    // LZ4 HC mode (--best = -12) for maximum compression
    // Note: Still much faster than ratio-focused compressors
    snprintf(cmd, sizeof(cmd), "lz4.exe --best -f %s %s > nul 2>&1", bin_name, out_name);

    auto start = std::chrono::high_resolution_clock::now();
    system(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::ifstream in(out_name, std::ios::binary | std::ios::ate);
    size_t sz = in.good() ? (size_t)in.tellg() : 0;
    in.close();

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, out_name);
    system(cmd);
    return {sz, time_ms};
}

CompressResult compress_zpaq(const std::vector<uint8_t>& data) {
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_temp_%d.bin", file_counter);
    snprintf(out_name, sizeof(out_name), "_temp_%d.zpaq", file_counter);
    file_counter++;

    std::ofstream out(bin_name, std::ios::binary);
    out.write((char*)data.data(), data.size());
    out.close();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    // ZPAQ with method 5 (best compression, very slow)
    snprintf(cmd, sizeof(cmd), "zpaq.exe a %s %s -m5 > nul 2>&1", out_name, bin_name);

    auto start = std::chrono::high_resolution_clock::now();
    system(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::ifstream in(out_name, std::ios::binary | std::ios::ate);
    size_t sz = in.good() ? (size_t)in.tellg() : 0;
    in.close();

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, out_name);
    system(cmd);
    return {sz, time_ms};
}

CompressResult compress_snappy(const std::vector<uint8_t>& data) {
    char bin_name[64], out_name[64];
    snprintf(bin_name, sizeof(bin_name), "_temp_%d.bin", file_counter);
    snprintf(out_name, sizeof(out_name), "_temp_%d.sz", file_counter);
    file_counter++;

    std::ofstream out(bin_name, std::ios::binary);
    out.write((char*)data.data(), data.size());
    out.close();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "del /q %s 2>nul", out_name);
    system(cmd);
    // snzip from https://github.com/kubo/snzip
    snprintf(cmd, sizeof(cmd), "snzip.exe -k %s > nul 2>&1", bin_name);

    auto start = std::chrono::high_resolution_clock::now();
    system(cmd);
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // snzip creates bin_name.sz
    char actual_out[80];
    snprintf(actual_out, sizeof(actual_out), "%s.sz", bin_name);
    std::ifstream in(actual_out, std::ios::binary | std::ios::ate);
    size_t sz = in.good() ? (size_t)in.tellg() : 0;
    in.close();

    snprintf(cmd, sizeof(cmd), "del /q %s %s 2>nul", bin_name, actual_out);
    system(cmd);
    return {sz, time_ms};
}

// Data generators are now in generators.hpp (use gen::all_types())

// ============================================================================
// SIZE PARSING
// ============================================================================

struct SizeSpec {
    size_t bytes;
    const char* name;
};

std::vector<SizeSpec> ALL_SIZES = {
    {4096, "4KB"},
    {16384, "16KB"},
    {65536, "64KB"},
    {262144, "256KB"},
    {1048576, "1MB"}
};

size_t parse_size(const std::string& s) {
    if (s == "4k" || s == "4K" || s == "4kb" || s == "4KB") return 4096;
    if (s == "16k" || s == "16K" || s == "16kb" || s == "16KB") return 16384;
    if (s == "64k" || s == "64K" || s == "64kb" || s == "64KB") return 65536;
    if (s == "256k" || s == "256K" || s == "256kb" || s == "256KB") return 262144;
    if (s == "1m" || s == "1M" || s == "1mb" || s == "1MB") return 1048576;
    return 0;
}

const char* size_name(size_t n) {
    for (auto& s : ALL_SIZES) if (s.bytes == n) return s.name;
    return "???";
}

// ============================================================================
// BENCHMARK RESULT
// ============================================================================

struct BenchResult {
    std::string name;
    std::string size_name;
    size_t original;
    size_t mzip, zstd19, gzip, bzip2, xz, brotli, sz7, rar, lz4, snappy;
    double time_mzip, time_zstd, time_gzip, time_bzip2, time_xz, time_brotli, time_7z, time_rar, time_lz4, time_snappy;
    double decomp_mzip, decomp_zstd;  // Decompression times in ms
    bool verified;
    const char* detected;
    double mzip_advantage;  // best_other / mzip (>1 = mzip wins)
    double mzip_speed;      // MB/s for mzip compression
    double mzip_decomp_speed;  // MB/s for mzip decompression
};

// ============================================================================
// BENCHMARK FUNCTIONS
// ============================================================================

BenchResult benchmark_one(const gen::DataType& dt, size_t size, bool use_external = true) {
    auto data = dt.generator(size);
    BenchResult r;
    r.name = dt.name;
    r.size_name = size_name(size);
    r.original = data.size();

    // Pre-allocate buffers for iterations
    std::vector<uint8_t> zbuf(ZSTD_compressBound(data.size()));
    std::vector<uint8_t> decomp_buf(data.size());
    std::vector<uint8_t> mzip_comp;

    std::vector<double> mzip_times, mzip_decomp_times;
    std::vector<double> zstd_times, zstd_decomp_times;

    // Warmup iterations (not timed)
    // Note: Using BALANCED mode instead of SMALL to avoid BWT trial compression bug
    for (int w = 0; w < g_warmup; w++) {
        mzip_comp = mzip::compress(data.data(), data.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::BALANCED);
        auto decomp = mzip::decompress(mzip_comp.data(), mzip_comp.size());
        ZSTD_compress(zbuf.data(), zbuf.size(), data.data(), data.size(), 19);
        (void)decomp;  // Suppress unused warning
    }

    // Timed iterations for mzip
    for (int i = 0; i < g_iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        mzip_comp = mzip::compress(data.data(), data.size(), 19, mzip::DEFAULT_BLOCK_SIZE, nullptr, mzip::CompressionMode::BALANCED);
        auto end = std::chrono::high_resolution_clock::now();
        mzip_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        auto decomp = mzip::decompress(mzip_comp.data(), mzip_comp.size());
        end = std::chrono::high_resolution_clock::now();
        mzip_decomp_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());

        // Verify on last iteration
        if (i == g_iterations - 1) {
            r.verified = (decomp.size() == data.size() && memcmp(decomp.data(), data.data(), data.size()) == 0);
        }
    }

    r.mzip = mzip_comp.size();
    r.time_mzip = median(mzip_times);
    r.decomp_mzip = median(mzip_decomp_times);
    r.mzip_speed = (r.time_mzip > 0) ? (data.size() / 1024.0 / 1024.0) / (r.time_mzip / 1000.0) : 0;
    r.mzip_decomp_speed = (r.decomp_mzip > 0) ? (data.size() / 1024.0 / 1024.0) / (r.decomp_mzip / 1000.0) : 0;

    auto analysis = mzip::analyze_block(data.data(), data.size());
    r.detected = mzip::block_type_name(analysis.type);

    // Timed iterations for zstd
    for (int i = 0; i < g_iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        r.zstd19 = ZSTD_compress(zbuf.data(), zbuf.size(), data.data(), data.size(), 19);
        auto end = std::chrono::high_resolution_clock::now();
        zstd_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        ZSTD_decompress(decomp_buf.data(), decomp_buf.size(), zbuf.data(), r.zstd19);
        end = std::chrono::high_resolution_clock::now();
        zstd_decomp_times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    r.time_zstd = median(zstd_times);
    r.decomp_zstd = median(zstd_decomp_times);

    // External compressors
    if (use_external) {
        auto res_gzip = compress_gzip(data);
        r.gzip = res_gzip.size;
        r.time_gzip = res_gzip.time_ms;

        auto res_bzip2 = compress_bzip2(data);
        r.bzip2 = res_bzip2.size;
        r.time_bzip2 = res_bzip2.time_ms;

        auto res_xz = compress_xz(data);
        r.xz = res_xz.size;
        r.time_xz = res_xz.time_ms;

        auto res_brotli = compress_brotli(data);
        r.brotli = res_brotli.size;
        r.time_brotli = res_brotli.time_ms;

        auto res_7z = compress_7z(data);
        r.sz7 = res_7z.size;
        r.time_7z = res_7z.time_ms;

        auto res_rar = compress_rar(data);
        r.rar = res_rar.size;
        r.time_rar = res_rar.time_ms;

        auto res_lz4 = compress_lz4(data);
        r.lz4 = res_lz4.size;
        r.time_lz4 = res_lz4.time_ms;

        auto res_snappy = compress_snappy(data);
        r.snappy = res_snappy.size;
        r.time_snappy = res_snappy.time_ms;
    } else {
        r.gzip = r.bzip2 = r.xz = r.brotli = r.sz7 = r.rar = r.lz4 = r.snappy = 0;
        r.time_gzip = r.time_bzip2 = r.time_xz = r.time_brotli = r.time_7z = r.time_rar = r.time_lz4 = r.time_snappy = 0;
    }

    // Calculate advantage (lz4 excluded - it's speed optimized, not ratio optimized)
    size_t best_other = r.zstd19;
    if (r.gzip > 0 && r.gzip < best_other) best_other = r.gzip;
    if (r.bzip2 > 0 && r.bzip2 < best_other) best_other = r.bzip2;
    if (r.xz > 0 && r.xz < best_other) best_other = r.xz;
    if (r.brotli > 0 && r.brotli < best_other) best_other = r.brotli;
    if (r.sz7 > 0 && r.sz7 < best_other) best_other = r.sz7;
    if (r.rar > 0 && r.rar < best_other) best_other = r.rar;
    // Note: lz4 not included in best_other - it prioritizes speed over ratio
    r.mzip_advantage = (double)best_other / r.mzip;

    return r;
}

// Global flag for detailed output mode
static bool g_detailed_output = false;

void print_result(const BenchResult& r, bool show_external = true) {
    bool win = r.mzip_advantage >= 1.0;

    if (g_detailed_output) {
        // Detailed mode: show all compressor sizes and times
        out("\n  === %s (%s: %zu bytes) ===\n", r.name.c_str(), r.size_name.c_str(), r.original);
        out("  %-12s %8s %8s %8s\n", "Compressor", "Size", "Ratio", "Time(ms)");
        out("  %-12s %8s %8s %8s\n", "----------", "----", "-----", "--------");

        // Collect all results
        struct CompResult { const char* name; size_t size; double time; };
        CompResult all[] = {
            {"mzip", r.mzip, r.time_mzip},
            {"zstd:19", r.zstd19, r.time_zstd},
            {"gzip:9", r.gzip, r.time_gzip},
            {"bzip2:9", r.bzip2, r.time_bzip2},
            {"xz:9", r.xz, r.time_xz},
            {"brotli:11", r.brotli, r.time_brotli},
            {"7z:mx9", r.sz7, r.time_7z},
            {"rar:m5", r.rar, r.time_rar},
            {"lz4:HC", r.lz4, r.time_lz4},
            {"snappy", r.snappy, r.time_snappy}
        };

        // Find identical results
        for (int i = 0; i < 10; i++) {
            if (all[i].size == 0) continue;
            double ratio = (double)r.original / all[i].size;
            const char* marker = (i == 0 && win) ? " [WIN]" : "";

            // Check for identical results with other compressors
            std::string identical;
            for (int j = i + 1; j < 10; j++) {
                if (all[j].size == all[i].size) {
                    if (!identical.empty()) identical += ",";
                    identical += all[j].name;
                }
            }

            if (!identical.empty()) {
                out("  %-12s %8zu %7.1fx %8.1f  (==%s)\n",
                       all[i].name, all[i].size, ratio, all[i].time, identical.c_str());
            } else {
                out("  %-12s %8zu %7.1fx %8.1f%s\n",
                       all[i].name, all[i].size, ratio, all[i].time, marker);
            }
        }
        out("  %s\n", r.verified ? "Verified OK" : "** VERIFICATION FAILED **");
    } else {
        // Normal mode
        out("  %-20s %-5s %6zu -> %5zu (%6.1fx) %s %s\n",
               r.name.c_str(), r.size_name.c_str(),
               r.original, r.mzip, (double)r.original / r.mzip,
               win ? "[WIN]" : "     ",
               r.verified ? "" : "[FAIL]");
    }
}

void print_summary(std::vector<BenchResult>& results) {
    // Sort by advantage
    std::sort(results.begin(), results.end(),
              [](const BenchResult& a, const BenchResult& b) { return a.mzip_advantage > b.mzip_advantage; });

    int wins = 0, fails = 0;
    double total_mzip_time = 0, total_zstd_time = 0;
    double total_gzip_time = 0, total_bzip2_time = 0, total_xz_time = 0;
    double total_brotli_time = 0, total_7z_time = 0, total_rar_time = 0, total_lz4_time = 0, total_snappy_time = 0;
    size_t total_original = 0;

    for (auto& r : results) {
        if (r.mzip_advantage >= 1.0) wins++;
        if (!r.verified) fails++;
        total_mzip_time += r.time_mzip;
        total_zstd_time += r.time_zstd;
        total_gzip_time += r.time_gzip;
        total_bzip2_time += r.time_bzip2;
        total_xz_time += r.time_xz;
        total_brotli_time += r.time_brotli;
        total_7z_time += r.time_7z;
        total_rar_time += r.time_rar;
        total_lz4_time += r.time_lz4;
        total_snappy_time += r.time_snappy;
        total_original += r.original;
    }

    out("\n================================================================================\n");
    out("                         TOP 10 mzip STRENGTHS\n");
    out("================================================================================\n");
    out("%-22s %-6s %8s %8s %8s  Advantage\n", "Type", "Size", "mzip", "Best", "Ratio");
    out("--------------------------------------------------------------------------------\n");
    for (int i = 0; i < 10 && i < (int)results.size(); i++) {
        auto& r = results[i];
        size_t best = std::min({r.zstd19, r.gzip > 0 ? r.gzip : SIZE_MAX, r.bzip2 > 0 ? r.bzip2 : SIZE_MAX,
                                r.xz > 0 ? r.xz : SIZE_MAX, r.brotli > 0 ? r.brotli : SIZE_MAX,
                                r.sz7 > 0 ? r.sz7 : SIZE_MAX, r.rar > 0 ? r.rar : SIZE_MAX});
        out("%-22s %-6s %8zu %8zu %8.1fx  %.2fx better\n",
               r.name.c_str(), r.size_name.c_str(), r.mzip, best,
               (double)r.original / r.mzip, r.mzip_advantage);
    }

    out("\n================================================================================\n");
    out("                         TOP 10 mzip WEAKNESSES\n");
    out("================================================================================\n");
    out("%-22s %-6s %8s %8s %8s  Gap     Winner\n", "Type", "Size", "mzip", "Best", "Ratio");
    out("--------------------------------------------------------------------------------\n");
    for (int i = (int)results.size() - 1; i >= 0 && i >= (int)results.size() - 10; i--) {
        auto& r = results[i];
        // Find best and who achieved it
        struct { size_t sz; const char* name; } comps[] = {
            {r.zstd19, "zstd"},
            {r.gzip, "gzip"},
            {r.bzip2, "bzip2"},
            {r.xz, "xz"},
            {r.brotli, "brotli"},
            {r.sz7, "7z"},
            {r.rar, "rar"}
        };
        size_t best = SIZE_MAX;
        const char* winner = "???";
        for (auto& c : comps) {
            if (c.sz > 0 && c.sz < best) {
                best = c.sz;
                winner = c.name;
            }
        }
        int gap = (int)r.mzip - (int)best;
        if (r.mzip_advantage < 1.0) {
            out("%-22s %-6s %8zu %8zu %8.1fx  +%-5d  %s\n",
                   r.name.c_str(), r.size_name.c_str(), r.mzip, best,
                   (double)r.original / r.mzip, gap, winner);
        } else {
            out("%-22s %-6s %8zu %8zu %8.1fx  (win)   -\n",
                   r.name.c_str(), r.size_name.c_str(), r.mzip, best,
                   (double)r.original / r.mzip);
        }
    }

    // Per-size analysis
    out("\n================================================================================\n");
    out("                         PER-SIZE ANALYSIS\n");
    out("================================================================================\n");

    // Group results by size
    std::map<std::string, std::vector<BenchResult*>> by_size;
    for (auto& r : results) {
        by_size[r.size_name].push_back(&r);
    }

    out("Size      Wins   Total   Win%%    Avg Advantage\n");
    out("--------------------------------------------------------------------------------\n");
    for (auto& sz : {"4KB", "16KB", "64KB", "256KB", "1MB"}) {
        auto it = by_size.find(sz);
        if (it == by_size.end()) continue;

        int size_wins = 0;
        double sum_adv = 0;
        for (auto* r : it->second) {
            if (r->mzip_advantage >= 1.0) size_wins++;
            sum_adv += r->mzip_advantage;
        }
        int total = (int)it->second.size();
        double avg_adv = sum_adv / total;
        out("%-8s  %3d    %3d     %5.1f%%   %.2fx\n", sz, size_wins, total,
               100.0 * size_wins / total, avg_adv);
    }

    // Category analysis - need to look up category from gen::all_types()
    out("\n================================================================================\n");
    out("                         PER-CATEGORY ANALYSIS\n");
    out("================================================================================\n");

    std::map<std::string, std::vector<BenchResult*>> by_category;
    // Map type names to categories
    std::map<std::string, std::string> name_to_category;
    for (const auto& dt : gen::all_types()) {
        name_to_category[dt.name] = dt.category;
    }
    for (auto& r : results) {
        auto it = name_to_category.find(r.name);
        if (it != name_to_category.end()) {
            by_category[it->second].push_back(&r);
        }
    }

    out("%-12s  Wins  Total  Win%%   AvgRatio  BestType\n", "Category");
    out("--------------------------------------------------------------------------------\n");
    for (auto& cat : {"NUMERIC", "TEXT", "STRUCTURED", "CODE", "LOG", "CONFIG", "BUILD", "BINARY", "ADDITIONAL"}) {
        auto it = by_category.find(cat);
        if (it == by_category.end()) continue;

        int cat_wins = 0;
        double sum_ratio = 0;
        double best_ratio = 0;
        std::string best_type;
        for (auto* r : it->second) {
            if (r->mzip_advantage >= 1.0) cat_wins++;
            double ratio = (double)r->original / r->mzip;
            sum_ratio += ratio;
            if (ratio > best_ratio) {
                best_ratio = ratio;
                best_type = r->name + " (" + r->size_name + ")";
            }
        }
        int total = (int)it->second.size();
        out("%-12s  %3d   %3d    %5.1f%%  %7.1fx  %s\n",
               cat, cat_wins, total, 100.0 * cat_wins / total,
               sum_ratio / total, best_type.c_str());
    }

    // Strategy analysis - what did mzip detect?
    out("\n================================================================================\n");
    out("                         STRATEGY DISTRIBUTION\n");
    out("================================================================================\n");

    std::map<std::string, std::vector<BenchResult*>> by_strategy;
    for (auto& r : results) {
        by_strategy[r.detected].push_back(&r);
    }

    // Sort strategies by usage count
    std::vector<std::pair<std::string, std::vector<BenchResult*>*>> sorted_strategies;
    for (auto& kv : by_strategy) {
        sorted_strategies.push_back({kv.first, &kv.second});
    }
    std::sort(sorted_strategies.begin(), sorted_strategies.end(),
              [](const auto& a, const auto& b) { return a.second->size() > b.second->size(); });

    out("%-20s  Count  Wins  Win%%   AvgRatio  Example\n", "Strategy");
    out("--------------------------------------------------------------------------------\n");
    for (auto& [strat, results_ptr] : sorted_strategies) {
        int strat_wins = 0;
        double sum_ratio = 0;
        std::string example;
        for (auto* r : *results_ptr) {
            if (r->mzip_advantage >= 1.0) strat_wins++;
            sum_ratio += (double)r->original / r->mzip;
            if (example.empty()) example = r->name;
        }
        int count = (int)results_ptr->size();
        out("%-20s  %5d  %4d  %5.1f%%  %7.1fx  %s\n",
               strat.c_str(), count, strat_wins, 100.0 * strat_wins / count,
               sum_ratio / count, example.c_str());
    }

    // Calculate off-meter: difference between best and worst size for each type
    out("\n================================================================================\n");
    out("                      SIZE VARIANCE (OFF-METER)\n");
    out("================================================================================\n");
    out("Types with largest performance swing across sizes:\n");
    out("--------------------------------------------------------------------------------\n");

    std::map<std::string, std::vector<BenchResult*>> by_name;
    for (auto& r : results) {
        by_name[r.name].push_back(&r);
    }

    struct OffMeter {
        std::string name;
        double best_adv;
        std::string best_size;
        double worst_adv;
        std::string worst_size;
        double swing;  // best_adv / worst_adv
    };
    std::vector<OffMeter> off_meters;

    for (auto& kv : by_name) {
        if (kv.second.size() < 2) continue;

        OffMeter om;
        om.name = kv.first;
        om.best_adv = 0;
        om.worst_adv = 1e9;

        for (auto* r : kv.second) {
            if (r->mzip_advantage > om.best_adv) {
                om.best_adv = r->mzip_advantage;
                om.best_size = r->size_name;
            }
            if (r->mzip_advantage < om.worst_adv) {
                om.worst_adv = r->mzip_advantage;
                om.worst_size = r->size_name;
            }
        }
        om.swing = om.best_adv / om.worst_adv;
        off_meters.push_back(om);
    }

    // Sort by swing (biggest variance first)
    std::sort(off_meters.begin(), off_meters.end(),
              [](const OffMeter& a, const OffMeter& b) { return a.swing > b.swing; });

    out("%-22s Best@Size        Worst@Size       Swing\n", "Type");
    out("--------------------------------------------------------------------------------\n");
    for (int i = 0; i < 10 && i < (int)off_meters.size(); i++) {
        auto& om = off_meters[i];
        char best_str[32], worst_str[32];
        snprintf(best_str, sizeof(best_str), "%.2fx@%s", om.best_adv, om.best_size.c_str());
        snprintf(worst_str, sizeof(worst_str), "%.2fx@%s", om.worst_adv, om.worst_size.c_str());
        out("%-22s %-16s %-16s %.1fx\n", om.name.c_str(), best_str, worst_str, om.swing);
    }

    double total_mb = total_original / 1024.0 / 1024.0;

    // Overall compressor scoreboard
    out("\n================================================================================\n");
    out("                      OVERALL COMPRESSOR SCOREBOARD\n");
    out("================================================================================\n");

    // Calculate total compressed sizes and win counts for each compressor
    size_t total_mzip = 0, total_zstd = 0, total_gzip = 0, total_bzip2 = 0;
    size_t total_xz = 0, total_brotli = 0, total_7z = 0, total_rar = 0, total_lz4 = 0, total_snappy = 0;
    int win_mzip = 0, win_zstd = 0, win_gzip = 0, win_bzip2 = 0;
    int win_xz = 0, win_brotli = 0, win_7z = 0, win_rar = 0, win_lz4 = 0, win_snappy = 0;

    for (auto& r : results) {
        total_mzip += r.mzip;
        total_zstd += r.zstd19;
        if (r.gzip > 0) total_gzip += r.gzip;
        if (r.bzip2 > 0) total_bzip2 += r.bzip2;
        if (r.xz > 0) total_xz += r.xz;
        if (r.brotli > 0) total_brotli += r.brotli;
        if (r.sz7 > 0) total_7z += r.sz7;
        if (r.rar > 0) total_rar += r.rar;
        if (r.lz4 > 0) total_lz4 += r.lz4;
        if (r.snappy > 0) total_snappy += r.snappy;

        // Find best compressor for this result
        size_t best = r.mzip;
        if (r.zstd19 < best) best = r.zstd19;
        if (r.gzip > 0 && r.gzip < best) best = r.gzip;
        if (r.bzip2 > 0 && r.bzip2 < best) best = r.bzip2;
        if (r.xz > 0 && r.xz < best) best = r.xz;
        if (r.brotli > 0 && r.brotli < best) best = r.brotli;
        if (r.sz7 > 0 && r.sz7 < best) best = r.sz7;
        if (r.rar > 0 && r.rar < best) best = r.rar;
        if (r.lz4 > 0 && r.lz4 < best) best = r.lz4;
        if (r.snappy > 0 && r.snappy < best) best = r.snappy;

        // Count wins (allow ties)
        if (r.mzip == best) win_mzip++;
        if (r.zstd19 == best) win_zstd++;
        if (r.gzip > 0 && r.gzip == best) win_gzip++;
        if (r.bzip2 > 0 && r.bzip2 == best) win_bzip2++;
        if (r.xz > 0 && r.xz == best) win_xz++;
        if (r.brotli > 0 && r.brotli == best) win_brotli++;
        if (r.sz7 > 0 && r.sz7 == best) win_7z++;
        if (r.rar > 0 && r.rar == best) win_rar++;
        if (r.lz4 > 0 && r.lz4 == best) win_lz4++;
        if (r.snappy > 0 && r.snappy == best) win_snappy++;
    }

    int n = (int)results.size();
    out("%-12s %7s %14s %7s  %4s  %5s  %7s  %4s\n",
           "Compressor", "Ratio", "Range", "MB/s", "Wins", "Win%", "Score", "Rank");
    out("------------------------------------------------------------------------------------\n");

    // Store in vector for sorting
    // Smart Score = ratio * speed^0.1 * (1 + 0.1*wins)
    // Ratio dominates, wins give bonus, speed is minor tiebreaker
    struct Score {
        const char* name;
        size_t total;
        int wins;
        double ratio;
        double min_ratio;   // Best ratio for any single file
        double max_ratio;   // Worst ratio for any single file
        double time_ms;
        double speed_mbs;   // MB/s
        double smart_score; // ratio * speed^0.1 * (1 + 0.1*wins)
    };
    std::vector<Score> scores;

    // Calculate per-compressor ratio ranges
    auto calc_ratio_range = [&](auto get_size) -> std::pair<double, double> {
        double min_r = 1e9, max_r = 0;
        for (auto& r : results) {
            size_t sz = get_size(r);
            if (sz > 0) {
                double ratio = (double)r.original / sz;
                if (ratio < min_r) min_r = ratio;
                if (ratio > max_r) max_r = ratio;
            }
        }
        return {min_r, max_r};
    };

    auto add_score = [&](const char* name, size_t total, int wins, double time_ms, double min_r, double max_r) {
        double ratio = (double)total_original / total;
        double speed = (time_ms > 0) ? total_mb / (time_ms / 1000.0) : 0;
        double win_bonus = 1.0 + 0.1 * wins;  // +10% per win
        double smart = ratio * pow(speed, 0.1) * win_bonus;
        scores.push_back({name, total, wins, ratio, min_r, max_r, time_ms, speed, smart});
    };

    auto [mzip_min, mzip_max] = calc_ratio_range([](const BenchResult& r) { return r.mzip; });
    auto [zstd_min, zstd_max] = calc_ratio_range([](const BenchResult& r) { return r.zstd19; });
    auto [gzip_min, gzip_max] = calc_ratio_range([](const BenchResult& r) { return r.gzip; });
    auto [bzip2_min, bzip2_max] = calc_ratio_range([](const BenchResult& r) { return r.bzip2; });
    auto [xz_min, xz_max] = calc_ratio_range([](const BenchResult& r) { return r.xz; });
    auto [brotli_min, brotli_max] = calc_ratio_range([](const BenchResult& r) { return r.brotli; });
    auto [sz7_min, sz7_max] = calc_ratio_range([](const BenchResult& r) { return r.sz7; });
    auto [rar_min, rar_max] = calc_ratio_range([](const BenchResult& r) { return r.rar; });
    auto [lz4_min, lz4_max] = calc_ratio_range([](const BenchResult& r) { return r.lz4; });
    auto [snappy_min, snappy_max] = calc_ratio_range([](const BenchResult& r) { return r.snappy; });

    add_score("mzip", total_mzip, win_mzip, total_mzip_time, mzip_min, mzip_max);
    add_score("zstd:19", total_zstd, win_zstd, total_zstd_time, zstd_min, zstd_max);
    if (total_gzip > 0) add_score("gzip:9", total_gzip, win_gzip, total_gzip_time, gzip_min, gzip_max);
    if (total_bzip2 > 0) add_score("bzip2:9", total_bzip2, win_bzip2, total_bzip2_time, bzip2_min, bzip2_max);
    if (total_xz > 0) add_score("xz:9", total_xz, win_xz, total_xz_time, xz_min, xz_max);
    if (total_brotli > 0) add_score("brotli:11", total_brotli, win_brotli, total_brotli_time, brotli_min, brotli_max);
    if (total_7z > 0) add_score("7z:mx9", total_7z, win_7z, total_7z_time, sz7_min, sz7_max);
    if (total_rar > 0) add_score("rar:m5", total_rar, win_rar, total_rar_time, rar_min, rar_max);
    if (total_lz4 > 0) add_score("lz4:HC", total_lz4, win_lz4, total_lz4_time, lz4_min, lz4_max);
    if (total_snappy > 0) add_score("snappy", total_snappy, win_snappy, total_snappy_time, snappy_min, snappy_max);

    // Sort by ratio (best first)
    std::sort(scores.begin(), scores.end(), [](const Score& a, const Score& b) { return a.ratio > b.ratio; });

    // Calculate smart score ranks
    std::vector<Score> by_smart = scores;
    std::sort(by_smart.begin(), by_smart.end(), [](const Score& a, const Score& b) { return a.smart_score > b.smart_score; });
    std::map<std::string, int> smart_ranks;
    for (int i = 0; i < (int)by_smart.size(); i++) {
        smart_ranks[by_smart[i].name] = i + 1;
    }

    for (auto& s : scores) {
        char range[32];
        snprintf(range, sizeof(range), "%.1f-%.1fx", s.min_ratio, s.max_ratio);
        out("%-12s %6.2fx %14s %6.1f  %4d  %4.1f%%  %7.1f  %4d\n",
               s.name, s.ratio, range, s.speed_mbs, s.wins, 100.0 * s.wins / n,
               s.smart_score, smart_ranks[s.name]);
    }

    out("\nOriginal: %.2f MB | Score = ratio * speed^0.1 * (1 + 0.1*wins)\n", total_mb);

    // Decompression speed comparison
    out("\n================================================================================\n");
    out("                         DECOMPRESSION SPEED\n");
    out("================================================================================\n");

    double total_decomp_mzip = 0, total_decomp_zstd = 0;
    for (auto& r : results) {
        total_decomp_mzip += r.decomp_mzip;
        total_decomp_zstd += r.decomp_zstd;
    }

    double mzip_decomp_mbs = (total_decomp_mzip > 0) ? total_mb / (total_decomp_mzip / 1000.0) : 0;
    double zstd_decomp_mbs = (total_decomp_zstd > 0) ? total_mb / (total_decomp_zstd / 1000.0) : 0;

    out("%-12s %8s %10s\n", "Compressor", "Time(ms)", "Speed(MB/s)");
    out("--------------------------------------------------------------------------------\n");
    out("%-12s %8.1f %10.1f\n", "mzip", total_decomp_mzip, mzip_decomp_mbs);
    out("%-12s %8.1f %10.1f\n", "zstd", total_decomp_zstd, zstd_decomp_mbs);

    if (zstd_decomp_mbs > mzip_decomp_mbs) {
        out("\nzstd decompresses %.1fx faster than mzip\n", zstd_decomp_mbs / mzip_decomp_mbs);
    } else {
        out("\nmzip decompresses %.1fx faster than zstd\n", mzip_decomp_mbs / zstd_decomp_mbs);
    }

    out("\n================================================================================\n");
    out("                            FINAL SUMMARY\n");
    out("================================================================================\n");
    out("Total: mzip wins %d / %d tests (%.1f%%)\n", wins, (int)results.size(), 100.0 * wins / results.size());
    out("Verification: %d / %d passed\n", (int)results.size() - fails, (int)results.size());

    double speed_ratio = total_zstd_time / total_mzip_time;
    if (speed_ratio >= 1.0) {
        out("Speed: mzip is %.1fx faster than zstd:19\n", speed_ratio);
    } else {
        out("Speed: mzip is %.1fx slower than zstd:19 (better ratio tradeoff)\n", 1.0 / speed_ratio);
    }
    out("================================================================================\n");
}

void list_types() {
    out("\nAvailable data types:\n");
    out("---------------------\n");
    std::string current_cat;
    for (const auto& dt : gen::all_types()) {
        if (dt.category != current_cat) {
            current_cat = dt.category;
            out("\n[%s]\n", current_cat.c_str());
        }
        out("  %-15s - %s\n", dt.key.c_str(), dt.name.c_str());
    }
    out("\nSizes: 4k, 16k, 64k, 256k, 1m\n");
}

void print_usage() {
    out("Usage: mzip_bench [options]\n");
    out("Options:\n");
    out("  (no args)           Full benchmark (summary mode, use -v for details)\n");
    out("  --quick             Quick: 64KB only, all types\n");
    out("  --type <name>       Single type, all sizes\n");
    out("  --size <size>       All types, single size (4k/16k/64k/256k/1m)\n");
    out("  --list              List available types\n");
    out("  --no-external       Skip external compressors (faster)\n");
    out("  --detailed          Show per-compressor timing and identical results\n");
    out("  -v, --verbose       Show per-test output (default: summary only)\n");
    out("  -o, --output <file> Save output to file\n");
    out("  --save              Auto-save to bench_YYYYMMDD_HHMMSS.txt\n");
    out("  --iterations <n>    Number of timed iterations per test (default: 3)\n");
    out("  --file <path>       Benchmark a real file (can use multiple times)\n");
    out("  --csv <path>        Export results to CSV file\n");
}

void export_csv(const std::vector<BenchResult>& results, const std::string& filepath) {
    std::ofstream f(filepath);
    if (!f) {
        out("Cannot write CSV: %s\n", filepath.c_str());
        return;
    }

    // Header
    f << "type,size,original,mzip,zstd,gzip,bzip2,xz,brotli,7z,rar,lz4,snappy,";
    f << "time_mzip,time_zstd,decomp_mzip,decomp_zstd,";
    f << "detected,verified,mzip_ratio,advantage,winner\n";

    for (const auto& r : results) {
        // Find winner
        struct { size_t sz; const char* name; } comps[] = {
            {r.mzip, "mzip"}, {r.zstd19, "zstd"}, {r.gzip, "gzip"},
            {r.bzip2, "bzip2"}, {r.xz, "xz"}, {r.brotli, "brotli"},
            {r.sz7, "7z"}, {r.rar, "rar"}, {r.lz4, "lz4"}, {r.snappy, "snappy"}
        };
        size_t best = SIZE_MAX;
        const char* winner = "???";
        for (auto& c : comps) {
            if (c.sz > 0 && c.sz < best) {
                best = c.sz;
                winner = c.name;
            }
        }

        f << r.name << "," << r.size_name << "," << r.original << ",";
        f << r.mzip << "," << r.zstd19 << "," << r.gzip << "," << r.bzip2 << ",";
        f << r.xz << "," << r.brotli << "," << r.sz7 << "," << r.rar << ",";
        f << r.lz4 << "," << r.snappy << ",";
        f << std::fixed << std::setprecision(2);
        f << r.time_mzip << "," << r.time_zstd << ",";
        f << r.decomp_mzip << "," << r.decomp_zstd << ",";
        f << r.detected << "," << (r.verified ? "yes" : "no") << ",";
        f << std::fixed << std::setprecision(2) << ((double)r.original / r.mzip) << ",";
        f << std::fixed << std::setprecision(3) << r.mzip_advantage << ",";
        f << winner << "\n";
    }

    f.close();
    out("\nResults exported to: %s\n", filepath.c_str());
}

// Benchmark a real file with all compressors
void benchmark_file(const std::string& filepath, bool use_external) {
    // Read file
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f) {
        out("Cannot open file: %s\n", filepath.c_str());
        return;
    }
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read((char*)data.data(), size);
    f.close();

    // Extract filename
    std::string filename = filepath;
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) filename = filepath.substr(pos + 1);

    out("================================================================================\n");
    out("  FILE BENCHMARK: %s (%zu bytes)\n", filename.c_str(), size);
    out("================================================================================\n\n");

    // Create a fake DataType for this file
    gen::DataType dt;
    dt.name = filename;
    dt.key = "file";
    dt.generator = [data](size_t) { return data; };

    // Run benchmark
    auto r = benchmark_one(dt, data.size(), use_external);

    // Print detailed results
    out("%-20s %8s %8s %8s  %s\n", "Compressor", "Size", "Ratio", "Time", "Winner");
    out("--------------------------------------------------------------------------------\n");

    struct CompResult { const char* name; size_t size; double time; };
    std::vector<CompResult> results = {
        {"mzip", r.mzip, r.time_mzip},
        {"zstd:19", r.zstd19, r.time_zstd},
    };
    if (use_external) {
        results.push_back({"gzip:9", r.gzip, r.time_gzip});
        results.push_back({"bzip2:9", r.bzip2, r.time_bzip2});
        results.push_back({"xz:9", r.xz, r.time_xz});
        results.push_back({"brotli:11", r.brotli, r.time_brotli});
        results.push_back({"7z:mx9", r.sz7, r.time_7z});
        results.push_back({"rar:m5", r.rar, r.time_rar});
        results.push_back({"lz4:9", r.lz4, r.time_lz4});
        results.push_back({"snappy", r.snappy, r.time_snappy});
    }

    // Find best
    size_t best_size = SIZE_MAX;
    const char* best_name = "";
    for (auto& cr : results) {
        if (cr.size > 0 && cr.size < best_size) {
            best_size = cr.size;
            best_name = cr.name;
        }
    }

    for (auto& cr : results) {
        if (cr.size == 0) continue;
        bool is_best = (cr.size == best_size);
        out("%-20s %8zu %7.2fx %7.1fms %s\n",
               cr.name, cr.size, (double)r.original / cr.size, cr.time,
               is_best ? "[BEST]" : "");
    }

    out("\n");
    out("Original:  %zu bytes\n", r.original);
    out("Best:      %s (%zu bytes, %.2fx)\n", best_name, best_size, (double)r.original / best_size);
    out("mzip:      %zu bytes (%.2fx) - %s\n", r.mzip, (double)r.original / r.mzip,
           r.verified ? "verified" : "FAILED");
    out("Detected:  %s\n", r.detected);

    if (r.mzip <= best_size) {
        out("\n>>> mzip WINS by %.2fx <<<\n", (double)best_size / r.mzip);
    } else {
        out("\n>>> %s wins by %.2fx <<<\n", best_name, (double)r.mzip / best_size);
    }
}

int main(int argc, char** argv) {
    bool quick = false;
    bool no_external = false;
    bool verbose = false;        // Show per-test output (default: summary only for full runs)
    bool detailed_output = false;
    std::string filter_type;
    size_t filter_size = 0;
    std::vector<std::string> files;
    std::string csv_path;
    std::string output_path;
    bool auto_save = false;

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--quick") {
            quick = true;
        } else if (arg == "--list") {
            list_types();
            return 0;
        } else if (arg == "--no-external") {
            no_external = true;
        } else if (arg == "--detailed") {
            g_detailed_output = true;
            verbose = true;  // --detailed implies verbose
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                out("Error: %s requires a filename\n", arg.c_str());
                return 1;
            }
        } else if (arg == "--save") {
            auto_save = true;
        } else if (arg == "--type" && i + 1 < argc) {
            filter_type = argv[++i];
        } else if (arg == "--size" && i + 1 < argc) {
            filter_size = parse_size(argv[++i]);
            if (filter_size == 0) {
                out("Invalid size. Use: 4k, 16k, 64k, 256k, 1m\n");
                return 1;
            }
        } else if (arg == "--iterations" && i + 1 < argc) {
            g_iterations = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--file" && i + 1 < argc) {
            files.push_back(argv[++i]);
        } else if (arg == "--csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            out("Unknown option: %s\n", arg.c_str());
            print_usage();
            return 1;
        }
    }

    // Setup output file if requested
    if (auto_save && output_path.empty()) {
        output_path = generate_output_filename();
    }
    if (!output_path.empty()) {
        g_output_file = fopen(output_path.c_str(), "w");
        if (!g_output_file) {
            printf("Error: Cannot write to %s\n", output_path.c_str());
            return 1;
        }
        printf("Saving output to: %s\n", output_path.c_str());
    }

    // For filtered runs (--type, --size, --quick), default to verbose
    bool is_filtered_run = quick || !filter_type.empty() || filter_size > 0;
    bool summary_only = !verbose && !is_filtered_run ? true : !verbose;

    // If files specified, benchmark those and exit
    if (!files.empty()) {
        // Benchmark each file individually
        std::vector<std::vector<uint8_t>> all_data;
        size_t total_size = 0;

        for (const auto& filepath : files) {
            benchmark_file(filepath, !no_external);
            out("\n");

            // Also collect data for combined benchmark
            std::ifstream f(filepath, std::ios::binary | std::ios::ate);
            if (f) {
                size_t size = f.tellg();
                f.seekg(0);
                std::vector<uint8_t> data(size);
                f.read((char*)data.data(), size);
                all_data.push_back(std::move(data));
                total_size += size;
            }
        }

        // If multiple files, also benchmark them combined
        if (all_data.size() > 1) {
            std::vector<uint8_t> combined;
            combined.reserve(total_size);
            for (const auto& d : all_data) {
                combined.insert(combined.end(), d.begin(), d.end());
            }

            out("================================================================================\n");
            out("  COMBINED BENCHMARK: %zu files (%zu bytes total)\n", files.size(), total_size);
            out("================================================================================\n\n");

            // Create a fake DataType for combined
            gen::DataType dt;
            dt.name = "COMBINED";
            dt.key = "combined";
            dt.generator = [combined](size_t) { return combined; };

            auto r = benchmark_one(dt, combined.size(), !no_external);

            // Print results
            out("%-20s %8s %8s %8s  %s\n", "Compressor", "Size", "Ratio", "Time", "Winner");
            out("--------------------------------------------------------------------------------\n");

            struct CompResult { const char* name; size_t size; double time; };
            std::vector<CompResult> results = {
                {"mzip", r.mzip, r.time_mzip},
                {"zstd:19", r.zstd19, r.time_zstd},
            };
            if (!no_external) {
                results.push_back({"gzip:9", r.gzip, r.time_gzip});
                results.push_back({"bzip2:9", r.bzip2, r.time_bzip2});
                results.push_back({"xz:9", r.xz, r.time_xz});
                results.push_back({"brotli:11", r.brotli, r.time_brotli});
                results.push_back({"7z:mx9", r.sz7, r.time_7z});
                results.push_back({"rar:m5", r.rar, r.time_rar});
                results.push_back({"lz4:9", r.lz4, r.time_lz4});
                results.push_back({"snappy", r.snappy, r.time_snappy});
            }

            size_t best_size = SIZE_MAX;
            const char* best_name = "";
            for (auto& cr : results) {
                if (cr.size > 0 && cr.size < best_size) {
                    best_size = cr.size;
                    best_name = cr.name;
                }
            }

            for (auto& cr : results) {
                if (cr.size == 0) continue;
                bool is_best = (cr.size == best_size);
                out("%-20s %8zu %7.2fx %7.1fms %s\n",
                       cr.name, cr.size, (double)r.original / cr.size, cr.time,
                       is_best ? "[BEST]" : "");
            }

            out("\n");
            out("Combined:  %zu bytes (%zu files)\n", r.original, files.size());
            out("Best:      %s (%zu bytes, %.2fx)\n", best_name, best_size, (double)r.original / best_size);
            out("mzip:      %zu bytes (%.2fx) - %s\n", r.mzip, (double)r.original / r.mzip,
                   r.verified ? "verified" : "FAILED");
            out("Detected:  %s\n", r.detected);

            if (r.mzip <= best_size) {
                out("\n>>> mzip WINS COMBINED by %.2fx <<<\n", (double)best_size / r.mzip);
            } else {
                out("\n>>> %s wins COMBINED by %.2fx <<<\n", best_name, (double)r.mzip / best_size);
            }
        }

        if (g_output_file) fclose(g_output_file);
        return 0;
    }

    // Determine sizes to test
    std::vector<SizeSpec> sizes_to_test;
    if (quick) {
        sizes_to_test = {{65536, "64KB"}};
    } else if (filter_size > 0) {
        sizes_to_test = {{filter_size, size_name(filter_size)}};
    } else {
        sizes_to_test = ALL_SIZES;
    }

    // Determine types to test
    std::vector<const gen::DataType*> types_to_test;
    if (!filter_type.empty()) {
        for (const auto& dt : gen::all_types()) {
            if (dt.key == filter_type) {
                types_to_test.push_back(&dt);
                break;
            }
        }
        if (types_to_test.empty()) {
            out("Unknown type: %s\n", filter_type.c_str());
            out("Use --list to see available types\n");
            if (g_output_file) fclose(g_output_file);
            return 1;
        }
    } else {
        for (const auto& dt : gen::all_types()) {
            types_to_test.push_back(&dt);
        }
    }

    // Print header
    out("================================================================================\n");
    out("  mzip BENCHMARK v0.6\n");
    out("================================================================================\n");
    out("  Tests: %zu types x %zu sizes = %zu tests\n",
           types_to_test.size(), sizes_to_test.size(),
           types_to_test.size() * sizes_to_test.size());
    out("  Timing: %d warmup + %d iterations (median)\n", g_warmup, g_iterations);
    out("  Data: seeded RNG (seed=42) for reproducibility\n");
    if (no_external) {
        out("  Compressors: mzip, zstd:19\n");
    } else {
        out("  Compressors: mzip, zstd:19, gzip:9, bzip2:9, xz:9, brotli:11,\n");
        out("               7z:mx9, rar:m5, lz4:HC, snappy\n");
        out("  Note: lz4/snappy are speed-focused, excluded from ratio ranking\n");
    }
    out("================================================================================\n\n");

    std::vector<BenchResult> all_results;

    int test_count = 0;
    int total_tests = (int)(types_to_test.size() * sizes_to_test.size());

    if (summary_only) {
        out("Running %d tests", total_tests);
        out_flush();
    }

    for (auto& size : sizes_to_test) {
        if (!summary_only) {
            out("=== SIZE: %s ===\n", size.name);
        }

        for (auto* dt : types_to_test) {
            if (!summary_only && !g_detailed_output) {
                out("  %-20s ... ", dt->name.c_str());
                out_flush();
            }

            auto r = benchmark_one(*dt, size.bytes, !no_external);
            all_results.push_back(r);
            test_count++;

            if (!summary_only) {
                if (g_detailed_output) {
                    // Detailed mode: show all compressors with timing
                    print_result(r, !no_external);
                } else {
                    // Normal mode: compact inline output
                    bool win = r.mzip_advantage >= 1.0;
                    out("%6zu -> %5zu (%6.1fx) %s %s\n",
                           r.original, r.mzip, (double)r.original / r.mzip,
                           win ? "[WIN]" : "     ",
                           r.verified ? "" : "[FAIL]");
                }
            } else {
                // Progress dots
                out(".");
                out_flush();
            }
        }
        if (!summary_only) {
            out("\n");
        }
    }

    if (summary_only) {
        out(" done.\n\n");
    }

    print_summary(all_results);

    // Export to CSV if requested
    if (!csv_path.empty()) {
        export_csv(all_results, csv_path);
    }

    // Close output file
    if (g_output_file) {
        fclose(g_output_file);
        printf("Output saved to: %s\n", output_path.c_str());
    }

    return 0;
}
