# mzip vs HuggingFace Xet — every class, every axis, and what lies beyond the chunk

Updated 2026-09-03. **Xet Auto** is xet-core's own `compression_scheme.rs` + `bg4_prediction.rs` +
`byte_grouping`, compiled verbatim against `lz4_flex 0.13.0` and verified byte-identical to upstream
main `7af65ba2` (2026-09-01), so this is the current production configuration. Every arm is
round-trip verified; arms are interleaved per repetition on one pinned core; medians of 3 or more.

## 1. Per 64 KiB chunk — the contest as a chunk store sees it

| class | n | ratio Xet → ours | SIZE | compress MB/s | COMPRESS | decompress MB/s | DECOMPRESS |
|---|---:|---|---|---|---|---|---|
| F32-nlp | 1 | 1.0722 → 1.2097 | **WIN −11.4%** | 762 → 1171 | **WIN 1.54x** | 2572 → 4530 | **WIN 1.76x** |
| F32-embed | 2 | 1.2522 → 1.5872 | **WIN −21.1%** | 591 → 1131 | **WIN 1.91x** | 2273 → 3539 | **WIN 1.56x** |
| F32-llm | 3 | 1.2878 → 1.4816 | **WIN −13.1%** | 865 → 1178 | **WIN 1.36x** | 2724 → 4032 | **WIN 1.48x** |
| F32-vision | 1 | 1.0505 → 1.1977 | **WIN −12.3%** | 763 → 1100 | **WIN 1.44x** | 2484 → 4443 | **WIN 1.79x** |
| F32-audio | 1 | 1.4785 → 2.3209 | **WIN −36.3%** | 571 → 1210 | **WIN 2.12x** | 2285 → 3277 | **WIN 1.43x** |
| F32-vislang | 1 | 1.4920 → 2.3326 | **WIN −36.0%** | 504 → 1102 | **WIN 2.19x** | 2100 → 2835 | **WIN 1.35x** |
| F32-diffus | 1 | 1.0551 → 1.1997 | **WIN −12.1%** | 725 → 1197 | **WIN 1.65x** | 2406 → 4959 | **WIN 2.06x** |
| BF16-llm | 4 | 1.1447 → 1.4885 | **WIN −23.1%** | 495 → 1078 | **WIN 2.18x** | 2275 → 3446 | **WIN 1.51x** |
| BF16-vislang | 1 | 1.1538 → 1.4932 | **WIN −22.7%** | 457 → 1107 | **WIN 2.42x** | 2012 → 3460 | **WIN 1.72x** |
| F16-llm | 2 | 1.0000 → 1.3230 | **WIN −24.4%** | 1489 → 861 | LOSS 1.7x\* | 21067 → 2142 | LOSS 9.8x\* |
| F16-audio | 1 | 1.0263 → 1.1661 | **WIN −12.0%** | 1135 → 966 | LOSS 1.2x\* | 7053 → 2586 | LOSS 2.7x\* |
| F16-diffus | 1 | 1.0001 → 1.1837 | **WIN −15.5%** | 1253 → 990 | LOSS 1.3x\* | 21203 → 2997 | LOSS 7.1x\* |
| GGUF-F16 | 1 | 1.0006 → 1.5012 | **WIN −33.3%** | 1942 → 838 | LOSS 2.3x\* | 21359 → 1847 | LOSS 11.6x\* |
| GGUF-Q | 7 | 1.0006 → 1.0667 | **WIN −6.2%** | 1893 → 722 | LOSS 2.6x\* | 19853 → 1288 | LOSS 15.4x\* |
| **tokenizer.json** | 8 | 1.4478–4.0601 → **2.70–7.08** | **WIN −41 to −56%** | 316–646 → 294–673 | **PARITY 0.84–1.04x** | 2390–2850 → 728–1918 | LOSS 1.3–3.4x |
| parquet | 3 | 1.0068 → 1.1387 | **WIN −11.6%** | 1551 → 231 | LOSS 6.7x\* | 12508 → 1208 | LOSS 10.4x\* |
| text | 8 | 1.9003 → 2.7556 | **WIN −31.0%** | 401 → 256 | LOSS 1.56x | 2537 → 1421 | LOSS 1.78x |
| repo-json | 1 | 4.6476 → 12.5571 | **WIN −63.0%** | 722 → 515 | LOSS 1.40x | 1651 → 2667 | **WIN 1.62x** |

`*` Xet stores these chunks RAW at ratio 1.000, so its compress and decompress there are **memcpy**.
No compressor beats memcpy; the number is the honest cost of taking 6–33% off files the Hub does not
compress at all today.

**We win on SIZE in all 18 classes.** On safetensors F32 and BF16 — the bulk of the Hub — we win all
three axes at once in nine classes with no exceptions. The tokenizer row is new (see §3).

## 2. What the per-chunk contest cannot do — measured ceilings, so nobody re-mines them

| idea | result | evidence |
|---|---|---|
| order-1 / row / column / vertical context on weight planes | **0.00–0.5%** — the exponent plane is memoryless | RESULT 66, 16 slices, per-chunk with table cost charged |
| bit-aligned exponent planes, base-free context | 0.3–1.3% | RESULT 50 |
| a faster huff0 decoder table (X1/X2/X2BIG) | **0%** — zstd already selects the best one | RESULT 60 |
| LZ4 at high compression, to keep Xet's format | **dominated** on both axes by zstd in 11/11 corpora | RESULT 63 |
| a delta *graph* (MST) instead of a star over one base's family | **0.0%** on 8 independent fine-tunes | RESULT 68 |
| cross-plane, H(mantissa \| exponent) | 2.3% BF16, 0.0–0.3% F16/F32 | RESULT 66 |

Per-chunk statistical coding of weights is finished. Everything below comes from knowing how a file
was **produced**.

## 3. Structure the byte coders miss — measured, lossless, verified

| lever | class | today | with the lever | condition |
|---|---|---:|---:|---|
| **sequential-integer filter** | tokenizer.json / vocab.json | 1.45–4.06 (Xet) | **2.70–7.08** | the vocab is serialised in id order (18 of 20 popular tokenizers; the 2 SentencePiece ones are inert, not harmed) |
| **fine-tune given base** (fast static coder) | BF16 fine-tunes | 1.1447 (Xet) | **2.94x** harmonic on 28 random real fine-tunes (2.2638 on the harder curated base-to-instruct pairs), 389/602 MB/s | the delta passes a sampled gate; base chunk resident |
| **LoRA-merge rank recovery** | merges, abliterations | 1.745–3.411 | **5.17–11.78**, up to ~13.9 with residual context | a knee exists in the singular spectrum (measured, not tagged) |
| **quantized GGUF given base** | GGUF quant repos | 1.0006 (Xet) | **empty residual** — 0 bytes plus a recipe | recipe = release + intermediate dtype + imatrix + threads + build flags |
| **parquet page transcode** | dataset parquet | 1.005 (Xet) | **1.749** | whole-file at rest; writer generation recorded per page |
| run-length plane for constant regions | masks, pruned, padded | 1 bit/byte floor | up to **−94%** of those chunks | a plane above ~90% one symbol (13 of 128 gpt2 chunks) |

## 4. The claim this all supports

The Hub's bytes are not random data. They are the output of a small number of deterministic
programs: a quantizer, a merge script, a format converter, a parquet writer, a dtype widener. We
have now shown four of those are invertible or reproducible from what the Hub already stores —
bit-exactly for the quantizer, by rank recovery for the merge, by transcode for the writer, by
recipe for the converter. A store that knows the programs keeps their inputs, not their outputs.
Content-addressed dedup cannot see any of this: between a base and its fine-tune, **0 of 128**
64 KiB chunks and **0 of 2048** 4 KiB pages are identical.

## 5. How to reproduce

```
ssh <box>
cd /root/mzip-hfbench/final && bash final_sweep.sh 3 3          # the per-chunk table, ~25 min
./plane-entropy/bench_pe <file> 65536 3                          # the Apache-2.0 coder
convert/seqint/seqint_pipe <file> 65536 5 1 1                    # tokenizer filter + zstd
convert/delta-fast/bench_delta <tag> <base> <ft> 65536 5         # fine-tune given base
python3 lowrank_probe.py                                         # rank recovery
../agents/hf-fairness/xetsim/target/release/xetsim <file> 65536 3   # Xet Auto
```

Master record with every numbered result: `MZIP-AT-HF-GRANULARITY.txt`.
