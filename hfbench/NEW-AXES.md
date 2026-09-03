# Beyond the chunk: two new axes against what HuggingFace stores

Written 2026-09-02. Every number below is measured on real Hub files on the same box as the rest
of the programme; grades: `[M]` measured and cross-checked, `[I]` measured once, `[H]` not measured.
Master record: `MZIP-AT-HF-GRANULARITY.txt` RESULT 49–58. Scripts: `pair_fetch.py`, `pair_fetch2.py`,
`delta_all.py`, `delta_coder_gap.py`, `gguf_given_base.py`, `official_diag.py`, `quant_bytes.py`.

## Why look beyond the chunk

Per-chunk lossless coding of weights is finished business: the plane coder sits within ~1% of the
order-0 ceiling, the remaining per-chunk levers (bit-aligned exponent plane, base-free context)
are worth 0.3–1.3% (RESULT 50), and everything Xet stores raw is a memcpy no coder can out-run.
The Hub's volume, however, is not base models. It is DERIVATIVES, and content-addressed dedup, the
mechanism Xet is built on, cannot see them:

* the Hub API returns ≥1,000 fine-tunes (its query cap) for each of Qwen2.5-7B-Instruct,
  Llama-3.1-8B-Instruct, Llama-3.2-1B-Instruct, Qwen2.5-1.5B-Instruct and gemma-2-2b-it, plus
  196–908 quantized variants each `[M]`;
* between a base and its fine-tune, identical 64 KiB chunks: **0 of 128** on every pair tested;
  identical 4 KiB pages: 0 of 2048 `[M]`. Every weight moves, so exact-chunk dedup gets nothing.

## Axis 1: a fine-tune given its base `[M]`

Same tensor, same element offset, base and fine-tune, 27 pairs (5 model pairs × up to 6 tensor
types). Code the signed difference of the 16-bit patterns (zigzag) as byte planes with FSE at
64 KiB blocks; every plane decoded and byte-compared.

| pair | tensors | fine-tune alone | as delta vs base | the fine-tune then costs |
|---|---:|---:|---:|---:|
| Qwen2.5-1.5B → Instruct | 4 | 1.49× | **3.07–3.25×** | **46–49%** of today |
| Qwen2.5-7B → Instruct | 6 | 1.49× | **2.65–3.02×** | **49–56%** |
| Llama-3.1-8B → Instruct | 6 | 1.49× | **2.25–2.59×** | **58–67%** |
| Llama-3.2-1B → Instruct | 3 | 1.49× | 1.84–1.93× | 77–81% |
| SmolLM2-135M → Instruct | 1 | 1.48× | 1.91× | 78% |
| Qwen2.5-7B-Instruct → Coder-7B-Instruct | 6 | 1.49× | 1.46–1.50× | **100–102% (no gain)** |

What the numbers say:
* the gain is a property of the pair, not the tensor: within a pair all tensor types land within
  7% of each other; it tracks how much of the sign+exponent byte the fine-tune preserved (96% for
  Qwen instruct, 89–93% for Llama-3.1, 75–79% for Llama-3.2, 23–34% for Coder);
* an LZ "reference" scheme does not work: zstd-19 with the base chunk as dictionary, or whole-file
  `--patch-from`, reaches only 1.28–1.46×, because LZ needs exact repeats (1–15% of elements) while
  70–98% of elements sit within four ULPs and only an entropy coder on the difference sees that;
* the coder matters: huff0's one-bit floor loses 15–20% on a plane that is 75% zeros; FSE lands
  within 0.7% of the bound; zigzag beats XOR by 7–15%;
* the negative result is the design constraint: the Coder model is a continued-pretrain, not a
  weight-space fine-tune, and coding it against its nominal base is 0–2% *worse* than coding it
  alone. The Hub's `base_model` tag does not imply weight proximity. A store must test the
  candidate base on a sample (same-high-byte fraction ≥75% pays, ~25% does not) and fall back.

The community population `[M]`: the eight most-downloaded third-party fine-tunes of
Qwen2.5-1.5B-Instruct with BF16 weights, same method:

| fine-tune | kind (from its card) | delta vs base | costs |
|---|---|---:|---:|
| xiamoent/Agent-G2-webshop-1.5b | full | 7.61× | **20%** of today |
| SZLHOLDINGS/SZL-Khipu-1.5B | LoRA merge | 3.35× | 44% |
| SZLHOLDINGS/SZL-Forge-1.5B-ReceiptAgent | LoRA merge | 2.83× | 53% |
| xjtupanda/HawkVL-2B | full | 2.36× | 63% |
| Efficient-Large-Model/Fast_dLLM_v2_1.5B | full | 2.19× | 68% |
| SakanaAI/TinySwallow-1.5B-Instruct | full | 1.80× | 83% |
| Sedibaai/SedibaLM | LoRA merge | 1.75× | 85% |
| unsloth/Qwen2.5-1.5B-Instruct (62k downloads) | mirror | byte-identical | 0% (dedup already catches this one) |

Light-touch task fine-tunes and LoRA merges cost 20–53% of today; the gate statistic predicts the
gain monotonically across all eight. LoRA alone does not guarantee a small delta.

**Correction `[M]`.** The static numbers above are 0.2–1.1% too high: the measurement script's zigzag
was not invertible for differences beyond ±32767. Lossless static FSE over all 26 pairs is 2.127×
harmonic (not 2.151×); the Coder-on-Instruct pair is 1–3% *worse* than coding alone once lossless.
Every ratio below is on the lossless symbols.

**The adaptive coder `[M]`** (`convert/delta-coder/`, 388 lines, libc only, chunk-independent,
verified by 26/26 file round-trips and a sanitizer cross-build that caught and fixed a real defect):
context = the base's exponent byte, no transmitted tables. Harmonic over 26 pairs 2.274× against
2.127× static, 98.9% of the measured ceiling. Qwen2.5-1.5B fine-tunes reach **3.49–3.72×**
(+14% over static), Qwen2.5-7B +11–13%, Llama-3.1-8B +8–10%. Speed 33–57 MB/s in this prototype
(a binary range coder; a multi-symbol variant would be faster by an unmeasured factor). Cost: ~52 B
of base reference per chunk and a 64 KiB base read per decoded chunk.

## Axis 2: a quantized GGUF given its base — costs nothing `[M] whole model, all seven types`

GGUF is 28.8% of the weight bytes the Hub serves. A Q8_0 block is one fp16 scale and 32 int8
quants that llama.cpp derives deterministically from the base weights. Re-running ggml's
`quantize_row_q8_0_reference` in exact float32 on the BF16 base tensor and comparing with the
published file, on 131,072 blocks / 4,194,304 quants of `blk.9.ffn_up.weight`:

| published Q8_0 of Qwen2.5-1.5B-Instruct | scales match | quants match | residual |
|---|---:|---:|---|
| bartowski/Qwen2.5-1.5B-Instruct-GGUF | **100.000%** | **100.0000%** | **empty** — costs 0 B given the base |
| Qwen/Qwen2.5-1.5B-Instruct-GGUF (official) | 0.275% | 35.85% | quantized from other weights |

The official mismatch is the weights, not the quantizer `[M]` (`convert/quant-repro/REPORT.md`
section 4): the official FP16 GGUF holds a function-preserving per-channel rescaling of the public
weights (attention columns scaled by the ratio of the norm gains, reciprocal row/column scales on the
MLP pairs, agreement to one F16 ulp) plus an untied `output.weight`; only 0.2% of matrix elements
equal the safetensors. Those scale vectors are not on the Hub, so the safetensors is not the base of
those files. Their base is the FP16 GGUF in the same repo (below). A recipe must therefore name the
base *blob and revision*, not the model.

The scale of the class `[M]` (Hub API, top-60 quantized-derivative repos by downloads):

| base | quantized repos | GGUF in the top 60 | GGUF share of their bytes | projected over all repos | base size |
|---|---:|---:|---:|---:|---:|
| Qwen2.5-7B-Instruct | 408 | 2.2 TB (439 files) | 90.6% | ~15 TB | 15.2 GB |
| Llama-3.1-8B-Instruct | 908 | 2.0 TB (357 files) | 88.4% | ~30 TB | 16.1 GB |
| Qwen2.5-1.5B-Instruct | 271 | 383 GB (354 files) | 84.0% | ~1.7 TB | 3.1 GB |

Calibration-dependent formats (GPTQ/AWQ/bnb), which cannot be regenerated without their
calibration data, are 0.0–0.4% of those bytes. The projection is linear and overstates (the top
60 are the heaviest uploaders); the sampled figures are hard counts: ≥2 TB of GGUF per popular
base, stored today as bytes, derivable from a 15 GB file.

**Whole-model, all types `[M]`** (`convert/quant-repro/`, llama.cpp built on the box, the full
3.1 GB safetensors converted and quantized, tensor-by-tensor byte comparison against every file
bartowski published for this model): **all seven quant types reproduce bit-exactly** — Q8_0, Q4_0,
Q4_K_M, Q5_K_M, Q6_K, Q3_K_M, Q2_K, each 338/338 tensors and 100.0000% of tensor-data bytes — under
one four-item recipe: llama.cpp release b3772, the F16 intermediate, his published imatrix, one
thread. Each item was shown necessary by the full variant matrix (RESULT 59 addendum 2): HEAD
reproduces only Q8_0 whatever the intermediate; the same b3772 source built with FP contraction off
falls to the 141–143 floor on every non-Q8 type and its Q4_0 is byte-identical to HEAD's, so the
build flags (release defaults, FMA contraction on) are part of the recipe; a BF16 intermediate costs
0 tensors on Q6_K, 3 on Q4_0, 29–40 on Q2_K/Q3_K_M and 94–101 on Q4_K_M/Q5_K_M; the imatrix moves
193–196 weight matrices. 141–143 tensors (norms, biases, small tensors kept at F32/F16) are invariant
to everything and are the floor, not a partial match. Read-time cost is the quantizer itself:
8–83 s per type for a 1.5B model on one core.

**The official repos `[M]`.** The official Qwen GGUFs derive from the official FP16 GGUF that sits in
the same repo: with HEAD, Q8_0 is 339/339 tensors bit-exact and Q4_K_M is 307/339 with every
same-typed tensor byte-identical (the 32 that differ differ only in the type the newer quantizer
assigns). The "provenance mismatch" against the safetensors is therefore closed: for those repos the
base is a sibling file, and the recipe needs the quantizer version only for the type table.

## Axis 3: dataset parquet, page transcode `[M]` (landed, restore-verified)

Xet stores parquet raw (1.005×) and mzip FAST gets 1.13× by zstd over the SNAPPY pages. Decoding each
SNAPPY page, storing the payload as zstd, and re-encoding with the recorded snappy generation on read:

| 7 files, 369 MB, 6,266 pages | Xet | mzip FAST | transcode zstd-3 | transcode zstd-19 |
|---|---:|---:|---:|---:|
| aggregate ratio | 1.005 | 1.130 | **1.511** | **1.749** |
| vs what Xet stores | — | −11% | −30 to −36% | **−37 to −46%** |

All seven restore byte-identically (full compare plus sha256, three reps, zero fallback pages); the
comparator was falsified in both directions. The writer fingerprint is real and has two axes, snappy
version generation and build flags, both of which change roughly half the pages; the container records
the generation and falls back per page when re-encoding is not exact, so it is lossless for any
writer. It is a whole-file, at-rest transform: 97–100% of 64 KiB chunk boundaries fall inside a page.
The Python tool restores at 92–107 MB/s; the Hub's writer mix is unknown.

## What a store would do with this

* **Chunk record gains a reference**: `(scheme, base chunk id, payload)`. The chunk hash stays over
  the uncompressed bytes, so dedup is untouched; the base's chunks are already in the store.
* **Alignment metadata**: tensor name → base tensor, from the safetensors headers both files
  already carry; ~30 bytes per chunk.
* **A gate, not a tag**: sample the candidate base; code as delta only when the sampled
  same-high-byte fraction clears the threshold. Cost of a wrong guess is bounded at +2%.
* **Recipes for GGUF**: `(base id + revision, tensor, quant type, quantizer version)` plus a
  residual that is empty when the recipe reproduces; regenerate at read time. Read cost = running
  the quantizer, which is being timed.
* **Accounting**: these are storage wins, not upload-bandwidth wins. The uploader still sends the
  bytes; the store stops keeping them.

## Unknowns, named

* Byte share of the Hub's uploads that are fine-tunes vs bases vs quantizations: the API gives
  counts and sampled sizes, not upload volume.
* Which quantizer release the official Qwen K-quants were made with (32 of 339 tensors differ in
  assigned type under HEAD; the matching release was not searched), and the release mix across other
  quantizer accounts (one account, one model, seven types measured).
* Three of the top community fine-tunes re-upload the architecture as F32 or F16 (dtype-widened
  copies); a delta in the base dtype after an exact-widening check would cover them — not measured.
* The adaptive delta coder's speed on a quiet core and with a multi-symbol (rANS) coder; the ratio is measured.
* All speeds for the delta path: the FSE numbers are the CLI's; a production coder was not timed.
