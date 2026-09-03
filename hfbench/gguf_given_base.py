#!/usr/bin/env python3
"""Quantized-given-base: can a Q8_0 GGUF tensor be regenerated from the BF16 safetensors base?
Parses the GGUF header of the official Q8_0 file, locates the SAME tensor as the BF16 slice we
hold (pairs/qwen25-1.5b.ft.bin = Qwen2.5-1.5B-Instruct model.layers.9.mlp.up_proj.weight from
element 4096), range-fetches the matching Q8_0 blocks, re-runs ggml's quantize_row_q8_0_reference
in exact float32 arithmetic on the BF16 values, and compares: fraction of blocks whose fp16 scale
matches, fraction of int8 quants that match, and the entropy of the residual (actual - predicted)
-- i.e. what a 'given the base' scheme would still have to store."""
import struct, subprocess, os, json, sys, numpy as np
H = "https://huggingface.co"; P = "/root/mzip-hfbench/pairs"
BF16_SLICE = f"{P}/qwen25-1.5b.ft.bin"   # Qwen2.5-1.5B-Instruct, model.layers.9.mlp.up_proj.weight, from element 4096
GGUFS = [("official", "Qwen/Qwen2.5-1.5B-Instruct-GGUF", "qwen2.5-1.5b-instruct-q8_0.gguf"),
         ("bartowski", "bartowski/Qwen2.5-1.5B-Instruct-GGUF", "Qwen2.5-1.5B-Instruct-Q8_0.gguf")]
TENSOR = "blk.9.ffn_up.weight"; ELEM0 = 4096; NELEM = 8 * 1024 * 1024 // 2   # 4 Mi elements
def curl(url, out, a, b):
    for t in range(4):
        subprocess.run(["curl", "-sSL", "--max-time", "300", "-r", f"{a}-{b}", "-o", out, url], capture_output=True)
        if os.path.exists(out) and os.path.getsize(out) == b - a + 1: return True
        import time; time.sleep(4 * (t + 1))
    return False
def gguf_header(url, out):
    if not curl(url, out, 0, 24 * 1024 * 1024 - 1): return None   # Qwen GGUF headers embed a 150k-entry vocab: ~10 MB
    b = open(out, "rb").read(); p = 0
    def u32(): nonlocal p; v = struct.unpack_from("<I", b, p)[0]; p += 4; return v
    def u64(): nonlocal p; v = struct.unpack_from("<Q", b, p)[0]; p += 8; return v
    def s(): n = u64(); nonlocal p; v = b[p:p+n].decode("utf-8", "replace"); p += n; return v
    assert b[:4] == b"GGUF", "not a GGUF file"; p = 4
    ver = u32(); nt = u64(); nkv = u64()
    SZ = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    def val(t):
        nonlocal p
        if t == 8: return s()
        if t == 9:
            et = u32(); n = u64(); return [val(et) for _ in range(n)]
        v = b[p:p+SZ[t]]; p += SZ[t]; return v
    kv = {}
    for _ in range(nkv):
        k = s(); t = u32(); v = val(t); kv[k] = v
    align = struct.unpack("<I", kv.get("general.alignment", struct.pack("<I", 32)))[0] if isinstance(kv.get("general.alignment"), bytes) else 32
    tensors = {}
    for _ in range(nt):
        name = s(); nd = u32(); dims = [u64() for _ in range(nd)]; typ = u32(); off = u64()
        tensors[name] = (dims, typ, off)
    data_start = (p + align - 1) // align * align
    return ver, kv, tensors, data_start
def bf16_to_f32(u16): return (u16.astype(np.uint32) << 16).view(np.float32)
def f16_bits(x32):  # IEEE fp16 bits of float32 values, round-to-nearest-even (what ggml_fp32_to_fp16 does on modern builds)
    return x32.astype(np.float16).view(np.uint16)
def q8_0_reference(x):
    """ggml quantize_row_q8_0_reference: per 32-block, d=amax/127 (f32), id=1/d (f32), q=roundf(x*id)"""
    blk = x.reshape(-1, 32).astype(np.float32)
    amax = np.abs(blk).max(axis=1).astype(np.float32)
    d = (amax / np.float32(127)).astype(np.float32)
    idv = np.where(d != 0, (np.float32(1) / d).astype(np.float32), np.float32(0)).astype(np.float32)
    x0 = (blk * idv[:, None]).astype(np.float32)
    q = np.where(x0 >= 0, np.floor(x0 + np.float32(0.5)), np.ceil(x0 - np.float32(0.5)))   # roundf: half away from zero
    return f16_bits(d), q.astype(np.int8)
bf = np.frombuffer(open(BF16_SLICE, "rb").read(), dtype=np.uint16)[:NELEM]
x = bf16_to_f32(bf)
pd, pq = q8_0_reference(x)
for tag, repo, fn in GGUFS:
    url = f"{H}/{repo}/resolve/main/{fn}"; hdr = f"{P}/gguf-{tag}.hdr"
    r = gguf_header(url, hdr)
    if not r: print(f"{tag}: header fetch failed"); continue
    ver, kv, tensors, data_start = r
    if TENSOR not in tensors: print(f"{tag}: {TENSOR} not in file; sample names: {list(tensors)[:5]}"); continue
    dims, typ, off = tensors[TENSOR]
    print(f"=== {tag}: {repo}/{fn}  GGUF v{ver}, {len(tensors)} tensors, data@{data_start}; {TENSOR} dims={dims} type={typ} (8=Q8_0) ===")
    if typ != 8: print("   not Q8_0, skipping"); continue
    blk0 = ELEM0 // 32; nblk = NELEM // 32
    a = data_start + off + blk0 * 34; b = a + nblk * 34 - 1
    out = f"{P}/gguf-{tag}.q8.bin"
    if not curl(url, out, a, b): print("   range fetch failed"); continue
    raw = open(out, "rb").read(); assert len(raw) == nblk * 34
    blocks = np.frombuffer(raw, dtype=np.uint8).reshape(nblk, 34)
    ad = blocks[:, :2].copy().view(np.uint16).reshape(-1); aq = blocks[:, 2:].view(np.int8)
    dm = (ad == pd); qm = (aq == pq)
    print(f"   scale d (fp16 bits) matches: {100*dm.mean():.3f}%   quants match: {100*qm.mean():.4f}%   blocks fully exact: {100*(qm.all(axis=1)&dm).mean():.3f}%")
    if dm.mean() < 0.999:
        dd = ad.astype(np.int32) - pd.astype(np.int32); print(f"   scale mismatch histogram (actual-pred, fp16 ulps): {dict(zip(*np.unique(dd[dd!=0], return_counts=True)))}" if (dd != 0).any() else "")
    res = (aq.astype(np.int16) - pq.astype(np.int16)).reshape(-1)
    vals, cnt = np.unique(res, return_counts=True); p_ = cnt / cnt.sum(); Hres = float(-(p_ * np.log2(p_)).sum())
    print(f"   residual (actual - predicted quant): {dict(zip(vals.tolist(), cnt.tolist()))}   entropy {Hres:.4f} bits/quant")
    # what a given-base scheme stores: the scale bytes (or their residual) + the quant residual
    bytes_today = nblk * 34; bytes_given = nblk * 2 + Hres * nblk * 32 / 8
    print(f"   GIVEN THE BASE: {bytes_today} B today -> ~{bytes_given:.0f} B (scales verbatim + coded residual) = {bytes_today/bytes_given:.1f}x; if scales also reproduce exactly: ~{max(1, Hres*nblk*32/8):.0f} B")
    json.dump(dict(tag=tag, repo=repo, file=fn, tensor=TENSOR, dims=dims, scale_match=float(dm.mean()), quant_match=float(qm.mean()), residual_entropy=Hres), open(f"{P}/gguf-{tag}.result.json", "w"), indent=1)
print("GGUF_GIVEN_BASE_DONE")
