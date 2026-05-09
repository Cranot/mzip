# Hunt Log

## 2026-04-04 Nginx access log at 16KB
- **Loss**: mzip=2682 vs bzip2=2524 (gap=158) — brotli was untested (=0)
- **Strategy**: COLUMNAR (9-col split, 2 BWT streams)
- **Claim**: At 16KB (~60 lines), two separate BWT streams have more header overhead than the column separation saves. bzip2's single BWT on full data sees cross-column patterns together.
- **Law**: Column separation × BWT block efficiency = constant. More columns = better sorting within each, but more overhead per stream and less context across streams.
- **Verdict**: SHIP IT (partial) — added trial comparison: COLUMNAR vs BWT_TEXT at <=32KB, pick smaller
- **Action**: Modified mzip.hpp line ~14083. At 16KB, trial falls back to BWT_TEXT.
- **Result**: 2682 → 2586 (saved 96 bytes, 36% of gap closed)
- **Remaining gap**: bzip2=2524 (62 bytes), brotli=2435 (151 bytes). brotli now revealed as true competitor at small sizes — its 120KB pre-built static dictionary gives it a structural advantage we can't match without shipping our own dictionary.
- **No regressions**: 64KB stays 6827, 256KB stays 22642, 1MB stays 85413

## 2026-04-04 Python at 16KB (+ 19 other BWT_TEXT types)
- **Loss**: mzip=2575 vs bzip2=2505 (gap=70)
- **Strategy**: BWT_TEXT (bwt9 → bwt5 multi-tree Huffman)
- **Claim**: Our bwt5 entropy coder produces ~2-4% more output than bzip2's at 16KB. Same pipeline (BWT→MTF→ZRLE→multi-tree Huffman), same params (GROUP_SIZE=44 vs bzip2's 50, N_ITERS=4, MAX_TREES=6). Raw BWT output=2566, bzip2=2505, gap=61 bytes. mzip format adds 9 bytes on top.
- **Investigation**: Tested n_trees=5 threshold (no effect — ZRLE output well above 2400). Gap is consistent across ALL 20 BWT_TEXT types at 16KB: +11 to +61 bytes, avg ~38 bytes.
- **Law**: Entropy coder maturity × implementation simplicity = constant. bzip2's 30 years of tuning bought ~2-4% at this size. Closing the gap requires matching their exact bit-level encoding choices.
- **Verdict**: STRUCTURAL — this is BWT entropy coder quality, not strategy selection. The gap is consistent across all text types. Fixing it would require bit-level optimization of the multi-tree Huffman encoder (diminishing returns, high complexity).
- **Action**: Skip. The 2-4% gap at 16KB is acceptable given we beat bzip2 at 64KB+ on most types.
- **Pattern**: ALL 20 losses at 16KB are BWT_TEXT vs bzip2. This is one root cause: bwt5 entropy coder quality.

## 2026-04-05 BWT_TEXT tree count optimization (HIGH LEVERAGE)
- **Root cause found**: At 16KB, 6 Huffman trees are allocated but 1-3 go UNUSED. Each unused tree costs ~20 bytes of code length overhead for zero benefit. Bit budget analysis showed tree lengths = 4.7-10.7% of output.
- **Investigation path**: GROUP_SIZE 44→50 (mixed results, no clear winner). N_ITERS 4→6 (no effect). Bit budget instrumentation revealed the tree overhead was the dominant factor.
- **Fix**: Trial-compress with 4, 5, and 6 trees for medium-sized data (1200-20000 ZRLE symbols ≈ 4-16KB text). Pick whichever gives smallest output. Decoder is unchanged — n_trees is stored in the bitstream.
- **Result**: BWT vs bzip2 gap dropped 58% on average across all text types at 16KB:
  - python: +61 → +19, rust: +54 → +24, unicode: +61 → +22
  - c: +19 → -3 (now WINS), cargo_toml: +11 → -5 (now WINS), markdown: +16 → 0 (TIE)
- **New landscape at 16KB**: 12 losses to brotli (dictionary advantage), 16 losses to bzip2 (avg 22 bytes, down from 38). 21 wins + 1 tie.
- **No regressions**: 64KB/256KB/1MB all unchanged or improved. 50/50 roundtrip verification passed.
- **Conservation law**: Tree overhead × data adaptation = constant. More trees = better per-group adaptation but higher fixed overhead. At small sizes, the crossover favors fewer trees. Trial compression finds the optimal point per-data.

## 2026-05-04 BWT 16-bit n_groups overflow + BG (single-block) format
- **Bug**: bwt5 silently corrupted on inputs >~4MB. `n_groups` field stored in 16 bits (`MultiTreeEncoder::encode_with_trees`, write_bits(n_groups, 16) in encode and decode). For ZRLE outputs >65535 groups (~3MB post-BWT) the value truncated, desynchronizing the decoder. Compress reported plausible sizes; decompress returned empty bytes.
- **Why nobody noticed**: mzip's MC (Multi-Chunk) mode at >1MB chunks the input into 1MB pieces, each well under the overflow threshold. Direct callers of bwt5 on big blocks hit the bug.
- **Fix**: New `'BC'` header that stores n_groups in 24 bits (supports ~16M groups, ~700MB post-BWT). Old `'BB'` format still decodable.
- **Bigger finding**: forced single-block BWT on enwik9 10MB prefix produces 2,707,003 bytes. Per-chunk MC mode produces 3,029,224 (worse than zstd:19's 2,921,957). On homogeneous prose, BWT patterns span chunks. Splitting kills it.
- **New format `'BG'`**: BWT-Big single-block. `'BG' + varint(orig_size) + bwt5_bytes`. Trialed alongside MC; pick smaller. Cap at 256MB for memory bound.
- **Heuristic upgrade**: `is_text_like` now multi-window-samples (5 windows × 4KB across the file). Old single-window catch-the-start heuristic missed mixed-content cases.
- **BWT param sweep on enwik prefixes**: GROUP_SIZE 44→50, MAX_TREES 6→4, N_ITERS stays 4. Wikipedia is homogeneous prose — fewer trees means less header, similar per-group fit. Saved ~7,400 bytes on 10MB.
- **Huffman code-length cap 17→20 bits**: marginal but free. ~560 bytes on 10MB.
- **Final result on enwik9 prefixes**:
  - 1MB: mzip=288,588 (3.63x). Beats brotli:11 (293,057), bzip2:9 (294,484), xz:9 (302,832), zstd:19 (312,639).
  - 10MB: mzip=2,699,034 (3.89x). Beats brotli:11 (2,827,632) by 4.5%, xz:9 (2,844,360) by 5.1%, bzip2:9 (3,054,639) by 11.6%.
- **vs session start (10MB)**: 2,921,957 → 2,699,034 = -222,923 bytes (-7.6%). Was losing to brotli; now beats it cleanly.
- **No regression on standard test types**: 5-30 byte regressions on a few code categories from NT=4 default, dwarfed by Wikipedia gain. Acceptable trade.
- **Research-driven roadmap**: see `BWT_ROADMAP.md` for the path to bzip3/bsc class (~2.20-2.30 MB on 10MB enwik9, ~500 KB further savings via arithmetic coder + context model + LZP).

## 2026-05-04 (cont.) Pre-BWT preprocessing wins where post-BWT couldn't
- **Failed approaches** (infrastructure built, no compression win on prose):
  - Order-0/order-1 range coder on post-MTF stream: +0.35-0.79% LARGER than our Huffman. Our existing 5-tree multi-tree Huffman with bitmap is already very close to entropy on the post-ZRLE alphabet; arithmetic's edge disappears when most symbols are ~half-the-time-zero.
  - LZP-40 (bzip3 port): shrinks input 4.4% but BWT residual compresses ~5% worse. Net 0.5% LOSS. BWT was already capturing the long matches via suffix array.
- **Real wins from PRE-BWT preprocessing**:
  - Per-file word dictionary (255 codes, 0xFE escape): top-N words by raw bytes saved, ESC+code replacement before BWT. Saves 8KB on 10MB enwik9.
  - Capital folding (0x06 marker before lowercased capital letter): doubles dict effectiveness ("the"/"The" share entry), shrinks BWT alphabet. Composes with dict.
  - Combined cap-fold + dict (header 'BS'): saves **23,239 bytes / 0.86%** vs prior best.
- **Tried and reverted**: digits/apostrophe in word_char (regressed — added unique long tokens); 510-code dict via 0xFD escape (regressed — extra ESC byte polluted BWT); long-word-biased scoring (regressed — short common words help too).
- **Final state on enwik prefixes**:
  - 1MB: 287,561 (3.65×). Beats brotli:11 (293,057) by 1.9%, bzip2 by 2.4%, xz by 5.2%.
  - 10MB: 2,675,801 (3.92×). Beats brotli:11 (2,827,632) by 5.4%, xz by 5.9%, bzip2 by 12.4%.
- **Session total on 10MB**: 2,921,957 → 2,675,801 = **−246,156 bytes / −8.4%**. Started losing to brotli by 3.4%; ended beating brotli by 5.4%.
- **No regression on standard 33 test types** — capfold+dict net-improves everything with English-text-like content (code, configs, prose).

## 2026-05-04 (cont.) Dynamic per-block params + dict-size + LZP-after-dict synergies
- **Per-block NT trial extended to all sizes**: was only n∈[1200,20000], now all blocks ≥1200 trial NT∈{3..7}. Each block picks locally optimal tree count. **−1,154 bytes on 10MB.**
- **Dict-size trial in BR/BS variants**: trial dict sizes {64, 128, 192, 255} per block, pick smallest. Smaller dict = less header overhead but fewer encoded words. **−2,059 bytes on 10MB.**
- **LZP-after-dict synergy ('BW' variant)**: apply LZP to dict-encoded stream (not raw input). Dict creates new repeating patterns that LZP captures. Trials min_match ∈ {10,20,40}. **−1,392 bytes on 10MB.**
- **Failed synergy attempts** (kept in code as auto-deselected candidates): XML entity collapse, bigram with separate ESC, direct single-byte codes — all regressed because new byte clusters polluted BWT.
- **Final state on enwik9 prefixes**:
  - 1MB: 286,307 (3.66×). Beats brotli:11 by 2.4%, bzip2 by 2.9%, xz by 5.8%.
  - 10MB: **2,671,197** (3.93×). Beats brotli:11 by **5.9%**, xz/7z by 6.5%, bzip2 by 14.4%.
- **Cumulative session win on 10MB**: 2,921,957 → 2,671,197 = **−250,760 bytes / −8.6%**.
- **No regression on 33 standard types**.

**Key insight**: byte-level pre-BWT preprocessing has a hard ceiling. Adding escape bytes creates new BWT clusters that don't pay back. Wins come from (a) capfold reducing alphabet, (b) dict exploiting word-level structure, (c) LZP in DICT-OUTPUT (new patterns emerged), (d) per-block dynamic parameter trial. Further byte-level wins diminishing; next major leap requires context-mixing arithmetic coder.
