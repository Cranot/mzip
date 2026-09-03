# SCOREBOARD AFTER THE CONVERSION PASS

Six tracks, each patched in its own tree, each re-measured interleaved against Xet on one pinned
core, each independently re-run by a separate verifier that rebuilt the artifact from source.
**All six verifiers confirmed.** Xet baseline verified as current upstream main 7af65ba2 (RESULT 44).

## What changed, per loss

| loss (before) | after | verdict |
|---|---|---|
| **repo JSON compress 0.17 MB/s** (4,425x slower than Xet) | **378.9 MB/s** — 2,171x faster; decompress 328 -> 2,254 MB/s | decompress LOSS 5.5x -> **WIN 1.26x**; compress still LOSS 2.04x |
| **tokenizer.json ratio 2.37** (whisper was the board's only ratio LOSS at 2.4519 vs Xet 2.5360) | **whisper 3.4934, qwen 3.2437, bert 2.2255** | ratio LOSS -> **WIN, 27-34% fewer bytes than Xet** |
| **GGUF quantised ratio 1.0308** (q3_K_M got exactly 1.0001) | **1.0667** (huff0) / **1.0691** (best-of); q3_K_M 1.0472, q4_0 1.1652 | **WIN 6.2-6.4% fewer bytes than Xet** (was 0.06%) |
| **text 2.1x slower both ways** | **347 MB/s compress, 2,574 MB/s decompress** | decompress **WIN 1.20x**; compress LOSS 1.03x (WIN 1.12x with the verify off) |
| **decode ceiling** (reference huff0) | **+32% to +45%** on every class, bytes identical | BF16/F32 decode now **2.0-2.6x FASTER than Xet** |
| **mzip FAST 4.5x slower than its own coder** | compress **1.41-2.68x**, decompress **1.28-2.03x**, zero byte change | closes 36-54% of the gap; bge-small compress now **WIN 1.09x** |

## The board now (best contender per class, vs current Xet)

| class | SIZE | COMPRESS | DECOMPRESS |
|---|---|---|---|
| F32 safetensors | **WIN 11-36%** | **WIN 1.3-2.2x** | **WIN 2.0-2.6x** (after asm huff0) |
| BF16 safetensors | **WIN 23%** | **WIN 2.2x** | **WIN 2.0x** |
| F16 safetensors | **WIN 12-24%** (Xet stores raw) | mixed | LOSS 5-9x (vs memcpy) |
| GGUF F16 | **WIN 33%** (Xet stores raw) | LOSS 2.3x | LOSS 8.6x (vs memcpy) |
| GGUF quantised | **WIN 6.2%** (was 0.06%) | LOSS 3.0x | LOSS 13x (vs memcpy) |
| tokenizer.json | **WIN 27-34%** (was a LOSS on one) | LOSS 1.75-2.36x | LOSS 2.7-3.4x |
| parquet | **WIN 11.5%** (Xet stores raw) | LOSS 10.9x | LOSS 13x (vs memcpy) |
| dataset text | **WIN 1.01x** (or 1.49x, see the dial) | LOSS 1.03x | **WIN 1.20x** |
| repo JSON | **WIN 2.26x** | LOSS 2.04x (was 4,425x) | **WIN 1.26x** |

## The dial the text track found

Text is not one answer, it is a choice, and both ends are measured:
* **entropy-coded**: ratio 2.8249 (WIN 1.49x) but both speeds LOSS 2.1x;
* **LZ4-class**: ratio 1.9257 (WIN 1.01x), compress LOSS 1.03x, decompress WIN 1.20x — and with
  mzip's top-level verify off, a WIN on all three (1.01x / 1.12x / 1.21x).
Any decompress win forbids an entropy stage: the measured budget is 9,898 MB/s and huff0 delivers
2,337. That is arithmetic, not tuning.

## What is structural and will not convert

1. **Anything against a memcpy.** On F16, GGUF and parquet Xet picks "None" and stores the chunk
   raw at 13-22 GB/s. Our own memcpy floor on the same bytes is 20-33 GB/s, which confirms that is
   what it is. Entropy decoding is 84-98% of our decode time on those files and the coded planes are
   95-100% of the bytes, so there is no overhead left to remove. The loss is the price of the ratio
   win (llama2-7b 1.0000 -> 1.5073), not an inefficiency.
2. **Compress throughput where we compress harder.** On repo JSON the bare codec with no framework
   at all runs 746.6 MB/s against Xet's 775.4 — zstd-1 is simply not faster than LZ4 at equal work.
   On the whisper tokenizer the bare zstd encoder is 370.9 MB/s against Xet's 496.7 while delivering
   1.38x its ratio. Matching Xet's compress speed there means compressing less: zstd --fast=3 gives
   536 MB/s at ratio 2.6133, beating Xet on both axes but surrendering 75% of the ratio win.
3. **q5_K_M's 4.17%** is above its own order-0 ceiling of 1.0464. Conditioning the quant symbol on
   its sub-block scale reaches further but needs ~38 KB of context tables per 64 KiB chunk to save
   ~0.2 KB.

## Two findings that corrected us, both against our own interest

* **The dictionary buys no ratio.** Under a repo-grouped held-out split the HF-trained dictionary
  gives 10.4844 versus stock's 10.5649. The +12% first measured was sibling-file leakage from an
  alternating split, where config.json and tokenizer_config.json from the same repo landed on
  opposite sides. Its real value is that it lets the whole ensemble be dropped at constant ratio.
* **The whisper tokenizer loss was a router bug, not a coder weakness.** mzip decides "text" by
  requiring 85% printable ASCII in the first 4 KiB, but HuggingFace tokenizer.json encodes
  byte-level BPE through the GPT-2 byte-to-U+0100 map, so every non-Latin vocabulary entry is a
  2-byte UTF-8 sequence and whisper's chunks measure 75.8-84.5%. Twenty of its 38 chunks failed the
  gate and were sent to the float-plane coder.
