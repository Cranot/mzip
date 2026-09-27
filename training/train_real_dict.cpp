// Train a zstd dictionary from a directory of real-world files.
// Usage: train_real_dict <input_dir> <dict_size> <output.bin>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <zstd.h>
#include "zdict_minimal.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <input_dir> <dict_size> <output.bin>\n", argv[0]);
        return 1;
    }
    std::string in_dir = argv[1];
    size_t target_size = (size_t)std::atoi(argv[2]);
    std::string out_path = argv[3];

    std::vector<uint8_t> samples;
    std::vector<size_t> sample_sizes;

    for (auto& entry : fs::directory_iterator(in_dir)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream f(entry.path(), std::ios::binary | std::ios::ate);
        size_t sz = (size_t)f.tellg();
        if (sz < 256) continue;
        f.seekg(0);
        std::vector<uint8_t> buf(sz);
        f.read((char*)buf.data(), sz);

        const size_t CHUNK = 4096;
        const size_t MAX_PER_FILE = 16;
        size_t taken = 0;
        size_t step = std::max(CHUNK, sz / MAX_PER_FILE);
        for (size_t off = 0; off + CHUNK <= sz && taken < MAX_PER_FILE; off += step) {
            samples.insert(samples.end(), buf.begin() + off, buf.begin() + off + CHUNK);
            sample_sizes.push_back(CHUNK);
            taken++;
        }
        size_t hdr = std::min(sz, (size_t)16384);
        samples.insert(samples.end(), buf.begin(), buf.begin() + hdr);
        sample_sizes.push_back(hdr);
    }

    fprintf(stderr, "Training dict (target %zu bytes) from %zu samples = %zu input bytes...\n",
            target_size, sample_sizes.size(), samples.size());

    std::vector<uint8_t> dict(target_size);
    size_t result = ZDICT_trainFromBuffer(
        dict.data(), dict.size(),
        samples.data(), sample_sizes.data(), (unsigned)sample_sizes.size());

    if (ZDICT_isError(result)) {
        fprintf(stderr, "ZDICT error: %s\n", ZDICT_getErrorName(result));
        return 1;
    }

    dict.resize(result);
    std::ofstream out(out_path, std::ios::binary);
    out.write((char*)dict.data(), dict.size());
    fprintf(stderr, "Wrote %zu bytes to %s\n", dict.size(), out_path.c_str());
    return 0;
}
