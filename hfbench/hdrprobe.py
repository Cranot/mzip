#!/usr/bin/env python3
"""Header-only dtype probe. Fetches ~2MB of each safetensors file, parses the
JSON header, reports dtype histogram and where the tensor region starts."""
import collections, json, struct, subprocess, sys, os

CAND = [
 ("sdxl-unet-f16", "stabilityai/stable-diffusion-xl-base-1.0", "unet/diffusion_pytorch_model.fp16.safetensors"),
 ("sdxl-vae-f16",  "stabilityai/sdxl-vae", "diffusion_pytorch_model.fp16.safetensors"),
 ("flux-schnell",  "black-forest-labs/FLUX.1-schnell", "flux1-schnell.safetensors"),
 ("whisper-lv3",   "openai/whisper-large-v3", "model.safetensors"),
 ("whisper-small", "openai/whisper-small", "model.safetensors"),
 ("clip-vit-L14",  "openai/clip-vit-large-patch14", "model.safetensors"),
 ("vit-base",      "google/vit-base-patch16-224", "model.safetensors"),
 ("qwen2.5-1.5b",  "Qwen/Qwen2.5-1.5B", "model.safetensors"),
 ("tinyllama-1.1b","TinyLlama/TinyLlama-1.1B-Chat-v1.0", "model.safetensors"),
 ("wav2vec2-base", "facebook/wav2vec2-base-960h", "model.safetensors"),
 ("musicgen-small","facebook/musicgen-small", "model.safetensors"),
 ("sd21-unet-f16", "stabilityai/stable-diffusion-2-1-base", "unet/diffusion_pytorch_model.fp16.safetensors"),
 ("dinov2-base",   "facebook/dinov2-base", "model.safetensors"),
 ("resnet50",      "microsoft/resnet-50", "model.safetensors"),
]

TMP = "/root/mzip-hfbench/wprobe"
os.makedirs(TMP, exist_ok=True)

for tag, repo, fn in CAND:
    url = f"https://huggingface.co/{repo}/resolve/main/{fn}"
    hp = f"{TMP}/{tag}.hdr"
    r = subprocess.run(["curl","-sSL","--max-time","120","-r","0-2000000","-o",hp,url],
                       capture_output=True)
    if r.returncode != 0 or not os.path.exists(hp) or os.path.getsize(hp) < 16:
        print(f"{tag:16s} FETCH-FAIL rc={r.returncode}"); continue
    raw = open(hp,"rb").read()
    if raw[:1] in (b"<", b"{") and b"error" in raw[:400].lower():
        print(f"{tag:16s} ERRBODY {raw[:120]!r}"); continue
    try:
        n = struct.unpack("<Q", raw[:8])[0]
    except Exception as e:
        print(f"{tag:16s} BADHDR {e}"); continue
    if n <= 0 or 8+n > len(raw):
        print(f"{tag:16s} HDRLEN {n} > probe {len(raw)}"); continue
    try:
        hdr = json.loads(raw[8:8+n])
    except Exception as e:
        print(f"{tag:16s} JSONFAIL {e}"); continue
    dt = collections.Counter(v["dtype"] for k,v in hdr.items()
                             if k != "__metadata__" and isinstance(v,dict) and "dtype" in v)
    # total file size = 8 + n + max end offset
    ends = [v["data_offsets"][1] for k,v in hdr.items()
            if k != "__metadata__" and isinstance(v,dict) and "data_offsets" in v]
    total = 8 + n + (max(ends) if ends else 0)
    print(f"{tag:16s} start={8+n:>9d} totalfile={total:>13,d} dtypes={dict(dt)}")
    os.remove(hp)
print("PROBEDONE")
