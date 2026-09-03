#!/usr/bin/env python3
"""Fetch 8MiB mid-network weight slices via HTTP range requests.

Selection rule (uniform across every model, so architecture comparisons are not
confounded by embedding tables):
  - parse the safetensors JSON header
  - drop tensors whose name looks like an embedding / lm_head / position table
  - drop tensors not of the model's MAJORITY dtype
  - pick the surviving tensor whose data_offsets[0] is closest to the MEDIAN
    start offset of the surviving set
  - range-get 8MiB beginning at that tensor's start
"""
import collections, json, os, struct, subprocess, sys, hashlib

W    = "/root/mzip-hfbench/weights2"
SLICE = 8 * 1024 * 1024
os.makedirs(W, exist_ok=True)

MODELS = [
 # tag,              repo,                                        file,                                          arch,        expect
 ("gpt2",            "openai-community/gpt2",                     "model.safetensors",                           "llm-dec",   "F32"),
 ("vit-base",        "google/vit-base-patch16-224",               "model.safetensors",                           "vision",    "F32"),
 ("wav2vec2-base",   "facebook/wav2vec2-base-960h",               "model.safetensors",                           "audio",     "F32"),
 ("clip-vitL14",     "openai/clip-vit-large-patch14",             "model.safetensors",                           "vis-lang",  "F32"),
 ("sd15-unet",       "stable-diffusion-v1-5/stable-diffusion-v1-5","unet/diffusion_pytorch_model.safetensors",   "diffusion", "F32"),
 ("qwen2.5-1.5b",    "Qwen/Qwen2.5-1.5B",                         "model.safetensors",                           "llm-dec",   "BF16"),
 ("tinyllama-1.1b",  "TinyLlama/TinyLlama-1.1B-Chat-v1.0",        "model.safetensors",                           "llm-dec",   "BF16"),
 ("smolvlm",         "HuggingFaceTB/SmolVLM-Instruct",            "model.safetensors",                           "vis-lang",  "BF16"),
 ("bloom-560m",      "bigscience/bloom-560m",                     "model.safetensors",                           "llm-dec",   "F16"),
 ("llama2-7b",       "NousResearch/Llama-2-7b-hf",                "model-00001-of-00002.safetensors",            "llm-dec",   "F16"),
 ("whisper-lv3",     "openai/whisper-large-v3",                   "model.safetensors",                           "audio",     "F16"),
 ("sdxl-unet",       "stabilityai/stable-diffusion-xl-base-1.0",  "unet/diffusion_pytorch_model.fp16.safetensors","diffusion", "F16"),
]

SKIP = ("embed", "wte", "wpe", "shared", "lm_head", "position", "token_type",
        "class_embedding", "patch_embed", "pos_emb", "logit")

meta = []
for tag, repo, fn, arch, expect in MODELS:
    url = f"https://huggingface.co/{repo}/resolve/main/{fn}"
    hp  = f"{W}/{tag}.hdr"
    r = subprocess.run(["curl","-sSL","--max-time","180","-r","0-3000000","-o",hp,url],
                       capture_output=True)
    if r.returncode != 0 or not os.path.exists(hp):
        print(f"{tag:15s} HDR-FETCH-FAIL"); continue
    raw = open(hp,"rb").read(); os.remove(hp)
    n = struct.unpack("<Q", raw[:8])[0]
    if n <= 0 or 8+n > len(raw):
        print(f"{tag:15s} HDR-TOO-BIG {n}"); continue
    hdr = json.loads(raw[8:8+n]); base = 8 + n
    ts = [(k,v) for k,v in hdr.items()
          if k != "__metadata__" and isinstance(v,dict) and "data_offsets" in v]
    dtmaj = collections.Counter(v["dtype"] for k,v in ts).most_common(1)[0][0]
    if dtmaj != expect:
        print(f"{tag:15s} DTYPE-DRIFT expected {expect} got {dtmaj}")
    filesize = base + max(v["data_offsets"][1] for k,v in ts)
    cand = [(k,v) for k,v in ts
            if v["dtype"] == dtmaj
            and not any(s in k.lower() for s in SKIP)
            and base + v["data_offsets"][0] + SLICE <= filesize]
    if not cand:
        print(f"{tag:15s} NO-CANDIDATE-TENSOR"); continue
    starts = sorted(v["data_offsets"][0] for k,v in cand)
    med = starts[len(starts)//2]
    name, tv = min(cand, key=lambda kv: abs(kv[1]["data_offsets"][0] - med))
    off = base + tv["data_offsets"][0]

    sp = f"{W}/{tag}.bin"
    r = subprocess.run(["curl","-sSL","--max-time","600","-r",f"{off}-{off+SLICE-1}","-o",sp,url],
                       capture_output=True)
    sz = os.path.getsize(sp) if os.path.exists(sp) else 0
    if sz != SLICE:
        print(f"{tag:15s} SLICE-FAIL got {sz}"); continue
    sha = hashlib.sha256(open(sp,"rb").read()).hexdigest()
    print(f"{tag:15s} {dtmaj:5s} {arch:10s} off={off:>12d} sz={sz} tensor={name}", flush=True)
    meta.append(dict(tag=tag, repo=repo, file=fn, arch=arch, dtype=dtmaj,
                     offset=off, size=sz, tensor=name, sha256=sha, filesize=filesize))

json.dump(meta, open(f"{W}/slices.json","w"), indent=1)
print(f"FETCHED {len(meta)}/{len(MODELS)}")
print("FETCHDONE")
