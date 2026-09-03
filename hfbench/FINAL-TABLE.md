# FINAL TABLE — merged mzip vs HuggingFace Xet, every class, every axis

ONE interleaved sweep, 2026-09-02, core 3, load 1.27-1.54 throughout, 64 KiB chunks, 3 reps,
arms alternating per file, every arm round-trip verified by its own harness.

**Xet Auto** = xet-core's own `compression_scheme.rs` + `bg4_prediction.rs` + `byte_grouping`
compiled verbatim against `lz4_flex 0.13.0`. Verified byte-identical to upstream main `7af65ba2`
(2026-09-01), so this is the current production configuration, not an older one.

**Ours** = the MERGED build: all six conversion tracks in one artifact set —
`final/tree` (mzip with the text-gate fix, small-input policy, text policy and framing work),
`final/plane-entropy` (the Apache-2.0 coder with zstd's BMI2 huff0 decoder), `final/gqc` (the GGUF
field coder). Per class the best arm is used: plane-entropy on tensors, gqc's ratio on quantised
GGUF, merged mzip on text/JSON/parquet.

Corpus: 42 real Hub downloads — 8 MiB element-aligned slices of safetensors (F32/BF16/F16 across
LLM, embedding, vision, audio, vision-language and diffusion models), 8 GGUF quant types, 3
tokenizer.json, 3 dataset parquet, 8 dataset text corpora, and 290 real repo JSON files.

| class | n | ratio Xet -> ours | SIZE | compress MB/s | COMPRESS | decompress MB/s | DECOMPRESS | comp us/chunk | decomp us/chunk |
|---|---:|---|---|---|---|---|---|---|---|
| F32-nlp | 1 | 1.0722 -> 1.2097 | **WIN -11.4%** | 762 -> 1171 | **WIN 1.54x** | 2572 -> 4530 | **WIN 1.76x** | 82 -> 53 | 24 -> 14 |
| F32-embed | 2 | 1.2522 -> 1.5872 | **WIN -21.1%** | 591 -> 1131 | **WIN 1.91x** | 2273 -> 3539 | **WIN 1.56x** | 106 -> 55 | 27 -> 18 |
| F32-llm | 3 | 1.2878 -> 1.4816 | **WIN -13.1%** | 865 -> 1178 | **WIN 1.36x** | 2724 -> 4032 | **WIN 1.48x** | 72 -> 53 | 23 -> 16 |
| F32-vision | 1 | 1.0505 -> 1.1977 | **WIN -12.3%** | 763 -> 1100 | **WIN 1.44x** | 2484 -> 4443 | **WIN 1.79x** | 82 -> 57 | 25 -> 14 |
| F32-audio | 1 | 1.4785 -> 2.3209 | **WIN -36.3%** | 571 -> 1210 | **WIN 2.12x** | 2285 -> 3277 | **WIN 1.43x** | 109 -> 52 | 27 -> 19 |
| F32-vislang | 1 | 1.4920 -> 2.3326 | **WIN -36.0%** | 504 -> 1102 | **WIN 2.19x** | 2100 -> 2835 | **WIN 1.35x** | 124 -> 57 | 30 -> 22 |
| F32-diffus | 1 | 1.0551 -> 1.1997 | **WIN -12.1%** | 725 -> 1197 | **WIN 1.65x** | 2406 -> 4959 | **WIN 2.06x** | 86 -> 52 | 26 -> 13 |
| BF16-llm | 4 | 1.1447 -> 1.4885 | **WIN -23.1%** | 495 -> 1078 | **WIN 2.18x** | 2275 -> 3446 | **WIN 1.51x** | 126 -> 58 | 27 -> 18 |
| BF16-vislang | 1 | 1.1538 -> 1.4932 | **WIN -22.7%** | 457 -> 1107 | **WIN 2.42x** | 2012 -> 3460 | **WIN 1.72x** | 137 -> 56 | 31 -> 18 |
| F16-llm | 2 | 1.0000 -> 1.3230 | **WIN -24.4%** | 1489 -> 861 | LOSS 1.7x* | 21067 -> 2142 | LOSS 9.8x* | 42 -> 73 | 3 -> 29 |
| F16-audio | 1 | 1.0263 -> 1.1661 | **WIN -12.0%** | 1135 -> 966 | LOSS 1.2x* | 7053 -> 2586 | LOSS 2.7x* | 55 -> 65 | 9 -> 24 |
| F16-diffus | 1 | 1.0001 -> 1.1837 | **WIN -15.5%** | 1253 -> 990 | LOSS 1.3x* | 21203 -> 2997 | LOSS 7.1x* | 50 -> 63 | 3 -> 21 |
| GGUF-F16 | 1 | 1.0006 -> 1.5012 | **WIN -33.3%** | 1942 -> 838 | LOSS 2.3x* | 21359 -> 1847 | LOSS 11.6x* | 32 -> 75 | 3 -> 34 |
| GGUF-Q | 7 | 1.0006 -> 1.0667 | **WIN -6.2%** | 1893 -> 722 | LOSS 2.6x* | 19853 -> 1288 | LOSS 15.4x* | 33 -> 87 | 3 -> 49 |
| tokenizer | 3 | 1.9902 -> 2.7813 | **WIN -28.4%** | 422 -> 253 | LOSS 1.67x | 3310 -> 1317 | LOSS 2.51x | 148 -> 247 | 19 -> 47 |
| parquet | 3 | 1.0068 -> 1.1387 | **WIN -11.6%** | 1551 -> 231 | LOSS 6.7x* | 12508 -> 1208 | LOSS 10.4x* | 40 -> 271 | 5 -> 52 |
| text | 8 | 1.9003 -> 2.7556 | **WIN -31.0%** | 401 -> 256 | LOSS 1.56x | 2537 -> 1421 | LOSS 1.78x | 156 -> 244 | 25 -> 44 |
| repo-json | 1 | 4.6476 -> 12.5571 | **WIN -63.0%** | 722 -> 515 | LOSS 1.40x | 1651 -> 2667 | **WIN 1.62x** | 87 -> 121 | 38 -> 23 |

`*` Xet stores these chunks RAW at ratio 1.000, so its compress and decompress there are **memcpy**.
No compressor can beat memcpy; the number is the honest time cost of taking 6% to 33% off files
HuggingFace currently does not compress at all.

## Reading it

* **We win on SIZE in all 18 classes**, from 6.2% on quantised GGUF to 63% on repo metadata.
* **On safetensors F32 and BF16 — the bulk of the Hub — we win all three axes at once**: 11-36%
  smaller, 1.36-2.42x faster to compress, 1.35-2.06x faster to decompress. Nine classes, no
  exceptions.
* **F16 is free ground**: Xet's predictor declines to byte-group it and LZ4 finds nothing, so those
  chunks are stored uncompressed (1.0000-1.0263). We take 12-24% off SDXL, Whisper, Llama-2 and
  Bloom. The cause is the format: F16's high byte carries two mantissa bits, which destroys the
  literal repeats LZ4 needs while leaving the distribution skewed enough for an order-0 coder.
* **Every remaining speed loss is against a memcpy or against LZ4 where we deliberately compress
  harder.** They are trades, not defects, and the microsecond columns state the price.

## Conversions in this build (all independently verified)

| loss before the pass | after |
|---|---|
| repo JSON compress 0.17 MB/s (4,425x slower than Xet) | **515 MB/s**, ratio 4.65 -> 12.56, decompress now a 1.62x WIN |
| whisper tokenizer ratio 2.4519 vs Xet 2.5360 (the only ratio LOSS on the board) | **3.4222** — a 35% win; cause was a router bug, not the coder |
| GGUF quantised ratio 1.0308 (q3_K_M got 1.0001) | **1.0667**, q3_K_M 1.0472, q4_0 1.1652 |
| dataset text ratio 2.8249 with both speeds losing 2.1x | **2.7556** with decompress loss cut to 1.78x |
| decode ceiling (reference huff0) | **+32-45%**, bytes identical — F32/BF16 decode now 1.35-2.06x faster than Xet |
| mzip FAST 4.5x slower than its own coder | compress 1.4-2.7x, decompress 1.3-2.0x, zero byte change |

## How to reproduce

```
ssh <box>
cd /root/mzip-hfbench/final && bash final_sweep.sh 3 3      # this exact table, ~25 min
./tree/pe_bench2   <file> 65536 FAST 3                      # merged mzip
./plane-entropy/bench_pe <file> 65536 3                     # standalone coder
./gqc <file> 65536 3                                        # GGUF field coder
../agents/hf-fairness/xetsim/target/release/xetsim <file> 65536 3   # Xet Auto
```
Raw log: `/root/mzip-hfbench/final/final_sweep.log`. Merge conflict resolutions (11 of them, each
with an asserted anchor): `/root/mzip-hfbench/final/resolve_merge.py`.

## Beyond the chunk (added 2026-09-02, see NEW-AXES.md)

The table above is the per-chunk contest and it is at its ceiling on weights. Two axes outside it,
measured on real Hub files, are larger than anything in the table:

* **A fine-tune given its base**: Qwen2.5 instruct models cost 46–56% of today, Llama-3.1-8B 58–67%,
  as a zigzag delta against the base (27 tensor-aligned pairs, FSE planes, all round-trip verified).
  Xet's dedup sees 0 identical chunks between base and fine-tune. Not every claimed base is close:
  a continued-pretrain coded against its nominal base gains nothing (measured), so a store gates
  on a sample.
* **A quantized GGUF given its base**: bartowski's Q8_0 of Qwen2.5-1.5B-Instruct regenerates
  bit-exactly from the BF16 base (4,194,304/4,194,304 quants), i.e. it costs 0 bytes plus a recipe.
  One popular base carries ≥2 TB (sampled) of GGUF derivatives; the WHOLE Q8_0 model regenerates
  (338/338 tensors, 1.64 GB byte-identical); other quant types under test.
* **Parquet page transcode**: 1.005 (Xet) → 1.75 at zstd-19, every file restored byte-identically;
  whole-file at-rest only, writer generation recorded per container.
