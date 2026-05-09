// Train zstd dictionaries for CODE and CONFIG type groups
// Generates training samples from our generators, trains dict, measures improvement
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <zstd.h>
#include "generators.hpp"
#include "zdict_minimal.h"

struct DictGroup {
    const char* name;
    std::vector<std::string> types;
};

void train_dict(const DictGroup& group, size_t dict_size,
                std::vector<uint8_t>& dict_out, size_t& actual_size) {
    std::vector<uint8_t> all_samples;
    std::vector<size_t> sample_sizes;
    size_t train_sizes[] = {512, 1024, 2048, 4096, 8192, 16384};

    for (auto& type_key : group.types) {
        auto gen_fn = gen::get(type_key);
        if (!gen_fn) { fprintf(stderr, "WARNING: no generator for '%s'\n", type_key.c_str()); continue; }
        for (size_t sz : train_sizes) {
            auto data = gen_fn(sz);
            all_samples.insert(all_samples.end(), data.begin(), data.end());
            sample_sizes.push_back(data.size());
        }
    }

    dict_out.resize(dict_size);
    size_t result = ZDICT_trainFromBuffer(dict_out.data(), dict_out.size(),
        all_samples.data(), sample_sizes.data(), sample_sizes.size());

    if (ZDICT_isError(result)) {
        fprintf(stderr, "ERROR training %s: %s\n", group.name, ZDICT_getErrorName(result));
        actual_size = 0;
        return;
    }
    actual_size = result;
    dict_out.resize(actual_size);
}

void measure(const DictGroup& group, const std::vector<uint8_t>& dict, size_t dict_size) {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    std::vector<uint8_t> comp(ZSTD_compressBound(16384));

    printf("  %-20s %6s %6s %5s  %6s %6s %5s\n",
        "Type", "4K_raw", "4K_dct", "save", "16K_raw", "16K_dct", "save");
    printf("  %s\n", std::string(72, '-').c_str());

    int total_save_4k = 0, total_save_16k = 0, count = 0;
    for (auto& type_key : group.types) {
        auto gen_fn = gen::get(type_key);
        if (!gen_fn) continue;

        auto data4k = gen_fn(4096);
        auto data16k = gen_fn(16384);

        size_t plain4 = ZSTD_compress(comp.data(), comp.size(), data4k.data(), data4k.size(), 19);
        size_t dict4 = ZSTD_compress_usingDict(cctx, comp.data(), comp.size(),
            data4k.data(), data4k.size(), dict.data(), dict_size, 19);

        size_t plain16 = ZSTD_compress(comp.data(), comp.size(), data16k.data(), data16k.size(), 19);
        size_t dict16 = ZSTD_compress_usingDict(cctx, comp.data(), comp.size(),
            data16k.data(), data16k.size(), dict.data(), dict_size, 19);

        int save4 = (int)plain4 - (int)dict4;
        int save16 = (int)plain16 - (int)dict16;
        total_save_4k += save4;
        total_save_16k += save16;
        count++;

        printf("  %-20s %6zu %6zu %+4d  %6zu %6zu %+4d\n",
            type_key.c_str(), plain4, dict4, save4, plain16, dict16, save16);
    }
    printf("  %s\n", std::string(72, '-').c_str());
    printf("  %-20s %6s %6s %+4d  %6s %6s %+4d\n",
        "TOTAL SAVED", "", "", total_save_4k, "", "", total_save_16k);

    ZSTD_freeCCtx(cctx);
}

void emit_header(const char* name, const std::vector<uint8_t>& dict, size_t actual_size,
                 const std::vector<std::string>& types, FILE* out) {
    fprintf(out, "// Auto-generated zstd dictionary for %s types\n", name);
    fprintf(out, "// Types: ");
    for (size_t i = 0; i < types.size(); i++) {
        if (i > 0) fprintf(out, ", ");
        fprintf(out, "%s", types[i].c_str());
    }
    fprintf(out, "\n// Size: %zu bytes\n", actual_size);
    fprintf(out, "static const uint8_t DICT_%s[] = {\n", name);
    for (size_t i = 0; i < actual_size; i++) {
        if (i % 16 == 0) fprintf(out, "    ");
        fprintf(out, "0x%02x,", dict[i]);
        if (i % 16 == 15 || i == actual_size - 1) fprintf(out, "\n");
    }
    fprintf(out, "};\n");
    fprintf(out, "static const size_t DICT_%s_SIZE = %zu;\n\n", name, actual_size);
}

int main() {
    DictGroup code_group = {"CODE", {
        "python", "javascript", "typescript", "java", "go", "rust", "c",
        "php", "css", "bash", "html"
    }};
    DictGroup config_group = {"CONFIG", {
        "yaml", "toml", "docker_compose", "terraform", "k8s",
        "package_json", "cargo_toml", "env", "makefile"
    }};
    DictGroup text_group = {"TEXT", {
        "text", "markdown", "email", "unicode"
    }};
    DictGroup log_group = {"LOG", {
        "nginx_log", "access_log", "syslog", "metrics", "json_log"
    }};
    DictGroup query_group = {"QUERY", {
        "sql", "graphql", "csv"
    }};

    std::vector<DictGroup*> all_groups = {
        &code_group, &config_group, &text_group, &log_group, &query_group
    };

    // Test sizes for new groups
    for (size_t dict_kb : {4, 8, 16}) {
        printf("\n========== %zuKB dictionaries ==========\n", dict_kb);
        for (auto* g : all_groups) {
            std::vector<uint8_t> d; size_t sz;
            train_dict(*g, dict_kb * 1024, d, sz);
            printf("%s: trained %zu bytes\n", g->name, sz);
            if (sz > 0) measure(*g, d, sz);
        }
    }

    // Generate final header — 16KB for CODE/CONFIG, test what's best for the rest
    printf("\n========== Generating final dicts ==========\n");

    FILE* out = fopen("mzip_dicts.h", "w");
    if (!out) { fprintf(stderr, "Cannot open mzip_dicts.h\n"); return 1; }

    fprintf(out, "// Auto-generated zstd dictionaries for mzip small-file compression\n");
    fprintf(out, "// Generated by train_dicts.cpp from generators.hpp training data\n");
    fprintf(out, "#pragma once\n");
    fprintf(out, "#include <cstdint>\n");
    fprintf(out, "#include <cstddef>\n\n");
    fprintf(out, "namespace mzip_dicts {\n\n");

    // 16KB for all — gains are dramatic enough to justify
    // Total: 5 x 16KB = 80KB binary (still less than brotli's 120KB)
    size_t sizes[] = {16384, 16384, 16384, 16384, 16384};
    for (size_t i = 0; i < all_groups.size(); i++) {
        std::vector<uint8_t> d; size_t sz;
        train_dict(*all_groups[i], sizes[i], d, sz);
        printf("%s: %zu bytes\n", all_groups[i]->name, sz);
        emit_header(all_groups[i]->name, d, sz, all_groups[i]->types, out);
    }

    fprintf(out, "} // namespace mzip_dicts\n");
    fclose(out);
    printf("Written to mzip_dicts.h\n");

    return 0;
}
