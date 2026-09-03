#!/usr/bin/env python3
"""Add a uRAW-bloat regression test for the NUM_EXTRACT leading-zero defect.

Placed in the existing `uRAW-BLOAT PATHOLOGY GUARDS` section, in the same idiom
as no_uraw_bloat_repetitive_syslog. Falsified: this test FAILS on the stock
build (uRAW, 52,490 B) and PASSES on the patched build (594 B).
"""
import sys, shutil, os

SRC = "/root/mzip/mzip_unit_tests.cpp"

ANCHOR_FN = """// ============================================================================
// CHAR_TEMPLATE Tests
// ============================================================================"""

NEW_FN = '''TEST(no_uraw_bloat_numeric_config_leading_zeros) {
    // NUM_EXTRACT parses each digit run into a uint32 and re-emits the VALUE, so a
    // ZERO-PADDED run cannot survive: the "02" of 0.02 and the "001" of 0.001 come
    // back as "2" and "1", one/two bytes short. (A run >= 2^32 wraps the same way.)
    // Real reproducer: nvidia/Qwen3.6-35B-A3B-NVFP4 config.json, 58,110 B. The
    // adoption branch took the lossy stream with NO per-block verify, the block then
    // failed the TOP-LEVEL end-to-end verify, and the WHOLE file fell to the uRAW
    // store -- 58,115 B (larger than the input) against brotli's 2,050 B = 28x.
    // initializer_range 0.02 and layer_norm_eps 1e-06 appear in essentially every HF
    // transformer config; the repetitive module-path list is what makes NUM_EXTRACT
    // win selection in the first place.
    // Measured: stock build 52,490 B (uRAW) -> patched build 594 B.
    std::string s = "{\\n  \\"initializer_range\\": 0.02,\\n  \\"layer_norm_eps\\": 1e-06,\\n"
                    "  \\"rms_norm_eps\\": 0.001,\\n  \\"ignored_modules\\": [\\n";
    const char* parts[] = {"down_proj", "gate_proj", "up_proj", "experts"};
    for (int i = 0; i < 200; i++) {
        for (const char* p : parts) {
            char buf[160];
            snprintf(buf, sizeof buf,
                     "    \\"model.language_model.layers.%d.mlp.shared_expert.%s\\",\\n", i, p);
            s += buf;
        }
    }
    s += "    \\"lm_head\\"\\n  ]\\n}";

    std::vector<uint8_t> in(s.begin(), s.end());
    auto c = mzip::compress(in.data(), in.size());
    ASSERT(!is_uraw_wholefile(c));
    ASSERT(c.size() * 8 < in.size());          // repetitive config -> must compress well
    auto d = mzip::decompress(c.data(), c.size());
    ASSERT_EQ(d.size(), in.size());
    ASSERT(memcmp(d.data(), in.data(), in.size()) == 0);
}

'''

ANCHOR_RUN = "    RUN_TEST(no_uraw_bloat_repetitive_syslog);"
NEW_RUN = ("    RUN_TEST(no_uraw_bloat_repetitive_syslog);\n"
           "    RUN_TEST(no_uraw_bloat_numeric_config_leading_zeros);")

src = open(SRC, encoding="utf-8", errors="surrogateescape").read()

if "no_uraw_bloat_numeric_config_leading_zeros" in src:
    print("ALREADY APPLIED - no change made"); sys.exit(0)

if src.count(ANCHOR_FN) != 1:
    print(f"REFUSING: function anchor matched {src.count(ANCHOR_FN)} times"); sys.exit(1)
if src.count(ANCHOR_RUN) != 1:
    print(f"REFUSING: runner anchor matched {src.count(ANCHOR_RUN)} times"); sys.exit(1)

shutil.copy2(SRC, SRC + ".bak-uravtest")
src = src.replace(ANCHOR_FN, NEW_FN + ANCHOR_FN)
src = src.replace(ANCHOR_RUN, NEW_RUN)
open(SRC, "w", encoding="utf-8", errors="surrogateescape").write(src)
print(f"PATCHED {SRC}  ({os.path.getsize(SRC):,} B)")
print(f"backup at {SRC}.bak-uravtest")
