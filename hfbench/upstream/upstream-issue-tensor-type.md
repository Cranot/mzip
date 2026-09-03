# `--tensor-type` override is silently discarded for `attn_output` on MoE models

**Summary.** `llama-quantize --tensor-type blk.N.attn_output.weight=Q4_K` (or the same entry via
`--tensor-type-file`) has no effect on mixture-of-experts models: the tensor comes out `Q6_K`. The
MoE rule that promotes `attn_output` to `Q6_K` when `n_expert >= 8` is applied after the user override
and wins.

**Reproduce** (commit d30500b, Linux x86-64, also with all SIMD off):

    llama-quantize --tensor-type-file types.txt model-bf16.gguf out.gguf Q4_K_M
    # types.txt contains: blk.0.attn_output.weight=Q4_K
    # out.gguf: blk.0.attn_output.weight is Q6_K

Observed on `allenai/OLMoE-1B-7B-0924` (64 experts): all 16 `attn_output` tensors ignore the pin. On
`ibm-granite/granite-3.0-3b-a800m-instruct` (40 experts): 7 do. Dense models honour the same pin.

**Why it matters.** Reproducing a published quant byte-for-byte from its source requires pinning each
tensor's type to what the published header records. Every other tensor kind honours the pin and
reproduces exactly (26.7M/26.7M superblocks on a 64-expert model from a current build); `attn_output`
is the one that cannot be pinned, so a file made by an older build whose MoE rule differed cannot be
reproduced without patching the quantiser.

**Expected.** An explicit per-tensor override should be final; heuristics should apply only where no
override matched. If the current precedence is intentional, the documentation for `--tensor-type`
should say which built-in rules outrank it.
