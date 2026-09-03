#!/usr/bin/env python3
"""Does the converter actually accept the parents the registry says it supports?

RESULT 109 counts a parent as resolvable when its architecture class is registered. Registered is not
the same as convertible: a model can name LlamaForCausalLM and still trip the converter on a tokenizer
it does not know, a config field it expects, or a shard layout. `convert_hf_to_gguf.py --vocab-only`
instantiates the model class and writes a metadata-only GGUF from config + tokenizer files -- a few MB,
no weights -- and fails exactly where a full conversion would fail before touching tensors. Run on every
resolvable parent from resolverate4's pass; the failure rate multiplies the resolution rate."""
import json, os, sys, subprocess, shutil, collections, tempfile
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api
from hfget import get_json
from recipe import dl
from resolverate4 import converter_registry
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
PY = "/root/llama.cpp/cvenv/bin/python"; CV = "/root/llama.cpp/convert_hf_to_gguf.py"
W = f"{B}/convcheck"; os.makedirs(W, exist_ok=True)
SMALL = (".json", ".model", ".txt", ".tiktoken", ".jinja", ".py")

def main():
    reg = converter_registry()
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, t in tags.items():
        for x in (t.get("tags") or []):
            p = x.split(":")
            if x.startswith("base_model:") and len(p) >= 3 and p[1] == "quantized":
                pairs.append((rid, ":".join(p[2:]))); break
    order = [p for p in pairs if "gguf" in p[0].lower()] + [p for p in pairs if "gguf" not in p[0].lower()]
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 140
    seen = set(); why = collections.Counter(); ok = fail = 0; att = 0
    for rid, base in order[:LIMIT]:
        if base in seen: continue
        db = api(f"{H}/api/models/{base}")
        if not db: continue
        sib = [s["rfilename"] for s in db.get("siblings", [])]
        if not any(f.endswith((".safetensors", ".bin", ".pt")) for f in sib): continue
        cfg = get_json(f"{H}/{base}/resolve/main/config.json") or {}
        if not any(a in reg for a in (cfg.get("architectures") or [])): continue
        seen.add(base); att += 1
        d = f"{W}/{base.replace('/', '__')}"; os.makedirs(d, exist_ok=True)
        for f in sib:
            if f.endswith(SMALL) and "/" not in f and not dl(base, f, f"{d}/{f}"):
                pass
        # the converter wants at least one weight file to exist to read the header for shapes in
        # some paths; --vocab-only should not, but give it the index if there is one
        r = subprocess.run([PY, CV, d, "--vocab-only", "--outfile", f"{d}/vocab.gguf", "--outtype", "f16"],
                           capture_output=True, text=True, timeout=600)
        if r.returncode == 0 and os.path.exists(f"{d}/vocab.gguf"):
            ok += 1
        else:
            fail += 1
            err = (r.stderr.strip().splitlines() or ["?"])[-1][:110]
            key = err.split(":")[0][:60] if ":" in err else err[:60]
            why[key] += 1
            print(f"  FAIL {base[:44]:44s} {err}")
        shutil.rmtree(d, ignore_errors=True)
    print(f"\nresolvable parents tried: {att}   converter accepts: {ok} = {100*ok/max(att,1):.1f}%   fails: {fail}")
    for k, v in why.most_common(8): print(f"    {v:3d}  {k}")
    json.dump(dict(att=att, ok=ok, fail=fail, why=dict(why)), open(f"{B}/qgq/convcheck.json", "w"), indent=1)
    print("CONVCHECK_DONE")

if __name__ == "__main__":
    main()
