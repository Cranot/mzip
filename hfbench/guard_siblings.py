#!/usr/bin/env python3
"""Add roundtrip verifies to four sibling adoption branches that take a specialist
encoder's output on faith: LINE_TEMPLATE, SORTED_DICT, KV_CONFIG, CODE_STREAM.
Same shape as the NUM_EXTRACT fix and the existing PHRASE_PARTITION/CHAR_TEMPLATE
guards. Exact-string replace on /root/mzip/mzip.hpp with backup; refuses on
non-unique anchor or double-apply. Then builds mzip_fixed3 + ut_repo3 and runs
the losslessness suite. Run only when the box is not timing.
Decoder signatures (read from source):
  decode_line_template(data, n)
  decode_sorted_dict(data, n, original_size)
  decode_kv_config(encoded, encoded_size, original_size)
  decode_code_stream(encoded, encoded_size, original_size)
"""
import shutil, subprocess, sys
P = "/root/mzip/mzip.hpp"
src = open(P, encoding="utf-8", errors="surrogateescape").read()
if "SIBLING GUARDS (2026-09-02)" in src: print("ALREADY APPLIED"); sys.exit(0)

def guard(name, decode_call, orig_block):
    return f"""            // SIBLING GUARDS (2026-09-02): {name} adopted its encoder's output with no roundtrip
            // check -- the same shape that let a lossy NUM_EXTRACT stream reach the top-level
            // verify and send a whole 58 KB HF config to uRAW (28x blowup). Mirror
            // PHRASE_PARTITION/CHAR_TEMPLATE: verify here, else fall to TEXT so the block flows
            // to the backstop ensemble. An `encoded.empty()` guard alone cannot detect a
            // well-formed stream that decodes to the wrong thing.
            std::vector<uint8_t> {name.lower()}_rt;
            if (!encoded.empty()) {name.lower()}_rt = {decode_call};
            if (encoded.empty() || {name.lower()}_rt.size() != this_block ||
                std::memcmp({name.lower()}_rt.data(), block_data, this_block) != 0) {{
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            }} else {{
{orig_block}            }}
"""

PATCHES = [
 ("LINE_TEMPLATE",
  """            auto encoded = MZ_TIMED("encode_line_template", encode_line_template(analysis.line_template, zstd_level));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already fully encoded, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::PHRASE_PARTITION) {""",
  """            auto encoded = MZ_TIMED("encode_line_template", encode_line_template(analysis.line_template, zstd_level));
""" + guard("LINE_TEMPLATE", "decode_line_template(encoded.data(), encoded.size())",
  """                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already fully encoded, don't re-compress
                res.blocks_text++;
""") + """        } else if (analysis.type == BlockType::PHRASE_PARTITION) {"""),

 ("SORTED_DICT",
  """            auto encoded = MZ_TIMED("encode_sorted_dict", encode_sorted_dict(analysis.sorted_dict, zstd_level));
            if (encoded.empty()) {
                // Fallback to regular TEXT if encoding failed
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::KV_CONFIG) {""",
  """            auto encoded = MZ_TIMED("encode_sorted_dict", encode_sorted_dict(analysis.sorted_dict, zstd_level));
""" + guard("SORTED_DICT", "decode_sorted_dict(encoded.data(), encoded.size(), this_block)",
  """                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
""") + """        } else if (analysis.type == BlockType::KV_CONFIG) {"""),

 ("KV_CONFIG",
  """            auto encoded = MZ_TIMED("encode_kv_config", encode_kv_config(analysis.kv_config, zstd_level));
            if (encoded.empty()) {
                // Fallback to TEXT if encoding failed
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
            }
        } else if (analysis.type == BlockType::SECTION_TEMPLATE) {""",
  """            auto encoded = MZ_TIMED("encode_kv_config", encode_kv_config(analysis.kv_config, zstd_level));
""" + guard("KV_CONFIG", "decode_kv_config(encoded.data(), encoded.size(), this_block)",
  """                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed
                res.blocks_text++;
""") + """        } else if (analysis.type == BlockType::SECTION_TEMPLATE) {"""),

 ("CODE_STREAM",
  """            auto encoded = MZ_TIMED("encode_code_stream", encode_code_stream(analysis.code_stream));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already compressed, don't re-compress
            res.blocks_text++;
        } else if (analysis.type == BlockType::REFERENCE) {""",
  """            auto encoded = MZ_TIMED("encode_code_stream", encode_code_stream(analysis.code_stream));
""" + guard("CODE_STREAM", "decode_code_stream(encoded.data(), encoded.size(), this_block)",
  """                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already compressed, don't re-compress
                res.blocks_text++;
""") + """        } else if (analysis.type == BlockType::REFERENCE) {"""),
]

for name, old, new in PATCHES:
    n = src.count(old)
    if n != 1: print(f"REFUSING: {name} anchor matched {n} times"); sys.exit(1)
shutil.copy2(P, P + ".bak-siblings")
for name, old, new in PATCHES: src = src.replace(old, new)
open(P, "w", encoding="utf-8", errors="surrogateescape").write(src)
print("PATCHED 4 branches; backup mzip.hpp.bak-siblings")

B = "/root/mzip-hfbench/build"
OBJS = f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"
LIBS = "-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name, srcf, extra in (("mzip_fixed3", "mzip_cli.cpp", ""), ("ut_repo3", "mzip_unit_tests.cpp", "-D_USE_MATH_DEFINES")):
    r = subprocess.run(f"cd {B} && g++ -O3 -std=c++17 -march=native {extra} -I{B} -I/root/mzip -o {B}/{name} /root/mzip/{srcf} {OBJS} {LIBS}", shell=True, capture_output=True, text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}", [l for l in r.stderr.splitlines() if ' error' in l][:4])
r = subprocess.run(f"{B}/ut_repo3", capture_output=True, text=True)
print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
print(subprocess.run("cd /root/mzip && git diff --stat", shell=True, capture_output=True, text=True).stdout)
print("ALLDONE_SIB")
