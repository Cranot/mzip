import collections, os
D = open("/root/mzip-hfbench/.advver_dir").read().strip()
rows = [l.rstrip("\n").split("\t") for l in open(f"{D}/results/memprobe.tsv") if l.strip()]
m = collections.defaultdict(dict)
for f, o, mem, sz, rt in rows:
    assert rt == "OK", (f, o, mem, rt)
    m[f].setdefault(int(mem), {})[int(o)] = int(sz)
mems = [16,32,48,56,64,72,80,96,112,128]
for f in sorted(m):
    done = [x for x in mems if len(m[f].get(x, {})) == 3]
    if not done: continue
    print(f"\n=== {f}  (min over orders 16/32/48, as mzip keep-smallest does) ===")
    base = min(m[f][64].values()) if 64 in m[f] and len(m[f][64]) == 3 else None
    for mem in done:
        mn = min(m[f][mem].values()); ordm = min(m[f][mem], key=lambda k: m[f][mem][k])
        d = "" if base is None else f"  vs64: {mn-base:+8d} ({100.0*(mn-base)/base:+.4f}%)"
        print(f"  mem={mem:<4} {mn:>10,}  ord={ordm:<3}{d}")
    if base is not None and 128 in m[f] and len(m[f][128])==3:
        best = min((min(v.values()), k) for k, v in m[f].items() if len(v)==3)
        print(f"  --> best mem over sweep = {best[1]} ({best[0]:,}); 128 gives {min(m[f][128].values()):,}")
