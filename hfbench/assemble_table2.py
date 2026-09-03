#!/usr/bin/env python3
"""assemble_table2.py <V>: real Xet Auto (hf-fairness scout's Rust harness, lz4_flex 0.13.0 + xet-core
BG4Predictor, quiet-box run) vs my always-bg4-lz4 C baseline vs mzip FAST/BALANCED (tbl/*.<V>.txt)."""
import sys, os, re
V = sys.argv[1]; D = "/root/mzip-hfbench"; RQ = f"{D}/agents/hf-fairness/results/rust_quiet"
ROWS = [
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
                p = l.split(); return dict(bytes=int(p[1]), ratio=float(p[2]), comp=float(p[3]), dec=float(p[4]))
    except FileNotFoundError: pass
    return None
def grab_auto(b):
    """rust_quiet/<b>.txt: 'auto  bytes1B bytes8B ratio comp dec rt none lz4 bg4'"""
    try:
        for l in open(f"{RQ}/{b}.txt"):
            if l.startswith("auto "):
                p = l.split(); return dict(bytes=int(p[1]), ratio=float(p[3]), comp=float(p[4]), dec=float(p[5]), none=int(p[7]), lz4=int(p[8]), bg4=int(p[9]))
    except FileNotFoundError: pass
    return None
def insize(b):
    for sub in ("weights","corpora","assets"):
        p = f"{D}/{sub}/{b}"
        if os.path.exists(p): return os.path.getsize(p)
    return 0
def f(x, k, fmt): return (fmt % x[k]) if x else "n/a"
agg = {}
print("| data | Xet Auto ratio (real) | Auto picks | my bg4-lz4 C | mzip FAST | mzip BAL | Xet comp MB/s | FAST comp | Xet dec MB/s | FAST dec | BAL dec |")
print("|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|")
for b, cls, label in ROWS:
    au = grab_auto(b); hf = grab(f"{D}/hfcmp/{b}.hf.txt", "bg4-lz4"); fa = grab(f"{D}/tbl/{b}.fast.{V}.txt", "FAST"); ba = grab(f"{D}/tbl/{b}.bal.{V}.txt", "BALANCED")
    picks = f"none {au['none']}/lz4 {au['lz4']}/bg4 {au['bg4']}" if au else "n/a"
    print(f"| {label} | {f(au,'ratio','%.4f')} | {picks} | {f(hf,'ratio','%.4f')} | {f(fa,'ratio','%.4f')} | {f(ba,'ratio','%.4f')} | {f(au,'comp','%.0f')} | {f(fa,'comp','%.0f')} | {f(au,'dec','%.0f')} | {f(fa,'dec','%.0f')} | {f(ba,'dec','%.0f')} |")
    n = insize(b); a = agg.setdefault(cls, dict(n=0, au=0, hf=0, fa=0, ba=0, rows=0, auc=0.0, fac=0.0, aud=0.0, fad=0.0, bad=0.0))
    if au and hf and fa and ba:
        a["n"] += n; a["au"] += au["bytes"]; a["hf"] += hf["bytes"]; a["fa"] += fa["bytes"]; a["ba"] += ba["bytes"]; a["rows"] += 1
        a["auc"] += n / au["comp"]; a["fac"] += n / fa["comp"]; a["aud"] += n / au["dec"]; a["fad"] += n / fa["dec"]; a["bad"] += n / ba["dec"]
# small2
s_au = grab_auto("small2"); s_fa = grab(f"{D}/tbl/small2.fast.{V}.txt", "FAST"); s_ba = grab(f"{D}/tbl/small2.bal.{V}.txt", "BALANCED")
if s_au and s_fa and s_ba:
    print(f"| HF repo JSON x290 (whole files) | {s_au['ratio']:.4f} | none {s_au['none']}/lz4 {s_au['lz4']}/bg4 {s_au['bg4']} | 2.9843 | {s_fa['ratio']:.2f} | {s_ba['ratio']:.2f} | {s_au['comp']:.0f} | {s_fa['comp']:.2f} | {s_au['dec']:.0f} | {s_fa['dec']:.0f} | {s_ba['dec']:.0f} |")
print("\nPer-class vs REAL Xet Auto (bytes summed; throughput byte-weighted MB/s):")
print("| class | files | Xet Auto | mzip FAST | mzip BAL | FAST bytes vs Xet | BAL bytes vs Xet | Xet comp | FAST comp | Xet dec | FAST dec | BAL dec |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
tot = dict(n=0, au=0, fa=0, ba=0)
for cls in ["F32","BF16","GGUF-F16","GGUF-Q","parquet","text"]:
    a = agg.get(cls)
    if not a or a["rows"] == 0: continue
    for k in ("n","au","fa","ba"): tot[k] += a[k]
    print(f"| {cls} | {a['rows']} | {a['n']/a['au']:.4f} | {a['n']/a['fa']:.4f} | {a['n']/a['ba']:.4f} | {100*(a['au']-a['fa'])/a['au']:+.1f}% | {100*(a['au']-a['ba'])/a['au']:+.1f}% | {a['n']/a['auc']:.0f} | {a['n']/a['fac']:.0f} | {a['n']/a['aud']:.0f} | {a['n']/a['fad']:.0f} | {a['n']/a['bad']:.0f} |")
print(f"\nNote: 'my bg4-lz4 C' = the always-bg4 liblz4 block-API baseline used in RESULT 28/34; on text it understated Xet (Auto picks plain LZ4 there).")
