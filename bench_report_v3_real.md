# mzip benchmark — matrix report

Generated from `bench_matrix.jsonl` by `bench_report.py`. **No compressor was run to produce this file.** Every number is a pure function of the matrix and can be recomputed from it.


## Corpus

- **Label:** `DEV-2026-08`  ·  **blind:** `False`
- Development corpus: mzip's encoders were built and tuned against these files. Held out from mzip's dictionary training (train_corpus/), NOT held out from encoder development. Not a blind evaluation.
- **This report covers 93 file(s)** (93 real, 0 generated) across 43 content type(s) and 12 source group(s).
- ⚠ **SUBSET.** The declared corpus has 102 files across 52 types; this run measured 93. Nothing here describes the full corpus.
- ⚠ **This report is restricted to REAL files** (`--real-only`).
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
| `gzip-9` | 93 | 0 | — |
| `bzip2-9` | 93 | 0 | — |
| `brotli-11` | 93 | 0 | — |
| `xz-9e` | 93 | 0 | — |
| `zstd-19` | 93 | 0 | — |
| `zstd-22` | 93 | 0 | — |
| `zstd-19+dict` | 93 | 0 | — |
| `mzip(noCM)` | 93 | 0 | — |
| `mzip+CM` | 93 | 0 | — |

**Roundtrip:** 558 observations independently decompressed and matched the input byte for byte; 0 failed. (The legacy harness verified mzip only and trusted every comparator.)


## Head-to-head: mzip+CM vs each tool

`ratio` = other/mzip per file (>1 means mzip is smaller). `n` is the number of files where BOTH sides produced a valid observation; `dropped` is how many were excluded and why.

| vs | n | dropped | micro | macro | macro-grp | geomean | 95% CI (geo) | median | p10 | p90 |
|---|---|---|---|---|---|---|---|---|---|---|
| `gzip-9` | 93 | 0 | 1.826x | 1.733x | 2.230x | **1.505x** | 1.399–1.644 | 1.349x | 1.144x | 2.108x |
| `bzip2-9` | 93 | 0 | 1.646x | 1.599x | 2.056x | **1.341x** | 1.257–1.469 | 1.225x | 1.141x | 1.547x |
| `brotli-11` | 93 | 0 | 1.399x | 1.323x | 1.643x | **1.199x** | 1.134–1.289 | 1.114x | 0.997x | 1.456x |
| `xz-9e` | 93 | 0 | 1.370x | 1.417x | 1.719x | **1.299x** | 1.231–1.397 | 1.221x | 1.136x | 1.508x |
| `zstd-19` | 93 | 0 | 1.556x | 1.484x | 1.851x | **1.335x** | 1.259–1.439 | 1.230x | 1.119x | 1.618x |
| `zstd-22` | 93 | 0 | 1.554x | 1.483x | 1.850x | **1.334x** | 1.258–1.439 | 1.229x | 1.119x | 1.617x |
| `zstd-19+dict` | 93 | 0 | 1.546x | 1.435x | 1.813x | **1.284x** | 1.210–1.386 | 1.168x | 1.060x | 1.595x |
| `mzip(noCM)` | 93 | 0 | 1.044x | 1.024x | 1.026x | **1.020x** | 1.009–1.039 | 1.000x | 1.000x | 1.050x |

## Compression ratio (original / compressed)

Computed only over files where the tool produced a valid observation, so the columns can have different denominators — `n` is stated for each.

| tool | n | micro | macro | macro-grp | geomean | median |
|---|---|---|---|---|---|---|
| `gzip-9` | 93 | 2.816x | 4.620x | 4.688x | 3.700x | 3.493x |
| `bzip2-9` | 93 | 3.125x | 5.574x | 6.240x | 4.151x | 3.728x |
| `brotli-11` | 93 | 3.676x | 6.459x | 6.816x | 4.644x | 4.157x |
| `xz-9e` | 93 | 3.753x | 6.015x | 6.685x | 4.286x | 3.864x |
| `zstd-19` | 93 | 3.306x | 5.787x | 6.181x | 4.172x | 3.691x |
| `zstd-22` | 93 | 3.310x | 5.792x | 6.186x | 4.175x | 3.691x |
| `zstd-19+dict` | 93 | 3.327x | 6.013x | 6.261x | 4.335x | 3.812x |
| `mzip(noCM)` | 93 | 4.924x | 8.085x | 9.862x | 5.457x | 4.697x |
| `mzip+CM` | 93 | 5.143x | 8.211x | 10.114x | 5.568x | 4.697x |

## Per-file verdict vs the best standard tool

**FRAMING NOTE — declared, not implicit.** mzip emits an *archive* (magic + type + length); the comparators emit a raw stream with no container. A per-file difference of **≤ 32 B** is therefore recorded as a TIE rather than a win or a loss, because at that scale the container header, not the compression, decides the sign. The threshold is stated here so a reader can recompute with a different one.

Over 93 of 93 files: **70 wins, 23 ties, 0 losses** (threshold 32 B).

⚠ On 2 of those files the best comparator was an unverified **sizer** (`zstd-*`), so the bar mzip was measured against is a reported number with no artifact behind it.


No losses.


## Leave-one-domain-out

The geometric-mean head-to-head recomputed with each source group DELETED. A large swing means the headline was carried by that family and should not be quoted as corpus-wide.

| removed | n files | vs gzip-9 | vs bzip2-9 | vs brotli-11 | vs xz-9e | vs zstd-19 | vs zstd-22 | vs zstd-19+dict | vs mzip(noCM) |
|---|---|---|---|---|---|---|---|---|---|
| *(none — full corpus)* | 93 | 1.505x | 1.341x | 1.199x | 1.299x | 1.335x | 1.334x | 1.284x | 1.020x |
| binary | 85 | 1.519x (+0.9%) | 1.345x (+0.3%) | 1.214x (+1.3%) | 1.319x (+1.6%) | 1.348x (+1.0%) | 1.347x (+1.0%) | 1.293x (+0.7%) | 1.022x (+0.1%) |
| config-ser | 78 | 1.505x (-0.0%) | 1.339x (-0.2%) | 1.220x (+1.8%) | 1.310x (+0.8%) | 1.354x (+1.5%) | 1.353x (+1.5%) | 1.305x (+1.6%) | 1.024x (+0.4%) |
| genomics | 92 | 1.499x (-0.4%) | 1.341x (-0.0%) | 1.198x (-0.1%) | 1.299x (+0.0%) | 1.334x (-0.0%) | 1.334x (-0.0%) | 1.284x (-0.1%) | 1.020x (-0.0%) |
| log | 90 | 1.486x (-1.2%) | 1.341x (-0.1%) | 1.189x (-0.9%) | 1.291x (-0.6%) | 1.326x (-0.7%) | 1.325x (-0.7%) | 1.274x (-0.8%) | 1.020x (-0.0%) |
| markup-doc | 85 | 1.513x (+0.5%) | 1.355x (+1.0%) | 1.209x (+0.8%) | 1.304x (+0.4%) | 1.340x (+0.4%) | 1.340x (+0.4%) | 1.290x (+0.4%) | 1.022x (+0.2%) |
| media | 86 | 1.507x (+0.2%) | 1.342x (+0.0%) | 1.191x (-0.7%) | 1.297x (-0.2%) | 1.326x (-0.6%) | 1.325x (-0.6%) | 1.272x (-0.9%) | 1.018x (-0.3%) |
| minified-web | 90 | 1.508x (+0.2%) | 1.347x (+0.4%) | 1.201x (+0.2%) | 1.303x (+0.3%) | 1.338x (+0.3%) | 1.337x (+0.3%) | 1.287x (+0.2%) | 1.021x (+0.1%) |
| numeric | 89 | 1.454x (-3.4%) | 1.302x (-2.9%) | 1.163x (-3.0%) | 1.270x (-2.3%) | 1.295x (-3.0%) | 1.294x (-3.0%) | 1.244x (-3.1%) | 1.016x (-0.4%) |
| source-code | 58 | 1.669x (+10.9%) ⚠ | 1.421x (+5.9%) | 1.274x (+6.2%) | 1.349x (+3.9%) | 1.426x (+6.9%) | 1.425x (+6.8%) | 1.398x (+8.8%) | 1.020x (-0.0%) |
| sql | 91 | 1.466x (-2.6%) | 1.310x (-2.4%) | 1.173x (-2.2%) | 1.273x (-2.0%) | 1.305x (-2.2%) | 1.304x (-2.2%) | 1.255x (-2.3%) | 1.021x (+0.0%) |
| tabular | 87 | 1.489x (-1.1%) | 1.343x (+0.2%) | 1.187x (-1.0%) | 1.294x (-0.4%) | 1.324x (-0.8%) | 1.323x (-0.8%) | 1.272x (-1.0%) | 1.020x (-0.1%) |
| text-misc | 92 | 1.509x (+0.3%) | 1.343x (+0.1%) | 1.201x (+0.2%) | 1.301x (+0.1%) | 1.337x (+0.2%) | 1.336x (+0.2%) | 1.287x (+0.2%) | 1.021x (+0.0%) |

## By source group

| group | files | mzip+CM ratio (geo) | vs best std (geo) | win | tie | loss |
|---|---|---|---|---|---|---|
| binary | 8 | 3.22x | 1.029x | 6 | 2 | 0 |
| config-ser | 15 | 7.09x | 1.091x | 9 | 6 | 0 |
| genomics | 1 | 12.96x | 1.252x | 1 | 0 | 0 |
| log | 3 | 16.23x | 1.323x | 3 | 0 | 0 |
| markup-doc | 8 | 7.34x | 1.064x | 5 | 3 | 0 |
| media | 7 | 1.62x | 1.279x | 7 | 0 | 0 |
| minified-web | 3 | 7.40x | 1.131x | 3 | 0 | 0 |
| numeric | 4 | 9.40x | 2.057x | 4 | 0 | 0 |
| source-code | 35 | 4.91x | 1.073x | 23 | 12 | 0 |
| sql | 2 | 23.41x | 2.893x | 2 | 0 | 0 |
| tabular | 6 | 7.94x | 1.278x | 6 | 0 | 0 |
| text-misc | 1 | 3.62x | 1.025x | 1 | 0 | 0 |

## By content type

| type | group | files | orig | mzip+CM | ratio | best std | which | verdict |
|---|---|---|---|---|---|---|---|---|
| Audio (WAV/PCM) | media | 1 | 1,281,256 | 728,152 | 1.76x | 1,046,052 | xz-9e | **win** +30.39% |
| Binary-ARM/PPC | binary | 1 | 5,372,896 | 1,216,935 | 4.42x | 1,317,580 | xz-9e | **win** +7.64% |
| Binary-x86 | binary | 5 | 1,280,445 | 428,432 | 2.99x | 444,926 | brotli-11 | **win** +3.71% |
| C/C++ | source-code | 2 | 302,258 | 59,479 | 5.08x | 68,491 | bzip2-9 | **win** +13.16% |
| CSS | markup-doc | 2 | 300,380 | 23,039 | 13.04x | 27,360 | brotli-11 | **win** +15.79% |
| CSV | tabular | 1 | 592,430 | 56,771 | 10.44x | 83,998 | bzip2-9 | **win** +32.41% |
| Delimited (semicolon/pipe CSV) | tabular | 2 | 3,999,992 | 781,754 | 5.12x | 1,027,476 | xz-9e | **win** +23.92% |
| Diff/patch | text-misc | 1 | 9,117 | 2,516 | 3.62x | 2,579 | brotli-11 | **win** +2.44% |
| Dockerfile | config-ser | 1 | 4,156 | 1,367 | 3.04x | 1,362 | brotli-11 | tie -0.37% |
| Env/dotfiles | config-ser | 2 | 12,049 | 3,219 | 3.74x | 3,209 | brotli-11 | tie -0.31% |
| FASTQ (genomics) | genomics | 1 | 2,555,256 | 197,237 | 12.96x | 246,924 | xz-9e | **win** +20.12% |
| Go | source-code | 2 | 145,460 | 31,640 | 4.60x | 35,232 | zstd-19+dict | **win** +10.20% |
| HTML | markup-doc | 1 | 43,537 | 1,269 | 34.31x | 1,377 | brotli-11 | **win** +7.84% |
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
| Protobuf-schema | config-ser | 1 | 61,115 | 12,611 | 4.85x | 13,596 | brotli-11 | **win** +7.24% |
| Python | source-code | 3 | 186,549 | 30,050 | 6.21x | 34,476 | brotli-11 | **win** +12.84% |
| Raster (BMP/PPM/TGA uncompressed) | media | 6 | 4,448,482 | 2,905,311 | 1.53x | 3,657,317 | brotli-11 | **win** +20.56% |
| SQL | sql | 2 | 259,523 | 9,922 | 26.16x | 16,244 | xz-9e | **win** +38.92% |
| SVG | markup-doc | 1 | 10,009 | 4,059 | 2.47x | 4,060 | brotli-11 | tie +0.02% |
| Shell | source-code | 8 | 15,301 | 7,380 | 2.07x | 7,367 | brotli-11 | tie -0.18% |
| SourceMap | minified-web | 1 | 589,892 | 59,339 | 9.94x | 68,917 | brotli-11 | **win** +13.90% |
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
