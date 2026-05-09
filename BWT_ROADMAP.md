# mzip BWT roadmap — closing the gap to bsc-m03/GLZA/cmix

Living doc. Started 2026-05-04. Goal: push mzip from "beats brotli on prose" toward
"competitive with bsc-m03 (best non-neural BWT) on prose" — and ultimately halve the
gap to cmix without adopting neural context mixing.

## Where we are (end of 2026-05-04, two squeeze sessions)

| Compressor | enwik9 1MB | enwik9 10MB | enwik9 1GB (published) | Class |
|---|---:|---:|---:|---|
| zstd:19 | 312,639 | 2,921,957 | — | LZ77 + entropy |
| gzip:9 | 356,643 | 3,720,323 | — | LZ77 + Huffman |
| bzip2:9 | 294,484 | 3,054,639 | 253,977,839 | BWT + multi-tree Huffman |
| xz:9 | 302,832 | 2,844,360 | ~150 MB | LZMA |
| brotli:11 | 293,057 | 2,827,632 | ~145 MB | LZ77 + Huffman + 120KB static dict |
| 7z:mx9 | 302,910 | 2,844,433 | ~150 MB | LZMA2 |
| **mzip (current)** | **286,307** | **2,671,197** | ~262 MB est. | **BWT + multi-tree Huffman + capfold + word dict + LZP-after-dict, all per-block dynamic-trial** |
| bzip3 | ~245,000 | ~2,300,000 | 175,208,099 | BWT + LZP + arithmetic + 1-symbol CM |
| bsc | — | — | 163,884,462 | LZP + BWT + QLFC + adaptive arithmetic |
| **bsc-m03** | — | — | **160,018,905** | **LZP + BWT + M03 context coder** |
| GLZA | — | — | ~165 MB est. | grammar-based + arithmetic + order-1 |
| fx2-cmix | ~150,000 | ~1,500,000 | 110,793,128 | Neural context mixing (current Hutter winner) |

**On 10MB enwik9 we now beat brotli by 5.9%, xz by 6.5%, bzip2 by 14.4%.** mzip is
the smallest output of any standard library compressor on this prose. Gap to bzip3
(~14%) and bsc-m03 (~20%) remains; closing it requires the architectural moves in
the **Open Phases** section below.

## The diagnosis (revised)

Our pipeline: pre-RLE → BWT (libsais) → MTF → Z-RLE → multi-tree Huffman with bitmap.
That is bzip2 from 2001 with the bitmap optimization. Three structural deficits vs SOTA:

1. **Huffman → ANS/arithmetic**: Huffman pays integer-bit penalty per symbol (~0.5 bits
   wasted on average). ANS hits entropy. **~3–5% reduction.** *FSE (Cyan4973) is a drop-in
   replacement, ANS-based, and we already link the zstd library that contains it.*
2. **No LZP preprocessing**: long literal runs bloat the BWT and pollute its locality.
   bzip3 uses LZP-40, bsc uses LZP-128. **~3–6% on prose, more on code.**
3. **MTF + memoryless coding throws away local context**: BWT output is locally
   homogeneous (same right-context cluster → same following byte). Modeling that with
   a 1-symbol-context model (bzip3) or M03's iterative refinement (bsc-m03) captures it.
   **~5–15% reduction.** This is the biggest single lever.

## Done this session (2026-05-04)

Two squeeze sessions. Detailed in `HUNT_LOG.md`. Quick summary:

### Squeeze 1: bug fix + tuning
- **Fixed 16-bit `n_groups` overflow** in BWT format (silent failure on >4MB blocks). New `'BC'` header with 24-bit field. `bwt_compress_v5.hpp`.
- **Added BG (BWT-Big single-block) format** for prose 1MB–256MB (later raised to 1GB cap). `mzip.hpp`. Pre-existing MC mode chunked too aggressively, killing prose compression.
- **Multi-window text detection** in `is_text_like` (5 sample points, not just first 4KB).
- **BWT param tuning**: GS 44→50→**45**, NT 6→4→**5**, NI=4 (saturates).
- **Huffman code-length cap 17→20 bits** — frees skewed distributions from bzip2 cap.

### Squeeze 2: preprocessing + dynamic params
- **P0a Cache `compact_of[256]`** — speed-only, unblocked deeper sweeps.
- **P0c Trial pre-RLE on/off** ('BC' vs 'BD' headers). Trial-pick.
- **P0d Lift BG cap to 1GB** in SMALL mode (256MB in BALANCED).
- **P0e Fix decode footguns**: bound-check `selector_mtf[pos]`, `decode_until_eob` takes `n_groups_bits` arg.
- **Word dictionary** ('BR' variant) — per-file top-255 letter words, ESC=0xFE escape encoding. `word_dict.hpp`.
- **Capital folding** ('BS' = capfold + dict) — lowercase + 0x06 marker. Halves alphabet, doubles dict effectiveness. `cap_fold.hpp`.
- **Per-block NT trial extended to all sizes** (was only n∈[1200,20000]).
- **Dict-size trial** {64, 128, 192, 255} per block — small dict has less header overhead.
- **LZP-after-dict synergy** ('BW' variant) with min_match trial {10, 20, 40}. LZP catches new repeats that emerge from dict substitution.

### Tried but failed (kept as auto-deselected candidates)
- **Range coder o0/o1** on post-MTF stream ('BE'/'BF'/'BI'/'BJ'). Loses to multi-tree Huffman by 0.35–0.79%. Our Huffman is already very close to entropy on this distribution.
- **LZP on raw input** ('BK'/'BM'). Removes 4.4% of input but BWT residual compresses 5% worse — net loss. BWT was already capturing the long matches.
- **XML entity collapse** ('BU'). New 0x01–0x05 byte clusters pollute BWT.
- **Bigram with separate ESC byte** ('BV'). Same issue — BG_ESC=0xFD adds new BWT cluster.
- **Direct single-byte codes** for top-6 words (bytes 0xF8–0xFD). Broke roundtrip on edge case; reverted.
- **Broader word_char (digits, apostrophe, all non-ws)**. Adds unique long tokens that displace recurring words from 255 dict slots.
- **510-code dict** via second escape 0xFD. New byte cluster doesn't pay back marginal codes.

### Cumulative session win on 10MB enwik9
| Stage | Bytes | Δ vs start |
|---|---:|---:|
| Session start | 2,921,957 | — |
| After Squeeze 1 (fix + tuning) | 2,689,780 | −7.9% |
| After Squeeze 2 (preprocessing + dynamic) | **2,671,197** | **−8.6%** |
| brotli:11 (was beating us at start) | 2,827,632 | we now beat by 5.9% |

## Open phases — what's next to try

Estimates are conservative. Each phase is independent unless noted; gains may overlap.

### Phase A — bzip3-style context-mixing arithmetic coder (highest-value architectural move)

The single biggest remaining lever. Replaces the Huffman backend with adaptive
arithmetic coding driven by a statistical predictor that models BWT output bytes
(no MTF). This is what bzip3 does to reach ~2.30 MB on a 10MB enwik9 prefix.

- [ ] **A1 Drop MTF entirely** — model BWT bytes directly with byte-history context.
- [ ] **A2 Multi-context arithmetic coder** — order-0 + order-1 + order-2 mixed via
      logistic blend. ~1500–2500 LOC.
- [ ] **A3 Reference**: `bzip3/src/cm.c` (Szewczyk, MIT-equivalent license). The cleanest
      single-file context-mixing coder in any current open-source project. ~600 LOC core.
- [ ] **A4 New format header** (e.g., 'BX' = "BWT bytes + CM coder"). Trial against
      existing variants.
- **Estimated win**: 5–10% on prose (~150–250 KB on 10MB). **Effort**: 2–3 weeks.
- **Stacked target**: ~2.30 MB on 10MB enwik9 = bzip3-class.

### Phase B — Static pre-trained English dictionary (Brotli pattern)

Currently dict is per-file (1.5 KB header). A STATIC dict (shipped in source code)
removes per-file overhead and allows MORE entries.

- [ ] **B1 Train dict** on a representative English corpus (e.g., enwik8 or a few
      Wikipedia articles). Top 500–2000 words by Zipf frequency × length savings.
- [ ] **B2 Embed in source code** as `static const char eng_dict[]`. ~50–200 KB raw,
      compresses to ~30–80 KB in the binary.
- [ ] **B3 Variant header** (e.g., 'BY' = static-dict mode). Decoder uses built-in dict;
      no per-file dict header.
- [ ] **B4 Detection**: only use static dict when content looks like English prose
      (high ratio of dict-word matches in a sample). Otherwise fall back to per-file.
- **Estimated win**: 3–5% on prose (~80–130 KB on 10MB). **Effort**: ~1 week including dict training.

### Phase C — HP-2017 XML stream split (Wikipedia-specific)

Separates `<text>` body from XML envelope, compresses each independently. Highly
effective on Wikipedia/XML; useless on other content.

- [ ] **C1 Detect XML structure** (look for `<page>` tags in sample).
- [ ] **C2 Stream split**: `<page>...</page>` envelope → metadata stream;
      `<text>...</text>` body → prose stream. Reference: `phda9_preprocess.h` (~850 LOC).
- [ ] **C3 Compress each stream with our existing pipeline**.
- [ ] **C4 Reassemble metadata**: stream join requires offset table; ~5KB overhead.
- **Estimated win**: 3–5% on Wikipedia (~80–130 KB on 10MB enwik9). **Effort**: ~1 week.

### Phase D — Suffix-array-based grammar (GLZA-style)

Pattern-stitch direction. libsais already builds the suffix array for BWT —
we can walk LCP intervals to find optimal repeating substrings nearly for free.

- [ ] **D1 Walk SA** to find substrings of length ≥30 occurring ≥3 times (LCP intervals).
- [ ] **D2 Score by `freq × len - storage_cost`** like our word dict but for arbitrary
      substrings (not just letters).
- [ ] **D3 Top-N substring dict** with ESC encoding (or shared with word dict ESC space).
- [ ] **D4 Risk**: same BWT-cluster-pollution issue as bigram experiment. May regress.
      Test variant carefully.
- **Estimated win**: uncertain, 0–5%. **Effort**: ~3 days. **Worth testing because nearly free** (libsais SA reusable).

### Phase E — Multi-pass dict refinement

After dict pass 1, the encoded stream has new patterns (sequences of ESC codes). A
second-pass dict could capture those — but using SHARED ESC space (not new escape byte
like our failed BV) to avoid BWT cluster pollution.

- [ ] **E1 Run pass 1**: word dict (current 'BS' path).
- [ ] **E2 Build pass 2 dict**: scan pass-1 output for patterns like `(ESC X SPACE ESC Y)`.
      Take top 50–100 patterns by frequency.
- [ ] **E3 Reserve top of pass-1 code space** for pass-2 codes (e.g., codes 200–249 in
      same ESC scheme — pass-1 uses 0–199).
- [ ] **E4 Encode**: pass 1 emits codes 0–199 only; pass 2 collapses bigram patterns to
      codes 200–249.
- **Estimated win**: 0.5–2% (~15–50 KB on 10MB). **Effort**: ~3 days.

### Phase F — Suffix-aware dict (English morphology)

Current dict treats "running" and "runs" as completely separate words. A suffix-aware
dict would encode `stem + suffix_code` for words sharing common stems.

- [ ] **F1 Common English suffix list**: -ing, -ed, -tion, -ly, -er, -est, -es, -s.
      ~20 entries.
- [ ] **F2 Encoder logic**: if word not in dict but ends with a known suffix and the
      stem IS in dict, emit `(stem_code) + (suffix_marker)`.
- [ ] **F3 Saves 1–3 bytes per non-dict word ending in common suffix**.
- **Estimated win**: 1–3% (~30–80 KB on 10MB). **Effort**: ~3 days.

### Phase G — Per-block GS trial (extend existing dynamic system)

Per-block NT trial works; per-block GS could too. Currently GS=45 fixed because
it's `constexpr`. Make it runtime, trial GS ∈ {35, 45, 55} per block.

- [ ] **G1 Refactor `GROUP_SIZE` from constexpr to template parameter or runtime arg**.
- [ ] **G2 Add GS trial loop** alongside NT trial in `MultiTreeEncoder::encode`.
- [ ] **G3 Encode the chosen GS in the bitstream** (~3 bits).
- **Estimated win**: 0.1–0.5% (~3–15 KB on 10MB). **Effort**: ~1 day. **Probably not worth alone**, but compose with other dynamic moves.

## Realistic stacked target

Each phase compounds onto previous (smaller savings on a smaller base).

| State | 10MB enwik9 | Class |
|---|---:|---|
| **Today (end of squeeze 2)** | **2,671,197** | **best non-context-coder** |
| + Phase D (SA-grammar) | ~2,640,000 | marginal |
| + Phase E (multi-pass dict) | ~2,610,000 | marginal |
| + Phase F (suffix-aware) | ~2,560,000 | meaningful |
| + Phase A (bzip3 CM coder) | ~2,330,000 | **bzip3-class** |
| + Phase B (static dict) | ~2,250,000 | between bzip3 and bsc |
| + Phase C (XML split, Wiki) | ~2,160,000 | bsc-class |
| cmix v21 (extrap) | ~1,500,000 | neural — different sport |

**Phase A is the headline next step.** It's the architectural change that unlocks
the bzip3/bsc tier. Everything else is incremental relative to that.

## Lessons learned (the deep pattern)

After two squeeze sessions, the pattern is unmistakable:

**1. BWT-friendliness of preprocessing matters more than raw byte savings.** Every new
escape byte we introduce creates a new BWT cluster competing for entropy budget. Only
preprocessors with EITHER (a) alphabet reduction (capfold) OR (b) ultra-uniform escape
structure (dict's 0xFE always followed by code byte) earn back their overhead.

**2. Our existing 5-tree multi-Huffman with bitmap is already very close to entropy on
post-MTF/ZRLE data.** Range coder o0/o1 LOST to it by 0.35–0.79%. The integer-bit
penalty Huffman pays is small for our specific distribution. To beat it, you need
context modeling that captures BWT-cluster structure (bzip3 cm.c approach).

**3. LZP regresses on raw input but WINS after dict.** Dict creates new repeating
patterns that didn't exist before; LZP captures those. Synergies between layers are
non-obvious — must be tested empirically.

**4. Trial-pick over many variants > picking one "best" pipeline.** Each input chunk
has a different optimal preprocessing combo. Adding more variants to trial costs encode
time but never hurts compressed size (auto-deselect).

**5. Per-block dynamic parameter trial works.** NT trial (extended to all sizes) and
dict-size trial both gave real wins. Expanding to GS trial would likely give more.

**6. Byte-level approaches have a hard ceiling.** We've squeezed about as much as
possible from byte-level pre-BWT preprocessing. The remaining gap to bzip3 (~14%) and
bsc-m03 (~20%) requires architectural moves: drop MTF, model BWT bytes directly with
context-mixing arithmetic coding.

## Reusable infrastructure built this session

These are working and tested even if their associated variants don't always win:

- `bwt_compress_v5.hpp` — the BWT pipeline with all 12+ format variants
- `range_coder.hpp` — LZMA-style binary range coder with adaptive Prob model
- `word_dict.hpp` — per-file word dictionary (build, encode, decode, truncate)
- `cap_fold.hpp` — capital folding with 0x06 marker
- `xml_entity.hpp` — XML entity collapse (kept as candidate, doesn't usually win)
- `bigram_dict.hpp` — bigram dict with separate ESC (kept as candidate, doesn't win on prose)

The `compress()` function in bwt_compress_v5.hpp is the orchestration layer that builds
12+ candidate outputs per call and picks the smallest. To add a new approach: implement
its encode/decode, add a new variant in `compress()`, add header dispatch in `decompress()`.

## Cross-cutting questions to keep in mind

1. **Format compatibility**: each phase changes the output format. We've been adding
   header variants (`BB`, `BC`, `BG`, `MC`). Plan a versioning scheme so old archives
   stay decodable as we evolve.
2. **Speed budget**: M03 (P5) is 3–10× slower than bzip3. Decide if SMALL mode (slowest,
   smallest) gets it, or only a new ULTRA mode. mzip already has CompressionMode.
3. **Memory budget**: P3 (256 FSE tables) is ~1MB; P5 (M03 iterative) can be 10–50MB
   working set. Compatible with the Hutter constraints (10GB RAM cap).
4. **Generality vs Wikipedia-specific**: P6 only helps on Wikipedia/XML. Should mzip
   detect Wikipedia content and apply it conditionally? Probably yes — same detection
   philosophy as the rest of mzip.

## Research sources

Non-neural BWT SOTA:
- [bzip3 (Szewczyk) overview](https://github.com/kspalaiologos/bzip3/blob/master/doc/overview.md)
- [bsc-m03 (Grebnov) — current SOTA non-neural](https://github.com/IlyaGrebnov/bsc-m03)
- [M03 algorithm thread (Maniscalco)](https://groups.google.com/g/comp.compression/c/dVWgvCl9ZpE)
- [QLFC paper (Grebnov 2008)](http://www.data-compression.org/787/QLFC---Quantized-Local-Frequency-Coding.html)
- [Fenwick — BWT Compression: Principles and Reflections](https://www.cs.auckland.ac.nz/~peter-f/FTPfiles/BWreflections.pdf)
- [Abel — Post-BWT stages preprint](https://www.juergen-abel.info/files/preprints/preprint_post_bwt_stages.pdf)

Entropy coding:
- [FSE (Cyan4973) — drop-in entropy coder](https://github.com/Cyan4973/FiniteStateEntropy)
- [FSE explained (fastcompression blog)](http://fastcompression.blogspot.com/2013/12/finite-state-entropy-new-breed-of.html)
- [Zstandard / FSE worked example (Nigel Tao)](https://nigeltao.github.io/blog/2022/zstandard-part-5-fse.html)
- [pzip-fse — bzip-shape compressor with FSE backend](https://github.com/phucnm/pzip-fse) (reference impl)

Grammar / pattern compression:
- [GLZA — Pareto-frontier grammar compressor](https://www.researchgate.net/publication/359472230_Grammatical_Ziv-Lempel_Compression)
- [TurboBench glza source mirror](https://github.com/powturbo/TurboBench/blob/master/glza/GLZAcompress.c)
- [Grammar precompression speeds up BWT (Springer 2012)](https://link.springer.com/chapter/10.1007/978-3-642-34109-0_34)

Wikipedia preprocessing:
- [STARLIT (Margaritov, 2021 Hutter submission)](https://github.com/amargaritov/starlit)
- [phda9_preprocess.h](https://github.com/amargaritov/starlit/blob/master/src/readalike_prepr/phda9_preprocess.h)
- [XWRT (Skibiński)](https://github.com/inikep/XWRT)
- [PAQ documentation (Mahoney)](http://mattmahoney.net/dc/paq.html)

Benchmarks:
- [Large Text Compression Benchmark (Mahoney) — definitive](https://www.mattmahoney.net/dc/text.html)
- [encode.su BZip3 thread](https://encode.su/threads/3867-BZip3)
- [encode.su bsc-m03 thread](https://encode.su/threads/3763-bsc-m03-(experimental-M03-sorting-compressor)/page4)

## Open questions / parking lot

- **PPMd** (engine in 7z's PPMd codec) sometimes beats LZMA on text. Out-of-tree
  for "BWT pipeline" but a possible standalone backend variant.
- **Suffix-array tricks**: libsais already builds the suffix array for BWT. Can we use
  it for LZP context lookup *for free* in the same pass? Probably yes; could save the
  150 LOC of LZP hash table — and is the foundation for Phase D (SA-grammar).
- **Two-pass adaptive PPM** as a hybrid backend.
- **Cross-block context**: enwik9 has long-range redundancy across BWT blocks (same
  templates referenced 100MB apart). Our per-block coding loses this. A small
  cross-block dictionary primer might recover some.
- **Tree-shape delta encoding tweaks** in MultiTreeEncoder — bzip2 wastes bits on its
  `±1` length-delta encoding. xz/zstd's RFC1951 canonical Huffman with run-length over
  zero codes is ~30% more compact. Worth ~0.5KB on 10MB. Low priority.
- **Selector encoding via static Huffman over rank-MTF** (instead of unary) — ~10–15
  KB savings claimed in original audit. Format-break risk; gate behind new header.

## When to revisit this roadmap

Before next session, the most valuable single move is **Phase A** (bzip3-style CM coder).
That's the architectural unlock to bzip3-class. Everything else is incremental relative.

If session goal is *quick wins*, do Phase D first (3 days, uses libsais SA we already
build) then Phase F (suffix-aware dict, 3 days). Combined estimate ~3–5%.

If session goal is *Wikipedia-specific*, Phase C (XML stream split) is high-leverage
for that content but useless on others.

If session goal is *generality*, Phase A + Phase B (static dict) together push us to
bsc-class on prose with no Wikipedia-specific assumptions.
