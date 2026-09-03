#!/usr/bin/env python3
"""GGUF -> HF tensor names, written from the names that actually appear rather than from memory.

Read off failing pairs directly (namedump.py). Four families were missing:
  * MoE experts. GGUF FUSES every expert into one tensor, blk.N.ffn_gate_exps.weight with dims
    [n_embd, n_ff, n_expert]; HF keeps mlp.experts.E.gate_proj.weight separately. This is a
    concatenation, not a rename -- expert e occupies slice e of the fused tensor. It is also where the
    bytes are: one such tensor in GLM-5.3 holds 3,221 M elements.
  * DeepSeek MLA. The base calls them layers.N.attn.wq_a / wq_b / wkv / wo_a / wo_b, against GGUF's
    blk.N.attn_q_a / attn_q_b / attn_kv_a_mqa / attn_kv_b / attn_output.
  * Vision towers and multimodal projectors: v.blk.N.* and mm.N.weight.
  * Prefix variants for models that drop the leading 'model.'.
"""
import re

ATTN = {
    "attn_q":      ["self_attn.q_proj", "attn.q_proj", "attention.wq", "attn.wq"],
    "attn_k":      ["self_attn.k_proj", "attn.k_proj", "attention.wk", "attn.wk"],
    "attn_v":      ["self_attn.v_proj", "attn.v_proj", "attention.wv", "attn.wv"],
    "attn_output": ["self_attn.o_proj", "attn.o_proj", "attention.wo", "attn.wo", "self_attn.dense"],
    "attn_qkv":    ["self_attn.qkv_proj", "attn.qkv_proj", "attn.c_attn", "self_attn.query_key_value"],
    "attn_q_a":    ["self_attn.q_a_proj", "attn.wq_a"],
    "attn_q_b":    ["self_attn.q_b_proj", "attn.wq_b"],
    "attn_kv_a_mqa": ["self_attn.kv_a_proj_with_mqa", "attn.wkv_a", "attn.wkv"],
    "attn_kv_b":   ["self_attn.kv_b_proj", "attn.wkv_b"],
}
FFN = {
    "ffn_gate": ["mlp.gate_proj", "feed_forward.w1", "mlp.w1"],
    "ffn_up":   ["mlp.up_proj", "feed_forward.w3", "mlp.w3", "mlp.fc1", "mlp.c_fc"],
    "ffn_down": ["mlp.down_proj", "feed_forward.w2", "mlp.w2", "mlp.fc2", "mlp.c_proj"],
}
EXPS = {"ffn_gate_exps": "gate_proj", "ffn_up_exps": "up_proj", "ffn_down_exps": "down_proj"}
SHEXP = {"ffn_gate_shexp": "mlp.shared_experts.gate_proj",
         "ffn_up_shexp":   "mlp.shared_experts.up_proj",
         "ffn_down_shexp": "mlp.shared_experts.down_proj"}
PRE = ["model.layers.{n}.", "layers.{n}.", "transformer.h.{n}.", "model.decoder.layers.{n}.",
       "gpt_neox.layers.{n}.", "transformer.blocks.{n}.", "model.language_model.layers.{n}."]
VPRE = ["visual.blocks.{n}.", "vision_tower.vision_model.encoder.layers.{n}.",
        "vision_model.encoder.layers.{n}.", "model.vision_tower.blocks.{n}."]

def _lay(n, sufs, pres=None):
    return [p.format(n=n) + s + ".weight" for p in (pres or PRE) for s in sufs]

def expert_names(g):
    """for a fused MoE tensor, the HF name of expert e -- returns (template, part) or None"""
    m = re.match(r"blk\.(\d+)\.(ffn_(?:gate|up|down)_exps)\.weight$", g)
    if not m: return None
    n, part = m.group(1), EXPS[m.group(2)]
    return [p.format(n=n) + "mlp.experts.{e}." + part + ".weight" for p in PRE]

def candidates(g):
    """HF names that would hold the same weights as GGUF tensor g. For a fused MoE tensor this is
    expert 0's name: if that exists, the whole fused tensor is reachable by concatenation."""
    if g.count(".weight") and not g.endswith(".weight"):
        return []                                   # blk.0.ffn_gate_exps.weight.in_sum2 and friends
    if g in ("token_embd.weight",):
        return ["model.embed_tokens.weight", "embed_tokens.weight", "transformer.wte.weight",
                "tok_embeddings.weight", "model.embedding.weight",
                "model.language_model.embed_tokens.weight"]
    if g == "output.weight":
        return ["lm_head.weight", "output.weight", "model.lm_head.weight"]
    ex = expert_names(g)
    if ex: return [t.format(e=0) for t in ex]
    m = re.match(r"blk\.(\d+)\.([a-z_0-9]+)\.weight$", g)
    if m:
        n, part = m.group(1), m.group(2)
        if part in ATTN: return _lay(n, ATTN[part])
        if part in FFN:  return _lay(n, FFN[part])
        if part in SHEXP: return _lay(n, [SHEXP[part]])
        if part == "ffn_gate_inp": return _lay(n, ["mlp.gate", "mlp.router"])
        return []
    m = re.match(r"v\.blk\.(\d+)\.([a-z_0-9]+)\.weight$", g)
    if m:
        n, part = m.group(1), m.group(2)
        sufs = ATTN.get(part) or FFN.get(part) or []
        sufs = sufs + ["attn.qkv", "attn.proj", "mlp.fc1", "mlp.fc2"]
        return _lay(n, sufs, VPRE)
    m = re.match(r"mm\.(\d+)\.weight$", g)
    if m:
        i = m.group(1)
        return [f"multi_modal_projector.linear_{i}.weight", f"mm_projector.{i}.weight",
                f"visual.merger.mlp.{i}.weight", f"model.mm_projector.{i}.weight"]
    return []
