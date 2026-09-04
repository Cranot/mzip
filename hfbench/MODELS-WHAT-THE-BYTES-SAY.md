# What the bytes say about models

Everything below was measured for a storage question and is read here for a different one: what are
these objects? Each claim names the RESULT in `MZIP-AT-HF-GRANULARITY.txt` it rests on and is marked
**measured** (a direct reading of a number in the record) or **inferred** (a reading I believe follows,
with its unrun check named). Nothing here is from the literature; if it agrees with the literature,
that is a check on both.

## 1. A trained weight is about three bits of information carried in sixteen

**Measured.** The entropy of the bf16 exponent plane is 2.6–2.8 bits, with an interquartile range of
0.05 bits, across 24 models from unrelated publishers, architectures and sizes (R116), and the same to
two decimals on embedding tables (R117). The mantissa byte is 7.96 bits — noise to any statistical
model (R71, R107). A 4-bit K-quant's codes are maximum-entropy: order-1 gains exactly 0.00% (R76);
unpacking int32-packed 4-bit codes gains 0.3% (R87).

**Inferred.** Of the 16 bits a weight occupies, ~3 carry its scale, ~8 are statistically random, and
~4.5 carry function — the bits a good quantiser keeps without quality loss. The gap between the
functional ~4.5 and the statistically distinguishable ~10.7 is over-parameterisation measured at the
bit level rather than the parameter level, and it is the reason quantisation works at all. Everything
above 10.7 is reproducibility payload: bytes a lossless store must keep and nothing uses.
*Unrun check:* perplexity of the 12/4 bit-plane layer-1 model (R115) against the full model — if it is
at Q4 quality, the "functional bits" line sits where this says.

## 2. The magnitude distribution is a constant of training, not of architecture

**Measured.** 23 of 24 tensors between 10.62 and 10.79 bits regardless of size or publisher (R116);
embeddings 10.64–10.81 (R117). The one outlier was an all-zero tensor; two tensors from a model named
`s50pct` sit at exactly 50% zeros (R117).

**Inferred.** Different data, scales and optimisers converge on the same log-scale spread of weight
magnitudes to two decimals of entropy. The spread is a property of *training as practised* — the init
scale and the optimiser's magnitude normalisation — not of what is learned. Departures from 2.7 bits
flag a non-standard process: pruning, structured sparsity, quantisation-aware training, re-casting.
*Unrun check:* a deliberate set — pruned, QAT, distilled, a diffusion U-Net, a pre-Adam model — to see
which ones move the constant and by how much. One outlier is a hint, not a fingerprint.

## 3. Storage precision thresholds learning

**Measured.** Fine-tunes stored in bf16 leave 85–93% of weights byte-identical to the base; the weights
that changed form a full-rank scatter with no low-rank structure (R90, R98). A fine-tune adds ~0.9 bits
per weight of information over its base against the base's ~10.7 (R90).

**Inferred.** Training is dense, so the identical weights are the ones whose cumulative update stayed
below half a bf16 ulp (~0.2% relative). That update existed during training and does not exist in the
file. A published bf16 fine-tune is the trained model rounded back onto the base's grid; "even full
fine-tuning is effectively a sparse update" has a storage explanation before it has a learning-theory
one, and the low-rank structure of a merged LoRA does not survive the rounding (R98). A child carries
roughly 8% of the bits of its parent; models on the Hub form a tree with thin edges.
*Unrun check:* the same fine-tune saved in fp32 — how many weights differ from the base then, and does
the delta have rank.

## 4. Artefacts are pure functions, and they remember their inputs

**Measured.** Quantisation reproduces byte-for-byte across seven files, five publishers, five
architectures, and across two different implementations of Q8_0 (R99, R101, R105, R106). It is
deterministic and hardware-independent (R99). The same weights quantised from f32, f16 and bf16
inputs agree with the published file on 100%, 61% and 3.8% of blocks; the disagreements cluster at
rounding ties, 98.8% of Q4_K misses in the sixteenth of values nearest a tie (R95, R99 addendum).
The Hub's `base_model` tags are wrong for 5.6% of resolvable parents (R110) and absent for the
largest derivative cluster on the Hub (R103); one F32 norm tensor decides in one request (R105).

**Inferred.** A quantised file is a *proof* of its parent: re-run and compare, and an exact match is a
certificate no tag can give. Content proves lineage; metadata claims it. And a quant carries the
precision its publisher used, readable from its tie statistics — any deterministic lossy transform
leaves the precision of its input encoded in the tie structure of its output. Forensics of ML
artefacts from their bytes is possible.
*Unrun check:* the tie signature on a second quantiser family (AWQ or GPTQ) — R83 found AWQ is not a
cast of its declared base, so the forensic reading there is untested.

## 5. The Hub's information is about half its bytes

**Measured.** Of weight bytes: 5.5% exact re-uploads, 7.8% one-ulp twins (R108), 27.7% with a declared
parent (R86), 80% of GGUF bytes derivable by recipe (R109). "Independent weights, same architecture"
is 3.5% (R108). Six config fields and one 64 KiB slice recovered the largest derivative cluster in
twenty minutes over 725 repositories (R100, R103).

**Inferred.** Genuinely new information is on the order of half the weight bytes uploaded, and the
derivation graph explaining the rest is recoverable from content alone. The Hub is a low-rank object
in model space and its rank is measurable.
*Unrun check:* the full accounting — roots versus derivatives by bytes — which is a day's work with
the tools already on the box.

## 6. Incentivised diversity is informationally null, and it has a signature

**Measured.** The largest cluster in the census: 96 uploads of one Qwen3.5-MoE base from a Bittensor
subnet, none declaring a parent, 55 byte-identical, the rest one or two ulps from a sibling in most
weights (R103, R104). 54% of weights at exactly ±1 ulp in a perturbed twin; ~0% in a genuine
fine-tune, where most weights are identical and the rest move by many ulps (R90, R104).

**Inferred.** A subnet that rewards unique models produced 6.6 TB of weights that are informationally
one model with noise, and the noise is distinguishable from learning by the ±1-ulp fraction alone,
with no reference beyond the sibling.

## 7. There is no local structure in weights

**Measured.** The exponent plane is memoryless along storage order: conditioning on the previous
weight or the one above recovers 0.000 bits (R107). Order-1 on quantised codes recovers 0.00% (R76).
The only dependence found anywhere is between a weight's exponent and its own top mantissa bits
(1.5% on bf16, 0.3% on f32 — R115, R117).

**Inferred.** Whatever a model knows is not in the correlation between a weight and its neighbours.
This is the byte-level face of permutation symmetry: neurons within a layer are permutable, so surviving
structure must be reordering-invariant, and reordering-invariant structure has no local signature.
A coder sees a bag of independent draws from one universal distribution; the information is in the
joint geometry of millions of them, and there is no intermediate scale where anything is visible.
That is why the coder side of this programme closed — not because the tools were weak, but because
there is nothing there to find.

## 8. A model is a multiset with a symmetry group, and every byte-level tool ignores the group

**Measured.** Two Hub uploads at Qwen3.6-35B-A3B's exact byte size correlated at r = −0.011 to +0.007
with the base on every tensor tested — "unrelated" by the census, by parent discovery and by the root
index — while carrying its value distribution to every printed digit (std 0.01644, kurtosis 7.52,
sorted vectors agreeing to one ulp). On a full 8192×2048 projection, 0 of 64 base rows appeared intact
in the copy and 64 of 64 base columns did: one permutation of the 2048 residual-stream dimensions,
applied to the input side of q_proj, k_proj and to the layer norm alike (corr with base[p] = 1.00000).
Un-permuted, the copies are 76.8% byte-identical to the base with a 1.53 bits/weight residual (R127).
Ordering the dimensions by a key the model carries itself — the layer-norm weight, ties broken by
column norm — reproduces 75.7% byte identity with no pairwise search at all (modelprint.py).

**Inferred.** A transformer's function is invariant under relabelling its hidden dimensions, reordering
heads and reordering experts. Its identity is therefore a multiset of values plus a group action, and
any scheme that hashes, chunks, correlates or watermarks weights in storage order is measuring a
representative of the orbit, not the model. Content-addressed dedup, base-model inference, copy
detection in incentive systems and licence attribution all share this blind spot, and an economic
incentive to evade copy detection is already manufacturing orbit members at 72 GB each. The
repair is the same everywhere: canonicalise before hashing — the reordering-invariant structure that
claim 7 says is all there is, is also all a fingerprint may use. Unrun check: head and expert
reorderings, and column scalings compensated by the norm, need the same treatment; the canonical key
above handles only the hidden-dimension permutation that was observed.

---

*Written 2026-09-03 from RESULTs 71–117; claim 8 added 2026-09-04 from RESULTs 126–128. The programme that produced these numbers was measuring
compression; the claims above are what the same numbers say when read as facts about models. Where a
claim is inferred, its unrun check is named so it can be promoted or killed.*
