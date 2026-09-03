
### FINAL STATE 2026-09-02 (all runs complete)
v7 FAST 64K: 48-63 MB/s compress per core, 0.7-1.5 GB/s decode, 16K..whole sweep in RESULT 27.
Identity: v5/v6/v7 BALANCED = pre-fix on 4 weight slices; corpora (50 MB text) pre vs v5 IDENTICAL
both modes; small2 FAST +0.21% is the only diff. Suites 50/50 (v5, v7), ASan clean, UBSan 8
pre-existing overflows listed in RESULT 24. Binaries: build/pe_bench_{pre,fix,v4,v5,v6,v7},
pe_files_{pre,v5}, ut_v5{,_asan}, ut_v7. Header backups in petree/mzip.hpp.bak-{allocfix,dictfast,v5,v6,v7}.
Nothing committed; /root/mzip untouched; nothing posted.
