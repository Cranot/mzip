import sys, io

P = "/root/mzip-hfbench/scratch-prof-src/mzip.hpp"
src = io.open(P, "r", encoding="utf-8", newline="").read()
lines = src.split("\n")   # 0-indexed; line N (1-indexed) == lines[N-1]

def L(n):
    return lines[n-1]

def expect(n, frag):
    if frag not in L(n):
        sys.exit("ANCHOR FAIL line %d: expected %r got %r" % (n, frag, L(n)))

# (line_start, line_end_inclusive, replacement_lines, anchor_fragment_on_line_start)
edits = []

def rep(a, b, new, anchor):
    expect(a, anchor)
    edits.append((a, b, new))

# --- descending order applied later; define in any order ---
rep(17100, 17100, ['                    { ms_format = std::move(ms); MZ_HIT("MS: inner compress_impl [incl]"); }'],
    "ms_format = std::move(ms);")
rep(17098, 17098, ['                auto back = MZ_TIMED("MS: verify decompress", decompress(ms.data(), ms.size(), nullptr));'],
    "auto back = decompress(ms.data()")
rep(17088, 17088, ['            auto inner = MZ_TIMED("MS: inner compress_impl [incl]", compress_impl(t.data(), size, zstd_level, block_size, nullptr, mode, false, false, false, false, false, false, false, false));'],
    "auto inner = compress_impl(t.data(), size")
rep(17087, 17087, ['            auto t = MZ_TIMED("MS: soa_apply winner", soa_apply(data, size, c.tid, c.W, c.cols));'],
    "auto t = soa_apply(data, size, c.tid")
rep(17081, 17081, ['            size_t p = MZ_TIMED("MS: proxy zstd-1 x16", ZSTD_compress(pbuf.data(), pbuf.size(), t.data(), t.size(), 1));'],
    "size_t p = ZSTD_compress(pbuf.data()")
rep(17080, 17080, ['            auto t = MZ_TIMED("MS: soa_apply x16", soa_apply(data, size, c.tid, c.W, c.cols));'],
    "auto t = soa_apply(data, size, c.tid")
rep(17074, 17074, ['        size_t raw_proxy = MZ_TIMED("MS: proxy zstd-1 baseline", ZSTD_compress(pbuf.data(), pbuf.size(), data, size, 1));'],
    "size_t raw_proxy = ZSTD_compress")
rep(17054, 17054, ['            bg_format = std::move(bg_temp); MZ_HIT("BG: bwt9 whole-file [incl]");'],
    "bg_format = std::move(bg_temp);")
rep(17044, 17044, ['        auto bg_body = MZ_TIMED("BG: bwt9 whole-file [incl]", bwt9::compress(data, size));'],
    "auto bg_body = bwt9::compress(data, size);")
rep(16897, 16900, ['    size_t zstd_size = MZ_TIMED("top-level zstd-19 whole-file", ZSTD_compress(zstd_buf.data(), zstd_buf.size(), data, size, zstd_level));'],
    "size_t zstd_size = ZSTD_compress(")
rep(16637, 16637, ['                            analysis.type = BlockType::PPMD; MZ_HIT("ppmd backstop");'],
    "analysis.type = BlockType::PPMD;")
rep(16630, 16630, ['                        auto pp = MZ_TIMED("ppmd backstop", ppmdbk::compress(block_data, this_block, ord, PPMD_MEM));'],
    "auto pp = ppmdbk::compress(block_data")
rep(16595, 16595, ['                        analysis.type = BlockType::BROTLI; MZ_HIT("brotli-11 backstop");'],
    "analysis.type = BlockType::BROTLI;")
rep(16590, 16591, ['                    if (MZ_TIMED("brotli-11 backstop", BrotliEncoderCompress(MZ_BROTLI_QUALITY, MZ_BROTLI_WINDOW, bmode, this_block, block_data, &bsz, bb.data()))'],
    "if (BrotliEncoderCompress(MZ_BROTLI_QUALITY")
rep(16544, 16544, ['                                analysis.type = BlockType::XZLIB; MZ_HIT("xz-BCJ backstop");'],
    "analysis.type = BlockType::XZLIB;")
rep(16532, 16533, ['                        if (MZ_TIMED("xz-BCJ backstop", lzma_stream_buffer_encode(filters, MZ_LZMA_CHECK_NONE, nullptr, block_data, this_block, xb.data(), &xpos, xbound)) == MZ_LZMA_OK'],
    "if (lzma_stream_buffer_encode(filters")
rep(16507, 16507, ['                    analysis.type = BlockType::XZLIB; MZ_HIT("xz-9e backstop");'],
    "analysis.type = BlockType::XZLIB;")
rep(16502, 16503, ['                if (MZ_TIMED("xz-9e backstop", lzma_easy_buffer_encode(9u | MZ_LZMA_PRESET_EXTREME, MZ_LZMA_CHECK_NONE, nullptr, block_data, this_block, xb.data(), &xpos, xbound)) == MZ_LZMA_OK'],
    "if (lzma_easy_buffer_encode(9u")
rep(16492, 16492, ['                    analysis.type = BlockType::BWT_TEXT; MZ_HIT("bwt9-result adopted @backstop");'],
    "analysis.type = BlockType::BWT_TEXT;")
rep(16446, 16446, ['                    analysis.type = BlockType::ZSTD_DICT; MZ_HIT("dict: scan zstd-19 x12");'],
    "analysis.type = BlockType::ZSTD_DICT;")
rep(16423, 16424, ['                size_t zest = MZ_TIMED("dict: incumbent zstd-19", ZSTD_compress(dtbuf.data(), dtbuf.size(), preprocess_data, preprocess_size, 19));'],
    "size_t zest = ZSTD_compress(dtbuf.data()")
rep(16407, 16409, ['                size_t sz = MZ_TIMED("dict: scan zstd-19 x12", ZSTD_compress_usingDict(dcctx, dtbuf.data(), dtbuf.size(), block_data, this_block, d.data, d.size, 19));'],
    "size_t sz = ZSTD_compress_usingDict(dcctx,")
rep(13289, 13289, ['        numeric_detection = MZ_TIMED("tiered::detect (numeric)", tieredcompress::detect(data, n, work.data()));'],
    "numeric_detection = tieredcompress::detect(data, n, work.data());")

for (a, b, new) in sorted(edits, key=lambda e: -e[0]):
    lines[a-1:b] = new

out = "\n".join(lines)

# --- header patches (string-level, after line edits) ---
old_table = "inline std::vector<Acc>& table(){ static std::vector<Acc> t; return t; }"
new_table = ("inline std::vector<Acc>& table(){ static std::vector<Acc>* t = new std::vector<Acc>(); return *t; }\n"
             "// NOTE (profiler fix): the table is INTENTIONALLY LEAKED. Upstream used a function-local\n"
             "// std::vector destroyed BEFORE the Dump object (Dump is constructed first by the comma\n"
             "// operator in MZ_DET/MZ_TIMED, so it is destroyed last) -> the dump read a destroyed vector.\n"
             "// Observed upstream: SIGSEGV (rc=139) during the dump, truncated/empty tables.")
if out.count(old_table) != 1:
    sys.exit("table() anchor count != 1: %d" % out.count(old_table))
out = out.replace(old_table, new_table)

old_slot_end = "    table().push_back(Acc{name,0.0,0,0}); return table().back();\n}"
new_slot_end = old_slot_end + "\ninline void hit(const char* name){ slot(name).hits++; }"
if out.count(old_slot_end) != 1:
    sys.exit("slot() anchor count != 1: %d" % out.count(old_slot_end))
out = out.replace(old_slot_end, new_slot_end)

old_m = '#define MZ_TIMED(name, expr) (mziptime::dumper(), mziptime::timed(name, [&]{ return (expr); }))'
new_m = old_m + '\n#define MZ_HIT(name) (mziptime::dumper(), mziptime::hit(name))'
if out.count(old_m) != 1: sys.exit("MZ_TIMED macro anchor count != 1")
out = out.replace(old_m, new_m)

old_m2 = '#define MZ_TIMED(name, expr) (expr)'
new_m2 = old_m2 + '\n#define MZ_HIT(name) ((void)0)'
if out.count(old_m2) != 1: sys.exit("MZ_TIMED else-macro anchor count != 1")
out = out.replace(old_m2, new_m2)

io.open(P, "w", encoding="utf-8", newline="").write(out)
print("PATCH OK, %d line-edits + 4 header edits" % len(edits))
