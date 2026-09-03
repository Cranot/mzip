# convert_hf_to_gguf: Qwen3.5-MoE conversion asserts on models without MTP layers

**Summary.** `conversion/qwen.py:303` asserts `self.opt_num_mtp_layers != 0`. Qwen3.5-MoE checkpoints
produced by mergekit (and any fine-tune that drops the multi-token-prediction head) have zero MTP layers
and fail conversion with a bare `AssertionError` rather than converting without the head.

**Reproduce** (commit d30500b, transformers 5.16.1):

    python convert_hf_to_gguf.py OliviaRossi/DAOS-Fusion --vocab-only --outfile v.gguf
    # File "conversion/qwen.py", line 303, in __init__
    #     assert self.opt_num_mtp_layers != 0
    # AssertionError

Also `OliviaRossi/UltiMerge`. Both `Qwen3_5MoeForCausalLM`, both mergekit outputs.

**Why it matters.** In a census of 69 Hub repositories whose declared parent is a converter-registered
architecture, these were the only two the converter refused for a reason other than environment or
missing files. MTP layers are an inference-time speculative-decoding aid; a checkpoint without them is
a valid model.

**Suggested fix.** Treat `opt_num_mtp_layers == 0` as "no MTP tensors to emit" instead of a hard failure,
or surface a message that names the condition and a flag to proceed.
