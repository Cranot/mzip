# mzip / HuggingFace session — handoff for a fresh Claude Code session

Written 2026-09-01. Everything below is on the Hetzner box unless stated.
Re-entry point: `ssh <box>`, then `ls hfbench/*.txt`.

## The one-paragraph version

mzip (`/root/mzip`, public repo `github.com/Cranot/mzip`) was benchmarked against
real HuggingFace data — 8 dataset shapes, 6 model-weight slices, 289 repo metadata
files. It wins 7 of 8 dataset shapes against the whole standard field, and is
**15.84% smaller than what HF actually stores** on model weights — but is ~1441x
slower to compress. A real losslessness bug was found, root-caused to two bytes,
fixed, and validated. Two of my own earlier claims were retracted after
measurement. Nothing has been committed or pushed.

## UNCOMMITTED CHANGES IN /root/mzip — the first thing to check

    git -C /root/mzip diff --stat
      mzip.hpp            +29/-4    NUM_EXTRACT roundtrip guard
      mzip_unit_tests.cpp +35       regression test for it

Backups: `mzip.hpp.bak-numextract`, `mzip_unit_tests.cpp.bak-uravtest`.
Both changes are VALIDATED (see below). Nothing committed, nothing pushed.
The tree was clean before these two edits.

## The bug that was found and fixed

`encode_num_extract` (mzip.hpp:10376) parses digit runs with
`num = num*10 + (data[i]-'0')` into a **uint32**, so it cannot represent:
  - **leading zeros** — "02" -> "2", "007" -> "7". Minimal reproducer is the
    two bytes `02` (encodes to 36 B, decodes to 1 B).
  - **values >= 2^32** — "4294967296" wraps.
Real trigger: `"initializer_range": 0.02` and `"layer_norm_eps": 1e-06`, which
appear in essentially every HF transformer config. The digit run after the
decimal point is the zero-padded one.

The adoption branch at mzip.hpp:16195 took that lossy stream with **no verify**,
it failed the top-level end-to-end verify, and the whole file fell to uRAW:
58,110 B in -> **58,115 B out** vs brotli's 2,050 (28x blowup) on
`nvidia/Qwen3.6-35B-A3B-NVFP4/config.json`.

Fix applied: roundtrip verify mirroring PHRASE_PARTITION (16211) and
CHAR_TEMPLATE (16046), which were hardened after identical incidents.

### Validation (all measured)
  - losslessness suite: **50 passed, 0 failed** (was 49 before the new test)
  - new test provably FAILS on stock encoder (49 passed / 1 failed) — falsified
    properly, via a separate source tree, because a quoted `#include` resolves
    relative to the including file first and `-I` cannot override it
  - reproducer: 58,115 -> **1,924** (30.2x), now beats brotli by 6.1%
  - 144 held-out HF config files: 116,456 -> **60,265** (-48.25%), flipping
    mzip from **-83% to +5.14% vs brotli**, **zero files regressed**
  - all 8 shape corpora: **byte-identical, delta +0**, roundtrips OK

### Still open on this bug
  - `encode_num_extract` is **still lossy at source**; the guard only routes
    around it. Real fix: reject digit runs with a leading 0 (len>1) or > UINT32_MAX
    in `detect_num_extract`, OR a format change storing digit-run length (which
    would unlock the genuine 36x encoding — but breaks stream format).
  - **Six sibling branches still adopt with no roundtrip verify**, none observed
    failing: LINE_TEMPLATE (16203), SORTED_DICT (16276), KV_CONFIG (16292),
    CODE_STREAM (16350), REFERENCE (16356, cross-block), DBF_CONSTCOL (15766).
    SORTED_DICT and KV_CONFIG have only an `encoded.empty()` guard — the exact
    state PHRASE_PARTITION's comment warns is insufficient.

## Headline measurements

### vs what HF ACTUALLY stores
xet-core's `CompressionScheme` = {None, LZ4, **ByteGrouping4LZ4**, Auto}
(`xet_core_structures/src/xorb_object/compression_scheme.rs`). The Hub stores
chunks 4-byte-grouped + LZ4. **Comparisons against raw or against zstd are not
valid savings claims.** Xet ALSO deduplicates before storage; nothing here
measures that.

6 model-weight slices, 50,331,648 B raw, all roundtrip verified:

    scheme            bytes       ratio   comp s  dec s  MB/s c  MB/s d
    lz4 (plain)       49,831,206  1.010x     0.1    0.1   499.8   811.3
    bg4-lz4  <- HF    42,269,122  1.191x     0.2    0.2   236.6   316.2
    bg4-lz4 -9        39,084,131  1.288x     3.9    0.2    12.5   287.3
    zstd-19 (ref)     40,704,479  1.237x    17.7    0.1     2.7   407.3
    mzip              35,572,346  1.415x   292.4    3.7     0.2    13.0

    mzip vs HF: **+15.84% smaller**, **1441x slower to compress**, 24x to decompress.

Per-model spread is LARGE: bge-small +30.98%, SmolLM2 +21.80%, Qwen2.5 +21.05%,
gpt2 +9.39%, bert-base +8.81%. Show the range, not the mean.

Three facts worth keeping:
  - **plain LZ4 gets 1.010x on weights** — BG4 does ~95% of Xet's win.
  - **mzip decompress is bimodal**: 0.03s (== Xet) on gpt2/bert-base, 1.1-1.3s
    (40-55x) on SmolLM2/Qwen/bge-small. "24x" is the aggregate, not universal.
  - **HF can have ~8% for free**: bg4-lz4 at -9 gives 1.288x vs their 1.191x,
    still 12.5 MB/s compress and unchanged decompress.

### Dataset shapes (8 corpora, 8MB each)
mzip wins **7 of 8** outright vs gzip/bzip2/xz/zstd-19/brotli-11/7z-PPMd.
vs zstd-19: +12.89% (code) to +29.15% (tabular-numeric). Only loss: qa-arc
(-3.50%), and that is likely an artifact of the length-prefixed test
serialization, not the data — re-test on arrow buffers before quoting.

### The universal ceiling
A direct context model (lpaq1, GPL) beats mzip on **8/8 text shapes** by
2.3-14.8%. `lpaq_x.cpp` is IN the repo but unlinked (`grep -c lpaq mzip.hpp` = 0)
and is GPL-2.0-or-later vs mzip's AGPL-3-or-commercial — shippable into the AGPL
arm, **not** into the commercial one. Clean-room is the path if pursued.

### mzip's own published numbers are STALE and understate it
README says enwik9 10 MiB = 2,671,197 and "beats brotli:11 by 5.9%".
Measured today: **2,427,140**, i.e. **9.14% better than advertised**, and the
real margins are brotli **14.16%**, bzip2 **20.57%**, xz 14.64%, zstd 16.94%.
Cause: the CM backend shipped and the table was never updated. Rig validated by
reproducing `internal/CM_SAUCE.md`'s own bwtcm figure (2,427,131) **byte-for-byte**.
NOTE the units trap: that benchmark is 10 **MiB** (10,485,760), not 10,000,000.

### Scale behaviour
mzip IMPROVES with size. On wikitext-103 slices: mzip vs 7z-PPMd +1.63% ->
+2.91% -> +4.21% at 16/24/32 MiB, vs zstd +17.4% -> +19.3%, and lpaq's lead
SHRINKS 7.91% -> 6.54%. Multi-block splitting past DEFAULT_BLOCK_SIZE (16MB)
costs nothing measurable.

## TWO CLAIMS I MADE AND RETRACTED — do not resurrect

1. **"87% of runtime is wasted, free 8-55x speedup."** WRONG. The bwt9 backstop
   is already memo-guarded (`if (!bwt9_memo_valid)`) with a deliberate comment
   on why a CAPPED result must not be cached. Its 0 hits mean it is a genuine
   trial that LOSES on HF data, not duplicated work. The families owning that
   time are load-bearing elsewhere (ablation.md: GRID 418,875 B, FILTER 645,475 B).
2. **"ASan found a heap-buffer-overflow in encode_num_extract."** WRONG, the bug
   was MINE — `bisect.cpp:44` underflowed `size_t` (`end - start - step` with
   step=4096 > end=475), asking the encoder to read 18 exabytes. Fixed bounds,
   same ASan build exits 0 clean. **mzip has no memory-safety defect here.**

Also: a first regression test attempt was **vacuous** (passed on broken code)
and was discarded — falsification caught it. Always falsify against stock.

## Docs on the box — /root/mzip-hfbench/

    MZIP-HF-BENCH.txt        first benchmark, wikitext parquet vs text
    MZIP-HEADROOM.txt        where the remaining bytes are (CM analysis)
    MZIP-PRODUCT-CASE.txt    the unwrap-parquet product case (~2x, coder-independent)
    MZIP-README-STALE.txt    the 9.14% self-understatement, with rig validation
    MZIP-DEEP-FINDINGS.txt   8-shape win/loss map, routing map, scale ladder
    MZIP-NUM-EXTRACT-BUG.txt the defect, root cause, 28x blowup
    MZIP-GUARD-AUDIT.txt     7 unguarded adoption paths
    MZIP-FIX-VALIDATED.txt   the fix + full validation + both retractions
    MZIP-VS-XET.txt          mzip vs HF's real scheme, ratio + timing + extrapolation

Raw data: `run2/matrix.json`, `run2/headroom.json`, `run2/bigladder.json`,
`weights/xet_full.json`, `weights/weights_multi.json`.
Corpora: `corpora/*.bin` (8), `weights/weights-{fp32,bf16}.bin`, `small2/` (289
real HF metadata files), `synth3.json` (the regression fixture generator output).
Binaries: `build/{mzip_fixed,mzip_cli,mzip_time,lpaq_x,bwtcm,ne_probe,bisect,ut_fixed,ut_stock_real}`.

## Box state changed this session
Installed: `brotli`, `p7zip-full`, `zpaq`, `lz4`, `libzstd-dev`, `libbrotli-dev`,
`liblzma-dev`. venv at `/root/mzip-hfbench/venv` with pyarrow + numpy.
Also: `/root/mzip-hfbench/build_mzip_linux.sh` is the **only** Linux build recipe
that exists — `build_evals.sh` in the repo is w64devkit-only. It lives outside
the repo and will be lost unless landed.

## The pending post (NOT sent)
Clement Delangue posted "over 4 petabytes of models and datasets uploaded to HF
just last week". Two drafts exist for a reply quantifying mzip's savings. The
defensible claim is the **percentage (15.84%)**, not the petabytes — "~530 TB/week"
assumes all 4 PB is weights AND ignores Xet's dedup. User has NOT approved posting.

## Traps paid for in this session
  - `pkill -f "sweep.py"` matches its **own ssh command line** and killed the
    session. Use the bracket form: `pkill -f "[s]weep.py"`.
  - A quoted `#include "mzip.hpp"` resolves **relative to the including file**
    before any `-I`. To A/B a header you need a separate source tree, not `-I`.
  - `tee` + unbuffered python: use `python -u`, else a long run shows nothing.
  - `bigcode/the-stack-smol` is **gated**; a 404 arrives as a 132-byte HTML page,
    not an error.
  - A column-name heuristic matched `func_name` for "code" (substring collision).
  - HF's benchmark units are MiB, not MB (see the README-stale note).

## What was in flight at handoff
A Workflow run (`mzip-size-speed-hunt`) chasing size+speed levers: 6 parallel
explorers (CompressionMode FAST/SMALL — never measured and unreachable via CLI;
weights transforms BG2/BG8/bit-plane/per-tensor; family ablation via
MZIP_FAMILY_MASK; weights-specific profiling; CM-on-weights widening; internal
constants audit), adversarial verification of each lever, then a **serial**
timing phase (concurrency would corrupt wall-clock), then Pareto synthesis.
Its result should be appended here.

---

# WORKFLOW RESULT (mzip-size-speed-hunt) — 26 agents, 0 errors, 309 timings

Artifacts on box: `/root/mzip-hfbench/timing_harness.py`, `drive.sh`,
`timing/res_*.jsonl`, `timing/bin/`. `/root/mzip` was NOT modified.

## THE TWO FRAMING FACTS
Surviving levers are worth ~1% (size) and ~2.7x (speed). The gaps are
2.3-14.8% (lpaq) and ~460x (HF ingest). **Both gaps are architectural, not
tunable.** The trial-everything candidate search IS the cost.

## FREE WINS — ship these, verified, no format change
1. **level 1 is byte-INERT but NOT time-inert.** 7-21% faster at
   **sha256-IDENTICAL** output. Prior phase measured sizes only and stopped.
2. **FAST+L1 on BF16 weights: 2.68x faster compress, sha256-IDENTICAL output**
   (bf16 `4fff35d2...`, Qwen `d2df4977...`). Shipped binary cross-decodes every
   FAST+L1 archive, RT_OK. Encoder-side setting, NOT a format fork.
   On BF16, FAST+L1 **strictly dominates every other mzip config**.
3. **Shape-aware policy**: FAST+L1 for BF16, BALANCED+L1 for F32.
   Best weights policy = 17,253,394 B = 1.4586x = **18.68% below bg4-lz4**.

## THE BEST RESULT IN THE PROGRAM (and it inverts a claim I published)
**F32 BALANCED ships `MS` (A4_NUMERIC SoA) and decompresses at 492.3 MB/s vs
HF bg4-lz4's 401.0 MB/s — mzip is 1.23x FASTER than HF on the read path.**
FAST is exactly the switch that destroys it: 492 -> 8.6 MB/s, a 57x collapse.
=> "mzip is ~24x slower to decompress" is REFUTED for F32/MS.
=> **MZIP-VS-XET.txt has an internal contradiction**: its prose table says
   MiniLM decompress is "~40x slower", but its own `xet_full.json` records
   0.024 s vs bg4 0.030 s — mzip FASTER. Fix that doc.
=> Do NOT key the FAST/BALANCED switch on dtype: refuted (bge-small F32
   behaves like BF16). Correct predicate is "does MS win?".

## MEASURED ZERO — do not retry
- **MZIP_FAMILY_MASK as a speed lever: exactly zero.** Even mask 0x0 (all 9
  families off) costs the same as all-on. Masking is SELECTION-ONLY; candidates
  are still computed. It licenses building a skip; it is not one.
- **SMALL mode: zero in BOTH axes** in the measured regime (all inputs <=8.4MB).
  Caveat: its only differing behaviour is BG_MAX_SIZE 256MiB->1GiB, i.e. it is
  untested in the one regime where it can differ.
- **detect_periodic_approx early-out: zero end-to-end** (+0.6% median / -0.0%
  min, n=9). The profiler attributed 3.4% to it; that was **instrumentation
  overhead charged to the wrapped call**.
- **MZTIME percentages are inflated 2.65-2.78x** (fp32: 2,492ms at 3.4% implies
  73.3s instrumented vs 27.6s real). Never price a lever from MZTIME alone.

## SIZE LEVERS THAT WORK (all small)
- `MZIP_PPMD_MEM=128`: code-python -1.25%, qa-gsm8k -1.43% (a NEW adoption),
  byte-identical on the other 6 corpora + both weight slices. **No format
  change** — decoder already clamps memMiB to [1,128]. Costs +3.4% time.
- `block_size=1MiB` on F32: -0.55% bytes but **decompress 492 -> 64 MB/s**.
- No lever anywhere exceeds **1.43%** on any single file.

## UNCOMFORTABLE RESULTS
- zpaq-m5 beats mzip on quantized qwen2.5-1.5b (5,524,842 vs 5,556,193, -0.56%).
- On quantized slices mzip's output equals standalone `bwtcm` to within
  **17-23 bytes** — mzip IS generic BWT-CM there, with no specialized path.
- bg4+zstd-19 beat plain mzip on weights-bf16 (5,564,440 vs 5,571,614) but NOT
  on bf16-layers. n=1 each way, conflicting, NOT averaged.

## SPEED VERDICT — archival/cold-tier for the WRITE path, honest
- Best byte-free policy: 0.469-0.623 MB/s vs HF's measured 263.6-322.3 MB/s
  = **461-612x short**.
- Ceiling arithmetic [derived, upper bound]: grant perfect 6-core scaling ->
  3.7 MB/s, still **63x short**. Threading never measured.
- **But the READ path is not lumped in**: F32/MS decompresses faster than HF.

## #1 RISK — THE MEASUREMENT REGIME
**Every number in this entire program is from a single-block input <= 8.4 MiB.**
DEFAULT_BLOCK_SIZE is 16 MiB; real HF shards are 10^2-10^4x larger. Above
256 MiB the whole-file BG path is OFF in BALANCED. If MS/SoA goes with it, then
**the largest byte contributor (A4, up to 341,402 B / 4.95%) AND the best speed
result (F32 decompress 492 MB/s) both evaporate at production scale.**
=> NEXT MEASUREMENT, before trusting any ranking above: compress+decompress one
   real >256 MiB safetensors shard, default vs FAST+L1, sizes + serial timing.

## OTHER NAMED UNKNOWNS
- Half of FAST is unmeasured: its per-block downgrade of LINEAR_GEN/GEOMETRIC/
  QUADRATIC/RECURRENCE/MODULAR to RAW above 64KB **never fired** on any of the
  10 inputs. On generator-friendly data it could cost far more than 4.95%.
- Threading: not measured at all.
- `MZIP_NO_CM` build was malformed and abandoned — CM's byte contribution on
  HF data is entirely unquantified.
- PPMD_MEM between 64 and 128 untested (128 may be the cap, not the knee).
- periodic_approx patch never run against the losslessness suite.
- All round-trips used the same binary that compressed, so a symmetric
  decoder bug would not be caught.
- The transform phase ran **no xet arm** — whether HF's backend already
  exploits F16-in-F32 dead bits is unknown.

## A METHODOLOGICAL CORRECTION TO MY OWN BRIEF
I told the timing agent "the box is idle". **That was false** —
`eon-liveness-deadman.mjs` burned a full core for the whole session (19h50m),
plus a codex review and other box jobs. Load median 2.10 (1.35-5.24).
The agent caught it and mitigated: `taskset -c 10` pinning, n>=3 medians,
min-values reported, interleaved A/Bs for close calls. Ratios stand; absolute
MB/s may be uniformly pessimistic by a few percent.

## PROVENANCE CONFLICT LEFT OPEN
My "236 MB/s" figure for HF ingest is LOWER than anything measured here
(263.6-322.3 MB/s on the same files). I do not know its provenance. Re-derive
before quoting either number publicly.

---

# SESSION 2 (Fable 5.1) — GRANULARITY, GGUF, AND THE NATIVE FIX

Full detail: /root/mzip-hfbench/MZIP-AT-HF-GRANULARITY.txt

## The foundation was wrong, and is now corrected
Xet compresses PER CHUNK (64 KiB target; XorbChunkHeader carries the scheme per
chunk; Auto = BG4Predictor popcount-KL > 0.02). File size never widens the
window. The 15.84% whole-file headline was NOT the drop-in comparison.

## Numbers at HF's real 64 KiB granularity (7 weight slices, 58,720,256 B)
                       bytes        vs HF bg4-lz4
  HF bg4-lz4           50,863,964   --
  bg4 + zstd-1         47,713,482   +6.19%   (HF could do this at ~lz4 speed)
  bg4 + zstd-19        43,512,873   +14.45%  (nearly the old mzip headline, stock)
  mzip (plain)         44,030,412   +13.43%
  bg4 + mzip           41,967,364   +17.49%  <- best; beats bg4+z19 on every slice
  per-dtype dict adds ~+3% to any zstd arm (dedup-compatible)
  stride-2 grouping is WORSE than stride-4 even on BF16 -- Xet's choice holds
Dtype split: BF16 plain mzip already +23%, bg4 adds nothing. F32 plain +3-9%,
bg4 roughly TRIPLES it (+11-15%).

## GGUF (28.8% of served weight bytes; HF gets ~1.000x on ALL of it)
  Q4_K_M incompressible for everyone. Q6_K/Q8_0: +2.7-4.2% for zstd-1 or mzip.
  F16 @64K: zstd-1 1.304x, bg4+zstd-1 1.304x, mzip 1.452x, bg4+mzip 1.508x.

## ROOT CAUSE of mzip's F32 weakness at 64K -- and the fix (built, measuring)
mzip.hpp ~17085: the SoA/MS transform is gated on a zstd-1 PROXY showing >2%.
At 64K zstd-1 sees +0.13..+0.37% for stride-4 (prunes 54-69/128 chunks);
BWT+CM gains ~8% from the same transform. At 1 MiB the proxy sees +9% and
passes. Fix: below 1 MiB, trial stride-2 and stride-4 with the REAL coder.
Built in scratch copy /root/mzip-hfbench/soatree -> build/mzip_soa + ut_soa.
NOT applied to /root/mzip. Measurement + suite pending at handoff time.

## Pending at handoff
  bigladder2 (256 MiB BF16 + F32 at 1M/16M/64M/whole, incl. FAST) -> big2.log
  soa_measure (native fix @64K vs plain vs external bg4+mzip) -> chunk/soa_measure.json
  ut_soa (losslessness suite on patched header)
  time64k.py written, NOT run -- run only on a quiet box: taskset -c 10.

## UPDATE 2026-09-02 — native small-block SoA fix VALIDATED and APPLIED
Measured (chunk/soa_measure.json): reproduces the external bg4+mzip result
within 5 KB aggregate; F32 +8.08/+8.31/+7.19% vs plain at 64K; never worse on
any slice; stock decoder decodes 100% of output (format unchanged); suite 50/50;
byte-identical above the 1 MiB gate on F32/BF16/text whole files.
Aggregate @64K: 41,962,356 = +17.50% vs HF bg4-lz4, +3.56% vs bg4+zstd-19.
APPLIED to /root/mzip/mzip.hpp (backup mzip.hpp.bak-soasmall), UNCOMMITTED.
/root/mzip now carries THREE uncommitted validated changes:
  mzip.hpp            NUM_EXTRACT roundtrip guard  + small-block SoA trial
  mzip_unit_tests.cpp uRAW-bloat regression test
Rebuilt as build/mzip_fixed2 (build/mzip_fixed is the pre-SoA binary, kept so
timing runs have a stable "stock" arm). Suite from the real tree: see apply log.
Also measured: dataset PARQUET @64K -- HF auto 1.0022x, zstd-19 1.1403x,
mzip 1.2218x (+17.97% vs HF). HF's scheme yields ~nothing on GGUF AND parquet.
Still pending at this update: bigladder2 (256 MiB ladder), time64k (serial).
Suite from the REAL tree after applying the SoA fix: 50 passed, 0 failed.
git diff --stat: mzip.hpp +77/-18 (both fixes), mzip_unit_tests.cpp +35.

## UPDATE 2026-09-02 (later) — ceilings, dead ends, guards staged
- CEILING at 64K: native-SoA mzip matches lpaq1 within 0.2% on F32/BF16 weights
  and beats it by 0.2-0.35% on GGUF F16/Q8_0/Q6_K. No modelling headroom left.
- DEAD: adjacent-element delta/XOR raises entropy on weights. Whole class ruled out.
- GGUF block-aware SoA: Q8_0 +1.47% (stride 34); K-quants neutral/worse. Floor.
- 64K time: bwt9 (the winning coder) is 87% of ~85 ms/chunk => ~0.77 MB/s/core.
  Chunks are independent => parallel scaling should be ~linear. parscale.py staged.
- HF cheapest upgrade is encoder-only: lz4hc-12 at 64K = +6.12% (std LZ4 frames).
- Served-mix extrapolation (top-100 survey): 15.6% central (10.5-17.8%), dedup excl.
- Sibling guards STAGED, not applied (build contention with timing run):
    guard_siblings.py  LINE_TEMPLATE, SORTED_DICT, KV_CONFIG, CODE_STREAM (anchors unique)
    guard_siblings2.py DBF_CONSTCOL (anchor unique)
    guard_siblings3.py REFERENCE (anchor unique)
  Apply order: 1 -> 2 -> 3, then build mzip_fixed3 + ut_repo3, run suite, then
  confirm byte-identity on the 8 corpora + weights (guards must be no-ops where
  encoders are lossless).
- POST-DRAFT-v2.md on the box: size claims final; speed line pending time64k.
- Pending: bigladder2 (slow; prints in submission order), time64k, then parscale.

## ROUND 3 IN FLIGHT (2026-09-02) — apply_round3.py
Applies to /root/mzip/mzip.hpp: (a) SoA forced trial W=4 only (backup
mzip.hpp.bak-w4only) -- W=2 never won, halves the fix's 2.5x compress cost;
(b) six sibling roundtrip guards (backups .bak-siblings/.bak-dbf/.bak-reference).
Builds build/mzip_fixed3 + ut_repo3; runs suite; checks 64K sizes identical to
mzip_soa on 3 slices and whole-file byte-identity vs mzip_fixed2 on 4 corpora.
If any check fails: restore from the backups in reverse order, rebuild, re-run suite.
First-pass 64K timing (contended) recorded in MZIP-AT-HF-GRANULARITY.txt RESULT 11;
"bg4+mzip FAIL" there is a harness bug (inverse split not applied), fixed in time64k.py.
time64k.py now uses build/mzip_fixed3 as the "native" arm when present.
QUEUE after round3 + ladder finish (quiet box): time64k.py 3 ; parscale.py 2.
ROUND 3 DONE: suite 50/50; guards byte-identical no-ops on 4 corpora; W4-only costs
<=0.012% vs W2+W4 (comment corrected in source). build/mzip_fixed3 is now the
"native" binary. /root/mzip: mzip.hpp ~+172/-31, mzip_unit_tests.cpp +35, UNCOMMITTED,
backups: .bak-numextract .bak-soasmall .bak-w4only .bak-siblings .bak-dbf .bak-reference

## 2026-09-02 — LADDER CRASH (my bug) + SHARED-BOX METHODOLOGY CHANGE
- bigladder2 died: shared zstandard.ZstdCompressor across 4 threads is NOT
  thread-safe ("cannot compress: Error (generic)"). Fixed (per-call compressors),
  scope reduced to 16M/64M/whole (64K-1M covered at 8 MiB scale), uses mzip_fixed3.
  Relaunched; log big2.log; old failure in big2.failed.log.
- The box runs the estate's autonomous loop (other agent fleets, codex, claude
  sessions). Observed load 73/83/102. Wall-clock timing is meaningless in bursts.
  time64k.py now records CPU time (process + reaped children rusage) alongside
  wall; CPU-time MB/s is the contention-robust figure to quote.
- quiet_gate.sh: waits for ladder ALLDONE_L2 + 1-min load < 1.5 x3, then runs
  time64k (pinned) and parscale; logs to quiet_gate.log; gives up after ~55 min
  (relaunch if "GATE TIMEOUT"). Outputs: time64k.quiet.log, parscale.log.
LADDER DONE (chunk/bigladder2.json): no size degradation 16M->256M; BF16 +21.55% vs HF,
F32 +9.30%; vs bg4-z19 BF16 +2.1-2.8%, F32 +0.15-0.53%; FAST identical on BF16, -3.4% F32.
QUIET TIMING DONE (chunk/time64k.json, time64k.quiet.log; parscale.json): see
MZIP-AT-HF-GRANULARITY.txt RESULT 15. Compress ~0.1 MB/s/core native (archival),
decompress ~50 MB/s CLI on SoA-winning chunks, ~9 MB/s on BWT (BF16). Parallel
scaling 92% at 4 workers, 82% at 6 cores. New lever: decode-cost-aware tiebreak
(BF16 +0.12% bytes for 5.6x faster decode). bestof4.py running/next.
ALL PLANNED MEASUREMENTS COMPLETE (2026-09-02 ~02:30). bestof4: per-chunk best-of-4
= +6.41% vs HF over 612 MB; single bg4-zstd-1 only +3.27% (retracted as headline).
POST-DRAFT-v2.md is FINAL pending owner review; nothing posted. Remaining owner
decisions: commit the 3-part mzip.hpp change + test (uncommitted, backed up); post.

## ROUND 4 IN FLIGHT (2026-09-02 ~03:00) — three scratch-tree experiments
All build in their own scratch copies; /root/mzip untouched until measured.
1. add_hf_dict.py -> dicttree, build/mzip_dict + ut_dict.
   DICT_HF_REAL id 13 (16 KB, zstd --train on 145 HF config/tokenizer JSON).
   Pre-measured: zstd-19+dict 44,150 B on 144 held-out vs native mzip 60,288 (-26.8%).
   Also patches emit_dicts_header.cpp + drops train_corpus/hf_dict.bin so regeneration
   reproduces it. FORMAT NOTE: archives choosing id 13 cannot be decoded by older
   builds ("ZSTD_DICT unknown dict_id") -- inherent to adding any dict (ids 6-12 same).
2. fast_mode.py -> fasttree, build/mzipx_fast (mode-exposing CLI).
   FAST redefined for numeric data: (a) SoA/MS allowed in FAST (was excluded; MS is the
   cheapest decode path), (b) in FAST adopt brotli when within 1% of a slow-decoding
   incumbent (BWT_TEXT/CM_TEXT/PPMD). BALANCED must stay byte-identical (checked).
   This is a POLICY change to a documented mode -> measure, present, owner decides.
3. numextract_source.py -> netree, build/mzip_ne + ut_ne.
   detect_num_extract declines blocks with a zero-padded run (len>1, leading '0') or a
   run >= 10 digits. Guard stays as defence in depth. Checks byte-identity on the
   reproducer, synth3 and 5 corpora vs mzip_fixed3.
Logs: add_hf_dict.log, fast_mode.log, numextract_source.log. Box load ~97 (other jobs).
ROUND 4 APPLIED (2026-09-02): DICT_HF_REAL id 13 (mzip_dicts.h +array + ALL_DICTS entry;
emit_dicts_header.cpp news[] + table line; train_corpus/hf_dict.bin, untracked -> add
to the commit) and the detect_num_extract source fix (mzip.hpp). git diff --stat:
4 files, +1221/-31. Build mzip_fixed4 + ut_repo4. Scratch measurements: held-out 144
HF files 60,288 -> 42,878 (-28.88%), suite 50/50, corpora byte-identical.
Re-verification from the real tree: verify_r4.py (see its output / task log).
FAST-mode redesign: fasttree only, NOT applied (see RESULT 17) -- owner decision.
Backups now: mzip.hpp .bak-numextract .bak-soasmall .bak-w4only .bak-siblings .bak-dbf
.bak-reference .bak-nesource ; mzip_dicts.h.bak-hfdict ; emit_dicts_header.cpp.bak-hfdict ;
mzip_unit_tests.cpp.bak-uravtest.
IF COMMITTING: author Cranot, message via -F file, explicit paths only (mzip.hpp,
mzip_dicts.h, emit_dicts_header.cpp, mzip_unit_tests.cpp, train_corpus/hf_dict.bin);
never git add -A -- the tree has many unrelated untracked probe files.
ROUND 4 VERIFIED from real tree: suite 50/50; held-out -28.88%; corpora/weights identical.
build/mzip_fixed4 is the current "native" binary. NOTHING LEFT RUNNING on the box.

## RESULT 20 (2026-09-02) — same-speed better-ratio scheme FOUND and measured
Per-plane huff0/FSE with dtype stride (4 F32, 2 BF16/F16) + raw fallback:
+17.87% vs HF bg4-lz4 at 64K, within 0.66% of the order-0 bound, EQUAL to native
mzip (+-0.6%), at ~900 MB/s enc / ~1.9 GB/s dec on BF16 (fse -b figures).
mzip's BWT+CM buys <=0.6% on weights at ~5000x cost. See MZIP-AT-HF-GRANULARITY.txt
RESULTS 18-20. Not implemented in mzip (format addition; owner decision). Tools:
/root/mzip-hfbench/fse/fse, perplane.py, ppfse.py, levers_diag.py; json in chunk/.
RESULT 21: fast scheme at ceiling at 64K (cross-plane +0.4%, chunk size immaterial);
byte-weighted huff0 throughput enc 1.9-2.6 GB/s, dec 3.3-5.9 GB/s (2-3x HF encode).
MEASUREMENT PROGRAM COMPLETE. Next is implementation (PLANE_ENTROPY in mzip and/or a
reference tool + Xet scheme proposal) -- owner decision. Tools: pushfurther.py, pp_throughput.py.

## PROTOTYPE IN FLIGHT (2026-09-02): PLANE_ENTROPY block type — SCRATCH ONLY (petree)
pe_proto.py builds build/mzip_pe, mzipx_pe, ut_pe from /root/mzip-hfbench/petree.
Design: BlockType::PLANE_ENTROPY = 0x3C. Payload [k:1][flags:1][clen varint x k][planes][tail].
k in {2,4} best-of; each plane zstd-1 frame or raw if coding does not help; tail raw.
Codec namespace mzpe:: inserted before the first `namespace mzip {`. Wired into: enum,
self-contained payload list, block_type_name, compact decoder switch, main decoder
branch, and an encoder trial placed before the brotli backstop (non-text blocks
>= 4096 B): trial-and-keep by size; in FAST adopt when within 5% of incumbent;
per-block roundtrip verify before adoption.
Expectations: BALANCED byte-identical to mzip_fixed4 on weights (PE loses to BWT by
~4% on size); FAST adopts PE on most numeric blocks -> decode 9 -> ~50+ MB/s (CLI);
stock decoder must refuse a PE archive (unknown block type) rather than mis-decode.
Format addition -> NOT applied to /root/mzip; owner decision. FAST would still run
bwt9 before adopting PE -- a true fast path must skip BWT/CM when PE is available.
PLANE_ENTROPY v1 RESULT (petree, scratch): builds OK, suite 50/50, BALANCED byte-identical
128/128 on 4 slices, FAST adopts PE on 128/128 blocks: BF16 decode 8.6 -> 54 MB/s (CLI),
sizes +9.9..+33.4% vs HF at 64K. Stock decoder refuses PE archives cleanly.
v2 IN FLIGHT: FAST early path (PE before backstops, skip them on success) -> compress speed.
PE v2: FAST compress only 0.2-0.6 MB/s; profile showed bwt9 still 87.8% because a specialist
encoder set use_generator and bypassed the early path. v3 in flight (skip backstops once any
complete encoding exists in FAST on numeric blocks). petree only; /root/mzip untouched.

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

### FINAL STATE 2026-09-02 (all runs complete)
v7 FAST 64K: 48-63 MB/s compress per core, 0.7-1.5 GB/s decode, 16K..whole sweep in RESULT 27.
Identity: v5/v6/v7 BALANCED = pre-fix on 4 weight slices; corpora (50 MB text) pre vs v5 IDENTICAL
both modes; small2 FAST +0.21% is the only diff. Suites 50/50 (v5, v7), ASan clean, UBSan 8
pre-existing overflows listed in RESULT 24. Binaries: build/pe_bench_{pre,fix,v4,v5,v6,v7},
pe_files_{pre,v5}, ut_v5{,_asan}, ut_v7. Header backups in petree/mzip.hpp.bak-{allocfix,dictfast,v5,v6,v7}.
Nothing committed; /root/mzip untouched; nothing posted.


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
| data | in MB | HF bg4-lz4 ratio | mzip FAST ratio | mzip BAL ratio | HF comp MB/s | FAST comp | BAL comp | HF dec MB/s | FAST dec | BAL dec |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| bert-base F32 | 8.0 | 1.0703 | 1.2096 | 1.2119 | 1138 | 240 | 0.05 | 5734 | 1904 | 615 |
| bge-small F32 | 8.0 | 1.5876 | 1.4801 | 2.3472 | 675 | 254 | 0.04 | 3929 | 1516 | 247 |
| gpt2 F32 | 8.0 | 1.0557 | 1.1907 | 1.1910 | 1096 | 248 | 0.05 | 6293 | 1817 | 397 |
| MiniLM F32 | 8.0 | 1.0604 | 1.1980 | 1.1981 | 1101 | 241 | 0.06 | 6119 | 1962 | 361 |
| wfp32 F32 | 8.0 | 1.1575 | 1.2642 | 1.3657 | 936 | 240 | 0.05 | 5465 | 1654 | 335 |
| wbf16 BF16 | 8.0 | 1.1452 | 1.4819 | 1.4864 | 676 | 298 | 0.06 | 3834 | 1612 | 5 |
| Qwen BF16 | 8.0 | 1.1499 | 1.4952 | 1.4971 | 685 | 315 | 0.05 | 3743 | 1718 | 12 |
| gguf F16 | 8.0 | 1.0006 | 1.5010 | 1.5077 | 2553 | 252 | 0.05 | 16720 | 1069 | 207 |
| gguf Q8_0 | 8.0 | 1.0004 | 1.0423 | 1.0437 | 2857 | 157 | 0.04 | 16434 | 981 | 182 |
| gguf Q6_K | 8.0 | 1.0000 | 1.0285 | 1.0295 | 2867 | 140 | 0.04 | 17100 | 904 | 135 |
| gguf Q4_K_M | 8.0 | 1.0002 | 1.0040 | 1.0046 | 2880 | 124 | 0.05 | 17214 | 1161 | 90 |
| wikitext-2 parquet | 6.1 | 1.0007 | 1.1305 | n/a | 6507 | 115 | n/a | 16315 | 1175 | n/a |
| alpaca text | 8.0 | 1.1052 | 2.3961 | 3.3186 | 507 | 171 | 0.08 | 3130 | 1142 | 7 |
| code-python | 8.0 | 1.3488 | 3.6588 | 4.9923 | 456 | 240 | 0.05 | 3165 | 1294 | 7 |
| dialogue-csv | 8.0 | 1.1520 | 2.7389 | 3.8626 | 473 | 175 | 0.09 | 3324 | 1188 | 7 |
| prose-wikitext | 8.0 | 1.1103 | 2.6104 | 3.4963 | 496 | 173 | 0.04 | 3155 | 1186 | 6 |
| qa-arc | 0.6 | 1.2472 | 2.6127 | 4.1903 | 530 | 175 | 0.05 | 3120 | 1045 | 8 |
| qa-gsm8k | 3.8 | 1.1028 | 2.5493 | 3.4726 | 517 | 166 | 0.10 | 3233 | 1139 | 7 |
| tabular-numeric | 8.0 | 1.6154 | 4.0031 | 5.8341 | 414 | 252 | 0.12 | 2618 | 1178 | 9 |
| tweets-jsonl | 3.5 | 1.3580 | 2.7152 | 3.7500 | 513 | 186 | 0.04 | 3452 | 1220 | 5 |
| HF repo JSON x290 (whole files) | 1.4 | 2.9843 | 10.56 | 14.21 | n/a | 0.17 | 0.03 | n/a | 325 | 69 |

Per-class aggregates (bytes summed; throughput byte-weighted):
| class | files | HF ratio | FAST ratio | BAL ratio | FAST vs HF size | BAL vs HF size | HF comp | FAST comp | BAL comp | HF dec | FAST dec | BAL dec |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| F32 | 5 | 1.1579 | 1.2601 | 1.3670 | +8.1% | +15.3% | 953 | 244 | 0.05 | 5352 | 1755 | 358 |
| BF16 | 2 | 1.1476 | 1.4885 | 1.4917 | +22.9% | +23.1% | 680 | 306 | 0.05 | 3788 | 1663 | 7 |
| GGUF-F16 | 1 | 1.0006 | 1.5010 | 1.5077 | +33.3% | +33.6% | 2553 | 252 | 0.05 | 16720 | 1069 | 207 |
| GGUF-Q | 3 | 1.0002 | 1.0247 | 1.0257 | +2.4% | +2.5% | 2868 | 139 | 0.04 | 16909 | 1005 | 125 |
| text | 8 | 1.2353 | 2.9001 | 4.0208 | +57.4% | +69.3% | 474 | 192 | 0.06 | 3098 | 1190 | 7 |

### THIRD PUSH 2026-09-02 (scout workflow + v10)
HF BASELINE CORRECTED: RESULT 36 = xet-core Auto compiled verbatim (agents/hf-fairness/xetsim, lz4_flex 0.13.0).
Auto = plain LZ4 on text (1.6-2.5x), bg4 on weights, None on GGUF/parquet. The C always-bg4 baseline
(RESULT 28/34) must not be quoted for text. tbl_vs_real_xet_v91.md = table vs real Xet.
v10 (petree_v10, separate tree): accept huff0 RLE plane (h==1) -> bge-small FAST 1.480 -> 2.350, wfp32 +7.2%,
all else identical; F32 class +16.4% vs Xet (was +9.4%). RESULT 37. Fold into petree AFTER the workflow ends.
Workflow wf_60398ac6-f50 (10 scouts, 2 refuters/lever, synthesis -> INNOVATION-REPORT.md): first run died on
the API session limit; relaunched 14:58 reusing artifacts under agents/<lens>/. Early signals: RESULT 35.

### PLANE-ENTROPY PACKAGE (2026-09-02, owner decision: permissive split-out)
See the OPEN-THREADS entry of the same date; package at /root/mzip-hfbench/plane-entropy/ (not a repo yet).
