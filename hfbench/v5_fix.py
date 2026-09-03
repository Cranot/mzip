#!/usr/bin/env python3
"""v5 scratch patches: (1) decoder work buffers sized to the stream (mirror of the encoder
fix); (2) detect_periodic_approx sample prefilter (MZIP_NO_PA_GATE=1 disables);
(3) detect_char_template: exclude the truncated last line from every loop (ASan over-read)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
# (1) decoder buffers
rep('''    std::vector<uint8_t> decompress_buf(MAX_BLOCK_SIZE);
    std::vector<uint8_t> unpreprocess_buf(MAX_BLOCK_SIZE);
''','''    // Work buffers sized to the stream (v5). A block's original size is <= original_size and
    // its preprocessed intermediate is <= 3x that (the encoder's preprocess_buf bound), so
    // min(MAX_BLOCK_SIZE, 3 * min(original_size, MAX_BLOCK_SIZE)) (64 KiB floor) holds every
    // stream the old 16 MiB buffers held. Before: 32 MiB zero-filled per call -- a 256 KiB
    // PLANE_ENTROPY chunk decoded at 15 MB/s while the same data in 64 KiB (compact MU
    // path, no work buffers) decoded at ~1.2 GB/s.
    const size_t dec_alloc = std::min((size_t)MAX_BLOCK_SIZE,
        std::max((size_t)65536, (size_t)3 * std::min((size_t)original_size, (size_t)MAX_BLOCK_SIZE)));
    std::vector<uint8_t> decompress_buf(dec_alloc);
    std::vector<uint8_t> unpreprocess_buf(dec_alloc);
''', "decoder buffers")
# (2) periodic_approx prefilter
rep('''    for (size_t p : {1, 2, 3, 4, 6, 8, 16, 32, 64, 128, 256}) {
        if (p > n / 2) continue;

        // First pass: find the "consensus" pattern by majority vote at each position
        std::vector<uint8_t> best_pattern(p);
''','''    static const bool pa_no_gate = std::getenv("MZIP_NO_PA_GATE") != nullptr;
    for (size_t p : {1, 2, 3, 4, 6, 8, 16, 32, 64, 128, 256}) {
        if (p > n / 2) continue;

        // Prefilter (v5): a period that qualifies (<= max_exception_rate exceptions over the
        // whole block) must look periodic in a sample too. Three 1365-byte windows
        // (start / middle / end, phase-aligned to p) are voted first; if their exception rate
        // exceeds 4x the limit the full two-pass scan is skipped. The full scan is 2n byte
        // visits with a modulo each, x11 periods: measured ~8 ms per 64 KiB weight block,
        // 96% of detector time, 0 hits. MZIP_NO_PA_GATE=1 disables the prefilter (A/B).
        if (n >= 8192 && !pa_no_gate) {
            const size_t W = 1365;
            const size_t offs[3] = {0, ((n / 2) / p) * p, ((n - W) / p) * p};
            std::vector<std::array<int, 256>> sv(p);
            for (auto& c : sv) c.fill(0);
            size_t sn = 0;
            for (size_t w = 0; w < 3; w++)
                for (size_t i = 0; i < W; i++) { sv[(offs[w] + i) % p][data[offs[w] + i]]++; sn++; }
            size_t agree = 0;
            for (size_t i = 0; i < p; i++) { int best = 0; for (int b = 0; b < 256; b++) if (sv[i][b] > best) best = sv[i][b]; agree += (size_t)best; }
            if ((sn - agree) > (size_t)(sn * max_exception_rate * 4.0)) continue;
        }

        // First pass: find the "consensus" pattern by majority vote at each position
        std::vector<uint8_t> best_pattern(p);
''', "periodic_approx prefilter")
# (3) char_template over-read
rep('''        if (idx == lines.size() - 1 && lines[idx].size() < first_line_len * 3 / 4) continue;
        min_len = std::min(min_len, lines[idx].size());
    }
''','''        if (idx == lines.size() - 1 && lines[idx].size() < first_line_len * 3 / 4) continue;
        min_len = std::min(min_len, lines[idx].size());
    }
    // The truncated last line was excluded from min_len above, but every later loop indexes
    // lines[idx][j] for j < min_len -- a heap over-read on that line (ASan: 1 byte past an
    // 18-byte final line, unit test columnar_roundtrip_nginx, pre-existing). Drop it here so
    // all later loops are consistent with min_len.
    if (!sample_indices.empty() && sample_indices.back() == lines.size() - 1 &&
        lines[sample_indices.back()].size() < first_line_len * 3 / 4) {
        sample_indices.pop_back();
        if (sample_indices.empty()) return false;
    }
''', "char_template over-read")
shutil.copy(P, P + ".bak-v5"); open(P, "w").write(s); print("PATCHED v5", P)
