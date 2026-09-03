#!/usr/bin/env python3
import collections, json, struct, subprocess, os
CAND = [
 ("bloom-560m",    "bigscience/bloom-560m", "model.safetensors"),
 ("opt-1.3b",      "facebook/opt-1.3b", "model.safetensors"),
 ("llama2-7b",     "NousResearch/Llama-2-7b-hf", "model-00001-of-00002.safetensors"),
 ("openllama3b",   "openlm-research/open_llama_3b_v2", "model.safetensors"),
 ("qwen2vl-2b",    "Qwen/Qwen2-VL-2B-Instruct", "model.safetensors"),
 ("siglip-base",   "google/siglip-base-patch16-224", "model.safetensors"),
 ("dinov2-large",  "facebook/dinov2-large", "model.safetensors"),
 ("whisper-turbo", "openai/whisper-large-v3-turbo", "model.safetensors"),
 ("sdxl-vae-fix",  "madebyollin/sdxl-vae-fp16-fix", "diffusion_pytorch_model.safetensors"),
 ("sd15-unet",     "stable-diffusion-v1-5/stable-diffusion-v1-5", "unet/diffusion_pytorch_model.safetensors"),
 ("gptj-6b",       "EleutherAI/gpt-j-6b", "model.safetensors"),
 ("smolvlm",       "HuggingFaceTB/SmolVLM-Instruct", "model.safetensors"),
 ("distilwhisper", "distil-whisper/distil-large-v3", "model.safetensors"),
 ("clap-audio",    "laion/clap-htsat-unfused", "model.safetensors"),
]
TMP="/root/mzip-hfbench/wprobe"; os.makedirs(TMP,exist_ok=True)
for tag,repo,fn in CAND:
    url=f"https://huggingface.co/{repo}/resolve/main/{fn}"
    hp=f"{TMP}/{tag}.hdr"
    r=subprocess.run(["curl","-sSL","--max-time","120","-r","0-2000000","-o",hp,url],capture_output=True)
    if r.returncode!=0 or not os.path.exists(hp) or os.path.getsize(hp)<16:
        print(f"{tag:15s} FETCH-FAIL"); continue
    raw=open(hp,"rb").read()
    try: n=struct.unpack("<Q",raw[:8])[0]
    except Exception: print(f"{tag:15s} BADHDR"); continue
    if n<=0 or 8+n>len(raw): print(f"{tag:15s} NOTSAFETENSORS/404 (len {n})"); continue
    try: hdr=json.loads(raw[8:8+n])
    except Exception as e: print(f"{tag:15s} JSONFAIL"); continue
    dt=collections.Counter(v["dtype"] for k,v in hdr.items() if k!="__metadata__" and isinstance(v,dict) and "dtype" in v)
    ends=[v["data_offsets"][1] for k,v in hdr.items() if k!="__metadata__" and isinstance(v,dict) and "data_offsets" in v]
    print(f"{tag:15s} start={8+n:>9d} total={8+n+(max(ends) if ends else 0):>13,d} dtypes={dict(dt)}")
    os.remove(hp)
print("PROBEDONE2")
