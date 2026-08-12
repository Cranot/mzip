// decode_one.cpp -- call mzip::decompress on a file exactly as the fuzzer does, nothing else.
// Isolates the library call from the CLI wrapper so a peak-memory spike can be attributed.
#include <cstdio>
#include <vector>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: decode_one <file.mz>\n"); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "open failed\n"); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)n);
    if (n > 0 && fread(buf.data(), 1, (size_t)n, f) != (size_t)n) { fclose(f); return 2; }
    fclose(f);
    try {
        auto d = mzip::decompress(buf.data(), buf.size());
        printf("decompressed=%zu\n", d.size());
    } catch (const std::exception& e) {
        printf("exception: %s\n", e.what());
    }
    return 0;
}
