

## SECOND PUSH 2026-09-02 (v8-v9.1): FAST redesign + huff0 planes + HF measured in-process
Record: MZIP-AT-HF-GRANULARITY.txt RESULT 28-33. Scratch tree /root/mzip-hfbench/petree only;
/root/mzip untouched; nothing committed; nothing posted. Header backups petree/mzip.hpp.bak-{v8,v81,v82,v83,v84,v9,v91}.
Binaries: build/pe_bench_{v8,v81,v83,v84,v9,v91}, pe_files_{v81,v83,v84,v9,v91}, ut_{v9,v91},
hf_bench (HF scheme in-process: liblz4.so.1 block API + bg4 split; prototypes declared inline,
no lz4 headers on the box), h0_*.o (reference FSE/huff0 objects from fse/lib).

FAST policy as of v9.1 (all in compress_impl's block loop unless noted):
  - per block: printable(4 KiB) >= 85% => text. Non-text >= 4 KiB: PLANE_ENTROPY tried FIRST
    (huff0 per plane, k by order-0 estimate with a 1% tie rule toward k=2, 256 KiB segments so
    planes fit HUF_BLOCKSIZE_MAX); if it compresses < 31/32 and round-trips, the block is done --
    no analyze_block, no trials. Payload is installed AFTER the encoder chain (v8 installed it
    before and the chain's default branch overwrote it -> every chunk uRAW; RESULT 29).
  - text > 16 KiB: TEXT type without analyze_block, final zstd level 3.
  - small text <= 16 KiB: pure BALANCED behaviour (fast_block=false): BWT/CM/PPMd/dicts, level 19
    selection -- that is where HF JSON gets 14x and it is cheap there.
  - non-text blocks PE could not compress (GGUF quantised): analyze_block, BWT_TEXT remapped to
    TEXT (zstd-3), no slow backstops.
  - FAST never runs bwt9 / xz / BCJ-xz / brotli-11 / PPMd except on small text blocks.
  - dictionary trial in FAST: small text only, level-1 selection, level-19 final.
  - whole-file zstd arm skipped in FAST when the container already <= 3/4 of input.
  - all modes: work buffers sized to input (enc+dec), fingerprint only for multi-block streams,
    periodic_approx equality prefilter, top-level 4 KiB sample analyses only when size > block_size.
BALANCED byte-identical to the pre-fix header at every version (weights 4 slices, 290 HF JSON,
alpaca@64K, 50 MB corpora both modes at v5). Suites 50/50 through v9.1.

Known FAST trade-offs (by design, documented): small2 whole files 10.6x vs BALANCED 14.2x (the
16 KiB-1 MiB text files take the zstd-3 bypass; knob = 16 KiB exemption). q4_k_m stored raw.

FINAL TABLE: see RESULT 33 / tbl_final_v91.log.
