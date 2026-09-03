#!/usr/bin/env python3
"""v8.3 (FAST policy, on top of v8.2):
 (a) per-block text/non-text decided once; small text blocks (<= 16 KiB) keep the full
     machinery incl. slow backstops (BWT/CM/PPMd/dicts are where small HF JSON gets 14x and
     they are cheap at that size); text > 16 KiB takes the zstd-3 bypass;
 (b) a non-text block the per-plane coder could not compress must not go to BWT_TEXT/CM
     (measured: q4_k_m chunks classified BWT_TEXT -> 78 ms per chunk for 1.003x) -> TEXT
     type (zstd-3);
 (c) the top-level 4 KiB sample analysis only matters when the input spans >1 block; skip
     it when size <= block_size (output-neutral in every mode)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag, count=1):
    global s
    assert s.count(old) == count, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
# (a) rewrite the bypass block
rep('''        bool fast_pe_taken = false, fast_text_taken = false;
        std::vector<uint8_t> fast_pe;
        if (mode == CompressionMode::FAST && this_block >= 1024) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            if (fp * 100 < fl * 85) {
                if (this_block >= 4096) {''','''        bool fast_pe_taken = false, fast_text_taken = false, blk_is_text = false;
        std::vector<uint8_t> fast_pe;
        if (mode == CompressionMode::FAST) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            blk_is_text = (fl > 0 && fp * 100 >= fl * 85);
        }
        // v8.3: small text blocks (<= 16 KiB) keep the full BALANCED machinery in FAST -- that is
        // where BWT/CM/PPMd/dictionaries take small HF JSON to ~14x, and it is cheap at that size.
        const bool fast_slow_ok = (mode != CompressionMode::FAST) || (blk_is_text && this_block <= 16384);
        if (mode == CompressionMode::FAST && this_block >= 1024) {
            if (!blk_is_text) {
                if (this_block >= 4096) {''', "bypass head")
rep('''            } else {
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
''','''            } else if (this_block > 16384) {
                // v8.1 FAST text bypass: measured 137 ms per 64 KiB text chunk in FAST, 99% of it the
                // BWT+CM backstop. FAST text = TEXT type without the detector pass, final zstd level 3.
                fast_text_taken = true;
            }
        }
        // Analyze block
        BlockAnalysis analysis{};
        if (fast_pe_taken) analysis.type = BlockType::PLANE_ENTROPY;          // payload installed after the chain
        else if (fast_text_taken) { analysis.type = BlockType::TEXT; analysis.text_score = 1.0; }
        else {
            analysis = analyze_block(block_data, this_block);
            // v8.3: a non-text FAST block the per-plane coder could not compress must not take the
            // BWT_TEXT/CM branch (q4_k_m: 78 ms per chunk for 1.003x). TEXT type = zstd-3 in FAST.
            if (mode == CompressionMode::FAST && !blk_is_text && analysis.type == BlockType::BWT_TEXT)
                analysis.type = BlockType::TEXT;
        }
''', "bypass tail")
# (a2) guards use fast_slow_ok
rep('''            if (mode != CompressionMode::FAST) {   // v8.1: FAST never runs the slow backstops
''','''            if (fast_slow_ok) {   // v8.1/8.3: FAST runs the slow backstops only on small text blocks
''', "bwt9 guard")
rep('''            if (mode != CompressionMode::FAST) {
''','''            if (fast_slow_ok) {
''', "xz+bcj guards", count=2)
rep('''            if (mode != CompressionMode::FAST && this_block <= brotli_cap_bytes) {
''','''            if (fast_slow_ok && this_block <= brotli_cap_bytes) {
''', "brotli guard")
rep('''                if (ppmd_texty && mode != CompressionMode::FAST) {
''','''                if (ppmd_texty && fast_slow_ok) {
''', "ppmd guard")
# dictionary trial: FAST small-text only (unchanged rule, keep) -- nothing to do
# (c) skip the top-level sample analysis when the input is a single block
rep('''    if (size <= MAX_BLOCK_SIZE) {
        // Sample start and end to detect homogeneous periodic data
        size_t sample_size = std::min(size, (size_t)4096);
        auto analysis_start = analyze_block(data, sample_size);''','''    // v8.3: the sample analyses below only ever set block_size = size, which changes nothing
    // when the input already fits one block (size <= block_size). Skip them then: two 4 KiB
    // analyze_block passes per call, output-neutral in every mode.
    if (size <= MAX_BLOCK_SIZE && size > block_size) {
        // Sample start and end to detect homogeneous periodic data
        size_t sample_size = std::min(size, (size_t)4096);
        auto analysis_start = analyze_block(data, sample_size);''', "sample analysis")
shutil.copy(P, P + ".bak-v83"); open(P, "w").write(s); print("PATCHED v8.3", P)
