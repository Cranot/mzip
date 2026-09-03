# mzip vs HuggingFace Xet: where we are, and what is left on the table

Written 2026-09-02 from the measurements in `MZIP-AT-HF-GRANULARITY.txt` (RESULT 20–41) and the ten
scout investigations under `agents/<lens>/`. Nothing here has been posted, committed, or shipped.

**Evidence grades.** `[M]` = measured in this session and re-run or cross-checked by a second
mechanism. `[I]` = measured by one scout, artifacts on the box, **not independently re-run**: the
adversarial refutation pass was launched three times and died on API limits every time, so every
scout lever below is `[I]` unless this session re-measured it directly. `[H]` = hypothesis, not
measured. Treat `[I]` as a claim with a reproduction recipe, not as a fact.

---

## 1. Where we are, all axes

64 KiB chunks, in-process, single thread, every chunk round-trip verified, shared 12-core box
(load stated per run; absolute MB/s vary up to 40% by core and neighbour, ratios do not).

**HF = real Xet Auto** `[M]`: xet-core's own `compression_scheme.rs` + `bg4_prediction.rs` compiled
verbatim against `lz4_flex 0.13.0`, and independently re-implemented in C; the two agree on 21/21
files. Auto picks byte-grouped LZ4 on 100% of weight chunks, raw on 98–100% of GGUF and parquet
chunks, and **plain LZ4 on 100% of text and 97% of small JSON**.

| class | share of weight bytes | HF ratio | mzip FAST | mzip BALANCED | FAST vs HF bytes |
|---|---:|---:|---:|---:|---:|
| F32 safetensors (5 files) | 65.5% (with BF16) | 1.142 | **1.366** | 1.367 | −16.4% |
| BF16 safetensors (2) | " | 1.147 | **1.489** | 1.492 | −23.0% |
| GGUF F16 (1) | 28.8% (all GGUF) | 1.001 (raw) | **1.501** | 1.508 | −33.3% |
| GGUF quantised Q8_0/Q6_K/Q4_K_M (3) | " | 1.000 (raw) | 1.025 | 1.026 | −2.4% |
| parquet (1) | dataset side | 1.001 (raw) | 1.131 | 1.222 | −11.4% |
| dataset text (8) | dataset side | 1.936 | **2.900** | 4.021 | −33.9% |
| repo JSON, 290 files whole | metadata | 4.65 | 10.57 | 14.21 | −56% |

Speed, byte-weighted per class, same harness:

| | HF compress | FAST compress | HF decode | FAST decode | BALANCED |
|---|---:|---:|---:|---:|---|
| F32 | 691 MB/s | 244 | 2,445 MB/s | 1,755 | 0.05 MB/s compress |
| BF16 | 496 | 306 | 2,101 | 1,663 | " |
| GGUF F16 | 1,892 | 252 | 16,990 (memcpy) | 1,069 | " |
| text | 390 | 192 | 2,334 | 1,190 | " |

Read it as: **on weights mzip FAST stores 16–33% fewer bytes than HuggingFace stores today, while
compressing 1.6–2.8× slower and decoding 1.3–1.4× slower than they do.** That speed gap is mzip's
framing, not the coder: measured head-to-head on one core (RESULT 42, and section 1b below) the
standalone coder is **faster than Xet on all three axes on every safetensors file**. Those two gaps are what
Tier 1 closes, and it is now done for the coder itself: measured standalone it encodes at
372–715 MB/s against their 484–839 and decodes at 0.9–2.7 GB/s against their 2.07–2.64, at the
same bytes. What remains between that and the table above is mzip's own framing around the coder. BALANCED is the cold-tier ratio mode and is not an upload-path option at
0.05 MB/s.

Two corrections this session made to its own earlier numbers, both against us:
* the text baseline. An always-byte-grouped LZ4 baseline understated HF on text by 57%; real Auto
  uses plain LZ4 there. FAST's text advantage is 34%, not 57%.
* the "one FAST loss". bge-small was never LZ4 beating an entropy coder: that slice is an F16
  checkpoint upcast to F32, so one byte plane is constant zero, and our coder stored constant
  planes raw because it required huff0's return to exceed 1 (huff0 returns exactly 1 for a
  single-symbol block). One character. bge-small went 1.480 → 2.350 `[M]`, F32 class −9.4% → −16.4%.

### 1b. The coder alone, head-to-head

Both arms alternating in one loop on one core, load 0.80, 64 KiB chunks, 5 reps:

| file | Xet ratio | PE ratio | size | Xet enc | PE enc | compress | Xet dec | PE dec | decompress |
|---|---:|---:|---|---:|---:|---|---:|---:|---|
| bert-base F32 | 1.0722 | 1.2097 | WIN -11.4% bytes | 790 | 1205 | WIN 1.52x | 2717 | 4136 | WIN 1.52x |
| bge-small F32 | 1.5445 | 2.3501 | WIN -34.3% bytes | 520 | 1199 | WIN 2.31x | 2390 | 2464 | WIN 1.03x |
| gpt2 F32 | 1.0351 | 1.1908 | WIN -13.1% bytes | 786 | 1128 | WIN 1.43x | 2467 | 3617 | WIN 1.47x |
| MiniLM F32 | 1.0529 | 1.1982 | WIN -12.1% bytes | 804 | 1135 | WIN 1.41x | 2640 | 3896 | WIN 1.48x |
| weights-fp32 F32 | 1.1347 | 1.3619 | WIN -16.7% bytes | 659 | 1062 | WIN 1.61x | 2264 | 2856 | WIN 1.26x |
| weights-bf16 BF16 | 1.1349 | 1.4821 | WIN -23.4% bytes | 472 | 1063 | WIN 2.25x | 1955 | 2415 | WIN 1.24x |
| Qwen2.5 BF16 | 1.1583 | 1.4954 | WIN -22.5% bytes | 510 | 1186 | WIN 2.33x | 2300 | 2778 | WIN 1.21x |
| GGUF F16 | 1.0006 | 1.5012 | WIN -33.3% bytes | 2018 | 880 | LOSS 2.29x slower | 22422 | 1416 | LOSS 15.84x slower |
| GGUF Q8_0 | 1.0002 | 1.0423 | WIN -4.0% bytes | 2071 | 721 | LOSS 2.87x slower | 21607 | 1023 | LOSS 21.11x slower |
| GGUF Q6_K | 1.0000 | 1.0285 | WIN -2.8% bytes | 2102 | 704 | LOSS 2.99x slower | 24090 | 1024 | LOSS 23.53x slower |
| GGUF Q4_K_M | 1.0002 | 1.0041 | WIN -0.4% bytes | 2070 | 858 | LOSS 2.41x slower | 22380 | 1519 | LOSS 14.74x slower |

Seven of seven safetensors files: fewer bytes *and* faster both ways. GGUF is a trade — Xet stores
it raw, so its speed there is memcpy and any compression costs time.

---

## 2. Levers, ranked

Rank = measured gain × share of bytes it applies to × inverse cost. Numbers are the scouts';
grade in brackets.

### Tier 1 — byte-neutral speed. No format change, prototypes exist, output byte-identical.

1. **Fused histogram + huff0 CTable API + no per-plane allocation** and
2. **joint-loop plane interleave + single output allocation + stored planes read in place**
   — **DONE `[M]`**, ported into the `plane-entropy` package and re-measured by this session in an
   interleaved same-core A/B against the frozen reference implementation: **encode 1.44–2.42×**
   (BF16 640 MB/s, MiniLM 710, bge-small 675, GGUF F16 374), **decode 1.39–2.15×** (BF16 1,800,
   MiniLM 2,660, bge-small 1,580, GGUF F16 920), **bytes identical on every input** — asserted by
   the test suite on every generator and size, by the 144 frozen stream vectors, by a build with
   SIMD compiled out, and against mzip's own coder on 24,288 chunks. This closes most of the speed
   gap: the coder now matches or beats Xet Auto's encode speed and is within 0.85× on decode.
3. **zstd's BMI2-assembly huff0 decoder** (identical bitstream, different implementation) `[I]`, still open:
   a further **+26–29%**: bf16 3,346 MB/s, MiniLM 5,057. That **matches HF's decode speed in the
   same harness** (3,252 / 5,108) while storing 23% / 16% fewer bytes. Cost: a dependency choice,
   zstd's huff0 rather than the reference one; the format does not change.
   After 1–3 the profile is 75–92% inside huff0 itself, i.e. the coder is at its own ceiling.

### Tier 2 — ratio, encoder policy only, already applied

4. **Constant-plane RLE** `[M]` — applied (v10). F32 class −9.4% → −16.4% vs HF.
5. **Miller-Madow bias correction in the plane-count rule** `[M]` — applied. Wrong plane-count
   picks at 4 KiB chunks: 1,897/16,384 → 3. No change at 16 KiB and above.

### Tier 3 — ratio, needs a format revision

6. **rol-1 bit-aligned exponent plane** `[I]`: rotate each element left one bit before splitting, so
   sign and all eight exponent bits share a byte. −1.10% bf16, −0.81% Qwen, −0.34% MiniLM/gpt2,
   −0.74% bge-small, −0.55% wfp32, −0.06% GGUF F16, and **+0.32% loss on bert-base** — with a
   sign-entropy guard (rotate only when the sign bit is near-random) it never loses on the corpus.
   Applies to the 65.5% safetensors share. Verdict: worth a v2 field, worth ~0.5% of weight bytes.
7. **GGUF quantised field split** `[I]`: parse ggml block layouts, code each field order-0. Q8_0
   1.042 → 1.059, Q6_K 1.029 → 1.051, Q4_K 1.023 → 1.051, Q5_0 1.004 → 1.065; 4.8–6.1% below what
   HF stores, and 5–6× faster to compress than our current path. Applies to most of the 28.8% GGUF
   share. **Precondition**: block type and phase must be detected without the file header —
   self-detection was exact on 128/128 chunks for Q8_0 and Q6_K but only 74/128 for Q4_K_M.
8. **Previous-row context on the exponent plane** `[I]`: −0.70% on one F32 file, −0.27% on another,
   ≈0 on the rest. Only worth it if a v2 already exists.

### Tier 4 — other data classes, outside the plane coder

9. **Parquet page transcode** `[I]`: decode each SNAPPY page, recompress, re-encode on read.
   wikitext-2 −35.2% vs our FAST and −42.6% vs what HF stores; gsm8k −36.0%. Risk: exact SNAPPY
   re-encoding depends on the writer generation, so it needs a writer fingerprint gate and a
   fallback. The Hub's writer mix is **unknown**.
10. **Text: stop detouring** `[I]`. Plain zstd-3 beats mzip's FAST text path by 0.87% in bytes *and*
    is 1.3× faster both ways; a mis-route of length-prefixed string chunks costs 3.0% on one corpus
    and 9.8% on another.
11. **Small repo JSON** `[I]`: zstd with a domain dictionary reaches 12.4× at roughly 4,000× FAST's
    current speed there, and zstd-6..9 with the dictionary reaches BALANCED's 14.2× at 118–204 MB/s
    versus BALANCED's 0.03. This is the single largest speed-for-ratio win in the whole programme.

---

## 3. Measured null results — do not retry

* Skipping huff0 on planes predicted incompressible: **0%**. huff0 already bails before building a
  table; the cost that hypothesis targeted is the histogram, which Tier 1.1 removes. `[I]`
* Order-1 modelling inside the exponent plane: every real coder got **larger** (+1.3% to +17%). `[I]`
* Delta or XOR of the high byte between adjacent elements: **+12% to +34% larger**. `[I]`
* Per-chunk choice between the plane coder and byte-grouped zstd: **0 bytes** on 6 of 7 slices,
  0.038% on the seventh — and that is the oracle, so no proxy can beat it. `[I]`
* LZ4 or zstd on individual planes (including the exponent plane alone): **≤0.22%**. `[I]`
* Per-chunk text-type sniffing to pick a level or dictionary: no gain at any budget. `[I]`
* A 9-bit joint sign+exponent symbol instead of rol-1: noise, both directions. `[I]`
* Cross-plane conditioning beyond rol-1: bounded at 0.13–0.49%, needs a conditioned coder. `[I]`

---

## 4. Unknowns, carried

* **Every scout lever is unrefuted.** The refutation pass never ran. Before anything from Tier 1,
  3 or 4 lands, re-run its load-bearing measurement independently.
* Absolute throughput on this box moves ±40% with the core and its hyperthread sibling. Only
  interleaved same-core A/B numbers are comparable; the tables above name their load.
* The Hub's real mix: quant-type shares within GGUF, parquet writer generations, and the share of
  bytes that dedup already removes before compression ever runs. All unmeasured, and the last one
  bounds every "PB saved" claim.
* Whether Xet's chunk hash is over uncompressed bytes (assumed, so a new scheme is dedup-neutral)
  — read from the code, not tested against a live store.
* GGUF quantised block detection without headers: 74/128 chunks for Q4_K_M.
* mzip's own BALANCED mode still holds a 0.2–0.4% residual over FAST on weights that nothing
  cheap has recovered.

---

## 5. What to build next, in order

1. ~~Finish the permissive package~~ **DONE**: `plane-entropy/` (Apache-2.0) carries the optimised
   coder, the frozen reference, the byte-exact spec, 162k assertions, a fuzzer and frozen vectors.
   Remaining: a quiet-box absolute benchmark (the current one ran at load 9.5), a name, the repo
   decision, and an adversarial review before publication.
2. **Decide on Tier 1.3** (zstd's assembly huff0). It is the difference between "1.2× slower to
   decode than HF" and "the same speed as HF". It is a dependency decision, not a format one.
3. **Re-measure the whole table on a quiet box** with the v10 fixes folded into mzip, and refresh
   the post draft. The current table still carries v9.1 numbers for several rows.
4. **Independently refute** Tier 3.6 (rol-1) and Tier 3.7 (GGUF fields) before designing a v2
   format. Both are real gains but both change the stream.
5. **Text and small-JSON policy** (Tier 4.10, 4.11): pure wins, no format change, and they remove
   the most embarrassing numbers in the table (0.17 MB/s on small files).
6. **Parquet** (Tier 4.9) last: the largest single-class gain, the largest correctness risk.

---

## 6. Reproduction

Everything is under `/root/mzip-hfbench`: `MZIP-AT-HF-GRANULARITY.txt` (RESULT 20–40, the master
record), `tbl_vs_real_xet_v91.md` (the table above), `agents/<lens>/` (each scout's scripts, trees,
logs), `plane-entropy/` (the Apache-2.0 package, its spec, tests, fuzzer and vectors),
`petree_v10/` (mzip with v10.2 applied), `build/` (the benchmark binaries).
