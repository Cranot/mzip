# SCOREBOARD — every measurement, every axis, WIN/LOSS against what HuggingFace stores today

Xet Auto = xet-core's `compression_scheme.rs` + `bg4_prediction.rs` + `byte_grouping`, compiled
verbatim against lz4_flex 0.13.0. Every arm ALTERNATES on one pinned core, per file, so load
hits all arms equally. 64 KiB chunks, 3 reps, every chunk round-trip verified. MB/s of input,
one thread. Samples are 8 MiB element-aligned ranges of real Hub files (`weights2/slices.json`
records repo, dtype, offset, tensor name and sha256 for each).

`plane-entropy` is the standalone Apache-2.0 tensor coder (the drop-in scheme candidate);
`mzip FAST` is the whole compressor, which handles every class.

## A. Best contender per file

| file | class | ratio Xet -> ours | SIZE | compress Xet -> ours | COMPRESS | decompress Xet -> ours | DECOMPRESS | arm |
|---|---|---|---|---|---|---|---|---|
| bert-base | F32-nlp | 1.0722 -> 1.2097 | **WIN -11.4%** | 687 -> 1097 | **WIN 1.60x** | 2255 -> 3361 | **WIN 1.49x** | plane-entropy |
| bge-small | F32-embed | 1.5445 -> 2.3501 | **WIN -34.3%** | 462 -> 1081 | **WIN 2.34x** | 1966 -> 2061 | **WIN 1.05x** | plane-entropy |
| gpt2 | F32-llm | 1.0351 -> 1.1908 | **WIN -13.1%** | 769 -> 1079 | **WIN 1.40x** | 2381 -> 3305 | **WIN 1.39x** | plane-entropy |
| MiniLM | F32-embed | 1.0529 -> 1.1982 | **WIN -12.1%** | 705 -> 1094 | **WIN 1.55x** | 2267 -> 3354 | **WIN 1.48x** | plane-entropy |
| weights-fp32 | F32-llm | 1.1347 -> 1.3619 | **WIN -16.7%** | 706 -> 1025 | **WIN 1.45x** | 2564 -> 2566 | **WIN 1.00x** | plane-entropy |
| gpt2-w2 | F32-llm | 2.0740 -> 2.2181 | **WIN -6.5%** | 1032 -> 1174 | **WIN 1.14x** | 2762 -> 2523 | LOSS 1.1x | plane-entropy |
| vit-base | F32-vision | 1.0505 -> 1.1977 | **WIN -12.3%** | 708 -> 1074 | **WIN 1.52x** | 2258 -> 3244 | **WIN 1.44x** | plane-entropy |
| wav2vec2-base | F32-audio | 1.4785 -> 2.3209 | **WIN -36.3%** | 509 -> 1054 | **WIN 2.07x** | 2008 -> 2042 | **WIN 1.02x** | plane-entropy |
| clip-vitL14 | F32-vislang | 1.4920 -> 2.3326 | **WIN -36.0%** | 488 -> 1090 | **WIN 2.23x** | 1931 -> 2057 | **WIN 1.07x** | plane-entropy |
| sd15-unet | F32-diffus | 1.0551 -> 1.1997 | **WIN -12.1%** | 686 -> 890 | **WIN 1.30x** | 2059 -> 2849 | **WIN 1.38x** | plane-entropy |
| weights-bf16 | BF16-llm | 1.1349 -> 1.4821 | **WIN -23.4%** | 466 -> 1025 | **WIN 2.20x** | 1926 -> 2284 | **WIN 1.19x** | plane-entropy |
| Qwen2.5-0.5B | BF16-llm | 1.1583 -> 1.4954 | **WIN -22.5%** | 446 -> 1043 | **WIN 2.34x** | 1883 -> 2393 | **WIN 1.27x** | plane-entropy |
| qwen2.5-1.5b | BF16-llm | 1.1344 -> 1.4834 | **WIN -23.5%** | 469 -> 1029 | **WIN 2.19x** | 1908 -> 2329 | **WIN 1.22x** | plane-entropy |
| tinyllama-1.1b | BF16-llm | 1.1514 -> 1.4930 | **WIN -22.9%** | 448 -> 1021 | **WIN 2.28x** | 1880 -> 2310 | **WIN 1.23x** | plane-entropy |
| smolvlm | BF16-vislang | 1.1538 -> 1.4932 | **WIN -22.7%** | 444 -> 1024 | **WIN 2.31x** | 1877 -> 2321 | **WIN 1.24x** | plane-entropy |
| bloom-560m | F16-llm | 1.0000 -> 1.1789 | **WIN -15.2%** | 1157 -> 954 | LOSS 1.2x | 13473 -> 2066 | LOSS 6.5x | plane-entropy |
| llama2-7b | F16-llm | 1.0000 -> 1.5073 | **WIN -33.7%** | 1760 -> 769 | LOSS 2.3x | 14857 -> 1225 | LOSS 12.1x | plane-entropy |
| whisper-lv3 | F16-audio | 1.0263 -> 1.1661 | **WIN -12.0%** | 1038 -> 939 | LOSS 1.1x | 5616 -> 1934 | LOSS 2.9x | plane-entropy |
| sdxl-unet | F16-diffus | 1.0001 -> 1.1837 | **WIN -15.5%** | 750 -> 932 | **WIN 1.24x** | 11990 -> 2018 | LOSS 5.9x | plane-entropy |
| gguf-f16 | GGUF-F16 | 1.0006 -> 1.5012 | **WIN -33.3%** | 1721 -> 759 | LOSS 2.3x | 12533 -> 1215 | LOSS 10.3x | plane-entropy |
| gguf-q8_0 | GGUF-Q | 1.0002 -> 1.0423 | **WIN -4.0%** | 1778 -> 631 | LOSS 2.8x | 12457 -> 884 | LOSS 14.1x | plane-entropy |
| gguf-q6_k | GGUF-Q | 1.0000 -> 1.0285 | **WIN -2.8%** | 1768 -> 615 | LOSS 2.9x | 12624 -> 891 | LOSS 14.2x | plane-entropy |
| gguf-q4_k_m | GGUF-Q | 1.0002 -> 1.0041 | **WIN -0.4%** | 1757 -> 737 | LOSS 2.4x | 11499 -> 1314 | LOSS 8.7x | plane-entropy |
| gguf-q4_0 | GGUF-Q | 1.0014 -> 1.1163 | **WIN -10.3%** | 1737 -> 147 | LOSS 11.8x | 12502 -> 776 | LOSS 16.1x | mzip FAST |
| gguf-q5_k_m | GGUF-Q | 1.0000 -> 1.0044 | **WIN -0.4%** | 1781 -> 616 | LOSS 2.9x | 12039 -> 896 | LOSS 13.4x | plane-entropy |
| gguf-iq4_xs | GGUF-Q | 1.0024 -> 1.0290 | **WIN -2.6%** | 1687 -> 96 | LOSS 17.7x | 11867 -> 841 | LOSS 14.1x | mzip FAST |
| gguf-q3_k_m | GGUF-Q | 1.0000 -> 1.0001 | **WIN -0.0%** | 1155 -> 529 | LOSS 2.2x | 10281 -> 1332 | LOSS 7.7x | plane-entropy |
| tok-bert-base-uncased | tokenizer | 1.4721 -> 2.2201 | **WIN -33.7%** | 214 -> 86 | LOSS 2.5x | 1915 -> 574 | LOSS 3.3x | mzip FAST |
| tok-qwen2.5-7b-instruct | tokenizer | 2.3055 -> 2.4322 | **WIN -5.2%** | 410 -> 200 | LOSS 2.1x | 2801 -> 1021 | LOSS 2.7x | mzip FAST |
| tok-whisper-large-v3 | tokenizer | 2.5360 -> 2.4519 | LOSS +3.4% | 448 -> 212 | LOSS 2.1x | 3298 -> 1020 | LOSS 3.2x | mzip FAST |
| wikitext-2-parq | parquet | 1.0012 -> 1.1305 | **WIN -11.4%** | 1757 -> 103 | LOSS 17.0x | 11883 -> 1044 | LOSS 11.4x | mzip FAST |
| fineweb-parq | parquet | 1.0007 -> 1.1408 | **WIN -12.3%** | 1169 -> 145 | LOSS 8.1x | 10914 -> 1084 | LOSS 10.1x | mzip FAST |
| squad-parq | parquet | 1.0187 -> 1.1427 | **WIN -10.9%** | 1387 -> 147 | LOSS 9.4x | 6140 -> 1059 | LOSS 5.8x | mzip FAST |
| alpaca-instruct | text | 1.5997 -> 2.3961 | **WIN -33.2%** | 365 -> 167 | LOSS 2.2x | 2360 -> 1093 | LOSS 2.2x | mzip FAST |
| code-python | text | 2.4567 -> 3.6588 | **WIN -32.9%** | 436 -> 229 | LOSS 1.9x | 2220 -> 1234 | LOSS 1.8x | mzip FAST |
| dialogue-csv | text | 1.7999 -> 2.7389 | **WIN -34.3%** | 363 -> 174 | LOSS 2.1x | 2578 -> 1079 | LOSS 2.4x | mzip FAST |
| prose-wikitext | text | 1.7218 -> 2.6104 | **WIN -34.0%** | 336 -> 163 | LOSS 2.1x | 2258 -> 1108 | LOSS 2.0x | mzip FAST |
| qa-arc | text | 1.9425 -> 2.6127 | **WIN -25.7%** | 366 -> 170 | LOSS 2.1x | 2589 -> 998 | LOSS 2.6x | mzip FAST |
| qa-gsm8k | text | 1.6895 -> 2.5493 | **WIN -33.7%** | 364 -> 152 | LOSS 2.4x | 2562 -> 1082 | LOSS 2.4x | mzip FAST |
| tabular-numeric | text | 2.4872 -> 4.0031 | **WIN -37.9%** | 426 -> 256 | LOSS 1.7x | 1972 -> 1252 | LOSS 1.6x | mzip FAST |
| tweets-jsonl | text | 1.8773 -> 2.7152 | **WIN -30.9%** | 495 -> 194 | LOSS 2.5x | 2717 -> 1241 | LOSS 2.2x | mzip FAST |
| small2-290files | repo-json | 4.6476 -> 10.5650 | **WIN -56.0%** | 782 -> 0 | LOSS 4599.4x | 1779 -> 331 | LOSS 5.4x | mzip FAST |

## B. All arms side by side

| file | class | Xet r/enc/dec | mzip FAST r/enc/dec | plane-entropy r/enc/dec |
|---|---|---|---|---|
| bert-base | F32-nlp | 1.0722 / 687 / 2255 | 1.2096 / 234 / 1728 | 1.2097 / 1097 / 3361 |
| bge-small | F32-embed | 1.5445 / 462 / 1966 | 2.3496 / 234 / 1340 | 2.3501 / 1081 / 2061 |
| gpt2 | F32-llm | 1.0351 / 769 / 2381 | 1.1907 / 197 / 1694 | 1.1908 / 1079 / 3305 |
| MiniLM | F32-embed | 1.0529 / 705 / 2267 | 1.1980 / 240 / 1754 | 1.1982 / 1094 / 3354 |
| weights-fp32 | F32-llm | 1.1347 / 706 / 2564 | 1.3617 / 222 / 1528 | 1.3619 / 1025 / 2566 |
| gpt2-w2 | F32-llm | 2.0740 / 1032 / 2762 | 2.2177 / 191 / 1499 | 2.2181 / 1174 / 2523 |
| vit-base | F32-vision | 1.0505 / 708 / 2258 | 1.1976 / 237 / 1699 | 1.1977 / 1074 / 3244 |
| wav2vec2-base | F32-audio | 1.4785 / 509 / 2008 | 2.3204 / 231 / 1331 | 2.3209 / 1054 / 2042 |
| clip-vitL14 | F32-vislang | 1.4920 / 488 / 1931 | 2.3321 / 233 / 1318 | 2.3326 / 1090 / 2057 |
| sd15-unet | F32-diffus | 1.0551 / 686 / 2059 | 1.1995 / 208 / 1440 | 1.1997 / 890 / 2849 |
| weights-bf16 | BF16-llm | 1.1349 / 466 / 1926 | 1.4819 / 280 / 1445 | 1.4821 / 1025 / 2284 |
| Qwen2.5-0.5B | BF16-llm | 1.1583 / 446 / 1883 | 1.4952 / 283 / 1458 | 1.4954 / 1043 / 2393 |
| qwen2.5-1.5b | BF16-llm | 1.1344 / 469 / 1908 | 1.4832 / 283 / 1437 | 1.4834 / 1029 / 2329 |
| tinyllama-1.1b | BF16-llm | 1.1514 / 448 / 1880 | 1.4928 / 277 / 1445 | 1.4930 / 1021 / 2310 |
| smolvlm | BF16-vislang | 1.1538 / 444 / 1877 | 1.4930 / 281 / 1451 | 1.4932 / 1024 / 2321 |
| bloom-560m | F16-llm | 1.0000 / 1157 / 13473 | 1.1787 / 213 / 1335 | 1.1789 / 954 / 2066 |
| llama2-7b | F16-llm | 1.0000 / 1760 / 14857 | 1.5071 / 229 / 953 | 1.5073 / 769 / 1225 |
| whisper-lv3 | F16-audio | 1.0263 / 1038 / 5616 | 1.1659 / 220 / 1294 | 1.1661 / 939 / 1934 |
| sdxl-unet | F16-diffus | 1.0001 / 750 / 11990 | 1.1836 / 220 / 1341 | 1.1837 / 932 / 2018 |
| gguf-f16 | GGUF-F16 | 1.0006 / 1721 / 12533 | 1.5010 / 226 / 944 | 1.5012 / 759 / 1215 |
| gguf-q8_0 | GGUF-Q | 1.0002 / 1778 / 12457 | 1.0423 / 142 / 884 | 1.0423 / 631 / 884 |
| gguf-q6_k | GGUF-Q | 1.0000 / 1768 / 12624 | 1.0285 / 125 / 794 | 1.0285 / 615 / 891 |
| gguf-q4_k_m | GGUF-Q | 1.0002 / 1757 / 11499 | 1.0040 / 110 / 1020 | 1.0041 / 737 / 1314 |
| gguf-q4_0 | GGUF-Q | 1.0014 / 1737 / 12502 | 1.1163 / 147 / 776 | 1.1159 / 634 / 900 |
| gguf-q5_k_m | GGUF-Q | 1.0000 / 1781 / 12039 | 1.0043 / 98 / 777 | 1.0044 / 616 / 896 |
| gguf-iq4_xs | GGUF-Q | 1.0024 / 1687 / 11867 | 1.0290 / 96 / 841 | 1.0287 / 627 / 917 |
| gguf-q3_k_m | GGUF-Q | 1.0000 / 1155 / 10281 | 1.0000 / 85 / 1012 | 1.0001 / 529 / 1332 |
| tok-bert-base-uncased | tokenizer | 1.4721 / 214 / 1915 | 2.2201 / 86 / 574 | n/a |
| tok-qwen2.5-7b-instruct | tokenizer | 2.3055 / 410 / 2801 | 2.4322 / 200 / 1021 | n/a |
| tok-whisper-large-v3 | tokenizer | 2.5360 / 448 / 3298 | 2.4519 / 212 / 1020 | n/a |
| wikitext-2-parq | parquet | 1.0012 / 1757 / 11883 | 1.1305 / 103 / 1044 | n/a |
| fineweb-parq | parquet | 1.0007 / 1169 / 10914 | 1.1408 / 145 / 1084 | n/a |
| squad-parq | parquet | 1.0187 / 1387 / 6140 | 1.1427 / 147 / 1059 | n/a |
| alpaca-instruct | text | 1.5997 / 365 / 2360 | 2.3961 / 167 / 1093 | n/a |
| code-python | text | 2.4567 / 436 / 2220 | 3.6588 / 229 / 1234 | n/a |
| dialogue-csv | text | 1.7999 / 363 / 2578 | 2.7389 / 174 / 1079 | n/a |
| prose-wikitext | text | 1.7218 / 336 / 2258 | 2.6104 / 163 / 1108 | n/a |
| qa-arc | text | 1.9425 / 366 / 2589 | 2.6127 / 170 / 998 | n/a |
| qa-gsm8k | text | 1.6895 / 364 / 2562 | 2.5493 / 152 / 1082 | n/a |
| tabular-numeric | text | 2.4872 / 426 / 1972 | 4.0031 / 256 / 1252 | n/a |
| tweets-jsonl | text | 1.8773 / 495 / 2717 | 2.7152 / 194 / 1241 | n/a |
| small2-290files | repo-json | 4.6476 / 782 / 1779 | 10.5650 / 0 / 331 | n/a |

## C. By class

| class | files | Xet ratio | ours | SIZE | Xet enc | ours | COMPRESS | Xet dec | ours | DECOMPRESS | arm |
|---|---:|---:|---:|---|---:|---:|---|---:|---:|---|---|
| F32-nlp | 1 | 1.0722 | 1.2097 | **WIN -11.4%** | 687 | 1097 | **WIN 1.60x** | 2255 | 3361 | **WIN 1.49x** | plane-entropy |
| F32-embed | 2 | 1.2522 | 1.5872 | **WIN -21.1%** | 558 | 1087 | **WIN 1.95x** | 2106 | 2553 | **WIN 1.21x** | plane-entropy |
| F32-llm | 3 | 1.2878 | 1.4816 | **WIN -13.1%** | 814 | 1089 | **WIN 1.34x** | 2560 | 2756 | **WIN 1.08x** | plane-entropy |
| F32-vision | 1 | 1.0505 | 1.1977 | **WIN -12.3%** | 708 | 1074 | **WIN 1.52x** | 2258 | 3244 | **WIN 1.44x** | plane-entropy |
| F32-audio | 1 | 1.4785 | 2.3209 | **WIN -36.3%** | 509 | 1054 | **WIN 2.07x** | 2008 | 2042 | **WIN 1.02x** | plane-entropy |
| F32-vislang | 1 | 1.4920 | 2.3326 | **WIN -36.0%** | 488 | 1090 | **WIN 2.23x** | 1931 | 2057 | **WIN 1.07x** | plane-entropy |
| F32-diffus | 1 | 1.0551 | 1.1997 | **WIN -12.1%** | 686 | 890 | **WIN 1.30x** | 2059 | 2849 | **WIN 1.38x** | plane-entropy |
| BF16-llm | 4 | 1.1447 | 1.4885 | **WIN -23.1%** | 457 | 1029 | **WIN 2.25x** | 1899 | 2328 | **WIN 1.23x** | plane-entropy |
| BF16-vislang | 1 | 1.1538 | 1.4932 | **WIN -22.7%** | 444 | 1024 | **WIN 2.31x** | 1877 | 2321 | **WIN 1.24x** | plane-entropy |
| F16-llm | 2 | 1.0000 | 1.3230 | **WIN -24.4%** | 1396 | 852 | LOSS 1.6x | 14131 | 1538 | LOSS 9.2x | plane-entropy |
| F16-audio | 1 | 1.0263 | 1.1661 | **WIN -12.0%** | 1038 | 939 | LOSS 1.1x | 5616 | 1934 | LOSS 2.9x | plane-entropy |
| F16-diffus | 1 | 1.0001 | 1.1837 | **WIN -15.5%** | 750 | 932 | **WIN 1.24x** | 11990 | 2018 | LOSS 5.9x | plane-entropy |
| GGUF-F16 | 1 | 1.0006 | 1.5012 | **WIN -33.3%** | 1721 | 759 | LOSS 2.3x | 12533 | 1215 | LOSS 10.3x | plane-entropy |
| GGUF-Q | 7 | 1.0006 | 1.0307 | **WIN -2.9%** | 1631 | 622 | LOSS 2.6x | 11843 | 988 | LOSS 12.0x | plane-entropy |
| tokenizer | 3 | 1.9902 | 2.3633 | **WIN -15.8%** | 321 | 140 | LOSS 2.3x | 2537 | 811 | LOSS 3.1x | mzip FAST |
| parquet | 3 | 1.0068 | 1.1380 | **WIN -11.5%** | 1398 | 128 | LOSS 10.9x | 8859 | 1062 | LOSS 8.3x | mzip FAST |
| text | 8 | 1.9003 | 2.8249 | **WIN -32.7%** | 388 | 183 | LOSS 2.1x | 2383 | 1129 | LOSS 2.1x | mzip FAST |
| repo-json | 1 | 4.6476 | 10.5650 | **WIN -56.0%** | 782 | 0 | LOSS 4599.4x | 1779 | 331 | LOSS 5.4x | mzip FAST |

## D. Every LOSS, and the measured lever that converts it

| # | loss | where | size of it | lever | measured potential | grade |
|---|---|---|---|---|---|---|
| 1 | **repo JSON compress, 0.17 MB/s** | 290 real repo files | 4,599x slower than Xet — the worst cell on the board | zstd + a domain dictionary instead of the BWT/CM path | 12.4x ratio (FAST gets 10.6x) at ~4,000x FAST's speed; zstd-6..9 + dict reaches BALANCED's 14.2x at 118-204 MB/s | [I] |
| 2 | **GGUF quantised ratio, 2.9%** | 7 quant types; q3_K_M gets 1.0000, exactly nothing | 28.8% of Hub weight bytes | parse the ggml block layout, code each field order-0 | Q8_0 1.042->1.059, Q6_K 1.029->1.051, Q4_K 1.023->1.051, Q5_0 1.004->1.065; 4.8-6.1% below what Xet stores; 5-6x faster to compress than today | [I] |
| 3 | **text compress and decompress, 2.1x** | 8 dataset corpora | dataset side of the Hub | drop mzip FAST's detours around zstd-3 on text | -0.87% bytes AND 1.32x compress, 1.27x decompress; plus a text-gate misroute costing 3.0% on alpaca and 9.8% on qa-arc | [I] |
| 4 | **parquet compress, 10.9x** | 3 parquet files | dataset side | page-level SNAPPY -> zstd transcode (restore-verified byte-identical) | ratio 1.13 -> 1.74 (-35.2% vs our own FAST, -42.6% vs what Xet stores); speed stays a loss, so this is an at-rest play | [I] |
| 5 | **whisper tokenizer.json ratio, +3.4%** | 1 file — the only outright RATIO loss on the board | small | unknown; the text path picks worse than plain LZ4 on this file | untested — this is the one loss with no lever yet | [H] |
| 6 | **F16 / GGUF decode, 3-12x** | Xet stores these raw, so its "decode" is memcpy | inherent to the trade | zstd's BMI2-assembly huff0 decoder (same bitstream) | +26-29% decode everywhere: 1,215 -> ~1,550 MB/s on GGUF F16 | [I] |
| 7 | **mzip FAST is 4.5x slower than the coder it contains** | every weight row: FAST 234 MB/s vs plane-entropy 1,097 on the same file | all of mzip | port the package's implementation back into mzip; and cut the two verify passes | framing is ~78% of FAST's compress time here; the two verifies alone are +53% / +19% / +84% | [I] |

## E. Where we could win MORE (headroom on cells we already win)

| where | current | lever | measured potential | grade |
|---|---|---|---|---|
| all safetensors | ratio 1.20-2.33 | rol-1 bit-aligned exponent plane, with a sign-entropy guard | -0.3% to -1.1% more bytes, never loses on the 8-file corpus | [I] |
| F32 weights | ratio | previous-row context on the exponent plane | -0.70% on one F32 file, -0.27% on another, ~0 elsewhere | [I] |
| all decode | 2.0-3.4 GB/s | zstd's asm huff0 | +26-29% | [I] |
| F16 models | Xet stores them RAW (1.0000) | nothing needed — this is free ground | we already take 12-24% off; it is the cleanest unclaimed class on the Hub | [M] |

Grades: [M] measured and re-run by this session; [I] one scout, artifacts on the box, NOT
independently re-run (the refutation pass never survived the API limits); [H] hypothesis.
