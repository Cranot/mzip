import sys, io
P = "/root/mzip-hfbench/scratch-prof-src/mzip.hpp"
s = io.open(P, "r", encoding="utf-8", newline="").read()

# EXACT early-out for detect_periodic_approx.
# The consensus vote counts already determine the exception count exactly:
#   exceptions E = n - sum(best_count over the p positions)
# so the accept test  (E/n <= max_exception_rate)  AND  (2 + p + 5E < n/2)
# can be decided BEFORE the second full scan that materialises the exception
# vector. When either test fails the period cannot be accepted, so skipping the
# second pass is behaviour-preserving by construction.
old = """        // Pick the most common byte at each position
        for (size_t i = 0; i < p; i++) {
            int best_count = 0;
            uint8_t best_byte = 0;
            for (int b = 0; b < 256; b++) {
                if (vote_counts[i][b] > best_count) {
                    best_count = vote_counts[i][b];
                    best_byte = (uint8_t)b;
                }
            }
            best_pattern[i] = best_byte;
        }
"""
new = """        // Pick the most common byte at each position
        size_t mz_total_matches = 0;
        for (size_t i = 0; i < p; i++) {
            int best_count = 0;
            uint8_t best_byte = 0;
            for (int b = 0; b < 256; b++) {
                if (vote_counts[i][b] > best_count) {
                    best_count = vote_counts[i][b];
                    best_byte = (uint8_t)b;
                }
            }
            best_pattern[i] = best_byte;
            mz_total_matches += (size_t)best_count;
        }
        // Exact pre-decision: the vote counts already fix the exception count.
        {
            size_t mz_exc = n - mz_total_matches;
            double mz_rate = (double)mz_exc / n;
            size_t mz_enc = 2 + p + 5 * mz_exc;
            if (!(mz_rate <= max_exception_rate && mz_enc < n / 2)) continue;
        }
"""
c = s.count(old)
if c != 1:
    sys.exit("ANCHOR periodic_approx count=%d (want 1)" % c)
s = s.replace(old, new)

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("PA PATCH OK")
