#!/usr/bin/env python3
"""v8.1: (a) install the FAST PE payload AFTER the encoder chain (v8 installed it before and
the chain's default branch overwrote it with raw bytes -> every chunk fell to uRAW);
(b) FAST text bypass: printable blocks >= 1024 B skip analyze_block, type TEXT, final zstd
level 3; (c) FAST dictionary trial only for blocks <= 16 KiB; (d) FAST never runs the
bwt9 / xz / BCJ-xz / brotli-11 / PPMd backstop trials."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag, count=1):
    global s
    assert s.count(old) == count, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
# (a) remove the early install
rep('''        bool use_generator = false;  // For LINEAR_GEN and PERIODIC, skip zstd entirely
        if (fast_pe_taken) {          // v8: complete PLANE_ENTROPY encoding already verified above
            memcpy(preprocess_data, fast_pe.data(), fast_pe.size());
            preprocess_size = fast_pe.size();
            use_generator = true;
        }
''','''        bool use_generator = false;  // For LINEAR_GEN and PERIODIC, skip zstd entirely
''', "remove early install")
# (b) bypass block: add text bypass
rep('''        bool fast_pe_taken = false;
        std::vector<uint8_t> fast_pe;
        if (mode == CompressionMode::FAST && this_block >= 4096) {
            size_t fl = 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            if (fp * 100 < fl * 85) {
                fast_pe = mzpe::encode(block_data, this_block);
                if (!fast_pe.empty() && fast_pe.size() < this_block - this_block / 32 &&
                    fast_pe.size() <= ZSTD_compressBound(this_block)) {
                    auto rt = mzpe::decode(fast_pe.data(), fast_pe.size(), this_block);
                    if (rt.size() == this_block && std::memcmp(rt.data(), block_data, this_block) == 0)
                        fast_pe_taken = true;
                }
            }
        }
        // Analyze block
        BlockAnalysis analysis{};
        if (fast_pe_taken) analysis.type = BlockType::PLANE_ENTROPY;
        else analysis = analyze_block(block_data, this_block);
''','''        bool fast_pe_taken = false, fast_text_taken = false;
        std::vector<uint8_t> fast_pe;
        if (mode == CompressionMode::FAST && this_block >= 1024) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            if (fp * 100 < fl * 85) {
                if (this_block >= 4096) {
                    fast_pe = mzpe::encode(block_data, this_block);
                    if (!fast_pe.empty() && fast_pe.size() < this_block - this_block / 32 &&
                        fast_pe.size() <= ZSTD_compressBound(this_block)) {
                        auto rt = mzpe::decode(fast_pe.data(), fast_pe.size(), this_block);
                        if (rt.size() == this_block && std::memcmp(rt.data(), block_data, this_block) == 0)
                            fast_pe_taken = true;
                    }
                }
            } else {
                // v8.1 FAST text bypass: measured 137 ms per 64 KiB text chunk in FAST, 99% of it the
                // BWT+CM backstop. FAST text = TEXT type without the detector pass, final zstd level 3,
                // dictionary trial only on small blocks (below), no slow backstops.
                fast_text_taken = true;
            }
        }
        // Analyze block
        BlockAnalysis analysis{};
        if (fast_pe_taken) analysis.type = BlockType::PLANE_ENTROPY;          // payload installed after the chain
        else if (fast_text_taken) { analysis.type = BlockType::TEXT; analysis.text_score = 1.0; }
        else analysis = analyze_block(block_data, this_block);
''', "bypass block")
# (a2) install after the chain, at the early path
rep('''        bool pe_fast_done = fast_pe_taken;
        if (!fast_pe_taken && mode == CompressionMode::FAST && this_block >= 4096) {''','''        if (fast_pe_taken) {   // v8.1: install AFTER the encoder chain (its default branch copies raw bytes)
            memcpy(preprocess_data, fast_pe.data(), fast_pe.size());
            preprocess_size = fast_pe.size();
            analysis.type = BlockType::PLANE_ENTROPY;
            use_generator = true;
        }
        bool pe_fast_done = fast_pe_taken || fast_text_taken;
        if (!fast_pe_taken && !fast_text_taken && mode == CompressionMode::FAST && this_block >= 4096) {''', "install after chain")
# (c) dict trial size cap in FAST
rep('''        const bool dict_trial_ok = !dict_gate_on || dict_blk_text;''',
    '''        const bool dict_trial_ok = (!dict_gate_on || dict_blk_text) &&
                                   (mode != CompressionMode::FAST || this_block <= 16384);   // v8.1: FAST dicts only where they matter (small files)''', "dict cap")
# (b2) final level for FAST text
rep('''            int effective_level = zstd_level;
            if (analysis.type == BlockType::TEXT && zstd_level < 9) {
                effective_level = 9;  // Level 9 is optimal for most text
''','''            int effective_level = zstd_level;
            if (analysis.type == BlockType::TEXT && zstd_level < 9) {
                effective_level = 9;  // Level 9 is optimal for most text
            }
            if (fast_text_taken) {
                effective_level = 3;  // v8.1 FAST text: zstd default level, GB/s-class decode
            }
            if (false) {
''', "effective level")
# (d) backstop guards
rep('''            // it beats bzip2/xz/zstd. Tried first so ties prefer our own tech over the external backstops.
            {
''','''            // it beats bzip2/xz/zstd. Tried first so ties prefer our own tech over the external backstops.
            if (mode != CompressionMode::FAST) {   // v8.1: FAST never runs the slow backstops
''', "bwt9 guard")
rep('''            // xz (liblzma -9 EXTREME) trial -> XZLIB. Genuine xz-quality LZMA; flips large-repetitive (SQL dumps).
            {
''','''            // xz (liblzma -9 EXTREME) trial -> XZLIB. Genuine xz-quality LZMA; flips large-repetitive (SQL dumps).
            if (mode != CompressionMode::FAST) {
''', "xz guard")
rep('''            // => zero regression by construction. (2026-08-08, gap-analysis workflow)
            {
''','''            // => zero regression by construction. (2026-08-08, gap-analysis workflow)
            if (mode != CompressionMode::FAST) {
''', "bcj guard")
rep('''            if (this_block <= brotli_cap_bytes) {
''','''            if (mode != CompressionMode::FAST && this_block <= brotli_cap_bytes) {
''', "brotli guard")
rep('''                if (ppmd_texty) {
''','''                if (ppmd_texty && mode != CompressionMode::FAST) {
''', "ppmd guard")
shutil.copy(P, P + ".bak-v81"); open(P, "w").write(s); print("PATCHED v8.1", P)
