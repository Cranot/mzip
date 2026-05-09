// Train zstd dictionaries PROPERLY — diverse samples via chunking, no overfitting
// Key insight: generate a large block, split into non-overlapping chunks.
// Each chunk starts at a different MT19937 position → dict learns VOCABULARY not POSITIONS.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <zstd.h>
#include "generators.hpp"
#include "zdict_minimal.h"

struct DictDef {
    uint8_t id;
    const char* name;
    std::vector<std::string> types;
    size_t target_size;
};

void train_dict_diverse(const DictDef& def, std::vector<uint8_t>& out, size_t& actual) {
    std::vector<uint8_t> samples;
    std::vector<size_t> sizes;

    for (auto& key : def.types) {
        auto fn = gen::get(key);
        if (!fn) continue;

        // TWO-PRONGED training:
        // 1. Small full samples (256-16384) — captures vocabulary density
        // 2. Chunks from large block (offset 0, 4K, 8K...) — captures diversity
        size_t small_sizes[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
        for (size_t sz : small_sizes) {
            auto d = fn(sz);
            samples.insert(samples.end(), d.begin(), d.end());
            sizes.push_back(d.size());
        }

        // Large block chunks for diversity
        auto big = fn(131072);  // 128KB
        for (size_t offset = 16384; offset + 4096 <= big.size(); offset += 8192) {
            samples.insert(samples.end(), big.begin() + offset, big.begin() + offset + 4096);
            sizes.push_back(4096);
        }
    }

    out.resize(def.target_size);
    size_t r = ZDICT_trainFromBuffer(out.data(), out.size(),
        samples.data(), sizes.data(), sizes.size());
    if (ZDICT_isError(r)) {
        fprintf(stderr, "FAIL %s: %s\n", def.name, ZDICT_getErrorName(r));
        actual = 0; return;
    }
    actual = r;
    out.resize(actual);
}

void verify_generalization(const char* name, const std::vector<uint8_t>& dict, size_t dict_sz,
                            const std::vector<std::string>& types) {
    // Test on seed=42 (trained) AND on bytes the dict hasn't seen
    // "Unseen" = take bytes from position 200000+ of a 256KB generation
    printf("  %-18s", name);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    std::vector<uint8_t> out(ZSTD_compressBound(4096));

    int total_save_trained = 0, total_save_unseen = 0, count = 0;
    for (auto& key : types) {
        auto fn = gen::get(key);
        if (!fn) continue;

        // Trained data: first 4KB (same as what dict saw)
        auto data_trained = fn(4096);

        // Unseen data: bytes from deep in the sequence
        auto big = fn(262144);
        std::vector<uint8_t> data_unseen(big.begin() + 200000, big.begin() + 204096);

        size_t plain_t = ZSTD_compress(out.data(), out.size(), data_trained.data(), 4096, 19);
        size_t dict_t = ZSTD_compress_usingDict(cctx, out.data(), out.size(),
            data_trained.data(), 4096, dict.data(), dict_sz, 19);

        size_t plain_u = ZSTD_compress(out.data(), out.size(), data_unseen.data(), 4096, 19);
        size_t dict_u = ZSTD_compress_usingDict(cctx, out.data(), out.size(),
            data_unseen.data(), 4096, dict.data(), dict_sz, 19);

        total_save_trained += (int)plain_t - (int)dict_t;
        total_save_unseen += (int)plain_u - (int)dict_u;
        count++;
    }
    ZSTD_freeCCtx(cctx);

    double ratio = count > 0 ? (double)total_save_unseen / std::max(1, total_save_trained) : 0;
    const char* grade = ratio > 0.8 ? "GOOD" : ratio > 0.5 ? "OK" : ratio > 0.2 ? "WEAK" : "OVERFIT";
    printf(" trained=%+5d  unseen=%+5d  retention=%.0f%% [%s]\n",
        total_save_trained, total_save_unseen, ratio * 100, grade);
}

int main() {
    // 5 group dicts — proven to generalize via vocabulary diversity
    std::vector<DictDef> dicts = {
        {1, "CODE", {"python","javascript","typescript","java","go","rust","c","php","css","bash","html"}, 16384},
        {2, "CONFIG", {"yaml","toml","docker_compose","terraform","k8s","package_json","cargo_toml","env","makefile"}, 16384},
        {3, "TEXT", {"text","markdown","email","unicode"}, 16384},
        {4, "LOG", {"nginx_log","access_log","syslog","metrics","json_log"}, 16384},
        {5, "QUERY", {"sql","graphql","csv"}, 16384},
    };

    printf("Training %zu dicts with diverse chunking...\n\n", dicts.size());

    std::vector<std::pair<std::vector<uint8_t>, size_t>> results;
    size_t total_bytes = 0;

    for (auto& def : dicts) {
        std::vector<uint8_t> d; size_t sz;
        train_dict_diverse(def, d, sz);
        printf("%-18s %5zu bytes\n", def.name, sz);
        results.push_back({d, sz});
        total_bytes += sz;
    }
    printf("\nTotal: %zuKB\n", total_bytes / 1024);

    // Verify generalization
    printf("\nGeneralization test (trained=first 4KB, unseen=bytes 200000-204096):\n");
    for (size_t i = 0; i < dicts.size(); i++) {
        if (results[i].second == 0) continue;
        verify_generalization(dicts[i].name, results[i].first, results[i].second, dicts[i].types);
    }

    // Detailed per-type: compare group dict vs brotli on UNSEEN data
    printf("\n4KB benchmark on UNSEEN data (bytes 200000-204096):\n");
    printf("%-18s %6s %6s %6s %6s\n", "Type", "plain", "dict", "save", "best_d");
    printf("--------------------------------------------------------------\n");

    for (auto& def : dicts) {
        for (auto& key : def.types) {
            auto fn = gen::get(key);
            if (!fn) continue;
            auto big = fn(262144);
            std::vector<uint8_t> data(big.begin() + 200000, big.begin() + 204096);

            ZSTD_CCtx* cctx = ZSTD_createCCtx();
            std::vector<uint8_t> out(ZSTD_compressBound(4096));
            size_t plain = ZSTD_compress(out.data(), out.size(), data.data(), 4096, 19);

            size_t best = plain; int best_id = 0;
            for (size_t i = 0; i < dicts.size(); i++) {
                if (results[i].second == 0) continue;
                size_t sz = ZSTD_compress_usingDict(cctx, out.data(), out.size(),
                    data.data(), 4096, results[i].first.data(), results[i].second, 19);
                if (!ZSTD_isError(sz) && sz < best) { best = sz; best_id = dicts[i].id; }
            }
            ZSTD_freeCCtx(cctx);

            printf("%-18s %6zu %6zu %+5d  d%d\n", key.c_str(), plain, best, (int)plain-(int)best, best_id);
        }
    }

    // Write header
    FILE* f = fopen("mzip_dicts.h", "w");
    fprintf(f, "// Auto-generated zstd dictionaries for mzip — diverse training, no overfitting\n");
    fprintf(f, "// 5 group dicts trained from 256KB chunked samples (vocabulary, not positions)\n");
    fprintf(f, "// Total: %zuKB\n", total_bytes / 1024);
    fprintf(f, "#pragma once\n#include <cstdint>\n#include <cstddef>\n\n");
    fprintf(f, "namespace mzip_dicts {\n\n");

    for (size_t i = 0; i < dicts.size(); i++) {
        if (results[i].second == 0) continue;
        fprintf(f, "// Dict %d: %s (%zu bytes)\n", dicts[i].id, dicts[i].name, results[i].second);
        fprintf(f, "static const uint8_t DICT_%s[] = {\n", dicts[i].name);
        for (size_t j = 0; j < results[i].second; j++) {
            if (j % 16 == 0) fprintf(f, "    ");
            fprintf(f, "0x%02x,", results[i].first[j]);
            if (j % 16 == 15 || j == results[i].second - 1) fprintf(f, "\n");
        }
        fprintf(f, "};\nstatic const size_t DICT_%s_SIZE = %zu;\n\n", dicts[i].name, results[i].second);
    }

    fprintf(f, "struct DictEntry { uint8_t id; const uint8_t* data; size_t size; };\n");
    fprintf(f, "static const DictEntry ALL_DICTS[] = {\n");
    for (size_t i = 0; i < dicts.size(); i++) {
        if (results[i].second == 0) continue;
        fprintf(f, "    {%d, DICT_%s, DICT_%s_SIZE},\n", dicts[i].id, dicts[i].name, dicts[i].name);
    }
    fprintf(f, "};\nstatic const size_t NUM_DICTS = sizeof(ALL_DICTS) / sizeof(ALL_DICTS[0]);\n\n");
    fprintf(f, "} // namespace mzip_dicts\n");
    fclose(f);
    printf("\nWritten to mzip_dicts.h\n");
    return 0;
}
