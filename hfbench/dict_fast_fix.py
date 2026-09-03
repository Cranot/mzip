#!/usr/bin/env python3
"""v4: FAST skips the 13x zstd-19 dictionary trial on non-text blocks and selects the
dictionary at level 1 on text blocks. BALANCED/SMALL unchanged unless MZIP_DICT_TEXTGATE
is set (experiment toggle to measure whether a dictionary ever wins a non-text block)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
old = '''        size_t dict_trial_max = std::getenv("MZIP_MAXRATIO") ? 1048576 : 262144;
        if (this_block <= dict_trial_max && this_block >= 256) {
            ZSTD_CCtx* dcctx = ZSTD_createCCtx();
            size_t best_dsz = SIZE_MAX;
            uint8_t best_did = 0;
            std::vector<uint8_t> dtbuf(ZSTD_compressBound(this_block));
            for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                auto& d = mzip_dicts::ALL_DICTS[di];
                size_t sz = ZSTD_compress_usingDict(dcctx,
                    dtbuf.data(), dtbuf.size(),
                    block_data, this_block, d.data, d.size, 19);
'''
new = '''        size_t dict_trial_max = std::getenv("MZIP_MAXRATIO") ? 1048576 : 262144;
        // v4 (FAST): all 13 dictionaries are text (code/config/log/json/yaml/...). On a non-text
        // block this 13x zstd-19 trial cannot win and was ~55 ms of the ~80 ms a FAST 64 KiB
        // weight chunk cost (13 x ~4 ms at 64K). FAST skips the trial on non-text blocks and
        // selects the dictionary at level 1 on text blocks (the single final encode stays 19).
        // BALANCED/SMALL are unchanged; MZIP_DICT_TEXTGATE applies the text gate to them too
        // (measurement toggle: does a dictionary EVER win a non-text block?).
        bool dict_blk_text = true;
        {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            dict_blk_text = (fl > 0 && fp * 100 >= fl * 85);
        }
        const bool dict_gate_on = (mode == CompressionMode::FAST) || std::getenv("MZIP_DICT_TEXTGATE");
        const bool dict_trial_ok = !dict_gate_on || dict_blk_text;
        const int dict_sel_level = (mode == CompressionMode::FAST) ? 1 : 19;
        if (dict_trial_ok && this_block <= dict_trial_max && this_block >= 256) {
            ZSTD_CCtx* dcctx = ZSTD_createCCtx();
            size_t best_dsz = SIZE_MAX;
            uint8_t best_did = 0;
            std::vector<uint8_t> dtbuf(ZSTD_compressBound(this_block));
            for (size_t di = 0; di < mzip_dicts::NUM_DICTS; di++) {
                auto& d = mzip_dicts::ALL_DICTS[di];
                size_t sz = ZSTD_compress_usingDict(dcctx,
                    dtbuf.data(), dtbuf.size(),
                    block_data, this_block, d.data, d.size, dict_sel_level);
'''
assert s.count(old) == 1, "dict trial anchor"
s = s.replace(old, new)
old2 = '''                size_t zest = ZSTD_compress(dtbuf.data(), dtbuf.size(),
                    preprocess_data, preprocess_size, 19);
                if (!ZSTD_isError(zest)) current_est = zest;'''
new2 = '''                size_t zest = ZSTD_compress(dtbuf.data(), dtbuf.size(),
                    preprocess_data, preprocess_size, dict_sel_level);
                if (!ZSTD_isError(zest)) current_est = zest;'''
assert s.count(old2) == 1, "zest anchor"
s = s.replace(old2, new2)
shutil.copy(P, P + ".bak-dictfast")
open(P, "w").write(s); print("PATCHED", P)
