#!/usr/bin/env python3
"""Family B: 8MiB drawn ENTIRELY FROM INSIDE one dense weight matrix.
Removes the tensor-boundary / constant-mask confound found in family A."""
import collections, hashlib, json, os, struct, subprocess

W = "/root/mzip-hfbench/weightsB"; SLICE = 8*1024*1024
os.makedirs(W, exist_ok=True)
MODELS = [
 ("gpt2",           "openai-community/gpt2",                     "model.safetensors",                            "llm-dec",  "F32"),
 ("vit-base",       "google/vit-base-patch16-224",               "model.safetensors",                            "vision",   "F32"),
 ("wav2vec2-base",  "facebook/wav2vec2-base-960h",               "model.safetensors",                            "audio",    "F32"),
 ("clip-vitL14",    "openai/clip-vit-large-patch14",             "model.safetensors",                            "vis-lang", "F32"),
 ("sd15-unet",      "stable-diffusion-v1-5/stable-diffusion-v1-5","unet/diffusion_pytorch_model.safetensors",    "diffusion","F32"),
 ("qwen2.5-1.5b",   "Qwen/Qwen2.5-1.5B",                         "model.safetensors",                            "llm-dec",  "BF16"),
 ("tinyllama-1.1b", "TinyLlama/TinyLlama-1.1B-Chat-v1.0",        "model.safetensors",                            "llm-dec",  "BF16"),
 ("smolvlm",        "HuggingFaceTB/SmolVLM-Instruct",            "model.safetensors",                            "vis-lang", "BF16"),
 ("bloom-560m",     "bigscience/bloom-560m",                     "model.safetensors",                            "llm-dec",  "F16"),
 ("llama2-7b",      "NousResearch/Llama-2-7b-hf",                "model-00001-of-00002.safetensors",             "llm-dec",  "F16"),
 ("whisper-lv3",    "openai/whisper-large-v3",                   "model.safetensors",                            "audio",    "F16"),
 ("sdxl-unet",      "stabilityai/stable-diffusion-xl-base-1.0",  "unet/diffusion_pytorch_model.fp16.safetensors","diffusion","F16"),
 ("smollm2-135m",   "HuggingFaceTB/SmolLM2-135M",                "model.safetensors",                            "llm-dec",  "BF16"),
 ("qwen2.5-0.5b",   "Qwen/Qwen2.5-0.5B",                         "model.safetensors",                            "llm-dec",  "BF16"),
 ("minilm",         "sentence-transformers/all-MiniLM-L6-v2",    "model.safetensors",                            "encoder",  "F32"),
]
SKIP = ("embed","wte","wpe","shared","lm_head","position","token_type","logit",
        "attn.bias","masked_bias","patch_embed","pos_emb","class_embedding")
meta=[]
for tag,repo,fn,arch,expect in MODELS:
    url=f"https://huggingface.co/{repo}/resolve/main/{fn}"
    hp=f"{W}/{tag}.hdr"
    r=subprocess.run(["curl","-sSL","--max-time","180","-r","0-3000000","-o",hp,url],capture_output=True)
    if r.returncode!=0 or not os.path.exists(hp): print(f"{tag:15s} HDRFAIL"); continue
    raw=open(hp,"rb").read(); os.remove(hp)
    n=struct.unpack("<Q",raw[:8])[0]
    if n<=0 or 8+n>len(raw): print(f"{tag:15s} HDRBIG"); continue
    hdr=json.loads(raw[8:8+n]); base=8+n
    ts=[(k,v) for k,v in hdr.items() if k!="__metadata__" and isinstance(v,dict) and "data_offsets" in v]
    dtmaj=collections.Counter(v["dtype"] for k,v in ts).most_common(1)[0][0]
    # dense weight matrices at least one slice long, name ends in .weight, 2-D
    cand=[(k,v) for k,v in ts
          if v["dtype"]==dtmaj and k.lower().endswith("weight")
          and len(v.get("shape",[]))==2
          and (v["data_offsets"][1]-v["data_offsets"][0])>=SLICE
          and not any(s in k.lower() for s in SKIP)]
    if not cand:
        print(f"{tag:15s} NO-DENSE>=8MiB (largest {max((v['data_offsets'][1]-v['data_offsets'][0]) for k,v in ts):,})")
        continue
    starts=sorted(v["data_offsets"][0] for k,v in cand); med=starts[len(starts)//2]
    name,tv=min(cand,key=lambda kv: abs(kv[1]["data_offsets"][0]-med))
    tsz=tv["data_offsets"][1]-tv["data_offsets"][0]
    off=base+tv["data_offsets"][0]            # start of that matrix
    sp=f"{W}/{tag}.bin"
    r=subprocess.run(["curl","-sSL","--max-time","600","-r",f"{off}-{off+SLICE-1}","-o",sp,url],capture_output=True)
    sz=os.path.getsize(sp) if os.path.exists(sp) else 0
    if sz!=SLICE: print(f"{tag:15s} SLICEFAIL {sz}"); continue
    z=100.0*open(sp,"rb").read().count(0)/sz
    print(f"{tag:15s} {dtmaj:5s} {arch:10s} zero%={z:5.2f} tensor={name} shape={tv['shape']} tsz={tsz:,}",flush=True)
    meta.append(dict(tag=tag,repo=repo,file=fn,arch=arch,dtype=dtmaj,offset=off,size=sz,
                     tensor=name,shape=tv["shape"],tensor_bytes=tsz,zero_pct=z,
                     sha256=hashlib.sha256(open(sp,"rb").read()).hexdigest()))
json.dump(meta,open(f"{W}/slicesB.json","w"),indent=1)
print(f"FETCHED {len(meta)}/{len(MODELS)}"); print("FETCHDONE_B")
