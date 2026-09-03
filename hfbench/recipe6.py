#!/usr/bin/env python3
"""The recipe with per-tensor types pinned from the published header.

recipe4 on bartowski/CogEvol-4B: every tensor of the SAME type is byte-exact (14.2M superblocks,
100.00%), and 111 of 426 tensors differ ONLY in type -- the publisher's llama.cpp assigned Q5_K, Q6_K,
Q8_0 or F32 where today's Q4_K_M default assigns Q4_K, on a hybrid-SSM architecture whose type
heuristics have moved between versions. The published header records every tensor's type, so the
recipe can carry them as overrides and stop depending on the heuristic at all.

    python3 recipe6.py <recipe4 workdir>
Re-fetches the parent, converts at bf16, quantises with the imatrix and a --tensor-type-file built from
the published header, compares tensor by tensor."""
import json, os, sys, subprocess
import numpy as np
B = "/root/mzip-hfbench"; H = "https://huggingface.co"
sys.path.insert(0, B)
from recipe import dl, read_header_file
from recipe4 import tree, compare
Q = "/root/llama.cpp/build/bin/llama-quantize"; PY = "/root/llama.cpp/cvenv/bin/python"
CV = "/root/llama.cpp/convert_hf_to_gguf.py"
TN = {0: "F32", 1: "F16", 2: "Q4_0", 3: "Q4_1", 6: "Q5_0", 7: "Q5_1", 8: "Q8_0", 9: "Q8_1", 10: "Q2_K",
      11: "Q3_K", 12: "Q4_K", 13: "Q5_K", 14: "Q6_K", 15: "Q8_K", 16: "IQ2_XXS", 17: "IQ2_XS", 18: "IQ3_XXS",
      19: "IQ1_S", 20: "IQ4_NL", 21: "IQ3_S", 22: "IQ2_S", 23: "IQ4_XS", 30: "BF16"}

def main():
    W = sys.argv[1].rstrip("/")
    meta = json.load(open(f"{W}/result.json"))
    rid, src, want = meta["rid"], meta["parent"], "Q4_K_M"
    pub = f"{W}/pub.gguf"
    H_pub = read_header_file(pub)
    # every tensor's type, as the published file has it
    lines = []
    for name, t in H_pub["tensors"].items():
        tn = TN.get(t["ttype"])
        if tn is None: print("  unknown ggml type", t["ttype"], name); return
        lines.append(f"{name}={tn}")
    tf = f"{W}/tensor_types.txt"
    open(tf, "w").write("\n".join(lines) + "\n")
    from collections import Counter
    print(f"{rid}: {len(lines)} tensors pinned -> {dict(Counter(l.split('=')[1] for l in lines))}")
    # parent back
    sf = tree(src)
    need = [f for f in sf if not f.lower().endswith((".gguf", ".md", ".gitattributes", ".bin", ".pt", ".pth", ".msgpack", ".h5")) and not f.startswith(".")]
    os.makedirs(f"{W}/src", exist_ok=True)
    for f in need:
        os.makedirs(os.path.dirname(f"{W}/src/{f}") or f"{W}/src", exist_ok=True)
        if not dl(src, f, f"{W}/src/{f}"): print("  fetch failed", f); return
    print("  parent fetched")
    conv = f"{W}/conv_bf16.gguf"
    r = subprocess.run([PY, CV, f"{W}/src", "--outtype", "bf16", "--outfile", conv], capture_output=True, text=True, timeout=3600)
    if r.returncode != 0: print("  convert failed:", r.stderr[-400:]); return
    out = f"{W}/q_pinned.gguf"
    args = [Q]
    if meta.get("imatrix") and os.path.exists(f"{W}/imatrix.dat"): args += ["--imatrix", f"{W}/imatrix.dat"]
    args += ["--tensor-type-file", tf, conv, out, want]
    r = subprocess.run(args, capture_output=True, text=True, timeout=3600)
    if r.returncode != 0: print("  quantize failed:", r.stderr[-600:]); return
    c = compare(out, pub)
    sa, sb = os.path.getsize(out), os.path.getsize(pub)
    print(f"  sizes: ours {sa:,}  published {sb:,}  {'SAME' if sa == sb else 'DIFFER'}")
    print(f"  tensors compared {c['tensors']}, byte-identical {c['ident']}, bytes same {c['bytes_same']:.4f}%, "
          f"superblocks same {c['blocks_same']:.3f}% of {c['blocks']:,}")
    exact = c["ident"] == c["tensors"] == len(H_pub["tensors"]) and c["bytes_same"] >= 99.9999
    print(f"\n  {'RECIPE EXACT with pinned types + imatrix' if exact else 'NOT exact -- see recipe5.py for the residual'}")
    meta["pinned"] = c; meta["pinned_exact"] = bool(exact)
    json.dump(meta, open(f"{W}/result.json", "w"), indent=1)
    for f in (conv,):
        try: os.remove(f)
        except OSError: pass
    print("RECIPE6_DONE")

if __name__ == "__main__":
    main()
