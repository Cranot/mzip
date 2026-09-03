# OWN-SAUCES — mzip's own levers, mapped onto the thin HF cells

Written 2026-09-02 (late), from /root/mzip (research tree, HEAD + uncommitted 2026-09-01/02 trials),
/root/mzip/internal/*.md (41,569 lines; read systematically, not exhaustively — the list of what was
read is in section 0), /root/mzip-hfbench/final/tree (the merged build), and
/root/mzip-hfbench/MZIP-AT-HF-GRANULARITY.txt RESULT 1–59 (read in full, it is the dead-list).

Evidence tags: **[M]** = the number is read from the cited measurement record, or is a probe I ran
myself on the box this session (stdout only; nothing written on the box except this file);
**[I]** = my inference from measured numbers, basis stated. Probe conditions: `taskset` not used,
box load 2.4–3.4 during my runs, so my MB/s are indicative; my ratios are exact (bytes).

The thin cells this document serves (FINAL-TABLE.md, 64 KiB chunks, vs current Xet Auto):

| cell | today | what is thin |
|---|---|---|
| GGUF-Q (7) | 1.0006 -> 1.0667, −6.2% | size only; speeds are against memcpy |
| F16 (4 rows) | −12..−33% size | compress 1.2–2.3x and decompress 2.7–11.6x slower than memcpy |
| tokenizer (3) | −28.4% size | compress LOSS 1.67x, decompress LOSS 2.51x vs LZ4 |
| text (8) | −31.0% size | compress LOSS 1.56x, decompress LOSS 1.78x vs LZ4 |
| parquet (3) | −11.6% size | both speeds vs memcpy |
| repo-json | −63.0% size, decompress WIN 1.62x | compress LOSS 1.40x (515 vs 722 MB/s) |
| tensors F32/BF16 | all three axes WIN | the plane coder is at ~0.5% of its order-0 ceiling |

---

## 0. What was read (so the coverage is auditable)

mzip: `CLAUDE.md`, `README.md`, `mzip.hpp` (MS/SoA 14017–14100 + 17134–17215, NUM_EXTRACT
10299–10470, REFERENCE 3539–3565 + 16079–16120, BlockType enum 355–400), `cm_backend.hpp`,
`ppmd_backend.hpp`, `bwt_compress_v9.hpp`, `tieredcompress.hpp`, `bigram_dict.hpp`, `word_dict.hpp`,
`range_coder.hpp`, `mzip_dicts.h` + `emit_dicts_header.cpp.bak-hfdict` diff.
internal/: `DEAD_MAP.md` (all sections), `CM_SAUCE.md`, `DCLM_RESULTS.md`, `MZIP_APPLY.md`,
`MZIP_SPEED_2026-07-31.md`, `DICT_INTEGRATION.md`, `bench_types_report.md`, `encoder_audit.md`,
`MDL_GENERATOR_PROTOTYPE_2026-08-05.md`, `ALP_ROOTCAUSE_2026-08-06.md`, `SYNTHESIS_LOOP.md`;
grep-census of `float|numeric|SoA|de-interleave|zigzag|exponent|mantissa|tensor|parquet|gguf|
tokenizer|JSON|memcpy|MB/s|huff|FSE|rANS|order-1|order-2|PPMd|bwt9|dictionary|SSE|AVX|delta|mixing`
across all 120 docs (parquet/gguf: 0 files; tensor: 4 archaeology mentions; so every GGUF/parquet
lever below is an [I] transfer from numeric/JSON work, and says so). Skimmed by header only:
`ENWIK9_DATA_EXPLOITS.md`, `ARCH_MIXER.md`, `ARCH_SYNTHESIS.md`, `EXPLORATION_NOTES.md`,
`CARD_DICTIONARY.md`, `CARD_INPUTS.md`, `CLASSMAP_V3.md`, `TIME_LEVERS`, `LOG_TSDELTA_SPEC` (all
enwik9-side; nothing that applies at 64 KiB chunks beyond what CM_SAUCE already states).
hfbench: `FINAL-TABLE.md`, `NEW-AXES.md`, `MZIP-HEADROOM.txt`, `MZIP-DEEP-FINDINGS.txt`,
`INNOVATION-REPORT.md`, `final/plane-entropy/BENCH.md`, `final/gqc.cpp` header, `final/tree/mzip.hpp`
FAST policy blocks, `convert/{tokenizer,text-path,gguf-quant,small-files,x2dec}` logs.

---

## 1. The dead-list — measured dead in MZIP-AT-HF-GRANULARITY.txt (do not re-propose)

| idea | where killed | the number |
|---|---|---|
| delta / XOR between adjacent weight elements | RESULT 9; RESULT 40 balanced-gap | "F32 raw 7.348 -> delta 7.465 / xor 7.449; BF16 6.254 -> 6.477 / 6.518" bits; coders "+12% to +34% larger" |
| order-1 inside the exponent plane | RESULT 19; RESULT 40 | "H0 exp 2.632, H1 exp\|prev 2.632, gain 0.0%"; real coders "+1.3% to +17%" larger |
| cross-plane context (mantissa-hi \| exponent) | RESULT 19; RESULT 21(B) | "1.4-2.2% of that plane = ~0.4% of the file"; conditioned FSE at 64K "+0.4% is not worth it" |
| zero bitmaps / run-length on weights | RESULT 19 | "no exact zeros ... dead for these models" |
| shared exponent-plane dictionary | RESULT 19 | "-0.53% / -0.48% (F32), -6.37% (BF16). WORSE. Dead." |
| per-chunk PE-vs-bg4-zstd hybrid | RESULT 40 lz-friendly | "0 gain on 6/7 slices" |
| GGUF SoA stride de-interleave | RESULT 10 | "Q8_0 +1.5% is the only structural gain; q6_k SoA WORSE" — superseded by gqc field split (landed) |
| q5_K_M scale-conditioned quant symbol | RESULT 46 §3 | "~38 KB of context tables per 64 KiB chunk to save ~0.2 KB" |
| LZ with the base chunk as dictionary (fine-tunes) | RESULT 49 | "1.4485 / 1.2835 / 1.2777 -- far below the XOR entropy route" |
| per-dataset zstd dictionaries on parquet at 64K | RESULT 40 parquet | "net after dictionary bytes -3.7..+4% -- marginal" |
| per-chunk text-type sniffing | RESULT 40 text-fast-path | "no gain" |
| huff0 (any entropy stage) after LZ4 on text, for a decompress WIN | RESULT 46 dial | budget "9,898 MB/s and huff0 delivers 2,337" |
| HF dictionary as a RATIO lever on repo JSON | RESULT 46 | "10.4844 versus stock's 10.5649" under a repo-grouped split (RESULT 17's −28.88% was sibling-file leakage) |
| bit-aligned exponent plane (rol-1) as a headline | RESULT 50 | "+0.45%" total with the sign-entropy guard; "Parked for v2" |
| previous-row context on 2-D exponent planes | RESULT 40 balanced-gap | "-0.70% wfp32, -0.27% bert, ~0 elsewhere" |
| stride-2 forced SoA trial | RESULT 12 | "0 B on F32, 42 B on BF16 and 643 B on GGUF F16 (<= 0.012%)" |
| huff0 X1/X2 decoder forcing | convert/x2dec/x2dec.log | forced X1 decodes 22–37% slower on every file (llama2 1628 -> 1021 MB/s); X2 == default |
| dropping a marginal plane on F16 | final/plane-entropy/bench_pe (my run) | whisper/sdxl/bloom already code "128 of 256" planes; llama2-7b codes 256/256 and both gain |

---

## 2. Already in the merged build — which mzip mechanism each HF class actually uses

Read from `/root/mzip-hfbench/final/tree/mzip.hpp` and confirmed by `MZIP_STATS=1 ./final/tree/pe_bench2 <file> 65536 FAST 1` (per-block telemetry on stdout) [M]. `final_sweep.sh` sets no `MZIP_*` env (grep of the script), and my default-arm bytes/hashes match `final/final_sweep.log` (prose 2.4964, whisper 3.4222, bert 2.1193) [M].

| HF class | arm in FINAL-TABLE | mechanism (source) | measured |
|---|---|---|---|
| F32 / BF16 / F16 / GGUF-F16 | `final/plane-entropy` standalone | byte-group k in {2,4} chosen by a Miller-Madow-corrected histogram estimate, one huff0 table per plane, raw fallback per plane, RLE plane (h==1) accepted, 262,080 B segments, zstd 1.5.5 BMI2 huff0 decoder (`zhuf_backend.h`, `PE_HUF_DEC` default DEC_ZSTD) | RESULT 20/37/39/41/42; BENCH.md: within "-0.36% .. -1.34%" of the order-0 bound |
| GGUF-Q | `final/gqc` | ggml block-type + phase self-detection per chunk, split into ggml fields, sub-byte quants unpacked, order-0 huff0 per field, `DELTA_D` on the fp16 scale for the 32-element types (`gqc.cpp:53` "measured: delta pays only on the 32-elem types") | RESULT 46: "1.0667 (huff0) / 1.0691 (best-of)"; gqc speeds 741 / 1,067 MB/s (RESULT 48) |
| tokenizer, text | merged mzip FAST | **ZSTD_DICT on every 64 KiB chunk**: zstd level 1 against the prepared HF dictionary id 14 (`DICT_HFA`, 32,768 B, `mzip_dicts.h:34067`), via the "small-file policy" (`mzip.hpp:15922–15952`), whose gate `size <= input_max (524,288)` is true for any chunk fed alone. The tokenizer track's dfast-L2 coder (`mz_fast_text_zstd`, 15664–15680) and the text-path dial (`mztl::policy`, 579–625: LZ4 / LZ4D / ZSTD3 / LZ4H0) are **both shadowed**; RESULT 48 recorded the dial as dead code and attributed it to dfast — the MZSTATS say the dict path is what runs. | my probe [M]: prose 128/128 chunks ZSTD_DICT, whisper 38/38, bert 7/8 (+1 PLANE_ENTROPY on the 7,310 B tail) |
| parquet | merged mzip FAST | the chunk is not text-like, so it goes to the numeric bypass and **PLANE_ENTROPY** codes the SNAPPY bytes (`15894–15960`) | my probe [M]: wikitext-2 parquet 97/98 chunks PLANE_ENTROPY at ~58.2 KB/64 KiB = 1.13x; RESULT 5/34 |
| repo-json (whole files, median 645 B) | merged mzip FAST | same ZSTD_DICT small-file policy | my probe [M]: 40/40 files ZSTD_DICT; RESULT 46 "378.9 MB/s" |
| all mzip-arm classes | FAST policy | numeric bypass tries PE first and skips the ~40 detectors when it compresses (15894); FAST never runs bwt9/xz/BCJ/brotli/PPMd except on text blocks <= 16 KiB (`fast_slow_ok`, 15954–15956); zstd whole-file arm skipped when the block container already reached 3/4 (17666–17680); PE adopted within 5% of the incumbent (17290–17298); per-block round-trip verify + top-level verify ON | RESULT 22–33 arc "0.64 -> 311 MB/s" |

**mzip mechanisms NOT exploited on any HF class at 64 KiB in the merged FAST build:** BWT+CM
(`cm_backend.hpp`, bwt9 mode 2), PPMd (`ppmd_backend.hpp`), word/bigram dictionaries + capfold + LZP
(inside BWT_TEXT), the MS SoA family incl. tid=2 per-lane delta+zigzag (superseded by PE on weights;
FAST disables MS anyway, `17846`), NUM_EXTRACT / JSON_COLUMNAR / LINEAR_GEN / CSV_COLUMNAR / template
encoders (FAST text bypass skips `analyze_block`, 15979–15985), REFERENCE cross-block dictionary
(`3539`, `16079–16120`; the SimHash is gated on `num_blocks > 1`, `16009–16017`, so a chunk fed alone
never builds one), brotli-11 / xz / lzma_opt2 backstops, the 12 non-HF trained dictionaries, and the
FAST text dial (LZ4 / LZ4D).

---

## 3. Ranked levers

Rank = expected bytes won x share of the thin cell it applies to x cheapness. Each entry: source,
quoted number in its original context, HF class + why, expected effect (basis), cost / probe, status.

### L1 — Sequential-integer transform for tokenizer.json (the LINEAR_GEN / NUM_EXTRACT / JSON_COLUMNAR family)  — NEW

- **Source.** `mzip.hpp:391` `NUM_EXTRACT = 0x33, // Extract embedded decimal numbers from text (900 bytes better than brotli on Makefiles!)`; `mzip.hpp:10299–10420` (template with one placeholder byte per number + varint number stream); `mzip.hpp:357` `LINEAR_GEN = 0x10, // Mathematical generator: a + b*i (3855x compression!)`; `mzip.hpp:389` `JSON_COLUMNAR = 0x32, // JSON lines: extract sequential numeric fields (LINEAR_GEN) ... (1085 bytes better than brotli!)`; README "Sequential database IDs | 32 B | bzip2: 3.4 KB | 106x smaller" [M]. None of these fire on tokenizer.json at 64 KiB: FAST takes the dict path (section 2) and BALANCED picks CM_TEXT on 6/8 bert chunks (my MZSTATS run) [M].
- **Why the class.** A HF `tokenizer.json` vocab is `{"tok": 0, "tok": 1, ...}` serialized in id order, so **87.6% (bert), 99.9% (qwen), 96.9% (whisper) of all integer runs in the file equal the previous integer + 1** (my probe) [M]. Those digits are the least LZ-compressible bytes in the chunk.
- **Measured bound (my probe, `probe_ids_ref.py` run via stdin, 64 KiB chunks reset per chunk, each `prev+1` integer replaced by one marker byte, zstd-3 before/after)** [M]:
  - bert: zstd-3 207,805 -> **133,694 B (2.2428x -> 3.4860x, −35.66%)**; zstd-1 −36.72%
  - qwen2.5-7b: 2,157,009 -> **1,725,173 B (3.2599x -> 4.0759x, −20.02%)**; zstd-1 −19.92%
  - whisper-lv3: 706,553 -> **561,322 B (3.5109x -> 4.4192x, −20.55%)**; zstd-1 −20.04%
  - control on non-tokenizer text: prose −0.01%, tweets-jsonl +0.25% (3.0% sequential) — the transform is inert where the structure is absent, so a gate on the sequential fraction is safe.
  - Lossless feasibility: raw `0x01` bytes in the three files = 0 / 0 / 0; zero-padded runs (which must stay verbatim, as NUM_EXTRACT's 2026-09-02 source fix already does) = 16 / 0 / 61 of 32,594 / 151,675 / 53,124 integers [M].
- **Expected effect.** Tokenizer class SIZE from −28% to roughly −45% vs Xet (bound: 4.08–4.42x vs Xet 2.31–2.54x on qwen/whisper) [I from the bound]; it also lifts bert above BALANCED's 3.2283 [M]. Speed: one linear digit scan + the same zstd -> the class stays in its current speed band (250–370 MB/s compress) [I]. Does not help the decompress LOSS (2.51x) — that is the entropy-stage arithmetic of RESULT 46.
- **Cost.** ~150 lines: scan for maximal digit runs not adjacent to `.`/digit, no leading zero; emit marker when value == prev+1 else verbatim; decode renders prev+1 in canonical decimal. Wire as a pre-transform in front of the FAST text coder (or as a JSON_COLUMNAR variant), gated by "sequential fraction >= 50% in the first 4 KiB". Probe already exists (this session's script; re-run: `ssh <box> 'cd /root/mzip-hfbench; venv/bin/python3 -' < probe_ids_ref.py assets2/tok-*.json`).
- **Status: NEW** (not in MZIP-AT-HF-GRANULARITY.txt; the tokenizer track measured only the router fix, RESULT 46).
- **Killer.** If the Hub's tokenizer.json files are commonly re-serialized with non-sequential vocab order (e.g. sorted by key), the sequential fraction collapses; the three files here were fetched as served. Check on 10 more tokenizers before building.

### L2 — Text / tokenizer arm selection: the dfast-L2 coder is shadowed by the dict path  — PARTIAL

- **Source.** `final/tree/mzip.hpp:15664–15680` (tokenizer track: "level 2 driven with the dfast strategy and a 17-bit hash/chain is >= plain level 3 on BOTH ratio and speed on every file tried"); `15922–15952` (small-file policy); RESULT 48 (dial is dead code); `convert/tokenizer/ceiling.log` [M].
- **Head-to-head in the merged build (my probe, 3 reps, `MZIP_HFDICT=0 MZIP_TEXTPOLICY=zstd3` reaches dfast-L2)** [M]:

  | file | dict path (shipped) | dfast-L2 | LZ4 (dial default when dict off) |
  |---|---|---|---|
  | prose-wikitext | 2.4964, 215 / 1230 MB/s | **2.6147** (+4.7% bytes), 169 / 1143 | 1.7355, 315 / 2582 |
  | tok-whisper | 3.4222, 275 / 1377 | **3.4934** (+2.0%), 234 / 1404 | 2.5994, 467 / 4289 |
  | code-python | 3.4949, 249 / 1283 | **3.6609** (+4.5%), 230 / 1231 | 2.4365, 414 / 2562 |

  Raising the dictionary's own level (`MZIP_FASTDICT_LVL=3`) reaches dfast's bytes (prose 2.6152, code 3.6725) but at 133–182 MB/s, i.e. slower than dfast; level 2 sits between (prose 2.5420 at 181) [M].
- **Why the class.** tokenizer + text are 11 of the 18 rows; the dict path is a trade the merge made silently (−2..−5% bytes for +8..+28% compress speed).
- **Expected effect.** Keeping the smaller of {dict-L1, dfast-L2} per chunk recovers +2..+4.7% bytes on text/tokenizer at dfast's speed [M]; a CDict built with dfast parameters (`ZSTD_createCDict_advanced`, hashLog/chainLog 17) may stack the two gains — **UNKNOWN**, one function call to measure.
- **Cost.** Encoder policy only; format unchanged (both block types exist). Probe = the three env-var runs above.
- **Status: PARTIAL** (both arms are built and measured; the selection is missing). Note the FINAL-TABLE text/tokenizer rows are the dict path, not the dfast numbers RESULT 46 reports (whisper 3.4934 there vs 3.4222 in the table).

### L3 — REFERENCE: previous chunk as the zstd dictionary, on text / tokenizer  — NEW on text; TRIED-DEAD on weights

- **Source.** `mzip.hpp:3539` "REFERENCE: Cross-Block Delta Encoding (Mutual Algorithmic Information) ... Uses zstd dictionary mode to compress block using a similar previous block"; `16079–16120` (MDL-scored, round-trip-guarded since 2026-09-02). No measured number exists in the source or docs; the v6 gate (`16009–16017`) means it never fired in this bench.
- **Measured (my probe, zstd-3, chunk i coded with chunk i−1 as dictionary, chunk 0 plain)** [M]: prose −4.92% (2.6127 -> 2.7479), tweets-jsonl −4.38%, whisper −3.11%, qwen −2.86%, bert −1.41%.
- **On weights it is dead**: RESULT 49 "zstd-19 with the base CHUNK as dictionary ... 1.4485 / 1.2835 / 1.2777 -- far below the XOR entropy route"; RESULT 9 "Weights carry no neighbour correlation".
- **Expected effect.** −3..−5% bytes on text, −1.4..−3% on tokenizer, at ~zstd-3 speed minus a dictionary load per chunk (the text-path track's trick of copying a pre-loaded LZ4 state, `mzip.hpp:626–640`, is the same cost model) [I].
- **Cost.** It is the one lever here that breaks chunk independence: decode of chunk i needs chunk i−1. In Xet terms it is exactly NEW-AXES' "chunk record gains a reference (scheme, base chunk id, payload)" with the base chunk in the same file (always present, dedup-neutral). Probe = the script above.
- **Status: NEW** for text/tokenizer at 64 KiB.

### L4 — BWT + context mixing (`cm_backend`) directly on text / tokenizer chunks — the ratio tier  — PARTIAL (ratio at MB/s; say so)

- **Source.** `cm_backend.hpp:1–5` "Order-0/1/2 + SSE/APM model + carryless binary range coder ... Per-call model state (~146 KB)"; CM_SAUCE.md results "1 MB | 266,472 | 2.033 bpc | mzip 286,307 (2.29) -> we win 6.9%"; MZIP-HEADROOM.txt "cm_backend BWT+CM | 2,454,790 | -0.000% | 3.0s | tied, 55x faster" (same bytes as the 165.8 s shipped ensemble) [M].
- **At 64 KiB on HF data** [M]: RESULT 34 BALANCED vs FAST — prose 3.4963 vs 2.6104, code 4.9923 vs 3.6588, class 4.0208 vs 2.9001; my run: bert tokenizer BALANCED **3.2283 vs FAST 2.1193 (+52%)**, CM_TEXT on 6 of 8 chunks, decode **16.0 MB/s**, compress 0.20 MB/s (ensemble).
- **Why the class.** These are the only classes where mzip's heavy coders have double-digit headroom over the fast arm; on weights BWT+CM buys "<= 0.6% over order-0 at ~5,000x the cost" (RESULT 20).
- **Expected effect.** −25..−35% bytes on text/tokenizer vs the merged row [M via BALANCED], at decode ~6–16 MB/s [M: RESULT 34 BAL dec 5–9, my 16.0] and compress **UNKNOWN standalone at 64 KiB** (whole-file 10.9 MB in 3.0 s = 3.6 MB/s [M HEADROOM]; the 0.05–0.2 MB/s in the table is the ensemble, "87% of mzip's runtime buys nothing on this input" [M HEADROOM FINDING 4]). A chunk store would only take this as a cold tier.
- **Cost.** A "BWT+CM only" text arm exists as a function (`cmbk::compress_bwt`); the probe is `bwt9_probe.cpp` / `cmtest` built on the box (no binary present now, `ls /root/mzip` shows only the .cpp) run over 64 KiB pieces.
- **Status: PARTIAL** (shipped in BALANCED; never isolated as a chunk arm). PPMd is the same tier: DEAD_MAP "PPMd beats ./mzip_cm.exe on 20/27 real held-out files ... Aggregate on 8 top files: -10.8%" whole-file on code; at 64 KiB BALANCED chose CM_TEXT over it on bert (my run) and HEADROOM shows "PPMd at mzip's own 64 MiB cap | 2,565,073 | +4.50% | correctly rejected" on wikitext — so PPMd-at-64K is UNKNOWN on code chunks, and its 64 MiB model arena per chunk is the wrong shape for a chunk store.

### L5 — Verify-off for the mzip-arm classes (compress speed)  — measured policy, not applied

- **Source.** `convert/text-path/final_ab3.txt` arms `aft` vs `aftnv` (same bytes; nv = no verify) [M]: alpaca 302 -> 341 MB/s (+13%), code 412 -> 474 (+15%), tabular 461 -> 568 (+23%), tweets 415 -> 479 (+15%); `convert/tokenizer/ab_tok2.log` `fixed` vs `fixed-nv`: prose 184 -> 215, qa-gsm8k 177 -> 211, gguf-q8_0 157 -> 184, squad parquet 153 -> 175 (+14..+20%). RESULT 40 encode-speed (scout, [I]): "Dropping the per-block verify decode: +21% / +4% / +22%; dropping the top-level verify too: 603 / 306 / 486 MB/s". RESULT 46: text compress "LOSS 1.03x (WIN 1.12x with the verify off)".
- **Why the class.** Xet does not verify; every mzip-arm compress LOSS row (tokenizer 1.67x, text 1.56x, repo-json 1.40x, parquet) pays two decode passes it does not have to. The plane-entropy arm already verifies outside the timed region (BENCH.md), so F16/GGUF-F16 rows do not gain.
- **Expected effect.** +13..+23% compress on tokenizer/text/repo-json/parquet [M], bytes identical. Repo-json would move from 515 toward ~600 MB/s, still short of 722 (the bare codec is 746.6 vs Xet 775.4, RESULT 46 §2) [I].
- **Cost.** A mode flag; the safety argument is that the specialist encoders that needed the guards (RESULT 12's six sibling guards, RESULT 29's uRAW failure) are not on the FAST text path.
- **Status: measured, policy decision** (the owner kept verify on by design).

### L6 — Repo-JSON compress: LZ4 + HF dictionary as a dial  — NEW on repo JSON

- **Source.** `final/tree/mzip.hpp:579–640` (`mztl` LZ4D: "A dict-loaded LZ4 stream is a POD ... Copying the loaded state is therefore equivalent to re-running LZ4_loadDict, and costs O(state)"); text-path `lz4d` arm measured only on the 8 corpora ("+0.79% ratio, -9% compress" vs plain LZ4, `mzip.hpp:620`) [M]; small-files track measured zstd-1+dict only (RESULT 40: "zstd-3 + domain dictionary = 12.36x").
- **Why the class.** repo-json's compress LOSS is 515 vs 722 MB/s with the ratio WIN at 12.56x; the bare zstd-1+dict codec is 746.6 MB/s, so mzip's framing costs 31% [M RESULT 46]. An LZ4-with-dictionary arm is the only own mechanism faster than zstd-1 that keeps a dictionary.
- **Expected effect.** Ratio between Xet's 4.65x and 12.56x (the dictionary carries most of the JSON gain: RESULT 17's held-out "ZSTD_DICT chosen 103/144") at >= Xet's compress speed [I]. A ratio-for-speed dial, not a win on both axes; RESULT 46 §2 already measured the zstd end of that dial on the whisper tokenizer ("zstd --fast=3 gives 536 MB/s at ratio 2.6133").
- **Cost.** The LZ4D path is unreachable for inputs <= 16 KiB in FAST (they take BALANCED, `fast_slow_ok`) — a gate change; probe with `MZIP_HFDICT=0 MZIP_TEXTPOLICY=lz4d` after lowering that gate.
- **Status: NEW** (not measured on small2).

### L7 — Decode-cost-aware selection for BALANCED (cold tier)  — PARTIAL

- **Source.** RESULT 15 "NEW LEVER (measured): on BF16, bg4+mzip vs native mzip = +0.12% bytes (5,650,265 vs 5,643,449) but 5.6x FASTER decompress (52.9 vs 9.45 MB/s). mzip's trial-and-keep is size-only; a decode-cost-aware tiebreak within ~0.2% would buy serving-path speed for free. Not implemented." FAST already has it (`17298`: adopt PE within 5%) [M].
- **Applies to** BALANCED only — the 0.05 MB/s cold tier; nothing in the FINAL-TABLE moves.
- **Status: PARTIAL**; low priority for a chunk store.

### L8 — GGUF-Q: nothing own-sauce is left per chunk beyond ~0.3–0.5%  — TRIED

- **Source.** `convert/gguf-quant/ab.log` [M]: `gqc-huf` vs `gqc-best` (adds FSE arms per field): q4_k_m 1.0611 -> 1.0660, q5_k_m 1.0369 -> 1.0417, q3_k_m 1.0472 -> 1.0509, iq4_xs 1.0567 -> 1.0584, Q8_0/Q4_0 identical — at decode 486 -> 265, 832 -> 385, 710 -> 366 MB/s. RESULT 40: "Q8_0 field-split order-0 bound 1.0628x (huff0 1.0597)"; RESULT 46 §3 (scale conditioning dead); `gqc.cpp:53` (scale delta already on where it pays).
- **The mzip mechanisms that would apply** (tid=2 per-lane delta+zigzag on the scale lane; CM on the quant stream) are the two already tried: delta is in gqc where it pays, and RESULT 9 puts lpaq within "+0.24%" of mzip on q8_0 and "+0.19%" on q6_k.
- **Expected effect.** <= 0.5% more bytes at half the decode speed (FSE arms) [M]. The GGUF-Q bytes are won by NEW-AXES Axis 2 (recipes), not per chunk.
- **Status: TRIED**; recommend closing the cell. The remaining engineering item is not ratio: header-free block detection "74/128 for Q4_K_M" (RESULT 40).

### L9 — F16 / GGUF-F16 speed: no own lever; the coder is huff0-bound  — dead by measurement

- **Source.** RESULT 46 §1 "Entropy decoding is 84-98% of our decode time on those files and the coded planes are 95-100% of the bytes"; my `bench_pe` run: whisper / sdxl / bloom "planes coded 128 of 256" (only the high plane is ever decoded), llama2-7b "256 of 256" with ratio 1.5073 (both planes gain); `x2dec.log`: the huff0 decoder choice is already optimal (forced X1: whisper 2433 -> 1876, llama2 1628 -> 1021 MB/s); zstd's BMI2 huff0 landed (FINAL-TABLE "+32-45%").
- **mzip's own coders** are slower than huff0 by construction (`range_coder.hpp` binary AC; `cm_backend` bit-wise) — nothing in the tree decodes faster than huff0.
- **Status:** no lever. The F16 rows are "the price of the ratio win" (RESULT 46).

### L10 — Adjacent levers already measured (listed so they are not re-proposed as own-sauce)

- rol-1 exponent-plane rotation: RESULT 50 "+0.45%" with guard, v2 format — **TRIED, parked**.
- MS tid=2 per-lane delta+zigzag (`mzip.hpp:14017` "tsgas -27.5%, gps -8.4%") on weights — **TRIED-DEAD** (RESULT 9). On parquet it would need PLAIN-encoded numeric pages; the three HF parquets here are string pages (my MZSTATS: PLANE_ENTROPY on 97/98 chunks) — **N/A** [I].
- Cross-trained per-dtype zstd dictionary on weights: RESULT 3 "+2.93% vs their bg4-zstd-3" — **superseded** by PE at the order-0 bound; RESULT 19 kills the exponent-plane variant.
- Parquet per chunk: PE 1.13 == zstd-3 1.1305 (RESULT 31) == FAST; BALANCED 1.2218 at 0.04 MB/s (RESULT 34); lpaq1 1.3334 (HEADROOM). **Nothing own at chunk granularity**; the at-rest transcode (RESULT 57, 1.75x) is the lever and is landed.
- lpaq-class direct CM: HEADROOM "lpaq1 ... 2,243,668 | -8.60% | 15.0s | WINS, blocked (GPL)"; DEEP-FINDINGS "best CM beats mzip by +2.28% .. +14.77%" on the 8 corpora — a clean-room CM is a research effort at 0.67 MB/s. **NEW but blocked**; cold tier only.
- word_dict / bigram_dict / capfold / LZP: live inside BWT_TEXT; CM_SAUCE "capfold standalone (worse ...); LZP-before-BWT (neutral/worse)" — **subsumed**, not a separate lever.
- DUAL_STREAM on BF16 (mzip's 2-stream interleave split): RESULT 38 "5,838,197 B = 1.4368x at 122 MB/s" vs PE 1.4826 — **TRIED-DEAD**.

---

## 4. The five to try first, in order

1. **L1 sequential-id transform on tokenizer.json** — a ~150-line lossless pre-transform whose bound is already measured at −20..−36% of the class's bytes at zstd speed; the only [I] left is the C-speed of the scan.
2. **L2 keep-smaller of {dict-L1, dfast-L2} on text/tokenizer chunks** (then try a dfast-parameter CDict) — both arms exist; +2..+4.7% bytes for a policy change, and it makes the FINAL-TABLE text row match RESULT 46.
3. **L3 previous-chunk dictionary (REFERENCE) on text/tokenizer** — −3..−5% on text at zstd-3 cost, provided the store accepts an intra-file chunk reference (it already needs one for the fine-tune axis).
4. **L5 verify-off on the mzip-arm FAST paths** — +13..+23% compress on every mzip-arm LOSS row with identical bytes; a policy decision, so it goes to the owner with the numbers rather than being built.
5. **L4 BWT+CM as an isolated cold-tier text arm** — measure its standalone 64 KiB compress speed first (the one UNKNOWN); the ratio side (+25..+52%) is already known and it is the largest byte lever on text that mzip owns outright.

---

## 5. UNKNOWNs and conditions of invalidity (named, not defaulted)

- L1's speed in C: not measured; the bound was produced by a Python regex. Its ratio is exact.
- L1 on the Hub population: 3 tokenizer files; a serialization that reorders vocab entries would remove the structure.
- L2's combined arm (dfast parameters + dictionary): not measured; needs `ZSTD_createCDict_advanced`.
- L3's per-chunk dictionary-load cost inside mzip: not measured (the zstd-3 numbers are from the `zstandard` module with a fresh `ZstdCompressionDict` per chunk, which is the slow way).
- L4's standalone BWT+CM compress speed at 64 KiB: not measured (no `cmtest` binary on the box).
- L6: not measured at all on repo JSON.
- All my MB/s were taken at load 2.4–3.4 without `taskset`; ratios are exact.
- The FINAL-TABLE text/tokenizer rows being the dict path rests on my MZSTATS runs of `final/tree/pe_bench2` plus `final_sweep.sh` setting no env; if the sweep binary was rebuilt with a different `MZIP_HFDICT` default after 2026-09-02 20:03, section 2 must be re-run.
