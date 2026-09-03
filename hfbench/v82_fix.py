#!/usr/bin/env python3
"""v8.2: (1) plane-count choice: k=4 only when its order-0 estimate is >1% better than k=2
(ties go to 2 planes -- longer planes, fewer tables; measured: the plain '<' rule cost 1.66%
on BF16 vs best-of-2); (2) FAST uses zstd level 3 for the backstop estimate and for the final
compress of non-bypassed blocks (was 19 / the caller's level; FAST defines its own level)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
rep('''    unsigned k = (plane_bits(d, n, 4) < plane_bits(d, n, 2)) ? 4u : 2u;''',
    '''    const double b2 = plane_bits(d, n, 2), b4 = plane_bits(d, n, 4);
    unsigned k = (b4 < b2 * 0.99) ? 4u : 2u;   // v8.2: ties -> 2 planes (measured +1.66% loss on BF16 with plain '<')''', "k rule")
rep('''                size_t z = MZ_TIMED("zstd-19 backstop", ZSTD_compress(tb.data(), tb.size(), preprocess_data, preprocess_size, 19));''',
    '''                size_t z = MZ_TIMED("zstd-19 backstop", ZSTD_compress(tb.data(), tb.size(), preprocess_data, preprocess_size,
                                                                        mode == CompressionMode::FAST ? 3 : 19));   // v8.2: FAST estimates at its own level''', "backstop estimate level")
rep('''            if (fast_text_taken) {
                effective_level = 3;  // v8.1 FAST text: zstd default level, GB/s-class decode
            }''','''            if (mode == CompressionMode::FAST) {
                effective_level = 3;  // v8.1/8.2 FAST: zstd default level for every zstd-coded block, GB/s-class decode
            }''', "effective level")
shutil.copy(P, P + ".bak-v82"); open(P, "w").write(s); print("PATCHED v8.2", P)
