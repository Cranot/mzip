#!/usr/bin/env python3
"""Guard the NUM_EXTRACT adoption branch with a roundtrip verify.

Mirrors the PHRASE_PARTITION (mzip.hpp:16211) and CHAR_TEMPLATE (16046) fixes,
which were added after identical incidents. Exact-string replacement; refuses to
run if the source does not match byte-for-byte, and refuses to double-apply.
"""
import sys, shutil, os

SRC = "/root/mzip/mzip.hpp"

OLD = """        } else if (analysis.type == BlockType::NUM_EXTRACT) {
            // NUM_EXTRACT: extract embedded decimal numbers from text
            // 900 bytes better than brotli on Makefiles!
            auto encoded = MZ_TIMED("encode_num_extract", encode_num_extract(block_data, this_block));
            memcpy(preprocess_data, encoded.data(), encoded.size());
            preprocess_size = encoded.size();
            use_generator = true;  // Already fully encoded, don't re-compress
            res.blocks_text++;
"""

NEW = """        } else if (analysis.type == BlockType::NUM_EXTRACT) {
            // NUM_EXTRACT: extract embedded decimal numbers from text
            // 900 bytes better than brotli on Makefiles!
            // ROUNDTRIP VERIFY (2026-09-01) -- encode_num_extract is NOT guaranteed
            // lossless. On a real HF model config (nvidia/Qwen3.6-35B-A3B-NVFP4
            // config.json, 58,110 B) it returns a well-formed 1,600-byte stream that
            // decodes back 5 bytes SHORT. This branch adopted it with NO per-block
            // verify, so the corrupt block failed the TOP-LEVEL end-to-end verify and
            // the WHOLE file fell to the uRAW store: 58,115 B (larger than the input)
            // against brotli's 2,050 B -- a 28x blowup on a file every standard codec
            // handles. An `encoded.empty()` guard would NOT have caught it; the stream
            // is well-formed and simply decodes to the wrong thing, exactly as the
            // PHRASE_PARTITION comment below warns. Mirror PHRASE_PARTITION/
            // CHAR_TEMPLATE: verify here, else fall to TEXT so the block flows to the
            // backstop ensemble (PPMD/XZLIB/BROTLI/bwt9).
            auto encoded = MZ_TIMED("encode_num_extract", encode_num_extract(block_data, this_block));
            std::vector<uint8_t> ne_rt;
            if (!encoded.empty()) ne_rt = decode_num_extract(encoded.data(), encoded.size(), this_block);
            if (encoded.empty() || ne_rt.size() != this_block ||
                std::memcmp(ne_rt.data(), block_data, this_block) != 0) {
                analysis.type = BlockType::TEXT;
                memcpy(preprocess_data, block_data, this_block);
                res.blocks_text++;
            } else {
                memcpy(preprocess_data, encoded.data(), encoded.size());
                preprocess_size = encoded.size();
                use_generator = true;  // Already fully encoded, don't re-compress
                res.blocks_text++;
            }
"""

src = open(SRC, encoding="utf-8", errors="surrogateescape").read()

if "ROUNDTRIP VERIFY (2026-09-01)" in src:
    print("ALREADY APPLIED - no change made")
    sys.exit(0)

n = src.count(OLD)
if n != 1:
    print(f"REFUSING: expected exactly 1 match of the target block, found {n}.")
    print("The source has changed; re-derive the patch rather than forcing it.")
    sys.exit(1)

shutil.copy2(SRC, SRC + ".bak-numextract")
open(SRC, "w", encoding="utf-8", errors="surrogateescape").write(src.replace(OLD, NEW))
print(f"PATCHED {SRC}")
print(f"backup at {SRC}.bak-numextract")
print(f"size {os.path.getsize(SRC):,} B")
