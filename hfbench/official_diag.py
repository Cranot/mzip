#!/usr/bin/env python3
"""Why does Qwen's OFFICIAL Q8_0 not regenerate from the current BF16 safetensors? Fetch the same
tensor region from the official FP16 GGUF (the quantizer's input) and compare it with bf16->f16
of the safetensors slice. If they differ, the official GGUFs were made from different weights."""
import struct, subprocess, os, time, numpy as np
H = "https://huggingface.co"; P = "/root/mzip-hfbench/pairs"
TENSOR = "blk.9.ffn_up.weight"; ELEM0 = 4096; NELEM = 4 * 1024 * 1024
def curl(url, out, a, b):
    for t in range(4):
        subprocess.run(["curl", "-sSL", "--max-time", "300", "-r", f"{a}-{b}", "-o", out, url], capture_output=True)
        if os.path.exists(out) and os.path.getsize(out) == b - a + 1: return True
        time.sleep(4 * (t + 1))
    return False
def gguf_header(url, out):
    if not curl(url, out, 0, 24 * 1024 * 1024 - 1): return None
    b = open(out, "rb").read(); p = 4
    def u32(): nonlocal p; v = struct.unpack_from("<I", b, p)[0]; p += 4; return v
    def u64(): nonlocal p; v = struct.unpack_from("<Q", b, p)[0]; p += 8; return v
    def s(): nonlocal p; n = u64(); v = b[p:p+n]; p += n; return v.decode("utf-8", "replace")
    SZ = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    def val(t):
        nonlocal p
        if t == 8: return s()
        if t == 9: et = u32(); n = u64(); return [val(et) for _ in range(n)]
        v = b[p:p+SZ[t]]; p += SZ[t]; return v
    ver = u32(); nt = u64(); nkv = u64(); kv = {}
    for _ in range(nkv): k = s(); t = u32(); kv[k] = val(t)
    align = 32
    tensors = {}
    for _ in range(nt):
        name = s(); nd = u32(); dims = [u64() for _ in range(nd)]; typ = u32(); off = u64(); tensors[name] = (dims, typ, off)
    return ver, kv, tensors, (p + align - 1) // align * align
bf = np.frombuffer(open(f"{P}/qwen25-1.5b.ft.bin", "rb").read(), dtype=np.uint16)[:NELEM]
ours_f16 = (bf.astype(np.uint32) << 16).view(np.float32).astype(np.float16).view(np.uint16)
for tag, repo, fn, typ_want, esz in [("official-fp16", "Qwen/Qwen2.5-1.5B-Instruct-GGUF", "qwen2.5-1.5b-instruct-fp16.gguf", 1, 2)]:
    url = f"{H}/{repo}/resolve/main/{fn}"; r = gguf_header(url, f"{P}/{tag}.hdr")
    if not r: print(f"{tag}: header fetch failed"); continue
    ver, kv, tensors, ds = r
    meta = {k: v for k, v in kv.items() if k.startswith("general.") and isinstance(v, str)}
    print(f"=== {tag}: GGUF v{ver}, {len(tensors)} tensors; general.*: { {k: v[:60] for k, v in meta.items()} }")
    if TENSOR not in tensors: print("  tensor missing"); continue
    dims, typ, off = tensors[TENSOR]; print(f"  {TENSOR} dims={dims} type={typ} (1=F16, 30=BF16)")
    a = ds + off + ELEM0 * esz; out = f"{P}/{tag}.f16.bin"
    if not curl(url, out, a, a + NELEM * esz - 1): print("  range fetch failed"); continue
    theirs = np.frombuffer(open(out, "rb").read(), dtype=np.uint16)
    eq = (theirs == ours_f16)
    print(f"  elements equal to bf16->f16 of the current safetensors: {100*eq.mean():.3f}%")
    if eq.mean() < 0.999:
        d = theirs.view(np.float16).astype(np.float32) - ours_f16.view(np.float16).astype(np.float32)
        print(f"  |diff| mean {np.abs(d).mean():.3e}  max {np.abs(d).max():.3e}   ours |x| mean {np.abs(ours_f16.view(np.float16).astype(np.float32)).mean():.3e}")
        # is it a permutation / shift? correlation of the two sequences
        o = ours_f16.view(np.float16).astype(np.float64); t = theirs.view(np.float16).astype(np.float64)
        print(f"  correlation(theirs, ours) = {np.corrcoef(o, t)[0,1]:.4f}   (1.0 = same weights up to rounding; ~0 = different weights or different layout)")
print("--- current HF revision of the safetensors repo:")
subprocess.run(["bash", "-c", "curl -sS --max-time 60 https://huggingface.co/api/models/Qwen/Qwen2.5-1.5B-Instruct | python3 -c 'import json,sys; d=json.load(sys.stdin); print(\"  sha\", d.get(\"sha\"), \" lastModified\", d.get(\"lastModified\"))'"])
subprocess.run(["bash", "-c", "curl -sS --max-time 60 https://huggingface.co/api/models/Qwen/Qwen2.5-1.5B-Instruct-GGUF | python3 -c 'import json,sys; d=json.load(sys.stdin); print(\"  GGUF repo sha\", d.get(\"sha\"), \" lastModified\", d.get(\"lastModified\"))'"])
print("OFFICIAL_DIAG_DONE")
