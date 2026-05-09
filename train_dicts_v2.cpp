// Train all zstd dictionaries: 5 group dicts + 9 per-type micro-dicts
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

void train_dict(const DictDef& def, std::vector<uint8_t>& out, size_t& actual) {
    std::vector<uint8_t> samples;
    std::vector<size_t> sizes;
    size_t train_sizes[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
    for (auto& key : def.types) {
        auto fn = gen::get(key);
        if (!fn) { fprintf(stderr, "WARN: no gen for '%s'\n", key.c_str()); continue; }
        for (size_t sz : train_sizes) {
            auto d = fn(sz);
            samples.insert(samples.end(), d.begin(), d.end());
            sizes.push_back(d.size());
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

int main() {
    // 5 group dicts (16KB each) + 9 micro-dicts (4KB each, saturates early)
    std::vector<DictDef> dicts = {
        // Group dicts
        {1, "CODE", {"python","javascript","typescript","java","go","rust","c","php","css","bash","html"}, 16384},
        {2, "CONFIG", {"yaml","toml","docker_compose","terraform","k8s","package_json","cargo_toml","env","makefile"}, 16384},
        {3, "TEXT", {"text","markdown","email","unicode"}, 16384},
        {4, "LOG", {"nginx_log","access_log","syslog","metrics","json_log"}, 16384},
        {5, "QUERY", {"sql","graphql","csv"}, 16384},
        // Micro-dicts for the 9 brotli losers at 4KB
        {6, "HTML_MICRO", {"html"}, 4096},
        {7, "MAKEFILE_MICRO", {"makefile"}, 4096},
        {8, "JSONLOG_MICRO", {"json_log"}, 4096},
        {9, "UNICODE_MICRO", {"unicode"}, 4096},
        {10, "BASH_MICRO", {"bash"}, 4096},
        {11, "ENV_MICRO", {"env"}, 4096},
        {12, "BASE64_MICRO", {"base64"}, 4096},
        {13, "CSV_MICRO", {"csv"}, 4096},
        // Image gradient too small (204 bytes training), skip
    };

    // Train and measure
    printf("Training %zu dicts...\n\n", dicts.size());
    size_t total_bytes = 0;
    std::vector<std::pair<std::vector<uint8_t>, size_t>> results;

    for (auto& def : dicts) {
        std::vector<uint8_t> d; size_t sz;
        train_dict(def, d, sz);
        printf("%-18s id=%2d  %5zu bytes  types: ", def.name, def.id, sz);
        for (auto& t : def.types) printf("%s ", t.c_str());
        printf("\n");
        results.push_back({d, sz});
        total_bytes += sz;
    }
    printf("\nTotal dict size: %zu bytes (%.1fKB)\n", total_bytes, total_bytes / 1024.0);

    // Verify against brotli targets
    printf("\n=== 4KB Verification vs brotli ===\n");
    const char* losers[] = {"html","makefile","json_log","unicode","bash","env","base64","image","csv"};
    int brotli_targets[] = {821, 697, 768, 549, 765, 1654, 3106, 26, 819};

    for (int li = 0; li < 9; li++) {
        auto fn = gen::get(losers[li]);
        if (!fn) continue;
        auto data = fn(4096);
        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        std::vector<uint8_t> out(ZSTD_compressBound(4096));

        size_t best = SIZE_MAX; int best_id = -1;
        for (size_t di = 0; di < dicts.size(); di++) {
            if (results[di].second == 0) continue;
            size_t sz = ZSTD_compress_usingDict(cctx, out.data(), out.size(),
                data.data(), data.size(), results[di].first.data(), results[di].second, 19);
            if (!ZSTD_isError(sz) && sz + 1 < best) {
                best = sz + 1; best_id = dicts[di].id;
            }
        }
        ZSTD_freeCCtx(cctx);
        int gap = (int)best - brotli_targets[li];
        printf("  %-15s best=%5zu (dict %2d)  brotli=%d  %s %+d\n",
            losers[li], best, best_id, brotli_targets[li],
            gap <= 0 ? "WIN" : "LOSE", gap);
    }

    // Write header
    FILE* f = fopen("mzip_dicts.h", "w");
    fprintf(f, "// Auto-generated zstd dictionaries for mzip\n");
    fprintf(f, "// 5 group dicts (16KB) + 8 micro-dicts (2-4KB) = %zuKB total\n", total_bytes / 1024);
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

    // Dict table for easy iteration
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
