#!/usr/bin/env python3
"""Candidates for the third recipe publisher and for a MoE recipe test.

Constraints are practical, not scientific: a parent whose safetensors fit the box (bf16 parent + one
conversion + one quant + the published quant must stay under ~60 GB), a publisher other than
mradermacher or bartowski, a Q4_K_M or Q8_0 target, and for the MoE case a parent whose config declares
experts. Prints two ranked lists from the tag census; nothing is downloaded."""
import json, sys, collections
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from hfget import get_json
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def tree_bytes(rid, kind="models"):
    t = api(f"{H}/api/{kind}/{rid}/tree/main?recursive=1&expand=1")
    if not isinstance(t, list): return None, {}
    files = {f["path"]: int(f.get("size") or (f.get("lfs") or {}).get("size") or 0) for f in t if f.get("type") == "file"}
    return files, files

tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
pairs = []
for rid, t in tags.items():
    if "gguf" not in rid.lower(): continue
    for x in (t.get("tags") or []):
        p = x.split(":")
        if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
            pairs.append((rid, ":".join(p[2:]))); break
pub = collections.Counter(r.split("/")[0] for r, _ in pairs)
print("GGUF publishers in the census:", pub.most_common(12), "\n")
third, moe = [], []
LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 160
seen = 0
for rid, base in pairs:
    if seen >= LIMIT: break
    owner = rid.split("/")[0]
    files, _ = tree_bytes(rid)
    if not files: continue
    seen += 1
    q = [f for f in files if f.lower().endswith(".gguf") and ("q4_k_m" in f.lower() or "q8_0" in f.lower()) and "-0000" not in f and "mmproj" not in f.lower()]
    if not q: continue
    im = [f for f in files if "imatrix" in f.lower() and not f.lower().endswith((".md", ".json"))]
    cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
    tc = cfg.get("text_config") or cfg
    experts = tc.get("num_experts") or tc.get("num_local_experts") or tc.get("n_routed_experts") or 0
    mt = cfg.get("model_type", "?")
    dtype = cfg.get("torch_dtype", "?")
    pf, _ = tree_bytes(base)
    if not pf: continue
    st = sum(v for k, v in pf.items() if k.endswith(".safetensors"))
    if st == 0: continue
    qf = min(q, key=lambda f: files[f])
    row = dict(rid=rid, base=base, owner=owner, q=qf, qsize=files[qf], imatrix=bool(im), mt=mt, experts=experts, dtype=dtype, parent_gb=st / 1e9)
    if experts and st < 45e9: moe.append(row)
    if owner not in ("mradermacher", "bartowski") and st < 20e9: third.append(row)
print(f"scanned {seen} GGUF repos\n")
print("=== THIRD PUBLISHER candidates (parent < 20 GB, not mradermacher/bartowski) ===")
for r in sorted(third, key=lambda r: r["parent_gb"])[:10]:
    print(f"  {r['parent_gb']:5.1f} GB  {r['owner']:20s} {r['rid'][:44]:44s} {r['mt']:12s} {r['dtype']:8s} imatrix={r['imatrix']}  {r['q'][-28:]}")
print("\n=== MoE candidates (config declares experts, parent < 45 GB) ===")
for r in sorted(moe, key=lambda r: r["parent_gb"])[:10]:
    print(f"  {r['parent_gb']:5.1f} GB  {r['owner']:20s} {r['rid'][:44]:44s} {r['mt']:12s} experts={r['experts']:<4} {r['dtype']:8s} imatrix={r['imatrix']}  {r['q'][-28:]}")
json.dump(dict(third=third, moe=moe), open(f"{B}/qgq/findcands.json", "w"), indent=1)
print("FINDCANDS_DONE")
