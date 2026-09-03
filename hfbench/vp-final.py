D = open("/root/mzip-hfbench/.advver_dir").read().strip()
def load(p, keyi=0, vari=1, szi=2):
    d = {}
    for l in open(p):
        f = l.rstrip("\n").split("\t")
        if len(f) < 3 or f[0].endswith("_DONE"): continue
        d[(f[keyi], f[vari])] = int(f[szi])
    return d
arm = load(f"{D}/results/arm.tsv")
held = load(f"{D}/results/heldarm.tsv")

theirs = {"prose-wikitext":(1908380,1908380),"code-python":(1343486,1326632),
 "tabular-numeric":(1034164,1034164),"tweets-jsonl":(728468,728468),
 "alpaca-instruct":(1652011,1652011),"qa-gsm8k":(848507,836384),
 "qa-arc":(124272,124272),"dialogue-csv":(1439491,1439491),
 "weights-fp32":(6155434,6155434),"weights-bf16":(5571614,5571614)}

print("=== REPRODUCTION of their 10-file arm (mine vs theirs) ===")
b64 = b128 = 0; allok = True
for k,(tb,tp) in theirs.items():
    mb, mp = arm[(k,"v64")], arm[(k,"v128")]
    ok = (mb==tb and mp==tp); allok &= ok
    if not k.startswith("weights"): b64 += mb; b128 += mp
    print(f"  {k:18s} base {mb:>9,} ({'=' if mb==tb else 'DIFF'})  128 {mp:>9,} ({'=' if mp==tp else 'DIFF'})  d={mp-mb:+8,}")
print(f"  ALL TEN REPRODUCE EXACTLY: {allok}")
print(f"\n  8-corpus aggregate: {b64:,} -> {b128:,}  delta {b128-b64:+,} ({100.0*(b128-b64)/b64:+.4f}%)  [they said -28,977 / -0.3192%]")

print("\n=== code-python memory curve, END TO END (non-monotone?) ===")
prev=None
for v in ["v64","v80","v96","v112","v128"]:
    s = arm[("code-python",v)]
    print(f"  {v:5s} {s:>9,}  vs64 {s-arm[('code-python','v64')]:+8,} ({100.0*(s-arm[('code-python','v64')])/arm[('code-python','v64')]:+.4f}%)"
          + ("" if prev is None else f"   step vs prev: {s-prev:+,}"))
    prev=s
print("\n=== qa-gsm8k memory curve, END TO END ===")
for v in ["v64","v80","v96","v112","v128"]:
    s = arm[("qa-gsm8k",v)]
    print(f"  {v:5s} {s:>9,}  vs64 {s-arm[('qa-gsm8k','v64')]:+8,}")

print("\n=== HELD-OUT (disjoint rows, same distributions) ===")
for k in ["code-python-B","code-python-C","qa-gsm8k-H1","qa-gsm8k-H2","alpaca-B"]:
    a,b = held[(k,"v64")], held[(k,"v128")]
    print(f"  {k:16s} {a:>9,} -> {b:>9,}  {b-a:+8,} ({100.0*(b-a)/a:+.4f}%)")
print("  prose-wt103-B  DISCARDED: byte-identical to prose-wikitext (not held out)")
