#!/usr/bin/env python3
"""v8.4: one FAST policy switch per block. fast_block = FAST and NOT a small text block.
Small text blocks (<= 16 KiB, printable) in FAST behave exactly as BALANCED: level-19
dictionary selection, level-9/19 final, level-19 estimate, all backstops (v8.3 already let
the backstops run; the level rules still said FAST -> 10.6x instead of 14.2x on HF JSON)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag, count=1):
    global s
    assert s.count(old) == count, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
rep('''        const bool fast_slow_ok = (mode != CompressionMode::FAST) || (blk_is_text && this_block <= 16384);
''','''        const bool fast_slow_ok = (mode != CompressionMode::FAST) || (blk_is_text && this_block <= 16384);
        const bool fast_block = !fast_slow_ok;   // v8.4: every FAST level/gate rule keys on this, so a small text block is pure BALANCED
''', "fast_block decl")
rep('''        const bool dict_gate_on = (mode == CompressionMode::FAST) || std::getenv("MZIP_DICT_TEXTGATE");
        const bool dict_trial_ok = (!dict_gate_on || dict_blk_text) &&
                                   (mode != CompressionMode::FAST || this_block <= 16384);   // v8.1: FAST dicts only where they matter (small files)
        const int dict_sel_level = (mode == CompressionMode::FAST) ? 1 : 19;''','''        const bool dict_gate_on = fast_block || std::getenv("MZIP_DICT_TEXTGATE");
        const bool dict_trial_ok = (!dict_gate_on || dict_blk_text) &&
                                   (!fast_block || this_block <= 16384);   // v8.1/8.4: FAST dicts only where they matter (small files)
        const int dict_sel_level = fast_block ? 1 : 19;''', "dict rules")
rep('''                                                                        mode == CompressionMode::FAST ? 3 : 19));   // v8.2: FAST estimates at its own level''',
    '''                                                                        fast_block ? 3 : 19));   // v8.2/8.4: FAST estimates at its own level''', "estimate level")
rep('''            if (mode == CompressionMode::FAST) {
                effective_level = 3;  // v8.1/8.2 FAST: zstd default level for every zstd-coded block, GB/s-class decode
            }''','''            if (fast_block) {
                effective_level = 3;  // v8.1/8.2/8.4 FAST: zstd default level for every zstd-coded block, GB/s-class decode
            }''', "effective level")
shutil.copy(P, P + ".bak-v84"); open(P, "w").write(s); print("PATCHED v8.4", P)
