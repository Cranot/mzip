
## FAST-PATH PROFILING RESULTS (2026-09-02) — petree scratch only; /root/mzip UNTOUCHED
Master record: MZIP-AT-HF-GRANULARITY.txt RESULT 23-25. In-process bench = build/pe_bench2.cpp
(args: file chunk both|FAST|BALANCED reps; prints FNV-1a over all outputs for byte-identity),
build/pe_files.cpp (whole files in a dir). Builds: pe_bench_pre (pre-fix header), _fix (alloc
fix), _v4 (+FAST dict gate), _v5 (+decoder buffers, periodic_approx prefilter, char_template fix).

Where a FAST 64K weight chunk's ~90 ms went (in-process, so NOT exec overhead as I had inferred):
  1. ~30%: work buffers sized from block_size=16 MiB for a 64K input (~112 MB zero-filled per
     compress() call; 443k minor faults / 279 MB RSS for 12 calls on one chunk). ALL MODES,
     all inputs < 16 MiB. Fix = alloc_block for the three buffers; block_size untouched.
     8/8 arms byte-identical. FAST 0.64 -> 0.95 MB/s.
  2. ~55 ms: the generic 13-dictionary trial at zstd LEVEL 19 on every block 256 B..256 KiB,
     every mode, every data type (+ a level-19 zest). All dicts are text. v4: FAST skips it on
     non-text blocks, selects at level 1 on text. FAST -> 4.7-5.6 MB/s; FAST bytes identical.
     BALANCED with the gate forced (MZIP_DICT_TEXTGATE=1): identical hashes on 4 weight slices
     -> no dictionary EVER wins a weight block; generalising the gate is a safe speedup.
  3. ~8 ms: detect_periodic_approx (11 periods x two full passes with modulo; 0 hits on
     weights). v5: 3-window sample prefilter (MZIP_NO_PA_GATE=1 disables).
  4. DECODER mirror bug: 2 x 16 MiB zero-filled per decompress() call; hidden at 64K (MU
     compact path) -> 0.9-1.5 GB/s; exposed at 256K/1M (all PLANE_ENTROPY blocks, 15/54 MB/s).
     v5 sizes them to min(16 MiB, 3*min(original_size,16 MiB)).
  4b. v6: compute_fingerprint (cross-block SimHash, ~4 ms per 64 KiB block, 72% of v5's
     remaining time) gated on num_blocks > 1 -- consumed only in multi-block streams.
     FAST 64K -> 42-55 MB/s (indicative, loaded box). Bytes unchanged.
  5. PRE-EXISTING ASan finding (repo code, not ours): detect_char_template mzip.hpp:6394
     1-byte heap over-read on a truncated last line (unit test columnar_roundtrip_nginx);
     identical on pre-fix and fixed trees. Read-only UB. Fixed in petree v5 (drop the index).
  Instrumentation bug: -DMZIP_TIME build segfaults at exit (Dump static outlives table()).

v5 numbers: FAST 64K compress 11.4-12.4 MB/s (from 0.64); v6 42-55 MB/s, decode 1.07-1.45 GB/s, bytes
unchanged; 16K..whole: 8-14 MB/s comp, 0.8-1.5 GB/s dec (RESULT 25). Identity/UT: see below.

Owner decisions (none taken): (a) apply items 1,2(gate),3,4,5 to /root/mzip -- 1,4,5 are pure
bug fixes; 2 and 3 change BALANCED only if the toggled identity runs stay identical on ALL
corpora (text corpora + HF JSON measured in v5cmp/); (b) PLANE_ENTROPY block type = format
addition; (c) commit the earlier 4 tracked changes + train_corpus/hf_dict.bin; (d) the post.
