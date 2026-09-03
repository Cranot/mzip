#!/usr/bin/env python3
"""assemble_table.py <V>: HF (bg4-lz4, hfcmp/*.hf.txt) vs mzip FAST/BALANCED (tbl/*.{fast,bal}.<V>.txt)
at 64K chunks, plus small2 whole files. Prints a markdown table + per-class aggregates."""
import sys, os, re, glob
V = sys.argv[1]; D = "/root/mzip-hfbench"
ROWS = [  # (basename, class, label)
 ("bert-base.slice","F32","bert-base F32"), ("bge-small.slice","F32","bge-small F32"), ("gpt2.slice","F32","gpt2 F32"),
 ("MiniLM-L6-v2.slice","F32","MiniLM F32"), ("weights-fp32.bin","F32","wfp32 F32"),
 ("weights-bf16.bin","BF16","wbf16 BF16"), ("Qwen2.5-0.5B.slice","BF16","Qwen BF16"),
 ("gguf-f16.slice","GGUF-F16","gguf F16"), ("gguf-q8_0.slice","GGUF-Q","gguf Q8_0"), ("gguf-q6_k.slice","GGUF-Q","gguf Q6_K"), ("gguf-q4_k_m.slice","GGUF-Q","gguf Q4_K_M"),
 ("wikitext-2-raw-train.parquet","parquet","wikitext-2 parquet"),
 ("alpaca-instruct.bin","text","alpaca text"), ("code-python.bin","text","code-python"), ("dialogue-csv.bin","text","dialogue-csv"),
 ("prose-wikitext.bin","text","prose-wikitext"), ("qa-arc.bin","text","qa-arc"), ("qa-gsm8k.bin","text","qa-gsm8k"),
 ("tabular-numeric.bin","text","tabular-numeric"), ("tweets-jsonl.bin","text","tweets-jsonl"),
]
def grab(path, key):
    try:
        for l in open(path):
            if l.startswith(key):
                p = l.split(); return dict(bytes=int(p[1]), ratio=float(p[2]), comp=float(p[3]), dec=float(p[4]), rt=p[5])
    except FileNotFoundError: pass
    return None
def insize(b):
    for sub in ("weights","corpora","assets"):
        p = f"{D}/{sub}/{b}"
        if os.path.exists(p): return os.path.getsize(p)
    return None
agg = {}
print(f"| data | in MB | HF bg4-lz4 ratio | mzip FAST ratio | mzip BAL ratio | HF comp MB/s | FAST comp | BAL comp | HF dec MB/s | FAST dec | BAL dec |")
print(f"|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
for b, cls, label in ROWS:
    hf = grab(f"{D}/hfcmp/{b}.hf.txt", "bg4-lz4"); fa = grab(f"{D}/tbl/{b}.fast.{V}.txt", "FAST"); ba = grab(f"{D}/tbl/{b}.bal.{V}.txt", "BALANCED")
    n = insize(b) or 0
    def f(x, k, fmt): return (fmt % x[k]) if x else "n/a"
    print(f"| {label} | {n/1048576:.1f} | {f(hf,'ratio','%.4f')} | {f(fa,'ratio','%.4f')} | {f(ba,'ratio','%.4f')} | {f(hf,'comp','%.0f')} | {f(fa,'comp','%.0f')} | {f(ba,'comp','%.2f')} | {f(hf,'dec','%.0f')} | {f(fa,'dec','%.0f')} | {f(ba,'dec','%.0f')} |")
    a = agg.setdefault(cls, dict(n=0, hf=0, fa=0, ba=0, rows=0, hfc=0.0, fac=0.0, bac=0.0, hfd=0.0, fad=0.0, bad=0.0))
    if hf and fa and ba:
        a["n"] += n; a["hf"] += hf["bytes"]; a["fa"] += fa["bytes"]; a["ba"] += ba["bytes"]; a["rows"] += 1
        # byte-weighted throughput: time = bytes / speed
        a["hfc"] += n / hf["comp"]; a["fac"] += n / fa["comp"]; a["bac"] += n / ba["comp"]
        a["hfd"] += n / hf["dec"]; a["fad"] += n / fa["dec"]; a["bad"] += n / ba["dec"]
# small2 whole files
s_fa = grab(f"{D}/tbl/small2.fast.{V}.txt", "FAST"); s_ba = grab(f"{D}/tbl/small2.bal.{V}.txt", "BALANCED")
s_hf = None
try:
    t = open(f"{D}/tbl/small2.hf.txt").read(); m = re.search(r"(\d+) -> (\d+)\s+ratio ([\d.]+)", t)
    if m: s_hf = dict(bytes=int(m.group(2)), ratio=float(m.group(3)), n=int(m.group(1)))
except FileNotFoundError: pass
if s_fa and s_ba:
    print(f"| HF repo JSON x290 (whole files) | {s_ba and 1452760/1048576:.1f} | {s_hf['ratio'] if s_hf else 'n/a'} | {s_fa['ratio']:.2f} | {s_ba['ratio']:.2f} | n/a | {s_fa['comp']:.2f} | {s_ba['comp']:.2f} | n/a | {s_fa['dec']:.0f} | {s_ba['dec']:.0f} |")
print("\nPer-class aggregates (bytes summed; throughput byte-weighted):")
print("| class | files | HF ratio | FAST ratio | BAL ratio | FAST vs HF size | BAL vs HF size | HF comp | FAST comp | BAL comp | HF dec | FAST dec | BAL dec |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
for cls in ["F32","BF16","GGUF-F16","GGUF-Q","parquet","text"]:
    a = agg.get(cls)
    if not a or a["rows"] == 0: continue
    print(f"| {cls} | {a['rows']} | {a['n']/a['hf']:.4f} | {a['n']/a['fa']:.4f} | {a['n']/a['ba']:.4f} | {100*(a['hf']-a['fa'])/a['hf']:+.1f}% | {100*(a['hf']-a['ba'])/a['hf']:+.1f}% | {a['n']/a['hfc']:.0f} | {a['n']/a['fac']:.0f} | {a['n']/a['bac']:.2f} | {a['n']/a['hfd']:.0f} | {a['n']/a['fad']:.0f} | {a['n']/a['bad']:.0f} |")
