#!/usr/bin/env python3
"""The recipe reproduced the file's size and every tensor, but 1.436% of bytes differ. Which bytes?

A Q4_K superblock is 144 bytes: d (fp16, 0-1), dmin (fp16, 2-3), twelve bytes of packed 6-bit scales
and mins (4-15), then 128 bytes of nibble codes (16-143). If the differing bytes sit in 0-15 and the
codes follow them, the publisher's llama.cpp searched scales slightly differently -- version drift in
make_qkx2_quants -- and the codes are downstream of that. If differences are uniform over 16-143 with
the scales identical, something else is going on.

Also answers the deployment question directly: with identical superblocks free and differing ones
stored, what does 'recipe + patch' cost against the published file? Compared with the arithmetic route
(RESULT 89: Q4_K given base, 13.2%)."""
import json, sys, struct, collections
import numpy as np
W = "/root/mzip-hfbench/recipe"
sys.path.insert(0, "/root/mzip-hfbench")
from recipe import read_header_file
from kquant_given import h0

def kv_version(path):
    """general.quantization_version and any string KV mentioning llama.cpp / version"""
    with open(path, "rb") as f: b = f.read(4 << 20)
    p = 8; nt, nkv = struct.unpack("<QQ", b[p:p+16]); p += 16
    out = {}
    def rstr():
        nonlocal p
        ln, = struct.unpack("<Q", b[p:p+8]); p += 8; s = b[p:p+ln]; p += ln; return s.decode("utf-8", "replace")
    SZ1 = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    def rd(t):
        nonlocal p
        if t == 8: return rstr()
        if t == 9:
            et, = struct.unpack("<I", b[p:p+4]); p += 4; n, = struct.unpack("<Q", b[p:p+8]); p += 8
            vals = [rd(et) for _ in range(min(n, 4))]
            for _ in range(max(0, n - 4)): rd(et)
            return vals
        fmt = {0: "<B", 1: "<b", 2: "<H", 3: "<h", 4: "<I", 5: "<i", 6: "<f", 7: "<?", 10: "<Q", 11: "<q", 12: "<d"}[t]
        v, = struct.unpack(fmt, b[p:p+SZ1[t]]); p += SZ1[t]; return v
    for _ in range(nkv):
        k = rstr(); t, = struct.unpack("<I", b[p:p+4]); p += 4; v = rd(t)
        if any(s in k for s in ("quantization_version", "file_type", "general.name", "general.version",
                                 "general.source", "general.quantized_by", "general.url")):
            out[k] = v if not isinstance(v, str) else v[:80]
    return out

def main():
    a, b = f"{W}/pub_q4.gguf", f"{W}/our_q4.gguf"
    print("published KV:", json.dumps(kv_version(a), indent=None)[:600])
    print("ours KV:     ", json.dumps(kv_version(b), indent=None)[:600])
    HA, HB = read_header_file(a), read_header_file(b)
    BS = {12: 144, 14: 210, 13: 176}
    off_hist = collections.Counter(); blk_tot = blk_same = 0; bytes_tot = 0
    patch_whole = 0; patch_xor_bits = 0.0
    scale_only = codes_only = both = 0
    per_type = collections.defaultdict(lambda: [0, 0])
    with open(a, "rb") as fa, open(b, "rb") as fb:
        for name, ta in HA["tensors"].items():
            tb = HB["tensors"][name]
            tt = ta["ttype"]
            if tt not in BS: continue
            n = ta["nbytes"]; bs = BS[tt]
            fa.seek(HA["data_start"] + ta["offset"]); fb.seek(HB["data_start"] + tb["offset"])
            xa = np.frombuffer(fa.read(n), dtype=np.uint8).reshape(-1, bs)
            xb = np.frombuffer(fb.read(n), dtype=np.uint8).reshape(-1, bs)
            diff = xa != xb
            blk_diff = diff.any(axis=1)
            blk_tot += len(xa); blk_same += int((~blk_diff).sum()); bytes_tot += n
            per_type[tt][0] += len(xa); per_type[tt][1] += int(blk_diff.sum())
            if blk_diff.any():
                d = diff[blk_diff]
                for off in np.nonzero(d.any(axis=0))[0]:
                    off_hist[int(off)] += int(d[:, off].sum())
                hdr = 16 if tt == 12 else (16 if tt == 13 else 192)     # Q6_K: scales at 192-207, d at 208
                if tt == 14:
                    sc = d[:, 192:210].any(axis=1); cd = d[:, :192].any(axis=1)
                else:
                    sc = d[:, :16].any(axis=1); cd = d[:, 16:].any(axis=1)
                scale_only += int((sc & ~cd).sum()); codes_only += int((cd & ~sc).sum()); both += int((sc & cd).sum())
                patch_whole += int(blk_diff.sum()) * bs
                xr = (xa[blk_diff] ^ xb[blk_diff]).ravel()
                patch_xor_bits += h0(xr) * xr.size
    print(f"\nsuperblocks: {blk_tot:,}   identical: {blk_same:,} = {100*blk_same/blk_tot:.2f}%   differing: {blk_tot-blk_same:,}")
    for tt, (n, d) in per_type.items():
        print(f"  ttype {tt}: {n:,} blocks, {100*d/max(n,1):.2f}% differ")
    print(f"\nwithin differing blocks:  scale-region only {scale_only:,}   codes only {codes_only:,}   both {both:,}")
    top = off_hist.most_common(12)
    print("byte offsets that differ most (Q4_K: 0-3 d/dmin, 4-15 packed scales, 16-143 codes):")
    print("  " + "  ".join(f"{o}:{c}" for o, c in top))
    print(f"\nrecipe + patch cost against the published file ({bytes_tot:,} B of quantised tensors):")
    print(f"  store differing superblocks whole:   {100*patch_whole/bytes_tot:.2f}%")
    print(f"  store XOR of differing blocks, h0:   {100*patch_xor_bits/8/bytes_tot:.2f}%")
    print(f"  arithmetic route, Q4_K given base (RESULT 89):  13.2%   -- for reference")
    print("RECIPE2_DONE")

if __name__ == "__main__":
    main()
