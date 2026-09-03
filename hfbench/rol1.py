#!/usr/bin/env python3
"""rol-1: rotate every element left by one bit before the byte-plane split, so sign + all
exponent bits share the high byte (BF16/F16: sign+7exp -> 8exp | sign+7mant... ; F32: sign+7exp
-> 8exp). Invertible (ror-1). Measures the REAL coder (bench_pe) on the rotated stream vs the
plain stream, per file, plus a sign-entropy guard the scout found necessary (bert-base lost)."""
import numpy as np, subprocess, os, sys
B = "/root/mzip-hfbench"; PE = f"{B}/final/plane-entropy/bench_pe"; T = f"{B}/pairs/rol.tmp"
FILES = [("bert-base F32", "weights/bert-base.slice", 4), ("bge-small F32", "weights/bge-small.slice", 4), ("gpt2 F32", "weights/gpt2.slice", 4),
         ("MiniLM F32", "weights/MiniLM-L6-v2.slice", 4), ("vit-base F32", "weights2/vit-base.bin", 4), ("clip F32", "weights2/clip-vitL14.bin", 4),
         ("sd15 F32", "weights2/sd15-unet.bin", 4), ("weights-bf16", "weights/weights-bf16.bin", 2), ("Qwen0.5B BF16", "weights/Qwen2.5-0.5B.slice", 2),
         ("qwen1.5b BF16", "weights2/qwen2.5-1.5b.bin", 2), ("tinyllama BF16", "weights2/tinyllama-1.1b.bin", 2), ("smolvlm BF16", "weights2/smolvlm.bin", 2),
         ("bloom F16", "weights2/bloom-560m.bin", 2), ("llama2-7b F16", "weights2/llama2-7b.bin", 2), ("whisper F16", "weights2/whisper-lv3.bin", 2), ("sdxl F16", "weights2/sdxl-unet.bin", 2)]
def ratio(path):
    r = subprocess.run(["taskset", "-c", "4", PE, path, "65536", "1"], capture_output=True, text=True).stdout
    for l in r.splitlines():
        if "  bytes" in l: f = l.split(); return int(f[1]), l.strip().endswith("OK")
    return None
def H(bits):
    p = bits.mean(); return 0.0 if p in (0, 1) else float(-(p*np.log2(p) + (1-p)*np.log2(1-p)))
tot_plain = tot_rol = tot_guard = 0
print("%-16s %10s %10s %8s | sign H  guarded %10s %8s" % ("file", "plain", "rol-1", "gain", "size", "gain"))
for name, rel, k in FILES:
    a = np.frombuffer(open(f"{B}/{rel}", "rb").read(), dtype=np.uint8); n = (len(a) // k) * k
    w = np.frombuffer(a[:n].tobytes(), dtype={2: np.uint16, 4: np.uint32}[k]); bits = 8 * k
    rot = ((w << 1) | (w >> (bits - 1))).astype(w.dtype)
    assert np.array_equal(((rot >> 1) | (rot << (bits - 1))).astype(w.dtype), w), "rol/ror not invertible"
    open(T, "wb").write(rot.tobytes() + a[n:].tobytes())
    p = ratio(f"{B}/{rel}"); r = ratio(T)
    sign = (w >> (bits - 1)).astype(np.uint8); hs = H(sign)
    # guard: rotate only if the sign bit is near-random (>= 0.98 bit), else keep plain
    g = r if hs >= 0.98 else p
    tot_plain += p[0]; tot_rol += r[0]; tot_guard += g[0]
    print("%-16s %10d %10d %+7.2f%% | %.3f   %10d %+7.2f%%  rt %s/%s" % (name, p[0], r[0], 100*(1-r[0]/p[0]), hs, g[0], 100*(1-g[0]/p[0]), p[1], r[1]))
print("TOTAL (16 files, 128 MiB): plain %d  rol-1 %d (%+.2f%%)  guarded %d (%+.2f%%)" % (tot_plain, tot_rol, 100*(1-tot_rol/tot_plain), tot_guard, 100*(1-tot_guard/tot_plain)))
os.remove(T); print("ROL1_DONE")
