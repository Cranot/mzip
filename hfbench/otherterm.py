"""Where does finalnum's OTHER_SAVE = 270 - 104.3 = 165.7 TB come from? Rebuild the non-GGUF per-chunk
saving class by class from before_after.M and final_savings.SAVE/LEVER, and show the gap."""
import json, sys
sys.path.insert(0, "/root/mzip-hfbench")
import before_after as BA
U = json.load(open("/root/mzip-hfbench/upload-mix/UPLOAD-MIX.json")); C = {k: v / 1e12 for k, v in U["classes"].items()}
SAVE = {"safetensors:BF16": 0.227, "safetensors:F32": 0.114, "safetensors:F16": 0.120, "safetensors:F8_E4M3": 0.176, "safetensors:I8": 0.102, "safetensors:U8": 0.021,
        "safetensors:I32": 0.088, "safetensors:U32": 0.033, "gguf:F16": 0.333, "gguf:BF16": 0.333, "gguf:F32": 0.333, "gguf:Q": 0.062, "torch-pickle": 0.133, "parquet": 0.116, "text": 0.310}
LEVER = {"gguf:Q": 0.494, "gguf:F16": 1.0, "parquet": 0.425}
FT_SHARE = BA.FT_SHARE
print(f"{'class':22s} {'TB':>7s} {'M per-chunk':>12s} {'SAVE per-chunk':>15s} {'lever':>7s}")
tm = ts = tl = 0.0
for k, b in sorted(C.items(), key=lambda x: -x[1]):
    xr, orr, reb, _ = BA.M.get(k, (1, 1, 0, ""))
    bb = b * (1 - FT_SHARE) if k == "safetensors:BF16" else b
    m = bb * (1 / xr - 1 / orr); s = (b / xr) * SAVE.get(k, 0); l = b * LEVER.get(k, 0)
    if k.startswith("gguf"): tag = "(gguf: handled by the parent model in finalnum)"
    else: tag = ""; tm += m; ts += s; tl += l
    if m or s or l: print(f"{k:22s} {b:7.1f} {m:12.1f} {s:15.1f} {l:7.1f}  {tag}")
print(f"\nnon-GGUF totals: M per-chunk {tm:.1f} TB; SAVE per-chunk {ts:.1f} TB; non-GGUF levers (parquet) {tl:.1f} TB")
print(f"finalnum literal OTHER_SAVE = 165.7 TB; gap vs M+parquet lever = {165.7 - tm - tl:.1f} TB; vs SAVE+lever = {165.7 - ts - tl:.1f} TB")
ftl = C["safetensors:BF16"] * FT_SHARE * 0.611
print(f"BF16 fine-tune lever in final_savings = {ftl:.1f} TB (finalnum adds its own ft term = 14.8 TB); GGUF-Q lever 0.494 x {C['gguf:Q']:.1f} = {0.494*C['gguf:Q']:.1f} TB; gguf per-chunk {sum((C[k]/BA.M[k][0])*SAVE[k] for k in ('gguf:Q','gguf:F16','gguf:BF16','gguf:F32')):.1f} TB")
print(f"UPLOAD-MIX.json: per_chunk {U['per_chunk']/1e12:.1f} TB, levers {U['levers']/1e12:.1f} TB, sum {(U['per_chunk']+U['levers'])/1e12:.1f} TB")
