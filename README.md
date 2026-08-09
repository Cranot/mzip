# mzip

**Detection-based compression that finds patterns zstd, brotli, and bzip2 miss.**

> *Store the formula, not the data.*

## TL;DR

A C++17 single-header library that detects mathematical structure and per-file patterns before reaching for an entropy coder. On data that has structure (numeric sequences, templates, columns, prose, audio, gradients, logs) it produces meaningfully smaller output than zstd:19, brotli:11, bzip2:9, xz:9, 7z, and rar. On generic small source code it is competitive but rarely beats brotli. Every output is round-trip-verified before the encoder commits to it.

**2026 update:** mzip now ships a **context-mixing (bzip3-class) entropy backend**, a **PPMd var.H backstop** (source/prose), an **xz/brotli ensemble backstop**, and structural encoders for numeric/tabular/SQL/log/**YAML**/**FASTQ**/**x86+ARM+PPC+RISC-V executables** (all trial-and-keep, never regress). On a held-out, type-stratified benchmark spanning **48 content types / 92 files** it is the overall ratio leader with **0 losses across 84 real files**. See [Primary Benchmark](#primary-benchmark-held-out-type-stratified).

| Benchmark | Result | Notes |
|---|---|---|
| **Held-out, type-stratified** (48 types, 84 real files) | **68 wins · 16 ties · 0 losses** — overall **7.27× ratio** | The fair, current headline. vs brotli 4.96× (+31.7%), xz 5.18× (+28.8%), zstd-22 4.47× (+38.5%). Real GitHub source, scientific time-series, k8s/CRD YAML, FASTQ genomics, ARM/PPC/RISC-V + x86 binaries, WASM, minified web, syslog — each vs gzip/bzip2/zstd-19/zstd-22/xz-9e/brotli-11 at max. All roundtrip-verified. |
| 250 synthetic tests (50 types × 5 sizes) | 235 / 250 (94.0%), avg **8.26×** | Formula-friendly suite (seeded generators) — skews high vs real data. Top ratio among the 8 compressors tested. |
| enwik9 10 MB Wikipedia prose | **2,671,197 bytes** | Beats brotli:11 by 5.9%, bzip2:9 by 14.4% — smallest of any standard library compressor here. |

**Sections:** [Why](#why-mzip) · [Strengths & limits](#key-strengths) · [Primary benchmark](#primary-benchmark-held-out-type-stratified) · [Synthetic suite](#benchmark-results) · [enwik9 prose](#long-form-prose-enwik9) · [Real-world files](#real-world-file-benchmark) · [Strategies](#compression-strategies) · [Quick start](#quick-start)

---

## Why mzip?

Most compressors treat all bytes as random and rely on LZ77 + entropy coding. Real data is rarely random — it has structure that LZ77 cannot reach: a sequential-ID column is a formula `v[i] = a + b·i`, a JSON API response is a template with variables, an audio waveform is a smooth function. mzip detects that structure first and substitutes the *minimal description* before the entropy coder ever runs.

A few representative wins (1 MB inputs, vs the best of zstd:19 / brotli:11 / bzip2:9 / xz:9 / 7z / rar):

| Input pattern | mzip output | Best other | Advantage |
|---|---|---|---|
| Sequential database IDs | **32 B** | bzip2: 3.4 KB | **106× smaller** |
| Repeating JSON API templates | **10 KB** | brotli: 49 KB | **4.9× smaller** |
| 16-bit audio PCM | **1.7 KB** | bzip2: 4.0 KB | **2.4× smaller** |
| RGB image gradient | **124 B** | brotli: 397 B | **3.2× smaller** |
| 10 MB enwik9 Wikipedia prose | **2.67 MB** | brotli:11: 2.83 MB | **5.9% smaller** |

The first four wins come from formula or template detection — algorithmic substitutions LZ77 has no path to. The fifth win comes from a tuned BWT pipeline (capfold + word dict + LZP-after-dict + multi-tree Huffman with per-block dynamic trial) that puts mzip at the top of standard library compressors on long-form English prose.

---

## Key Strengths

> **mzip is a specialist, not a general archiver.** On data that has structure — numeric sequences, templates, columns, prose, audio, gradients, logs, SQL, XML, K8s — it produces meaningfully smaller output than zstd:19, brotli:11, bzip2:9, xz:9, 7z, or rar. On generic small handwritten source code (a few KB of TypeScript, Markdown, or Python) brotli's 120 KB static dictionary still usually wins. The 94.0% synthetic win rate (formula-friendly suite) and 38.3% real-world win rate (47-file GitHub corpus) are both honest measurements of those two regimes. Read both, then pick the tool for the data you actually have.

### When to use mzip

- Long-form English text (≥ 1 MB): prose, books, email archives, Wikipedia-class content. mzip beats brotli:11 by 5.9% on enwik9 10 MB.
- Generated / templated data: K8s manifests, OpenAPI output, log streams, repeating JSON, SQL INSERTs, generated docs.
- Numeric arrays and time series: sequential IDs, timestamps, counters, sensor data, audio PCM, GPS coordinates, float arrays.
- Stream-separable formats: CSV, fixed-width logs, HTML, large XML, JSON Lines.
- Anything ≥ 256 KB where compression ratio is the budget you care about.

### When *not* to use mzip

- Latency-bound read paths: mzip decompresses ~38× slower than zstd. If your hot path opens compressed files repeatedly, use zstd.
- Small handwritten source code (a few KB of code/config/markdown): brotli usually wins by 3–20%.
- Already-compressed or encrypted data: nothing to detect, RAW path falls back to zstd:19 — just use zstd directly.
- General folder backup / archival: ZPAQ, 7z LZMA2, and xz are mature CLI archivers with broader format support. mzip is a library, not an archive format.

### Where mzip wins big

| Data category | Synthetic win rate | Why |
|---|---|---|
| Numeric sequences (IDs, timestamps, counters) | 100% (40/40) | Formula compression: `v[i] = a + b·i` beats any LZ77 |
| Binary / audio / sensor | 100% (20/20) | Delta + ALP for floats, Paeth/E8E9 predictors |
| Long-form text (prose / markdown) | 100% (15/15) | BWT pipeline + capfold + word dict + LZP-after-dict |
| Code (JS, Py, Go, Rust, ...) at scale | 94.5% (52/55) | Identifier-stream separation, BWT, ZSTD_DICT trial |
| Logs (access / syslog / JSON log) | 80–96% | Columnar separation + BWT per column |
| Anything ≥ 256 KB | 97% (97/100) | More data → more patterns to detect |

### Where mzip loses

| Scenario | Winner | Why |
|---|---|---|
| Small handwritten source code (4–30 KB) | brotli:11 | brotli ships a 120 KB pre-built English/web dictionary; per-file approaches can't fully match it without shipping their own |
| Random / encrypted / already-compressed data | zstd | No patterns to detect — entropy coder is all that's left |
| Decompression speed | zstd | mzip is ~38× slower to decompress (BWT vs LZ77 inverse). Trade-off, not a bug. See [Decompression Speed (the trade-off)](#decompression-speed-the-trade-off). |

---

## Primary Benchmark (held-out, type-stratified)

**This is mzip's fairest real-world measure and the current source of truth.** `benchmark_types.py` compresses **48 content types / 92 files** — 84 real files (real GitHub source in 15+ languages, real scientific time-series, k8s/CRD YAML, FASTQ genomics, x86 + ARM/PPC/RISC-V executables, WASM, minified JS/CSS + source-maps, fetched `proto`/`rst`/`tsv`/`svg`/`ndjson`/`diff`) plus 8 labeled synthetic — against **every standard at max settings** (gzip-9, bzip2-9, zstd-19, zstd-22, xz-9e, brotli-11). Every mzip output is roundtrip-verified (0 failures). Reproduce: `bash build_evals.sh && python3 benchmark_types.py`. **Full per-type table, methodology, and exact tool versions: [EVALS.md](EVALS.md).**

| Real files (overall) | mzip+CM | brotli-11 | xz-9e | zstd-22 | bzip2-9 | gzip-9 |
|---|--:|--:|--:|--:|--:|--:|
| **Compression ratio** | **7.27×** | 4.96× | 5.18× | 4.47× | 3.94× | 3.54× |

mzip's output is smaller by **+31.7% vs brotli, +28.8% vs xz, +38.5% vs zstd-22**. The context-mixing backend alone contributes **+3.68%** over mzip-without-CM. (This corpus is deliberately harder than earlier versions — it now includes near-incompressible ARM/x86 binaries and WASM, so the aggregate ratio is lower than a code-only corpus while the *margin over every standard* holds.)

**Per-file standing (84 real files): 67 strict wins · 17 framing-ties · 0 losses** — where "best standard" now includes **zstd-19+dict** (mzip's own dictionary), so mzip beats or ties *dict-equipped* competitors on every real file. A "framing-tie" means mzip's compressed payload is within 32 B of the best standard — the gap is mzip's self-describing archive header (~5–14 B/file) versus a raw stream, not a compression loss. (Across all 92 files including synthetic: 75 wins / 17 ties / 0 losses.)

**Fairness — mzip beats dict-equipped compressors, not just no-dict ones.** mzip uses trained dictionaries, so a fair comparator must too. Two do: **brotli-11** (a ~120 KB built-in dictionary) is the headline comparator (**mzip −31.7%**); and, as the strongest possible test, **zstd-19 handed mzip's *own* dictionary** (`train_corpus/code_dict.bin`, the exact one mzip's ZSTD_DICT encoder uses) — on the dict-relevant code/config regime mzip is still **−11.6% smaller** (per-file −2.1% to −17.1%), because it layers CM + structural transforms + xz/brotli backstops on top of the same dictionary. (Plain zstd/xz run without a dictionary, so mzip's margin over *them* additionally includes a dict advantage — stated, not hidden. Across the full 48-type corpus mzip is also −38% vs `zstd-19+dict`, though the code dictionary is inert on the ~60% numeric/binary/WASM portion, so the sharp code/config figure above is the meaningful one. Full per-type column: [EVALS.md](EVALS.md).)

Where mzip pulls ahead hardest (mzip+CM ratio vs the best standard for that type; **bold** = a structural encoder shipped in the 2026 cycle):

| Type | mzip+CM | Best standard | Why mzip wins |
|---|--:|--:|---|
| **YAML-CRD** (k8s, large nested) | **57.54×** | xz 42.73× | `'MY'` indentation de-indent (depth + body streams) |
| Numeric (gas time-series) | **29.41×** | xz 2.76× | formula / ALP float coding + `bwt9` backstop |
| SQL | **26.16×** | xz 15.98× | INSERT-tuple columnar transpose + CM |
| Log | **20.58×** | bzip2 14.44× | per-column separation + CM |
| **FASTQ** (genomics) | **12.96×** | xz 10.35× | `'MF'` 4-line record de-interleave |
| **Minified-CSS** | **11.54×** | brotli 10.25× | PPMd high-order context model |
| **Binary-ARM/PPC** | **4.42×** | xz 4.08× | arch BCJ filter (ARM/PPC/RISC-V) |
| Audio (WAV/PCM) | **1.76×** | xz 1.22× | 16-bit samples → BWT/CM (general tools get ~1.1×) |

*Audio caveat (honest):* on real WAVs mzip beats every **general** compressor by 15–38% (and comes within 4.9% of FLAC on piano), but the purpose-built lossless audio codec **FLAC still wins** (test1.wav: mzip 1.76× vs FLAC 2.30×). mzip is a general compressor, not an audio codec — for audio, use FLAC. Full audio table + FLAC numbers: [EVALS.md](EVALS.md).

Two 2026 correctness fixes turned former blow-ups into wins: repetitive **syslog** now compresses **35.45×** (was a 22× archive-inflation bug), and **BIGINT-UNSIGNED SQL** now compresses **70.19×** (was a 65–139× bug where an oversized-integer parse dumped the whole file near-raw). Both are now guarded by unit tests.

**How it gets there — three trial-and-keep layers, none of which can regress** (the encoder keeps the smallest roundtrip-verified candidate per block):

- **CM backend (`cm_backend.hpp`)** — a BWT + context-mixing range coder (bzip3-class), wired into `bwt9` as **mode 2**, so *every* BWT call-site (general blocks, DBF, CSV, BWT_TEXT) trials it and keeps it when smaller. This carries most of mzip's edge on text / code / logs.
- **Ensemble backstop** — each block also trials **xz (liblzma -9e)** and **brotli-11** and keeps the smallest, so mzip never loses where those tools would win (this flipped SQL / YAML / config). `bwt9` is itself a universal backstop that catches BWT-friendly data the type detectors miss (this flipped real float / sensor arrays that previously lost to bzip2).
- **Encoder-firing audit (`diagnose_encoders.py`)** — per-block telemetry (`MZIP_STATS=1`) cross-referenced with each type's expected encoder and win-gap, to systematically find where a specialized encoder *doesn't fire*. This is how the numeric losses and an under-representative TypeScript sample were found and fixed.

> **Fairness notes.** `real_bench/` is held-out; the dictionaries are trained on a *separate* `train_corpus/` (mzip is never benchmarked on its own training data); binary/YAML/FASTQ/minified corpora are fetched permissively (MIT/public-domain — no GPL) by `build_evals.sh`; `samples/` are synthetic (labeled). mzip is a trial-everything ensemble — it trades **compression speed** for ratio: **~0.1 MB/s compress** on this box (single-shot, indicative), but **~11 MB/s decompress** (decode just inverts the winning transform). zstd / brotli compress 10–100×+ faster; if write-side latency matters, use them. Ratios are exact and machine-independent; the speed figures are not.

The synthetic suite and 47-file GitHub corpus below predate this harness and remain for reference — these held-out numbers are the headline.

---

## Benchmark Results

> *Secondary benchmark — a formula-friendly synthetic suite. The [primary held-out benchmark](#primary-benchmark-held-out-type-stratified) above is the fair real-world measure; these synthetic numbers skew high because the suite includes formula-compressible content (sequential IDs, timestamps, gradients) that real data rarely contains.*

All synthetic benchmarks below are generated deterministically by `generators.hpp` (seeded RNG, seed=42). Click sample links to download the exact input bytes the table reports on.

> **Roundtrip-verified.** mzip's encoder verifies its own output decodes back to the input before returning. Strategies that produce non-roundtrippable bytes are auto-discarded in favor of the next-best valid candidate. All 250 synthetic + 47 real-world results below pass verification.

### Overall Compressor Scoreboard (250 tests: 50 types × 5 sizes)

| Compressor | Avg Ratio | Range | Wins | Win% | Rank |
|------------|-----------|-------|------|------|------|
| **mzip** | **8.26x** | 1.0–32768x | **235** | **94.0%** | **1** |
| brotli:11 | 5.78x | 1.0–1716x | 13 | 5.2% | 2 |
| bzip2:9 | 5.66x | 1.0–1001x | 5 | 2.0% | 3 |
| rar:m5 | 5.97x | 1.0–1014x | 0 | 0.0% | 4 |
| xz:9 | 5.89x | 1.0–997x | 0 | 0.0% | 5 |
| 7z:mx9 | 5.88x | 1.0–922x | 0 | 0.0% | 6 |
| zstd:19 | 5.14x | 1.0–2641x | 0 | 0.0% | 7 |
| gzip:9 | 4.78x | 1.0–240x | 0 | 0.0% | 8 |

*Verified: 250/250 roundtrips pass. Total input: 66.60 MB. lz4/snappy excluded (speed-focused, optimize for a different point on the curve).*

### Decompression Speed (the trade-off)

mzip optimizes for the smallest output and accepts a slower decode in return. If your workload is read-heavy and latency-bound, this is the wrong tool — use zstd. If you write once and store / transfer often, the savings show up on every later read.

| Compressor | Decode time (66.6 MB) | Decode speed |
|---|---:|---:|
| zstd:19 | 92 ms | 722 MB/s |
| mzip | 3,523 ms | 19 MB/s |

zstd is roughly **38× faster to decompress**. Most of the gap is BWT-inverse vs LZ77-inverse — a structural difference, not a tuning gap. Compression throughput depends on the strategy chosen; the production trial-everything ensemble (CM + BWT + dictionaries + xz + brotli per block) runs at **~0.2 MB/s** — ratio over speed (see the [primary benchmark](#primary-benchmark-held-out-type-stratified) fairness note). Streaming / incremental decode is on the roadmap but not in main yet.

### Win Rate by Size

| Size | Wins | Total | Win% |
|------|------|-------|------|
| 4KB | 43 | 50 | 86.0% |
| 16KB | 48 | 50 | 96.0% |
| 64KB | 47 | 50 | 94.0% |
| 256KB | 48 | 50 | 96.0% |
| 1MB | 49 | 50 | 98.0% |

### Top 10 mzip Wins

| Type | mzip | 2nd Best | Advantage |
|------|------|----------|-----------|
| [Database IDs (1MB)](samples/1m/input/db_ids.bin) | **32B (32768x)** | 3.4KB | 106.8x better |
| [Timestamps (1MB)](samples/1m/input/timestamps_regular.bin) | **32B (32768x)** | 2.7KB | 84.2x better |
| [Database IDs (256KB)](samples/256k/input/db_ids.bin) | **32B (8192x)** | 937B | 29.3x better |
| [Timestamps (256KB)](samples/256k/input/timestamps_regular.bin) | **32B (8192x)** | 772B | 24.1x better |
| [Database IDs (64KB)](samples/64k/input/db_ids.bin) | **32B (2048x)** | 301B | 9.4x better |
| [Timestamps (64KB)](samples/64k/input/timestamps_regular.bin) | **32B (2048x)** | 287B | 9.0x better |
| [Image gradient (256KB)](samples/256k/input/gradient.bin) | **53B (4946x)** | 323B | 6.1x better |
| [Image gradient (64KB)](samples/64k/input/gradient.bin) | **39B (1680x)** | 212B | 5.4x better |
| [Timestamps (16KB)](samples/16k/input/timestamps_regular.bin) | **32B (512x)** | 160B | 5.0x better |
| [JSON API (1MB)](samples/1m/input/json.json) | **10KB (104x)** | 49KB | 4.9x better |

### Where Others Win (Top 10 Gaps)

The remaining gaps are all on small (4–16KB) text/code/config where brotli's pre-built 120KB English dictionary gives an edge no per-file approach can fully match without shipping its own dictionary.

| Type | Size | mzip | Best | Gap |
|------|------|------|------|-----|
| Terraform | 1MB | 41KB | **brotli: 40KB** | +825B |
| Terraform | 64KB | 3.4KB | **brotli: 3.0KB** | +379B |
| HTML | 4KB | 1004B | **brotli: 821B** | +183B |
| Terraform | 256KB | 10KB | **brotli: 10KB** | +161B |
| JSON log | 16KB | 2.4KB | **brotli: 2.3KB** | +148B |
| Makefile | 4KB | 809B | **brotli: 697B** | +112B |
| JSON log | 4KB | 866B | **brotli: 768B** | +98B |
| INI config | 64KB | 10KB | **bzip2: 10KB** | +87B |
| Unicode text | 4KB | 626B | **brotli: 549B** | +77B |
| Bash | 4KB | 825B | **brotli: 765B** | +60B |

---

## Long-Form Prose (enwik9)

[`enwik9`](https://www.mattmahoney.net/dc/text.html) is the canonical benchmark for text compressors — the first 10⁹ bytes of an English Wikipedia dump. The numbers below are mzip vs every standard library compressor on the first 1 MB and 10 MB prefixes.

| Compressor | enwik9 1 MB | enwik9 10 MB | Class |
|---|---:|---:|---|
| zstd:19 | 312,639 | 2,921,957 | LZ77 + entropy |
| gzip:9 | 356,643 | 3,720,323 | LZ77 + Huffman |
| bzip2:9 | 294,484 | 3,054,639 | BWT + multi-tree Huffman |
| xz:9 | 302,832 | 2,844,360 | LZMA |
| 7z:mx9 | 302,910 | 2,844,433 | LZMA2 |
| brotli:11 | 293,057 | 2,827,632 | LZ77 + Huffman + 120 KB static dict |
| **mzip** | **286,307** | **2,671,197** | **BWT + capfold + word dict + LZP-after-dict, per-block dynamic trial** |
| *bzip3* † | *~245,000* | *~2,300,000* | *BWT + LZP + arithmetic + 1-symbol context model (CLI, not a library)* |
| *bsc-m03* † | *—* | *—* | *LZP + BWT + M03 context coder (CLI, research-grade, not a library)* |

mzip produces the smallest output on both prefixes **among standard library compressors** — beats brotli:11 by 5.9%, xz/7z by 6.5%, bzip2:9 by 14.4% on 10 MB; beats brotli:11 by 2.4%, xz/7z by 5.8%, bzip2:9 by 2.9% on 1 MB.

† **The honest ceiling.** bzip3 and bsc-m03 are CLI archive tools (not embeddable libraries) that replace the entropy backend with adaptive arithmetic coding driven by a post-BWT context model. **mzip now closes much of this gap inside the library:** `cm_backend.hpp` adds exactly that — a BWT + context-mixing range coder (bzip3-class), wired as `bwt9` mode 2 (see [Primary Benchmark](#primary-benchmark-held-out-type-stratified) above), trial-and-keep so it never regresses. bsc-m03 / ZPAQ / cmix still lead at the cost of much slower compression and no library API.

---

## Per-Category Tables

Synthetic data, generated at each size by `generators.hpp` so results are reproducible.



### NUMERIC

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Timestamps** | **32B** vs 287B | **32B** vs 772B | **32B** vs 2.6KB | [64k](samples/64k/input/timestamps_regular.bin) [256k](samples/256k/input/timestamps_regular.bin) [1m](samples/1m/input/timestamps_regular.bin) |
| **Database IDs** | **32B** vs 301B | **32B** vs 937B | **32B** vs 3.3KB | [64k](samples/64k/input/db_ids.bin) [256k](samples/256k/input/db_ids.bin) [1m](samples/1m/input/db_ids.bin) |
| **Integer array** | **3.3KB** vs 4.4KB | **12KB** vs 17KB | **51KB** vs 67KB | [64k](samples/64k/input/int_array.bin) [256k](samples/256k/input/int_array.bin) [1m](samples/1m/input/int_array.bin) |
| **GPS coordinates** | **9.7KB** vs 11KB | **38KB** vs 44KB | **154KB** vs 179KB | [64k](samples/64k/input/gps_track.bin) [256k](samples/256k/input/gps_track.bin) [1m](samples/1m/input/gps_track.bin) |
| **Float temperature** | **11KB** vs 22KB | **40KB** vs 87KB | **151KB** vs 331KB | [64k](samples/64k/input/floats.bin) [256k](samples/256k/input/floats.bin) [1m](samples/1m/input/floats.bin) |
| **Sensor 16-bit** | **26KB** vs 27KB | **107KB** vs 111KB | **430KB** vs 445KB | [64k](samples/64k/input/sensor_16bit.bin) [256k](samples/256k/input/sensor_16bit.bin) [1m](samples/1m/input/sensor_16bit.bin) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### STRUCTURED

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **GraphQL queries** | **2.8KB** vs 2.8KB | **7.5KB** vs 7.8KB | **25KB** vs 28KB | [64k](samples/64k/input/graphql.graphql) [256k](samples/256k/input/graphql.graphql) [1m](samples/1m/input/graphql.graphql) |
| **SQL dump** | **4.6KB** vs 4.7KB | **14KB** vs 15KB | **53KB** vs 56KB | [64k](samples/64k/input/sql.sql) [256k](samples/256k/input/sql.sql) [1m](samples/1m/input/sql.sql) |
| **JSON API** | **1016B** vs 3.7KB | **2.8KB** vs 12KB | **9.8KB** vs 48KB | [64k](samples/64k/input/json.json) [256k](samples/256k/input/json.json) [1m](samples/1m/input/json.json) |
| **XML document** | **1020B** vs 2.2KB | **2.9KB** vs 8.0KB | **10KB** vs 29KB | [64k](samples/64k/input/xml.xml) [256k](samples/256k/input/xml.xml) [1m](samples/1m/input/xml.xml) |
| **CSV data** | **8.1KB** vs 9.8KB | **27KB** vs 33KB | **100KB** vs 122KB | [64k](samples/64k/input/csv.csv) [256k](samples/256k/input/csv.csv) [1m](samples/1m/input/csv.csv) |
| **Base64 data** | **47KB** vs 48KB | **189KB** vs 192KB | **758KB** vs 771KB | [64k](samples/64k/input/base64.txt) [256k](samples/256k/input/base64.txt) [1m](samples/1m/input/base64.txt) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### CODE

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **JavaScript** | **4.0KB** vs 5.2KB | **10KB** vs 12KB | **37KB** vs 43KB | [64k](samples/64k/input/javascript.js) [256k](samples/256k/input/javascript.js) [1m](samples/1m/input/javascript.js) |
| **Python** | **5.2KB** vs 5.3KB | **12KB** vs 12KB | **33KB** vs 40KB | [64k](samples/64k/input/python.py) [256k](samples/256k/input/python.py) [1m](samples/1m/input/python.py) |
| **TypeScript** | **4.2KB** vs 4.4KB | **12KB** vs 12KB | **40KB** vs 45KB | [64k](samples/64k/input/typescript.ts) [256k](samples/256k/input/typescript.ts) [1m](samples/1m/input/typescript.ts) |
| **HTML** | **5.4KB** vs 5.7KB | **16KB** vs 18KB | **61KB** vs 68KB | [64k](samples/64k/input/html.html) [256k](samples/256k/input/html.html) [1m](samples/1m/input/html.html) |
| **CSS** | 4.1KB vs **4.1KB** | **11KB** vs 11KB | **40KB** vs 43KB | [64k](samples/64k/input/css.css) [256k](samples/256k/input/css.css) [1m](samples/1m/input/css.css) |
| **Go** | **3.3KB** vs 3.4KB | **8.1KB** vs 8.5KB | **24KB** vs 28KB | [64k](samples/64k/input/golang.go) [256k](samples/256k/input/golang.go) [1m](samples/1m/input/golang.go) |
| **Rust** | **3.5KB** vs 3.5KB | **8.9KB** vs 9.1KB | **28KB** vs 31KB | [64k](samples/64k/input/rust.rs) [256k](samples/256k/input/rust.rs) [1m](samples/1m/input/rust.rs) |
| **Java** | **3.7KB** vs 3.9KB | **10.0KB** vs 10KB | **29KB** vs 36KB | [64k](samples/64k/input/java.java) [256k](samples/256k/input/java.java) [1m](samples/1m/input/java.java) |
| **C** | **4.9KB** vs 5.2KB | **14KB** vs 15KB | **47KB** vs 54KB | [64k](samples/64k/input/c.c) [256k](samples/256k/input/c.c) [1m](samples/1m/input/c.c) |
| **Bash** | **3.5KB** vs 3.7KB | **9.9KB** vs 10KB | **34KB** vs 37KB | [64k](samples/64k/input/bash.sh) [256k](samples/256k/input/bash.sh) [1m](samples/1m/input/bash.sh) |
| **PHP** | **3.2KB** vs 3.3KB | **7.8KB** vs 8.6KB | **23KB** vs 27KB | [64k](samples/64k/input/php.php) [256k](samples/256k/input/php.php) [1m](samples/1m/input/php.php) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### CONFIG

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Docker Compose** | **1.7KB** vs 2.1KB | **5.4KB** vs 5.6KB | **17KB** vs 19KB | [64k](samples/64k/input/docker_compose.yaml) [256k](samples/256k/input/docker_compose.yaml) [1m](samples/1m/input/docker_compose.yaml) |
| **Terraform** | **2.8KB** vs 3.0KB | 10KB vs **10KB** | 41KB vs **40KB** | [64k](samples/64k/input/terraform.tf) [256k](samples/256k/input/terraform.tf) [1m](samples/1m/input/terraform.tf) |
| **K8s manifests** | **2.8KB** vs 3.3KB | **7.4KB** vs 7.6KB | **21KB** vs 24KB | [64k](samples/64k/input/k8s.yaml) [256k](samples/256k/input/k8s.yaml) [1m](samples/1m/input/k8s.yaml) |
| **YAML config** | **3.8KB** vs 3.8KB | **11KB** vs 11KB | **38KB** vs 41KB | [64k](samples/64k/input/yaml_config.yaml) [256k](samples/256k/input/yaml_config.yaml) [1m](samples/1m/input/yaml_config.yaml) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### LOG

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Access log** | **6.3KB** vs 6.8KB | **21KB** vs 24KB | **85KB** vs 94KB | [64k](samples/64k/input/access_log.log) [256k](samples/256k/input/access_log.log) [1m](samples/1m/input/access_log.log) |
| **Nginx access log** | **6.5KB** vs 6.8KB | **21KB** vs 22KB | **82KB** vs 87KB | [64k](samples/64k/input/nginx_access.log) [256k](samples/256k/input/nginx_access.log) [1m](samples/1m/input/nginx_access.log) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### BINARY

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Image gradient** | **39B** vs 212B | **53B** vs 323B | **124B** vs 397B | [64k](samples/64k/input/gradient.bin) [256k](samples/256k/input/gradient.bin) [1m](samples/1m/input/gradient.bin) |
| **Audio PCM** | **1.7KB** vs 4.0KB | **1.7KB** vs 4.0KB | **1.7KB** vs 4.0KB | [64k](samples/64k/input/audio_pcm.bin) [256k](samples/256k/input/audio_pcm.bin) [1m](samples/1m/input/audio_pcm.bin) |
| **Sparse bitmap** | **689B** vs 880B | **2.6KB** vs 3.0KB | **10KB** vs 11KB | [64k](samples/64k/input/sparse_bitmap.bin) [256k](samples/256k/input/sparse_bitmap.bin) [1m](samples/1m/input/sparse_bitmap.bin) |
| **Protobuf-like** | **40KB** vs 41KB | **160KB** vs 163KB | **640KB** vs 650KB | [64k](samples/64k/input/protobuf.bin) [256k](samples/256k/input/protobuf.bin) [1m](samples/1m/input/protobuf.bin) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### ADDITIONAL

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Natural text** | **6.7KB** vs 7.2KB | **26KB** vs 28KB | **104KB** vs 111KB | [64k](samples/64k/input/natural_text.txt) [256k](samples/256k/input/natural_text.txt) [1m](samples/1m/input/natural_text.txt) |
| **Markdown docs** | **3.6KB** vs 3.8KB | **10KB** vs 11KB | **36KB** vs 41KB | [64k](samples/64k/input/markdown.md) [256k](samples/256k/input/markdown.md) [1m](samples/1m/input/markdown.md) |
| **Email headers** | **6.6KB** vs 6.8KB | **20KB** vs 21KB | **75KB** vs 79KB | [64k](samples/64k/input/email_headers.txt) [256k](samples/256k/input/email_headers.txt) [1m](samples/1m/input/email_headers.txt) |
| **Unicode text** | **2.4KB** vs 2.4KB | **7.5KB** vs 7.5KB | **27KB** vs 28KB | [64k](samples/64k/input/unicode.txt) [256k](samples/256k/input/unicode.txt) [1m](samples/1m/input/unicode.txt) |
| **Syslog** | **8.8KB** vs 9.4KB | **32KB** vs 34KB | **126KB** vs 133KB | [64k](samples/64k/input/syslog.log) [256k](samples/256k/input/syslog.log) [1m](samples/1m/input/syslog.log) |
| **Metrics** | 7.8KB vs **7.8KB** | 29KB vs **29KB** | **117KB** vs 117KB | [64k](samples/64k/input/metrics.txt) [256k](samples/256k/input/metrics.txt) [1m](samples/1m/input/metrics.txt) |
| **JSON log** | **7.2KB** vs 8.0KB | **29KB** vs 30KB | **116KB** vs 122KB | [64k](samples/64k/input/json_log.json) [256k](samples/256k/input/json_log.json) [1m](samples/1m/input/json_log.json) |
| **Timestamps (jitter)** | **14KB** vs 15KB | **56KB** vs 61KB | **224KB** vs 244KB | [64k](samples/64k/input/timestamps_jitter.bin) [256k](samples/256k/input/timestamps_jitter.bin) [1m](samples/1m/input/timestamps_jitter.bin) |

*Format: **mzip** vs 2nd-best. Bold = winner.*

### BUILD

| Type | 64KB | 256KB | 1MB | Samples |
|------|------|-------|-----|---------|
| **Makefile** | **3.8KB** vs 4.6KB | **12KB** vs 16KB | **46KB** vs 61KB | [64k](samples/64k/input/makefile) [256k](samples/256k/input/makefile) [1m](samples/1m/input/makefile) |
| **package.json** | **4.6KB** vs 4.8KB | **14KB** vs 15KB | **54KB** vs 57KB | [64k](samples/64k/input/package.json) [256k](samples/256k/input/package.json) [1m](samples/1m/input/package.json) |
| **Cargo.toml** | **3.3KB** vs 3.5KB | **9.5KB** vs 10KB | **32KB** vs 38KB | [64k](samples/64k/input/cargo.toml) [256k](samples/256k/input/cargo.toml) [1m](samples/1m/input/cargo.toml) |

*Format: **mzip** vs 2nd-best. Bold = winner.*
## Real-World File Benchmark

> *Legacy benchmark, superseded by the [primary held-out benchmark](#primary-benchmark-held-out-type-stratified) above (which adds the CM backend, more content types, and more compressors). Kept for historical comparison.*

47 files (7.2 MB total) pulled from public GitHub repos — React, Linux kernel, Django, Bootstrap, lodash, plus 20+ programming-language files. Mix of source code, configs, logs, JSON, CSV, markdown. All 47 round-trip-verified.

Reproduce the full per-file table with the real-world benchmark — see [Run Benchmarks](#run-benchmarks).

### Result

- **mzip wins or ties on 18 / 47 files (38.3%)**
- brotli:11 wins 27, bzip2:9 wins 1, xz:9 wins 1, all others 0

The synthetic 94.0% does not survive intact on real GitHub source code — the synthetic suite includes formula-compressible content (sequential IDs, timestamps, gradients, audio, generated templates) that hand-written code rarely contains. The 12 pre-trained group dictionaries (5 synthetic-trained + 7 real-data-trained on public corpora not in this benchmark) close most of the gap on logs, CSV, JSON, SQL, XML, and structured config. Read both numbers together.

### Where mzip wins (top advantages)

| File | Size | mzip ratio | 2nd best | Advantage |
|------|-----:|-----------:|----------|-----------|
| sql_schema.sql | 4.1 KB | **20.86×** | brotli: 1.1 KB | **+81.8%** |
| java_arraylist.java | 64.6 KB | 9.00× | brotli: 11.2 KB | +36.1% |
| xml_maven.xml | 45.4 KB | 11.29× | brotli: 5.8 KB | +30.3% |
| apache_log_sample.log | 2.26 MB | 22.83× | brotli: 116 KB | +12.5% |
| go_http.go | 128 KB | 4.23× | brotli: 34.3 KB | +11.7% |
| docker-compose.yml | 3.9 KB | 4.23× | brotli: 1.0 KB | +8.2% |
| dashboard.html | 42.5 KB | 34.04× | brotli: 1.3 KB | +7.1% |
| terraform_main.tf | 6.3 KB | 3.83× | brotli: 1.8 KB | +7.1% |
| app.log | 464 KB | 7.90× | bzip2: 60.3 KB | +2.5% |
| lodash.js | 532 KB | 7.85× | bzip2: 69.2 KB | +2.1% |
| models.rs | 16.8 KB | 21.24× | brotli: 826 B | +1.9% |
| nginx_access.log | 417 KB | 12.11× | bzip2: 34.9 KB | +1.3% |
| styles.css | 19.6 KB | 9.28× | bzip2: 2.1 KB | +1.3% |
| handlers.go | 14 KB | 17.78× | brotli: 814 B | +1.2% |
| events.csv | 578 KB | 7.13× | bzip2: 82.0 KB | +1.1% |
| metrics.prom | 176 KB | 10.12× | bzip2: 17.5 KB | +1.1% |
| users.json | 170 KB | 10.11× | bzip2: 17.0 KB | +1.0% |
| linux_kernel.c | 281 KB | 4.41× | bzip2: 64.3 KB | +1.0% |

### Where brotli still wins

Brotli's 120 KB pre-built static English/web dictionary holds an edge on small handwritten markdown / source code where the input doesn't match any of mzip's 12 group dictionaries strongly enough.

| File | Size | mzip gap |
|------|-----:|---------:|
| contributing.md | 6.6 KB | +11.8% |
| api_docs.md | 17 KB | +10.7% |
| Dockerfile | 4.1 KB | +8.7% |
| json_github_api.json | 6.6 KB | +8.5% |
| vscode_main.ts | 19.8 KB | +7.1% |

…and 22 more, mostly handwritten source code 4–50 KB, typical gap +3% to +7%.

### Per-category breakdown

| Category | mzip wins | Total | Win% |
|----------|-----------|-------|------|
| Logs | 3 | 3 | 100% |
| CSV / columnar | 1 | 1 | 100% |
| Metrics | 1 | 1 | 100% |
| XML | 1 | 1 | 100% |
| Web (HTML/CSS) | 2 | 3 | 67% |
| Config files | 2 | 4 | 50% |
| JSON | 1 | 2 | 50% |
| SQL | 1 | 2 | 50% |
| Source code (general) | 6 | 25 | 24% |
| Markdown | 0 | 3 | 0% |
| Other | 0 | 2 | 0% |

---

## Compression Strategies

The encoder runs detection on every block, picks a candidate strategy from one of five families below, then trials multiple variants per block and keeps the smallest output that round-trip-verifies.

| Family | Picked when | Headline strategies |
|---|---|---|
| **Formula / numeric** | Bytes look like a generator output: linear, periodic, geometric, modular, smooth | LINEAR_GEN, NUMERIC (delta / strided / ALP), PERIODIC, MODULAR, LINEAR_PRED |
| **Templates / structured text** | Repeating lines or blocks with a few varying tokens (logs, generated docs, K8s, SQL INSERTs) | TEMPLATE, SECTION_TEMPLATE, ML_TEMPLATE, WORD_TEMPLATE / MULTI_WORD_TEMPLATE, LINE_GROUP_TEMPLATE |
| **Stream separation** | Fixed columns, tag/content split, key/value records | COLUMNAR / BLOCK_COLUMNAR, CSV_COLUMNAR, JSON_COLUMNAR, HTML_STREAM, URL_STREAM, DBF_CONSTCOL |
| **Binary / executable** | x86 code, raw RGB, sparse bitmaps, base64 text | E8E9_X86 + LZMA_OPTIMAL, PAETH_RGB, SPARSE, BASE64_DECODE |
| **Text backends** | None of the above fits — prose, code, config, mixed | BWT_TEXT, BG (single-block BWT for ≥1 MB), MC (per-chunk pick), ZSTD_DICT (4–16 KB code/config), WORD_ENCODED, KV_CONFIG, RAW (fallback to zstd:19) |

The BWT text backend (BWT_TEXT / BG) is itself a per-block trial over: pre-RLE on/off, dict size ∈ {64, 128, 192, 255}, number of Huffman trees ∈ {3..7}, LZP-after-dict min-match ∈ {10, 20, 40}, capfold on/off. The smallest valid combination wins.

For the full enum of named strategies (~30) with selection rules, grep `BlockType::` and `case BlockType::` in [`mzip.hpp`](mzip.hpp) — each block-type carries a one-line `// what it does` comment at its definition.

---

## Quick Start

**Requirements:** a C++17 compiler and the zstd library headers + shared object. Install zstd if you don't already have it:

```bash
# macOS
brew install zstd

# Debian / Ubuntu
sudo apt install libzstd-dev

# Fedora
sudo dnf install libzstd-devel

# Windows / MSYS2
pacman -S mingw-w64-x86_64-zstd
```

### Option 1: Single-header (recommended)

The amalgamated header bundles mzip + the BWT pipeline + libsais. You only need to add zstd.

```cpp
// In ONE translation unit:
#include <zstd.h>
#define MZIP_IMPLEMENTATION
#include "mzip_amalgamated.hpp"

// In every other translation unit that uses mzip:
#include "mzip_amalgamated.hpp"

// Usage:
auto compressed   = mzip::compress(data.data(), data.size());
auto decompressed = mzip::decompress(compressed.data(), compressed.size());
```

### Option 2: Separate headers

```cpp
#include <zstd.h>      // include zstd first
#include "mzip.hpp"

auto compressed   = mzip::compress(data.data(), data.size());
auto decompressed = mzip::decompress(compressed.data(), compressed.size());
```

### Build

```bash
# Single-header build (libsais bundled inside)
g++ -std=c++17 -O3 -march=native -o mzip_cli mzip_cli.cpp -lzstd

# Separate-headers build (needs libsais.c too)
g++ -std=c++17 -O3 -march=native -o mzip_cli mzip_cli.cpp libsais.c -lzstd
```

If your zstd headers are not on the default search path, add `-I/path/to/zstd/include -L/path/to/zstd/lib`.

### CLI Usage

```bash
# Compress
./mzip_cli compress input.bin output.mzip

# Decompress
./mzip_cli decompress output.mzip restored.bin
```

---

## Run Benchmarks

```bash
# Build (assumes zstd is installed; otherwise add -I/-L flags as in Quick Start above)
g++ -std=c++17 -O3 -march=native -o mzip_bench mzip_bench.cpp libsais.c -lzstd

# Synthetic suite — 50 types × 5 sizes = 250 tests, ~10–15 min
./mzip_bench --csv full_bench.csv

# Quick (64 KB only)
./mzip_bench --quick

# Single type, all sizes
./mzip_bench --type graphql

# Single real file
./mzip_bench --file path/to/file.bin

# All 47 real-world files
./mzip_bench --file real_bench/*

# Regenerate the README tables from a fresh CSV
python generate_readme_tables.py full_bench.csv
```

### Held-out type benchmark + encoder audit (CM backend)

```bash
# Build all eval binaries (mzip+CM, baseline, zstd sizer, probes) + fetch/derive the extra real corpora
bash build_evals.sh

# Type-stratified benchmark vs gzip/bzip2/zstd-19/zstd-22/xz-9e/brotli-11, held-out real files, roundtrip-verified
python3 benchmark_types.py            # -> bench_types_report.md  (38 types / 76 files)

# Systematic "which encoder fired / what didn't fire" audit (LOSS / MISSED-SPECIAL / BACKSTOP-RELIANT)
python3 diagnose_encoders.py          # -> encoder_audit.md

# Per-block encoder telemetry on any file
MZIP_STATS=1 ./mzip_cm.exe c file out
```

---

## Files

| File | Description |
|------|-------------|
| `mzip.hpp` | Main library — include this |
| `mzip_amalgamated.hpp` | Single-header build (mzip + BWT + libsais bundled) |
| `bwt_compress_v5.hpp` / `v8.hpp` / `v9.hpp` | BWT pipelines (v5 = current prose backend) |
| `word_dict.hpp` | Per-file word-dictionary preprocessor (BWT_TEXT helper) |
| `cap_fold.hpp` | Capital-letter folding (BWT_TEXT helper) |
| `bigram_dict.hpp`, `xml_entity.hpp` | Pre-BWT preprocessing candidates (auto-deselected when they don't help) |
| `range_coder.hpp` | LZMA-style binary range coder (variant backend) |
| `mzip_dicts.h` | 12 pre-trained zstd group dictionaries (5 synthetic + 7 real-data: MD/YAML/HCL/SQL/XML/CODE/JSON). Embedded ~2 MB. |
| `train_corpus/` | Real-world corpus used to train dicts 6–12 (held out from `real_bench/`); regenerable via `train_corpus/fetch.sh`. |
| `lzma_optimal2.hpp`, `lzma_decoder.hpp` | LZMA optimal encoder + decoder (LZMA_OPTIMAL strategy) |
| `mzip_base64.hpp` | Base64 detect / decode helper (BASE64_DECODE strategy) |
| `generators.hpp` | Single source of truth for benchmark / test data |
| `libsais.h` | BWT suffix array (Apache 2.0) |
| `stb_image.h`, `stb_image_write.h` | Image IO (Public Domain) — for image strategies |
| `mzip_bench.cpp` | Benchmark tool — `--csv` exports results |
| `mzip_cli.cpp` | Command-line interface |
| `mzip_test.cpp` | Quick debug / single-type test |
| `mzip_unit_tests.cpp` | Unit tests for core strategies |
| `cm_backend.hpp` | **BWT + context-mixing (bzip3-class) entropy backend** — wired as `bwt9` mode 2 + ensemble candidate |
| `brotli_shim.hpp` / `liblzma_shim.hpp` | Minimal decls to link brotli / liblzma as ensemble backstop candidates |
| `build_evals.sh` | Builds all eval binaries (mzip+CM, baseline, zstd sizer, probes) + fetches/derives the held-out corpora |
| `benchmark_types.py` | **Held-out type-stratified benchmark** (mzip+CM vs gzip/bzip2/zstd/xz/brotli) → `bench_types_report.md` |
| `diagnose_encoders.py` | **Encoder-firing audit** (`MZIP_STATS` telemetry → LOSS / MISSED-SPECIAL / BACKSTOP) → `encoder_audit.md` |
| `generate_readme_tables.py` | Auto-generate the markdown tables above from `full_bench.csv` |
| `summarize_real_bench.py` | Auto-generate `real_bench_summary.md` from `real_bench_results.txt` |
| `samples/` | Sample files at 4 / 16 / 64 / 256 KB and 1 MB |
| `real_bench/` | 47 real-world files used by the real-world benchmark |
| `full_bench.csv` | Latest synthetic benchmark CSV (one row per (type, size)) |

---

## License & Contact

**Dual-licensed: AGPL-3.0 OR commercial.**

- **AGPL-3.0** — free for open-source projects. If you deploy mzip as part of a network service (SaaS, hosted API, etc.), the AGPL requires you to make your source available.
- **Commercial** — for proprietary or closed-source use, open a [GitHub issue](https://github.com/Cranot/mzip/issues/new) tagged `commercial-license` and I'll follow up. Same for bug reports, benchmarks on your own data, or proposing a new strategy.

Third-party code bundled in the repo: [libsais](https://github.com/IlyaGrebnov/libsais) (Apache 2.0), [stb_image](https://github.com/nothings/stb) (Public Domain). zstd is required at link-time but not bundled (BSD).
