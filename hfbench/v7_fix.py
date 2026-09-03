#!/usr/bin/env python3
"""v7: replace the vote-table prefilter in detect_periodic_approx with an equality
prefilter (data[i] != data[i+p] over three sample windows): no allocation, no 256xp scan."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
start = s.index("        // Prefilter (v5): a period that qualifies")
end = s.index("        // First pass: find the \"consensus\" pattern by majority vote at each position", start)
new = '''        // Prefilter (v7): a period that qualifies (<= max_exception_rate exceptions e over the
        // block) has data[i] == data[i+p] for all but <= 2e of positions. Three 1365-position
        // windows (start / middle / end) are compared; if the mismatch rate exceeds
        // 8 x max_exception_rate (i.e. e > 4x the limit even in the best case) the full
        // two-pass vote scan is skipped. ~45K byte compares per block, no allocation. The
        // full scan is 2n visits with a modulo each, x11 periods: measured ~8 ms per 64 KiB
        // weight block, 96% of detector time, 0 hits; the v5 vote-table prefilter still cost
        // ~0.45 ms (256 x p table per period). MZIP_NO_PA_GATE=1 disables (A/B).
        if (n >= 8192 && !pa_no_gate) {
            const size_t W = 1365;
            const size_t span = n - p - W;                 // last valid window start (n >= 8192 > p + W)
            const size_t offs[3] = {0, span / 2, span};
            size_t mism = 0;
            for (size_t w = 0; w < 3; w++) {
                const uint8_t* a = data + offs[w];
                const uint8_t* b = a + p;
                for (size_t i = 0; i < W; i++) mism += (a[i] != b[i]);
            }
            if (mism > (size_t)(3.0 * W * max_exception_rate * 8.0)) continue;
        }

'''
s = s[:start] + new + s[end:]
shutil.copy(P, P + ".bak-v7"); open(P, "w").write(s); print("PATCHED v7", P)
