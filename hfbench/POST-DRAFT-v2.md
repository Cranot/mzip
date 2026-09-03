# Post draft v2 — reply to Clement's "4 PB/week" post

Status: SIZE numbers final (measured at Xet's real 64 KiB chunk granularity, all
round-trip verified). SPEED: ratio-mode numbers final; FAST-mode figures = v9.1 (RESULT 33/34 in
MZIP-AT-HF-GRANULARITY.txt; shared box load ~3, dedicated core). HF baseline = xet-core's real Auto
(compression_scheme.rs + bg4_prediction.rs verbatim, lz4_flex 0.13.0): RESULT 36. It picks plain LZ4 on
text, so the older always-bg4 text numbers (RESULT 28/34) must not be quoted as HF.
Nothing here has been posted.

---

## Draft A — technical, leads with what HF is leaving on the table

> 4 PB/week is a lot of bytes to have opinions about, so we measured — at the
> granularity Xet actually compresses (64 KiB chunks, `ByteGrouping4LZ4`), not
> whole files.
>
> Three things we found, in order of how useful they are to you:
>
> **1. Your scheme is a no-op on two big classes.** On GGUF (~29% of served
> weight bytes in the top-100) and on dataset parquet, bg4-lz4 lands at
> 1.000–1.002×. LZ4 can't find repeats inside quantized blocks or SNAPPY pages;
> an entropy coder still can: zstd-1 gets 1.13× on parquet, mzip 1.22×; on F16
> GGUF mzip gets 1.51× where you get 1.001×.
>
> **2. You have +6% for free, at the same speed.** Per chunk, pick the smallest
> of {lz4, bg4-lz4, zstd-1, bg4-zstd-1}: **+6.4%** over bg4-lz4 across 612 MB of
> real weights, and zstd-1 compresses as fast as lz4 (we measured 560–940 MB/s vs
> 620–800). No single arm is safe — bg4+zstd-1 wins on BF16 but *loses* 5% on some
> F32 regions, because zstd codes literals with one Huffman table per block. If
> you'd rather not touch the decoder at all: bg4+lz4hc-12 is +5.9% and emits
> standard LZ4 frames. zstd-19 is +14.5% at 7–15 MB/s. A per-dtype dictionary
> adds ~+3% and is dedup-compatible.
>
> **3. The real finding: on weights, the whole gain is order-0 entropy of the
> exponent byte — and you can have all of it at your speed.** Byte-group by dtype
> (stride 4 for F32, 2 for BF16/F16), Huffman/FSE-code each plane on its own
> table, pass incompressible planes through raw: **+17.9% over bg4-lz4** across
> 9 real inputs — **BF16 +23%, GGUF F16 +34%** — within 0.7% of the entropy bound.
> Byte-weighted with the reference huff0: **1.9–2.6 GB/s encode, 3.3–5.9 GB/s
> decode** — two to three times faster than bg4-lz4 to encode, because the
> incompressible planes are detected and passed through. That's a new
> `CompressionScheme` id, dedup-compatible. Your current bg4 is the right idea;
> lz4 just has no entropy coder to cash it in, and zstd's single Huffman table per
> block loses it when the planes are concatenated.
>
> The coder itself is Apache-2.0 (huff0 underneath, BSD), a standalone package with a byte-exact
> spec, a frozen reference implementation, 162k test assertions and a fuzzer, so it can become a
> new `CompressionScheme` id without a licensing conversation: [REPO — owner to decide]. Measured
> on its own at your chunk size it encodes at 372–715 MB/s and decodes at 0.9–2.7 GB/s against
> your 484–839 and 2.07–2.64 — the same speed class, 12–34% fewer bytes. mzip stays dual-licensed.
>
> mzip itself matches that ratio at your chunk size (±0.6%, seven models, every
> chunk verified, flat from 16 MiB to whole-file) — but at ~5,000× the compute.
> On weights the heavy machinery buys ≤0.6% more; where it earns its keep is text,
> code and dataset parquet (+13–29% over zstd-19, +18% over your scheme on
> parquet).
>
> The honest catch, measured on an idle box at your chunk size: mzip's ratio mode
> compresses at ~0.1 MB/s per core versus your ~700 — roughly 7,000× slower — so that
> mode is a cold-tier trade, not an upload-path one. Its decode is bimodal: ~160 MB/s
> library-level on F32/F16, ~9 MB/s on BF16 where BWT wins.
>
> So we built the fast path. A FAST mode with the per-plane block type above (planes coded with
> huff0, incompressible planes passed through raw), measured in-process on 64 KiB chunks against
> bg4-lz4 in the same harness: **9% (F32 class), 23% (BF16), 33% (GGUF F16) and 34% (dataset text)
> fewer bytes than what you store today**, every chunk round-trip verified. Compress runs at
> 240–315 MB/s per core on weights against your ~480–840 (lz4_flex, your release profile);
> decode at 1.5–2.0 GB/s against your 2.1–2.6. The reference huff0 coder alone does 2–2.6 GB/s encode and 3.3–5.9 GB/s decode on these
> planes, so the remaining gap is our framing and gather/scatter, not the coder. One honest loss:
> on an embedding table full of exact repeats (bge-small) your LZ4 beats an order-0 coder (1.59× vs
> 1.48×); our ratio mode gets 2.35× there. On GGUF quantised tensors, where you store raw, we take
> 2.4%; on parquet, 13%.

> Also, two findings for us, not you: mzip's F32 weakness at 64K turned out to be
> a proxy-gating bug in our own SoA transform — fixed, +8% on F32 chunks, format
> unchanged. And a 16 KB dictionary trained on Hub config/tokenizer JSON takes
> mzip another 29% down on those files (144 held out) — the same files every
> model repo ships.

## Draft C — the derivative angle (reproduction test PASSED on all seven quant types; numbers final)

> One more thing, and it is bigger than chunk compression. Your dedup is content-addressed, so a
> fine-tune — every weight nudged — shares zero chunks with its base: we measured 0 of 128 on
> every pair we tried. But coded as a delta against the base, Qwen2.5 instruct weights cost 46–56%
> of what they cost alone, Llama-3.1-8B 58–67% (27 tensor-aligned pairs, all verified). And a
> community Q8_0 GGUF regenerates *bit-exactly* from the BF16 safetensors — all 4.2M quants — so
> given the base it costs a recipe and nothing else. One popular base has over 2 TB of GGUF
> derivatives in its top sixty quantizer repos alone. Content addressing can't see any of this;
> a base reference in the chunk record can.

## Draft B — short

> We measured mzip against Xet's actual `ByteGrouping4LZ4`, at your real 64 KiB
> chunk granularity, on 7 real models: **+17.5% smaller**, +3.6% over your best
> stock option (bg4+zstd-19). Every chunk round-trip verified.
>
> Two things that are yours regardless: your scheme is ~1.000× on GGUF and on
> dataset parquet (LZ4 finds nothing there; any entropy coder does), and
> lz4hc-12 gives you +6% today with zero decoder change.
>
> Catch: ratio mode is ~7,000× slower to compress than lz4 (cold tier). Our FAST mode stores
> 9–33% fewer weight bytes and 34% fewer dataset-text bytes than your Auto scheme at 240–315 MB/s
> compress and 1.5–2.0 GB/s decode per core (in-process, 64 KiB chunks, your real xet-core code).

## Numbers behind every claim (all in /root/mzip-hfbench/MZIP-AT-HF-GRANULARITY.txt)
- 64K aggregate, 7 slices: HF 50,863,964 | bg4+zstd-19 43,512,873 | mzip(native SoA fix) 41,962,356
- GGUF @64K: Q4_K_M 1.000/1.002; Q8_0 1.000/1.043; F16 1.001 vs mzip 1.452 (1.508 w/ bg4)
- parquet @64K: HF 1.0022x, zstd-19 1.1403x, mzip 1.2218x
- lz4hc-12 @64K: +6.12% ; zstd-1 +6.19% ; zstd-19 +14.45% ; dict +2.93%
- served-mix: 15.6% central (10.5–17.8% by dtype mix), dedup excluded

## What must NOT be said
- HF text ratio from the always-bg4-lz4 harness (1.10–1.62x): Xet Auto uses plain LZ4 on text (1.6–2.5x). Use RESULT 36.
- "15.84%" (whole-file; not the drop-in comparison)
- "saves 530 TB/week" (assumes all-weights, ignores dedup)
- "mzip is 24x slower to decompress" as a universal (bimodal; F32/MS path is faster than HF)
