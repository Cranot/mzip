# mzip benchmark — matrix report

Generated from `bench_matrix.jsonl` by `bench_report.py`. **No compressor was run to produce this file.** Every number is a pure function of the matrix and can be recomputed from it.


## Corpus

- **Label:** `DEV-2026-08`  ·  **blind:** `False`
- Development corpus: mzip's encoders were built and tuned against these files. Held out from mzip's dictionary training (train_corpus/), NOT held out from encoder development. Not a blind evaluation.
- **This report covers 102 file(s)** (93 real, 9 generated) across 52 content type(s) and 13 source group(s).
- Measured at git `38e82e40ed6b`, 2026-08-13T07:54:14Z.

## Tools, and what their numbers are worth

| tool | mode | size evidence | roundtrip verified | version | sha256 |
|---|---|---|---|---|---|
| `gzip-9` | stdout | artifact | yes | gzip 1.14 | `b30df4d89223` |
| `bzip2-9` | stdout | artifact | yes | bzip2, a block-sorting file compressor.  Version 1.0 | `4dd06837ccca` |
| `brotli-11` | stdout | artifact | yes | brotli 1.2.0 | `ba1fb21bdacc` |
| `xz-9e` | stdout | artifact | yes | xz (XZ Utils) 5.8.3 | `c3d660fb270e` |
| `zstd-19` | sizer | reported (no artifact) | **no** | (no version probe) | `0baabf3bbea0` |
| `zstd-22` | sizer | reported (no artifact) | **no** | (no version probe) | `0baabf3bbea0` |
| `zstd-19+dict` | sizer | reported (no artifact) | **no** | (no version probe) | `0baabf3bbea0` |
| `mzip(noCM)` | file | artifact | yes | (no version probe) | `74d75653793b` |
| `mzip+CM` | file | artifact | yes | (no version probe) | `93c6c562177a` |

`sizer` tools print a size and write no artifact, so their numbers can never be roundtrip-checked. They are reported, and marked, rather than silently mixed with verified sizes.


## Validity ledger

An invalid observation is UNKNOWN. It is never coerced to a size, and it never enters a statistic — but it is always counted here, because a statistic over a silently shrunken denominator is exactly the defect this pipeline was built to remove.

| tool | ok | invalid | statuses seen |
|---|---|---|---|
| `gzip-9` | 102 | 0 | — |
| `bzip2-9` | 102 | 0 | — |
| `brotli-11` | 102 | 0 | — |
| `xz-9e` | 102 | 0 | — |
| `zstd-19` | 102 | 0 | — |
| `zstd-22` | 102 | 0 | — |
| `zstd-19+dict` | 102 | 0 | — |
| `mzip(noCM)` | 102 | 0 | — |
| `mzip+CM` | 102 | 0 | — |

**Roundtrip:** 612 observations independently decompressed and matched the input byte for byte; 0 failed. (The legacy harness verified mzip only and trusted every comparator.)


## Head-to-head: mzip+CM vs each tool

`ratio` = other/mzip per file (>1 means mzip is smaller). `n` is the number of files where BOTH sides produced a valid observation; `dropped` is how many were excluded and why.

| vs | n | dropped | micro | macro | macro-grp | geomean | 95% CI (geo) | median | p10 | p90 |
|---|---|---|---|---|---|---|---|---|---|---|
| `gzip-9` | 102 | 0 | 1.815x | 1.808x | 2.313x | **1.543x** | 1.417–1.695 | 1.356x | 1.144x | 2.143x |
| `bzip2-9` | 102 | 0 | 1.627x | 1.600x | 2.001x | **1.345x** | 1.256–1.465 | 1.223x | 1.119x | 1.574x |
| `brotli-11` | 102 | 0 | 1.389x | 1.348x | 1.631x | **1.221x** | 1.152–1.310 | 1.130x | 0.998x | 1.461x |
| `xz-9e` | 102 | 0 | 1.361x | 1.421x | 1.642x | **1.307x** | 1.242–1.395 | 1.222x | 1.095x | 1.587x |
| `zstd-19` | 102 | 0 | 1.541x | 1.490x | 1.783x | **1.347x** | 1.273–1.446 | 1.238x | 1.119x | 1.629x |
| `zstd-22` | 102 | 0 | 1.539x | 1.489x | 1.782x | **1.346x** | 1.272–1.445 | 1.238x | 1.119x | 1.629x |
| `zstd-19+dict` | 102 | 0 | 1.532x | 1.442x | 1.749x | **1.299x** | 1.223–1.396 | 1.170x | 1.060x | 1.617x |
| `mzip(noCM)` | 102 | 0 | 1.045x | 1.023x | 1.028x | **1.019x** | 1.008–1.037 | 1.000x | 1.000x | 1.050x |

## Compression ratio (original / compressed)

Computed only over files where the tool produced a valid observation, so the columns can have different denominators — `n` is stated for each.

| tool | n | micro | macro | macro-grp | geomean | median |
|---|---|---|---|---|---|---|
| `gzip-9` | 102 | 2.832x | 4.723x | 4.679x | 3.762x | 3.522x |
| `bzip2-9` | 102 | 3.158x | 5.932x | 6.483x | 4.316x | 3.812x |
| `brotli-11` | 102 | 3.700x | 6.705x | 7.048x | 4.754x | 4.182x |
| `xz-9e` | 102 | 3.775x | 6.462x | 7.304x | 4.441x | 3.889x |
| `zstd-19` | 102 | 3.335x | 6.125x | 6.624x | 4.309x | 3.766x |
| `zstd-22` | 102 | 3.339x | 6.130x | 6.628x | 4.311x | 3.766x |
| `zstd-19+dict` | 102 | 3.354x | 6.367x | 6.776x | 4.467x | 3.972x |
| `mzip(noCM)` | 102 | 4.920x | 8.984x | 11.072x | 5.693x | 4.824x |
| `mzip+CM` | 102 | 5.140x | 9.104x | 11.318x | 5.804x | 4.824x |

## Per-file verdict vs the best standard tool

**FRAMING NOTE — declared, not implicit.** mzip emits an *archive* (magic + type + length); the comparators emit a raw stream with no container. A per-file difference of **≤ 32 B** is therefore recorded as a TIE rather than a win or a loss, because at that scale the container header, not the compression, decides the sign. The threshold is stated here so a reader can recompute with a different one.

Over 102 of 102 files: **78 wins, 24 ties, 0 losses** (threshold 32 B).

⚠ On 3 of those files the best comparator was an unverified **sizer** (`zstd-*`), so the bar mzip was measured against is a reported number with no artifact behind it.


No losses.


## Leave-one-domain-out

The geometric-mean head-to-head recomputed with each source group DELETED. A large swing means the headline was carried by that family and should not be quoted as corpus-wide.

| removed | n files | vs gzip-9 | vs bzip2-9 | vs brotli-11 | vs xz-9e | vs zstd-19 | vs zstd-22 | vs zstd-19+dict | vs mzip(noCM) |
|---|---|---|---|---|---|---|---|---|---|
| *(none — full corpus)* | 102 | 1.543x | 1.345x | 1.221x | 1.307x | 1.347x | 1.346x | 1.299x | 1.019x |
| binary | 93 | 1.564x (+1.4%) | 1.352x (+0.5%) | 1.239x (+1.5%) | 1.329x (+1.7%) | 1.363x (+1.2%) | 1.362x (+1.1%) | 1.310x (+0.8%) | 1.021x (+0.1%) |
| config-ser | 84 | 1.546x (+0.2%) | 1.353x (+0.6%) | 1.237x (+1.3%) | 1.312x (+0.4%) | 1.363x (+1.2%) | 1.362x (+1.2%) | 1.315x (+1.2%) | 1.023x (+0.4%) |
| genomics | 101 | 1.538x (-0.3%) | 1.344x (-0.0%) | 1.220x (-0.0%) | 1.307x (+0.0%) | 1.347x (-0.0%) | 1.346x (-0.0%) | 1.299x (-0.0%) | 1.019x (-0.0%) |
| log | 98 | 1.511x (-2.1%) | 1.340x (-0.3%) | 1.210x (-0.9%) | 1.303x (-0.3%) | 1.338x (-0.7%) | 1.337x (-0.7%) | 1.289x (-0.8%) | 1.020x (+0.0%) |
| markup-doc | 94 | 1.553x (+0.7%) | 1.357x (+0.9%) | 1.232x (+0.9%) | 1.312x (+0.4%) | 1.353x (+0.5%) | 1.353x (+0.5%) | 1.305x (+0.5%) | 1.021x (+0.2%) |
| media | 95 | 1.548x (+0.3%) | 1.345x (+0.0%) | 1.215x (-0.5%) | 1.306x (-0.1%) | 1.340x (-0.5%) | 1.340x (-0.5%) | 1.289x (-0.8%) | 1.017x (-0.3%) |
| minified-web | 99 | 1.547x (+0.3%) | 1.350x (+0.4%) | 1.224x (+0.2%) | 1.310x (+0.3%) | 1.351x (+0.3%) | 1.350x (+0.3%) | 1.302x (+0.2%) | 1.020x (+0.1%) |
| numeric | 98 | 1.497x (-3.0%) | 1.309x (-2.7%) | 1.189x (-2.6%) | 1.280x (-2.0%) | 1.311x (-2.7%) | 1.310x (-2.7%) | 1.263x (-2.8%) | 1.016x (-0.4%) |
| scientific | 101 | 1.542x (-0.1%) | 1.344x (-0.0%) | 1.221x (-0.0%) | 1.307x (+0.0%) | 1.347x (-0.0%) | 1.346x (-0.0%) | 1.299x (-0.0%) | 1.019x (-0.0%) |
| source-code | 67 | 1.710x (+10.8%) ⚠ | 1.415x (+5.2%) | 1.299x (+6.4%) | 1.355x (+3.7%) | 1.434x (+6.4%) | 1.433x (+6.4%) | 1.406x (+8.3%) | 1.019x (-0.1%) |
| sql | 99 | 1.481x (-4.0%) | 1.299x (-3.4%) | 1.183x (-3.1%) | 1.273x (-2.6%) | 1.309x (-2.8%) | 1.309x (-2.8%) | 1.263x (-2.8%) | 1.020x (+0.1%) |
| tabular | 96 | 1.530x (-0.8%) | 1.347x (+0.2%) | 1.211x (-0.8%) | 1.302x (-0.3%) | 1.338x (-0.7%) | 1.337x (-0.7%) | 1.288x (-0.8%) | 1.019x (-0.1%) |
| text-misc | 99 | 1.553x (+0.7%) | 1.353x (+0.6%) | 1.224x (+0.2%) | 1.310x (+0.3%) | 1.352x (+0.4%) | 1.351x (+0.4%) | 1.303x (+0.3%) | 1.020x (+0.1%) |

## By source group

| group | files | mzip+CM ratio (geo) | vs best std (geo) | win | tie | loss |
|---|---|---|---|---|---|---|
| binary | 9 | 2.98x | 1.028x | 7 | 2 | 0 |
| config-ser | 18 | 7.69x | 1.091x | 12 | 6 | 0 |
| genomics | 1 | 12.96x | 1.252x | 1 | 0 | 0 |
| log | 4 | 19.73x | 1.234x | 3 | 1 | 0 |
| markup-doc | 8 | 7.34x | 1.064x | 5 | 3 | 0 |
| media | 7 | 1.62x | 1.279x | 7 | 0 | 0 |
| minified-web | 3 | 7.40x | 1.131x | 3 | 0 | 0 |
| numeric | 4 | 9.40x | 2.057x | 4 | 0 | 0 |
| scientific | 1 | 4.25x | 1.246x | 1 | 0 | 0 |
| source-code | 35 | 4.91x | 1.073x | 23 | 12 | 0 |
| sql | 3 | 33.75x | 2.833x | 3 | 0 | 0 |
| tabular | 6 | 7.94x | 1.278x | 6 | 0 | 0 |
| text-misc | 3 | 3.70x | 1.045x | 3 | 0 | 0 |

## By content type

| type | group | files | orig | mzip+CM | ratio | best std | which | verdict |
|---|---|---|---|---|---|---|---|---|
| Audio (WAV/PCM) | media | 1 | 1,281,256 | 728,152 | 1.76x | 1,046,052 | xz-9e | **win** +30.39% |
| Base64(synth) | text-misc | 1 | 65,536 | 48,610 | 1.35x | 49,398 | brotli-11 | **win** +1.60% |
| Binary-ARM/PPC | binary | 1 | 5,372,896 | 1,216,935 | 4.42x | 1,317,580 | xz-9e | **win** +7.64% |
| Binary-x86 | binary | 5 | 1,280,445 | 428,432 | 2.99x | 444,926 | brotli-11 | **win** +3.71% |
| C/C++ | source-code | 2 | 302,258 | 59,479 | 5.08x | 68,491 | bzip2-9 | **win** +13.16% |
| CSS | markup-doc | 2 | 300,380 | 23,039 | 13.04x | 27,360 | brotli-11 | **win** +15.79% |
| CSV | tabular | 1 | 592,430 | 56,771 | 10.44x | 83,998 | bzip2-9 | **win** +32.41% |
| Delimited (semicolon/pipe CSV) | tabular | 2 | 3,999,992 | 781,754 | 5.12x | 1,027,476 | xz-9e | **win** +23.92% |
| Diff/patch | text-misc | 1 | 9,117 | 2,516 | 3.62x | 2,579 | brotli-11 | **win** +2.44% |
| Dockerfile | config-ser | 1 | 4,156 | 1,367 | 3.04x | 1,362 | brotli-11 | tie -0.37% |
| Email(synth) | text-misc | 1 | 65,536 | 6,344 | 10.33x | 6,949 | bzip2-9 | **win** +8.71% |
| Env/dotfiles | config-ser | 2 | 12,049 | 3,219 | 3.74x | 3,209 | brotli-11 | tie -0.31% |
| FASTQ (genomics) | genomics | 1 | 2,555,256 | 197,237 | 12.96x | 246,924 | xz-9e | **win** +20.12% |
| Go | source-code | 2 | 145,460 | 31,640 | 4.60x | 35,232 | zstd-19+dict | **win** +10.20% |
| GraphQL(synth) | config-ser | 1 | 65,536 | 2,463 | 26.61x | 2,899 | bzip2-9 | **win** +15.04% |
| HTML | markup-doc | 1 | 43,537 | 1,269 | 34.31x | 1,377 | brotli-11 | **win** +7.84% |
| INI(synth) | config-ser | 1 | 65,536 | 9,716 | 6.75x | 10,279 | bzip2-9 | **win** +5.48% |
| JSON | config-ser | 2 | 180,403 | 17,768 | 10.15x | 18,907 | bzip2-9 | **win** +6.02% |
| Java | source-code | 1 | 66,194 | 10,060 | 6.58x | 10,043 | zstd-19+dict | tie -0.17% |
| JavaScript | source-code | 2 | 558,227 | 63,652 | 8.77x | 74,364 | bzip2-9 | **win** +14.40% |
| Log | log | 3 | 3,273,035 | 159,076 | 20.58x | 226,680 | bzip2-9 | **win** +29.82% |
| Makefile | config-ser | 2 | 79,210 | 19,439 | 4.07x | 20,994 | brotli-11 | **win** +7.41% |
| Markdown | markup-doc | 3 | 106,261 | 22,991 | 4.62x | 24,866 | brotli-11 | **win** +7.54% |
| Metrics | tabular | 1 | 179,785 | 17,121 | 10.50x | 17,981 | bzip2-9 | **win** +4.78% |
| Minified-CSS | minified-web | 1 | 232,803 | 20,182 | 11.54x | 22,709 | brotli-11 | **win** +11.13% |
| Minified-JS | minified-web | 1 | 87,533 | 24,820 | 3.53x | 27,446 | brotli-11 | **win** +9.57% |
| NDJSON | tabular | 1 | 135,684 | 15,791 | 8.59x | 17,314 | bzip2-9 | **win** +8.80% |
| Numeric-gas | numeric | 1 | 4,194,304 | 142,610 | 29.41x | 1,517,024 | xz-9e | **win** +90.60% |
| Numeric-gyro | numeric | 1 | 4,194,304 | 411,287 | 10.20x | 537,360 | xz-9e | **win** +23.46% |
| Numeric-taxi | numeric | 1 | 4,194,304 | 740,698 | 5.66x | 860,588 | bzip2-9 | **win** +13.93% |
| Numeric-temp | numeric | 1 | 4,194,304 | 911,310 | 4.60x | 1,010,079 | bzip2-9 | **win** +9.78% |
| Other-langs | source-code | 12 | 922,579 | 161,612 | 5.71x | 180,910 | brotli-11 | **win** +10.67% |
| Protobuf(synth) | binary | 1 | 65,536 | 41,191 | 1.59x | 42,316 | bzip2-9 | **win** +2.66% |
| Protobuf-schema | config-ser | 1 | 61,115 | 12,611 | 4.85x | 13,596 | brotli-11 | **win** +7.24% |
| Python | source-code | 3 | 186,549 | 30,050 | 6.21x | 34,476 | brotli-11 | **win** +12.84% |
| Raster (BMP/PPM/TGA uncompressed) | media | 6 | 4,448,482 | 2,905,311 | 1.53x | 3,657,317 | brotli-11 | **win** +20.56% |
| SQL | sql | 2 | 259,523 | 9,922 | 26.16x | 16,244 | xz-9e | **win** +38.92% |
| SQL-BigIntUnsigned | sql | 1 | 262,935 | 3,746 | 70.19x | 10,179 | zstd-19+dict | **win** +63.20% |
| SVG | markup-doc | 1 | 10,009 | 4,059 | 2.47x | 4,060 | brotli-11 | tie +0.02% |
| Scientific-Matrix (sparse) | scientific | 1 | 2,571,276 | 604,807 | 4.25x | 753,472 | xz-9e | **win** +19.73% |
| Shell | source-code | 8 | 15,301 | 7,380 | 2.07x | 7,367 | brotli-11 | tie -0.18% |
| SourceMap | minified-web | 1 | 589,892 | 59,339 | 9.94x | 68,917 | brotli-11 | **win** +13.90% |
| Syslog | log | 1 | 543,360 | 15,329 | 35.45x | 15,336 | xz-9e | tie +0.05% |
| TOML(synth) | config-ser | 1 | 65,536 | 7,751 | 8.46x | 8,068 | bzip2-9 | **win** +3.93% |
| TSV | tabular | 1 | 577,428 | 56,750 | 10.17x | 84,158 | bzip2-9 | **win** +32.57% |
| Terraform | config-ser | 1 | 6,403 | 1,673 | 3.83x | 1,800 | brotli-11 | **win** +7.06% |
| TypeScript | source-code | 5 | 1,139,444 | 137,187 | 8.31x | 161,267 | bzip2-9 | **win** +14.93% |
| WASM | binary | 2 | 3,092,032 | 924,637 | 3.34x | 938,404 | xz-9e | **win** +1.47% |
| XML | config-ser | 1 | 46,516 | 4,111 | 11.32x | 5,909 | brotli-11 | **win** +30.43% |
| YAML | config-ser | 3 | 37,224 | 4,774 | 7.80x | 4,981 | brotli-11 | **win** +4.16% |
| YAML-CRD (k8s, large nested) | config-ser | 2 | 5,130,443 | 89,169 | 57.54x | 115,025 | brotli-11 | **win** +22.48% |
| reStructuredText | markup-doc | 1 | 18,965 | 5,970 | 3.18x | 5,965 | brotli-11 | tie -0.08% |

---

*Statistics: micro = Σorig/Σcomp (size-weighted). macro = mean of per-file values (one file, one vote). macro-grp = mean over source groups of each group's mean (one domain, one vote). geomean = geometric mean of per-file ratios. CI = 2,000-resample percentile bootstrap, fixed seed. Framing threshold 32 B, declared above.*
