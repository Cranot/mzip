// Emit mzip_dicts.h: 5 synthetic legacy + 6 real-data v2 + 1 JSON_REAL.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <fstream>
#include "mzip_dicts.h"

static std::vector<uint8_t> read_file(const char* p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    size_t sz = f.tellg();
    if (!sz) return {};
    f.seekg(0);
    std::vector<uint8_t> v(sz);
    f.read((char*)v.data(), sz);
    return v;
}

static void emit_array(FILE* f, const char* name, const uint8_t* data, size_t sz) {
    fprintf(f, "static const uint8_t %s[%zu] = {\n  ", name, sz);
    for (size_t i = 0; i < sz; i++) {
        fprintf(f, "0x%02x,", data[i]);
        if ((i+1) % 16 == 0) fprintf(f, "\n  ");
        else fprintf(f, " ");
    }
    fprintf(f, "\n};\nstatic const size_t %s_SIZE = %zu;\n\n", name, sz);
}

int main() {
    struct New { const char* name; const char* file; uint8_t id; };
    New news[] = {
        {"DICT_MD_REAL",   "train_corpus/md_dict.bin",   6},
        {"DICT_YAML_REAL", "train_corpus/yaml_dict.bin", 7},
        {"DICT_HCL_REAL",  "train_corpus/hcl_dict.bin",  8},
        {"DICT_SQL_REAL",  "train_corpus/sql_dict.bin",  9},
        {"DICT_XML_REAL",  "train_corpus/xml_dict.bin",  10},
        {"DICT_CODE_REAL", "train_corpus/code_dict.bin", 11},
        {"DICT_JSON_REAL", "train_corpus/json_dict.bin", 12},
        {"DICT_HF_REAL",   "train_corpus/hf_dict.bin",   13},
    };

    FILE* f = fopen("mzip_dicts_new.h", "w");
    fprintf(f, "// Auto-generated zstd dictionaries for mzip\n");
    fprintf(f, "// IDs 1-5: synthetic-data dicts (legacy, kept for backward compat)\n");
    fprintf(f, "// IDs 6-12: real-data dicts trained on public GitHub samples NOT in real_bench/\n");
    fprintf(f, "// Trial-pick selects the smallest output per block.\n#pragma once\n#include <cstdint>\n#include <cstddef>\n\nnamespace mzip_dicts {\n\n");

    emit_array(f, "DICT_CODE",   mzip_dicts::DICT_CODE,   mzip_dicts::DICT_CODE_SIZE);
    emit_array(f, "DICT_CONFIG", mzip_dicts::DICT_CONFIG, mzip_dicts::DICT_CONFIG_SIZE);
    emit_array(f, "DICT_TEXT",   mzip_dicts::DICT_TEXT,   mzip_dicts::DICT_TEXT_SIZE);
    emit_array(f, "DICT_LOG",    mzip_dicts::DICT_LOG,    mzip_dicts::DICT_LOG_SIZE);
    emit_array(f, "DICT_QUERY",  mzip_dicts::DICT_QUERY,  mzip_dicts::DICT_QUERY_SIZE);

    size_t total_new = 0;
    for (auto& n : news) {
        auto data = read_file(n.file);
        if (data.empty()) { fprintf(stderr, "MISSING: %s\n", n.file); continue; }
        emit_array(f, n.name, data.data(), data.size());
        total_new += data.size();
        fprintf(stderr, "  %s: %zu bytes\n", n.name, data.size());
    }

    fprintf(f, "struct DictEntry { uint8_t id; const uint8_t* data; size_t size; };\n");
    fprintf(f, "static const DictEntry ALL_DICTS[] = {\n");
    fprintf(f, "    {1, DICT_CODE,      DICT_CODE_SIZE},\n");
    fprintf(f, "    {2, DICT_CONFIG,    DICT_CONFIG_SIZE},\n");
    fprintf(f, "    {3, DICT_TEXT,      DICT_TEXT_SIZE},\n");
    fprintf(f, "    {4, DICT_LOG,       DICT_LOG_SIZE},\n");
    fprintf(f, "    {5, DICT_QUERY,     DICT_QUERY_SIZE},\n");
    fprintf(f, "    {6, DICT_MD_REAL,   DICT_MD_REAL_SIZE},\n");
    fprintf(f, "    {7, DICT_YAML_REAL, DICT_YAML_REAL_SIZE},\n");
    fprintf(f, "    {8, DICT_HCL_REAL,  DICT_HCL_REAL_SIZE},\n");
    fprintf(f, "    {9, DICT_SQL_REAL,  DICT_SQL_REAL_SIZE},\n");
    fprintf(f, "    {10, DICT_XML_REAL, DICT_XML_REAL_SIZE},\n");
    fprintf(f, "    {11, DICT_CODE_REAL,DICT_CODE_REAL_SIZE},\n");
    fprintf(f, "    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\n");
    fprintf(f, "    {13, DICT_HF_REAL,  DICT_HF_REAL_SIZE},\n");
    fprintf(f, "};\nstatic const size_t NUM_DICTS = sizeof(ALL_DICTS) / sizeof(ALL_DICTS[0]);\n\n");
    fprintf(f, "} // namespace mzip_dicts\n");
    fclose(f);
    return 0;
}
