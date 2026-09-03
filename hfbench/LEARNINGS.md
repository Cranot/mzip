# What this programme learned, apart from the numbers

The measurements are in `MZIP-AT-HF-GRANULARITY.txt`, 99 numbered results. This is the part that
transfers: the structure that turned out to matter, the ideas that died and why, and the mistakes in
method that cost real time — several of which were already written down before I made them again.

## The thesis, arrived at by elimination

Per-chunk statistical coding of weights is **finished**. Not approximately: an order-1 context model
over the exponent plane, with its table cost charged, gains **0.00%** per 64 KiB chunk across sixteen
real slices. Row and column scale models, vertical prediction, sub-byte and bit-plane separation, a
faster Huffman decode table — all measured, all dead. The order-0 assumption the coder rests on is not
an approximation; it is correct.

So every remaining win comes from knowing **how a file was produced**, not from modelling its bytes.
The Hub is the output of a small number of deterministic programs — a quantiser, a merge script, a
format converter, a parquet writer — and a store that knows those programs keeps their inputs rather
than their outputs. Four of them turned out to be invertible or reproducible from what the Hub already
holds:

| program | what it makes recoverable | measured |
|---|---|---|
| llama.cpp's quantiser | one quant from another, or from the base | 14.8x on Q4_K_M; 5.2x per repo |
| a LoRA merge | the merge from base plus two thin factors | up to 11.8x, in 18% of fine-tunes |
| a parquet writer | the file from its decoded pages | 1.75x, byte-identical restore |
| the safetensors→GGUF converter | an F16 GGUF from the safetensors | exactly, 0 bytes |

## Ideas that died, with the reason

Recording these matters as much as the wins: each looked obviously right beforehand.

- **Context modelling on weights.** 0.00% per chunk. The exponent plane really is memoryless.
- **Sub-byte splitting of 8-bit formats.** Nibble, field-aware and bit-plane splits are all *worse*
  than coding the byte. For a one-byte element the 256-symbol alphabet already models the joint
  distribution of sign, exponent and mantissa; splitting the fields discards the dependency between
  them. The conditional column came out exactly equal to the byte column, as the chain rule requires —
  an identity that also confirmed the probe was arithmetically sound.
- **A delta graph over a fine-tune family.** The minimum spanning tree equals the star: every
  fine-tune's cheapest parent is the base itself. Independent trainings share nothing with each other.
  Note the contrast with quantisations, where the star is also right but for the opposite reason —
  there every node is a function of the *same* weights, so the most precise node dominates.
- **Chaining quant predictions.** A lower-precision source predicts worse, by more than its smaller
  size saves. Keep the highest-precision file; the choice is within 2% either way.
- **Recompression residual for gzip.** Two deflate streams from different producers diverge everywhere
  after the first divergence; the difference between them is noise and costs the whole file (−0.1%).
  The lever is all-or-nothing per file.
- **LZ4 at high compression**, to stay in Xet's format: dominated by zstd on both axes in 11 of 11
  corpora.
- **Keeping a smaller file whole** in a quant repo: 5.1x against 5.2x. Nearly a tie, and the wrong way.

## Traps in method, all of them paid for

**A null result from a weak instrument is not evidence of absence.** The sharpest lesson of the
session. I attributed unpredicted K-quant scales to the missing imatrix, then *retracted* it because
removing the imatrix moved a range-based predictor by only 7 points. With ggml's real search
implemented, the same comparison showed 33 points. The original claim was right; my retraction was an
artefact of an instrument too blunt to see the effect. Before concluding "no effect", ask whether the
instrument could have detected one.

**Sample by the thing you are measuring.** A first sweep drew one file per repo from a repo-uniform
shuffle and concluded that HDF5, protobuf and Arrow are incompressible. The dataset population's top
twelve authors hold 19.6% of repos and **0.4% of the bytes**, so that sweep measured junk. Re-run
byte-weighted, the conclusions changed.

**A zero that contradicts a filename is a parser bug.** A provenance census reported an imatrix named
in 0 of 60 repos, including repos whose names end in `-i1-GGUF`, the publisher's own marker for an
imatrix quant. The embedded tokenizer vocabulary pushes GGUF metadata past a 1 MiB header window. At
24 MiB: 15 of 35. The same trap was already recorded from earlier work on the same file format.

**`pgrep -f` matches the command line that contains the pattern.** Killing a background job by pattern
killed my own shell, twice, because the pattern appeared in the ssh command doing the killing. Kill by
PID. This too was already in my notes before I repeated it.

**Heredocs over ssh mangle multi-line payloads.** Also already recorded; also repeated. Write the file
locally, copy it, run it.

**`gh auth refresh` updates gh's token, not git's.** The push kept failing with the old credential from
git's store until `gh auth setup-git` pointed git at the refreshed one.

**Speed numbers need a quiet machine; ratios do not.** Ratios are load-independent and can be trusted
from a busy box. Every speed figure here was re-taken on a pinned core with the load recorded.

## What the adversarial reviews were worth

Three rounds against the coder found nothing wrong with the compressor and a great deal wrong with its
**tests**: a fuzzer that counted outcomes instead of asserting them and always exited zero, frozen
vectors that were silently skipped when the file was missing, a portability build that left half the
vector paths compiled in, a sanitizer configured to recover instead of halt. Also three real defects in
the code: a varint that accepted a non-canonical tenth byte, a size bound that overflowed near the
maximum, and a C entry point that could throw across the boundary.

The reviewer also refused twice on wording before saying anything useful — describing what code *does*
gets through where describing what it *defends against* does not — and both early rounds silently
reviewed without building, because the sandbox defaulted to read-only.

## The one thing that was structural, not incremental

Every large win came from noticing that two files on the Hub are **the same information in different
clothes**, and that the transformation between them is deterministic. The measurements that mattered
were not "how compressible is this file" but "what program produced it, and do we hold that program's
inputs". That question is worth asking of any store whose contents are machine-generated.

## Added after the FP8 attempt

**Check the instrument before believing a negative.** An FP8-given-base probe returned 0.05-0.96%
exact, which looks like a clean refutation. It was not: the E4M3 encoder mishandled every subnormal
(226 of 240 patterns round-tripped) and the scale was modelled as one per tensor when checkpoints
store one per 128x128 block. Both errors produce exactly the same output as a true negative. The fix
was to build the encoder as a nearest-representable mapping derived from the decoder, so the two
cannot disagree, and to self-test it against all 256 patterns before using it. Prefer instruments that
are correct by construction over instruments that look right.

**The same obstacle keeps reappearing under different names.** Official Qwen GGUFs, and now an
AWQ-FP8 checkpoint, both turned out to hold a per-channel rescaling of the published weights rather
than the weights themselves. Whenever a quantisation cannot be reproduced from its declared base, ask
first whether a function-preserving transform was applied before quantising -- it is the commonest
reason, and the scale vectors that would undo it are not published.

## Session of 2026-09-03 — opening the classes we had never opened

The programme had measured weights to exhaustion and never looked at what else the week contains. Four
classes were opened; two were dead ends, and saying so is most of their value.

### The instrument failed six times, and every failure looked like a result

Not one of these announced itself. Each produced a plausible number that a reader would have accepted.

**Weight by the thing you are estimating, not by the thing you sampled.** The first tar census weighted
each archive's member mix by the bytes SAMPLED from it. One repo with large members became 93% of the
total and the answer came back "97% of archive bytes have no file extension", which read convincingly
as Docker layers. Weighted by each tar's real size, it is 65% video and 18% images. The sample weight
and the population weight are different quantities and swapping them silently inverts the conclusion.

**When a class resists inference, read it.** Three predictions about the archive class missed in a row --
zip-dominant (it is tar), deflate-dominant (it is stored), containers (it is video). Every one inferred
content from a proxy: the extension, the sampled byte count. The run that settled it printed the member
NAMES: ./N8VlTva3z0E.mp4, 000047-00121010.mp3, task_434/images/ep_826275/frame_000585.png. One look at
the data beat three inferences from its metadata.

**A test whose winning branch is a tautology cannot fail.** The packed-weights probe chose its code
width by minimising entropy-per-bit, and 8 bits always won -- because at 8 bits "unpacking" an int32
into four bytes IS the raw byte view, so the packed and unpacked columns were equal by construction
(7.719 against 7.719). It reported a 0.3% gain, which happened to be near the truth, from a comparison
that could not have reported anything else. Forcing every width showed the real answer: no width beats
raw. Same defect as a regression test that passes without its fix.

**A blank is not a refutation.** The Q/K permutation branch is gated on the head count from the base's
config.json, fetched with a helper built for the /api/ endpoints, which returns None for resolve/main/
files. So nh was 0, the branch never ran, the column printed "-", and TWO rounds concluded the
permutation hypothesis was refuted. A direct fetch returns 24, 32, 40. With the fix, the permutation
won on exactly Q and K and nowhere else, and the aggregate went from 25.3% to 7.7% -- 4.0x to 13.0x.
The absent measurement was read as a benign default, and the default said "no".

**A permuted tensor cannot be compared on a prefix.** llama.cpp interleaves each head's row halves
across the WHOLE tensor, so the GGUF's first N rows are a scattered set of base rows. Prefix
comparisons gave 61% in one sample and 3.9% in another; both were partial overlap, neither was a
measurement. If a transform is global, the comparison window must be too.

**Measuring one tensor and calling it the model -- twice in one day.** Morning: a per-type quant run
printed three identical rows because the largest tensor COMMON to each pair is the embedding, Q6_K in
every file. Afternoon: the first cross-repo table read 99.14% and 14x on six rows that were all
output.weight, because the selector sorts by size. Per kind it is 7.7% and 13.0x, and attn_q's real
behaviour was hiding behind that selector. Identical numbers across rows that should differ is the
cheapest available signal that a selection is wrong -- look for it deliberately.

### Do not bill for work someone else already did

The fine-tune result was 26.5% of standalone, 3.77x, until 11 of its 24 tensors turned out to be
100.00% byte-identical to the base. Those are contiguous identical bytes, which Xet's chunk dedup
already collapses; counting them credits us with dedup's work. On the tensors a fine-tune actually
changed it is 48.5%, 2.06x. The honest number is roughly half the flattering one.

The same discipline produced the strongest argument in the programme. Within those CHANGED tensors,
39.6% of individual weights are still identical to the base -- but scattered, so no 64 KiB chunk ever
repeats and dedup sees none of them. That is the case for coding against a parent rather than hashing
against it, and it is measured rather than asserted.

### Negative results on the biggest classes are the load-bearing ones

The largest class of the week is not weights. It is 380 TB of archives, and ~83% of it is
already-compressed video and imagery: no compressor gets anything, mzip included. The 31.75 TB of
int32-packed weights gains 0.3% from unpacking, because a good quantiser spends its codebook and
leaves near-maximum-entropy codes. Both classes now get nothing in the table.

Publishing those two zeroes is what makes the rest credible. A pitch that finds a win in every class
is a pitch nobody believes; the one that reports 380 TB of nothing has earned the number next to it.

### The thesis, sharpened

Per-chunk statistical coding of weights is finished -- order-1 on K-quant codes is 0.00%, unpacking
int32 gains 0.3%, and the largest class on the Hub is incompressible media. Everything left comes from
PROVENANCE, and provenance does not respect repo boundaries. The Hub already records it: base_model
tags cover 27.7% of weight bytes and 80.5% of GGUF bytes. Given a public parent, a Q8_0 costs 7.7% of
itself and a Q5_K or Q6_K reproduces EXACTLY, leaving only its scales. The remaining question is not
whether the arithmetic works but how often the parent resolves -- which is a metadata question, not a
compression one.

## 2026-09-03, evening — "landed" is verified by `git ls-files`, never by the add succeeding

The commit that homed this directory reported 298 files. It landed 75. The repository's `.gitignore`
carries `*.py` and `*.txt` for its own reasons, `git add hfbench` obeyed them silently, and the
commit message, written from the copy count rather than the index, said otherwise. The measurement
record itself -- the one file everything else here cites -- was among the 223 left on one disk.

Found by accident: a later copy of the record into the tree produced no diff, which made no sense
until `git check-ignore -v` named the pattern. Fixed with `!hfbench/**` and a second commit.

Three things worth keeping from it:

- **A directory add is not a file add.** `git add dir/` stages the subset of `dir/` that survives
  every ignore rule, and reports success either way. The only count that means anything is
  `git ls-files dir/ | wc -l` afterwards, compared to `find dir/ -type f | wc -l`.
- **Inherited ignore rules are invisible from inside the subtree.** `hfbench/` had no `.gitignore`
  of its own; the exclusion came from the repo root, 100 lines up, written months earlier for
  unrelated scratch. Anyone auditing the subtree would have found nothing wrong with it.
- **This is the day's sixth instance of the same shape** -- a proxy (files copied) accepted in
  place of the thing (files tracked). The others were file counts for checkpoints, extensions for
  archive contents, sampled bytes for tar weights, tensor size for tensor kind, and a blank column
  for a refutation. Every one was corrected by looking at the actual object instead of its shadow.

## 2026-09-03, night — stop approximating the program; run it

For sixteen results the programme approximated llama.cpp with arithmetic and coded what was left:
Q8_0 at 1.4%, K-quants at 9-13% of themselves given the parent. Running the actual toolchain on the
actual source reproduced the published file with zero bytes different. The residual coder was solving
a problem that did not exist once the right input was found.

### The wrong answer came from the right file

The first run quantised the publisher's own f16 and got 61% of superblocks identical. Three
hypotheses, in the order they were held:

1. **Version drift.** The publisher's build was one day older. Plausible, unfalsified, wrong.
2. **Hardware.** FMA contraction flipping the last bit of a scale. Tested: native build twice, then
   a build with FMA/AVX/AVX512 off. All three byte-identical. **Falsified in one run.**
3. **Input precision.** The source is float32; the publisher quantised from f32, not the f16 he also
   shipped. f32 -> 100.00%, f16 -> 61.04%, bf16 -> 3.84%. Monotonic, complete, nothing left over.

The lesson is not "input precision matters" -- it is that the second hypothesis was the *expensive*
one to hold and the *cheapest* one to kill, and it should have been killed before the first
hypothesis was even entertained. Determinism of a tool is a five-minute test. Version bisection is a
day. Test the cheap kill first, whatever your intuition ranks it.

### `cmp` is the wrong instrument for a container

A 1,216-byte difference in GGUF key-value metadata shifts every tensor's offset, and a byte-wise
compare reports 2.3 GB "differing" between two files whose tensor data is identical. Two probes
today reported nonsense because of it before a tensor-level comparator was written. If a format has a
header and a body, compare the body at the header's offsets -- the same discipline as the archive
class earlier, where the answer was in the member names, not the byte stream.

### Two filter bugs, same shape as the morning's

- The model-info endpoint returns siblings **without sizes**; a size filter read every file as 0 and
  rejected all 400 candidates as "too small". FIND_FAILED looked like "the Hub has no such repo".
- The imatrix filter required `.dat`; bartowski names his `.imatrix` or `.gguf`. The recipe would
  have run without the imatrix and failed for the wrong reason, and that failure would have read as
  "the imatrix path does not reproduce".

Both are the day's recurring defect: an absent or differently-shaped field read as a value, producing
a confident negative. The count for the day is now eight. Every one was found by printing the
actual objects (`maxf16=0.0GB` on every row; `imatrix NONE` beside a repo known to have one) rather
than trusting the summary the filter produced.

### What the recipe result does and does not change

Per file: everything. A resolved K-quant costs a pointer and four version strings instead of 9-13% of
itself, and the store never needs a residual coder for it. Aggregate: half a point, because the
arithmetic route had already taken most of the GGUF class and the headline is bounded by resolution
and by the classes nothing reaches. Write both numbers next to each other or the reader will pick
the one that flatters.
