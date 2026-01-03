// mzip CLI - Command line interface for mzip compression
// Usage: mzip_cli.exe [c|d] input output [level]
//   c = compress
//   d = decompress
//   level = 1-22 (default 19)

#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

void print_usage() {
    std::cerr << "mzip CLI - Adaptive compression\n";
    std::cerr << "Usage:\n";
    std::cerr << "  mzip_cli c <input> <output> [level]  - compress\n";
    std::cerr << "  mzip_cli d <input> <output>          - decompress\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  level: 1-22 (default 19)\n";
}

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool write_file(const char* path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    const char* mode = argv[1];
    const char* input = argv[2];
    const char* output = argv[3];
    int level = 19;

    if (argc >= 5) {
        level = std::atoi(argv[4]);
        if (level < 1) level = 1;
        if (level > 22) level = 22;
    }

    auto input_data = read_file(input);
    if (input_data.empty()) {
        std::cerr << "Error: Cannot read input file: " << input << "\n";
        return 1;
    }

    if (strcmp(mode, "c") == 0) {
        // Compress
        mzip::CompressResult res;
        auto compressed = mzip::compress(input_data.data(), input_data.size(), level, 0, &res);

        if (compressed.empty()) {
            std::cerr << "Error: Compression failed: " << res.error << "\n";
            return 1;
        }

        if (!write_file(output, compressed)) {
            std::cerr << "Error: Cannot write output file: " << output << "\n";
            return 1;
        }

        std::cerr << input_data.size() << " -> " << compressed.size()
                  << " (" << (double)input_data.size() / compressed.size() << "x)\n";

    } else if (strcmp(mode, "d") == 0) {
        // Decompress
        mzip::DecompressResult res;
        auto decompressed = mzip::decompress(input_data.data(), input_data.size(), &res);

        if (decompressed.empty()) {
            std::cerr << "Error: Decompression failed: " << res.error << "\n";
            return 1;
        }

        if (!write_file(output, decompressed)) {
            std::cerr << "Error: Cannot write output file: " << output << "\n";
            return 1;
        }

        std::cerr << input_data.size() << " -> " << decompressed.size() << "\n";

    } else {
        std::cerr << "Error: Unknown mode '" << mode << "'. Use 'c' or 'd'.\n";
        print_usage();
        return 1;
    }

    return 0;
}
