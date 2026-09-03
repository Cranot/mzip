#!/usr/bin/env python3
"""v8 (FAST only): (1) numeric bypass -- on a non-text block >= 4096 B, PLANE_ENTROPY is tried
BEFORE analyze_block; if it compresses to < 31/32 of the block and round-trips, the block is
done and every detector/trial is skipped. (2) skip the whole-file zstd arm when the block
container already reached <= 3/4 of the input. (3) mzpe::encode picks k by per-plane
order-0 entropy (one encode instead of two); MZIP_PE_BEST2=1 restores best-of-2 (A/B)."""
import sys, shutil
P = sys.argv[1]; s = open(P).read()
def rep(old, new, tag):
    global s
    assert s.count(old) == 1, tag + " anchor count=%d" % s.count(old)
    s = s.replace(old, new)
# (1a) analyze_block becomes conditional
rep('''        // Analyze block
        BlockAnalysis analysis = analyze_block(block_data, this_block);
''','''        // v8 FAST numeric bypass: on a non-text block the per-plane coder is tried FIRST; if it
        // compresses (< 31/32 of the block) and round-trips, the block is complete and the
        // ~40 detectors + strategy trials + specialist encoders are skipped entirely. Measured
        // before this: after the v4-v7 cuts, ~1.1 ms/64K chunk remained, of which the PE
        // encode itself was ~20% and detectors/trials most of the rest. Blocks PE does not
        // compress (quantised GGUF, already-compressed data) take the normal path.
        bool fast_pe_taken = false;
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
''', "analyze_block")
# (1b) install the PE payload right after the per-block state is declared
rep('''        bool use_generator = false;  // For LINEAR_GEN and PERIODIC, skip zstd entirely
''','''        bool use_generator = false;  // For LINEAR_GEN and PERIODIC, skip zstd entirely
        if (fast_pe_taken) {          // v8: complete PLANE_ENTROPY encoding already verified above
            memcpy(preprocess_data, fast_pe.data(), fast_pe.size());
            preprocess_size = fast_pe.size();
            use_generator = true;
        }
''', "use_generator decl")
# (1c) the v3 early path does not re-encode; backstops stay skipped
rep('''        bool pe_fast_done = false;
        if (mode == CompressionMode::FAST && this_block >= 4096) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;''','''        bool pe_fast_done = fast_pe_taken;
        if (!fast_pe_taken && mode == CompressionMode::FAST && this_block >= 4096) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;''', "early path guard")
# (2) whole-file zstd arm skip in FAST when the container is already good
rep('''    std::vector<uint8_t> zstd_buf(ZSTD_compressBound(size));
    size_t zstd_size = ZSTD_compress(
        zstd_buf.data(), zstd_buf.size(),
        data, size, zstd_level
''','''    // v8 FAST: the whole-file zstd arm is a container-family candidate; when the block
    // container already reached <= 3/4 of the input it cannot plausibly win on the data
    // FAST targets (zstd-1 on weights ~1.05x) -- skip it (marked unavailable, ~10% of a chunk).
    const bool skip_zstd_arm = (mode == CompressionMode::FAST) && (out_pos * 4 <= size * 3);
    std::vector<uint8_t> zstd_buf(skip_zstd_arm ? 0 : ZSTD_compressBound(size));
    size_t zstd_size = skip_zstd_arm ? (size_t)-1 : ZSTD_compress(
        zstd_buf.data(), zstd_buf.size(),
        data, size, zstd_level
''', "zstd arm")
# (3) k by entropy
rep('''inline std::vector<uint8_t> encode(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    auto a = encode_k(d, n, 2), b = encode_k(d, n, 4);
    return (b.size() < a.size()) ? b : a;
}''','''// Order-0 bits for plane j of k (histogram); used to choose k without encoding twice.
inline double plane_bits(const uint8_t* d, size_t n, unsigned k) {
    size_t L = n / k; double bits = 0.0;
    std::vector<uint32_t> h(256);
    for (unsigned j = 0; j < k; j++) {
        std::fill(h.begin(), h.end(), 0u);
        for (size_t i = 0; i < L; i++) h[d[i*k + j]]++;
        double hb = 0.0;
        for (unsigned c = 0; c < 256; c++) if (h[c]) { double p = (double)h[c] / (double)L; hb -= p * std::log2(p); }
        bits += std::min(8.0, hb + 0.02) * (double)L;   // raw plane = 8 bits/byte; small table cost
    }
    return bits;
}
inline std::vector<uint8_t> encode(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    static const bool best2 = std::getenv("MZIP_PE_BEST2") != nullptr;
    if (best2) { auto a = encode_k(d, n, 2), b = encode_k(d, n, 4); return (b.size() < a.size()) ? b : a; }
    // v8: pick k by per-plane order-0 entropy (two histogram passes, ~30 us at 64K) and encode once.
    unsigned k = (plane_bits(d, n, 4) < plane_bits(d, n, 2)) ? 4u : 2u;
    return encode_k(d, n, k);
}''', "encode k")
if "#include <cmath>" not in s.split("namespace mzpe")[0]:
    s = s.replace("namespace mzpe {", "#include <cmath>\nnamespace mzpe {", 1)
shutil.copy(P, P + ".bak-v8"); open(P, "w").write(s); print("PATCHED v8", P)
