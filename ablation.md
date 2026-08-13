# Candidate-family ablation (A0–A8)

Measured with `./mzip_cm.exe` over 102 file(s) of corpus `DEV-2026-08`. Every candidate the encoder considered was logged, winner or loser.


## Are these counterfactuals real?

The costs below are computed from logged candidate sizes, so they are a PREDICTION about what would ship if a family did not exist. Predictions here get confronted with reality: `--verify F` re-runs the encoder over every file with F actually masked off and compares the REAL archive size against the predicted one. Three families have been through it (2026-08-13):

| family | mask | files agreeing | mismatches |
|---|---|---|---|
| A5_GRID | `0x1DF` | **102 / 102** | 0 |
| A8_FILTER | `0x0FF` | **102 / 102** | 0 |
| A4_NUMERIC | `0x1EF` | **102 / 102** | 0 |

Those three are the largest contributors (418,875 + 645,475 + 384,417 B), i.e. **1.45 MB of the 1.50 MB total is now confirmed against real masked runs, not just derived.** The remaining six rest on the same `mz_family_on()` guard — one line per candidate, the same code path all three exercised — but have not individually been re-run, and that distinction is kept rather than rounded away.


## What each family is worth

`cost` = bytes the archive would GROW by if the family did not exist (`ship(without F) − ship(all)`). A family can win files and still cost 0: that means the runner-up was just as good, so the win was cosmetic and the family is not load-bearing there.

| family | fires on | wins | LOAD-BEARING (cost>0) | total cost (B) | worst single file |
|---|---|---|---|---|---|
| A0_STORE | 102 | 0 | 0 | 0 | — |
| A1_GENERAL | 102 | 0 | 0 | 0 | — |
| A2_CONTAINER | 48 | 43 | 43 | 408 | `dashboard.html` +10 |
| A3_BWT | 6 | 0 | 0 | 0 | — |
| A4_NUMERIC | 5 | 3 | 3 | 384,417 | `test1.wav` +162,979 |
| A5_GRID | 6 | 6 | 6 | 418,875 | `synthetic_fem.mtx` +148,676 |
| A6_RECORD | 3 | 3 | 3 | 39,261 | `reads_10k.fastq` +27,364 |
| A7_TEMPORAL | 3 | 3 | 3 | 16,349 | `app.log` +6,401 |
| A8_FILTER | 55 | 21 | 21 | 645,475 | `board.tga` +149,389 |

## Families that never fire, and families that fire but never matter

- **Never fired on this corpus:** none
- **Fired but cost 0 everywhere** (removing it would not change one archive byte): A0_STORE, A1_GENERAL, A3_BWT

Both are honest negatives about this corpus, NOT about the format: a family that never fires here may be the only thing that works on data this corpus does not contain.


## Per-domain: which family carries which kind of data

| domain | files | dominant winner | total cost of the top family |
|---|---|---|---|
| binary | 9 | A8_FILTER (5/9) | 8,365 B |
| config-ser | 18 | A2_CONTAINER (13/18) | 122 B |
| genomics | 1 | A6_RECORD (1/1) | 27,364 B |
| log | 4 | A7_TEMPORAL (3/4) | 16,349 B |
| markup-doc | 8 | A2_CONTAINER (5/8) | 49 B |
| media | 7 | A8_FILTER (6/7) | 606,685 B |
| minified-web | 3 | --FALLBACK (3/3) | 0 B |
| numeric | 4 | --FALLBACK (2/4) | 0 B |
| scientific | 1 | A5_GRID (1/1) | 148,676 B |
| source-code | 35 | A2_CONTAINER (22/35) | 209 B |
| sql | 3 | A6_RECORD (2/3) | 11,897 B |
| tabular | 6 | A5_GRID (5/6) | 270,199 B |
| text-misc | 3 | A2_CONTAINER (2/3) | 19 B |

## Files where a specialized family is load-bearing

| file | domain | ships as | family | archive | without that family | cost |
|---|---|---|---|---|---|---|
| `corpus_extra/audio/test1.wav` | media | MS | A4_NUMERIC | 728,152 | 891,131 | **+162,979** (22.4%) |
| `corpus_extra/raster/board.tga` | media | MI | A8_FILTER | 565,250 | 714,639 | **+149,389** (26.4%) |
| `corpus_extra/raster/board.bmp` | media | MI | A8_FILTER | 565,264 | 714,648 | **+149,384** (26.4%) |
| `corpus_extra/grids/synthetic_fem.mtx` | scientific | MM | A5_GRID | 604,807 | 753,483 | **+148,676** (24.6%) |
| `corpus_extra/phonegyro_sensor.bin` | numeric | MS | A4_NUMERIC | 411,287 | 537,371 | **+126,084** (30.7%) |
| `corpus_extra/delim/stocks_semi.csv` | tabular | MT | A5_GRID | 390,877 | 509,254 | **+118,377** (30.3%) |
| `corpus_extra/delim/stocks_pipe.csv` | tabular | MT | A5_GRID | 390,877 | 509,137 | **+118,260** (30.3%) |
| `corpus_extra/raster/baboon.ppm` | media | MI | A8_FILTER | 553,466 | 651,091 | **+97,625** (17.6%) |
| `corpus_extra/raster/baboon.bmp` | media | MI | A8_FILTER | 553,661 | 650,805 | **+97,144** (17.5%) |
| `corpus_extra/raster/baboon.tga` | media | MI | A8_FILTER | 553,654 | 650,798 | **+97,144** (17.5%) |
| `corpus_extra/tsgas_series.bin` | numeric | MS | A4_NUMERIC | 142,610 | 237,964 | **+95,354** (66.9%) |
| `corpus_extra/fastq/reads_10k.fastq` | genomics | MF | A6_RECORD | 197,237 | 224,601 | **+27,364** (13.9%) |
| `corpus_extra/yaml/prom_bundle.yaml` | config-ser | MY | A8_FILTER | 68,642 | 91,319 | **+22,677** (33.0%) |
| `real_bench/events.csv` | tabular | MT | A5_GRID | 56,771 | 76,894 | **+20,123** (35.4%) |
| `corpus_extra/raster/fruits.pgm` | media | MI | A8_FILTER | 114,016 | 130,015 | **+15,999** (14.0%) |
| `corpus_extra/misc/events.tsv` | tabular | MT | A5_GRID | 56,750 | 69,428 | **+12,678** (22.3%) |
| `corpus_extra/sqlbig/bigint_unsigned.sql` | sql | MQ | A6_RECORD | 3,746 | 10,536 | **+6,790** (181.3%) |
| `corpus_extra/yaml/certmgr.yaml` | config-ser | MY | A8_FILTER | 20,527 | 27,098 | **+6,571** (32.0%) |
| `real_bench/app.log` | log | ML | A7_TEMPORAL | 49,320 | 55,721 | **+6,401** (13.0%) |
| `real_bench/apache_log_sample.log` | log | ML | A7_TEMPORAL | 81,905 | 87,415 | **+5,510** (6.7%) |
| `corpus_extra/bin/libbrotlienc.dll` | binary | MB | A8_FILTER | 259,914 | 265,393 | **+5,479** (2.1%) |
| `real_bench/users_dump.sql` | sql | MQ | A6_RECORD | 9,721 | 14,828 | **+5,107** (52.5%) |
| `real_bench/nginx_access.log` | log | ML | A7_TEMPORAL | 27,851 | 32,289 | **+4,438** (15.9%) |
| `corpus_extra/bin/liblzma-5.dll` | binary | MB | A8_FILTER | 85,978 | 87,605 | **+1,627** (1.9%) |
| `corpus_extra/bin/libwinpthread-1.dll` | binary | MB | A8_FILTER | 23,170 | 24,193 | **+1,023** (4.4%) |
| `corpus_extra/ts/typescript_types.d.ts` | source-code | MY | A8_FILTER | 66,550 | 67,505 | **+955** (1.4%) |
| `corpus_extra/misc/users.ndjson` | tabular | MT | A5_GRID | 15,791 | 16,552 | **+761** (4.8%) |
| `corpus_extra/bin/libbrotlidec.dll` | binary | MB | A8_FILTER | 25,051 | 25,266 | **+215** (0.9%) |
| `real_bench/django_models.py` | source-code | MY | A8_FILTER | 15,536 | 15,666 | **+130** (0.8%) |
| `real_bench/clojure_core.clj` | source-code | MY | A8_FILTER | 48,555 | 48,606 | **+51** (0.1%) |
| `real_bench/swift_stdlib.swift` | source-code | MY | A8_FILTER | 15,867 | 15,889 | **+22** (0.1%) |
| `corpus_extra/bin/libbrotlicommon.dll` | binary | MB | A8_FILTER | 34,319 | 34,340 | **+21** (0.1%) |
| `real_bench/dashboard.html` | markup-doc | MU | A2_CONTAINER | 1,269 | 1,279 | **+10** (0.8%) |
| `real_bench/styles.css` | markup-doc | MU | A2_CONTAINER | 1,953 | 1,963 | **+10** (0.5%) |
| `real_bench/handlers.ts` | source-code | MU | A2_CONTAINER | 1,103 | 1,113 | **+10** (0.9%) |
| `real_bench/vscode_main.ts` | source-code | MU | A2_CONTAINER | 4,589 | 4,599 | **+10** (0.2%) |
| `real_bench/xml_maven.xml` | config-ser | MU | A2_CONTAINER | 4,111 | 4,121 | **+10** (0.2%) |
| `real_bench/api_docs.md` | markup-doc | MU | A2_CONTAINER | 1,021 | 1,031 | **+10** (1.0%) |
| `real_bench/flask_app.py` | source-code | MU | A2_CONTAINER | 13,390 | 13,400 | **+10** (0.1%) |
| `real_bench/services.py` | source-code | MU | A2_CONTAINER | 1,124 | 1,134 | **+10** (0.9%) |
