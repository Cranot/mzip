#!/usr/bin/env python3
"""GGUF repos with NO base_model tag: does the file itself name its parent?

19.5% of the week's GGUF bytes sit in repos that declare no parent (RESULT 91's multi-file and
single-file buckets), and the recipe route was priced as unavailable for them. But a GGUF header carries
general.source.url / general.base_model.N.repo_url when the converter was given them, and mradermacher's
pipeline writes them whether or not the repo is tagged. One header read per repo says how many of the
untagged are recipes after all."""
import json, struct, sys, collections
sys.path.insert(0, "/root/mzip-hfbench")
from kquant_given import api, fetch
B = "/root/mzip-hfbench"; H = "https://huggingface.co"

def kv_strings(rid, fn, want=("general.source.url", "general.base_model.0.repo_url", "general.source.repo_url",
                             "general.base_model.0.name", "general.base_model.0.organization", "general.quantized_by")):
    b = fetch(rid, fn, 0, 24 << 20)
    if b is None: return None
    b = bytes(b)
    if b[:4] != b"GGUF": return None
    p = 8; nt, nkv = struct.unpack("<QQ", b[p:p+16]); p += 16
    out = {}
    def rstr():
        nonlocal p
        ln, = struct.unpack("<Q", b[p:p+8]); p += 8; s = b[p:p+ln]; p += ln; return s.decode("utf-8", "replace")
    SZ = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
    def skip(t):
        nonlocal p
        if t == 8: return rstr()
        if t == 9:
            et, = struct.unpack("<I", b[p:p+4]); p += 4; n, = struct.unpack("<Q", b[p:p+8]); p += 8
            for _ in range(n): skip(et)
            return None
        p += SZ[t]; return None
    try:
        for _ in range(nkv):
            k = rstr(); t, = struct.unpack("<I", b[p:p+4]); p += 4
            v = skip(t)
            if k in want and isinstance(v, str): out[k] = v
    except Exception:
        return out or None
    return out

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_models.jsonl") if l.strip()]
    untagged = []
    for r in rows:
        c = r.get("bytes_in_window") or {}
        g = sum(float(v or 0) for k, v in c.items() if k.startswith("gguf"))
        if g <= 0: continue
        t = (tags.get(r["id"]) or {}).get("tags") or []
        if any(x.startswith("base_model:") for x in t): continue
        untagged.append((r["id"], g))
    untagged.sort(key=lambda x: -x[1])
    tot = sum(g for _, g in untagged)
    LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    print(f"untagged GGUF repos in sample: {len(untagged)} ({tot/1e12:.2f} TB); probing largest {min(LIMIT, len(untagged))}")
    named_b = probed_b = 0.0; named = probed = 0; how = collections.Counter(); ex = []
    for rid, g in untagged[:LIMIT]:
        d = api(f"{H}/api/models/{rid}")
        if not d: continue
        gg = [s["rfilename"] for s in d.get("siblings", []) if s["rfilename"].lower().endswith(".gguf") and "mmproj" not in s["rfilename"].lower()]
        if not gg: continue
        kv = kv_strings(rid, min(gg, key=len))
        probed += 1; probed_b += g
        if not kv: how["header unreadable / no KV"] += 1; continue
        src = kv.get("general.source.url") or kv.get("general.base_model.0.repo_url") or kv.get("general.source.repo_url")
        if src and "huggingface.co/" in src:
            named += 1; named_b += g; how["parent URL in header"] += 1
            if len(ex) < 6: ex.append((rid, src.split("huggingface.co/")[1]))
        elif kv.get("general.base_model.0.name"):
            how["parent NAME only (org+name, resolvable)"] += 1; named += 1; named_b += g
        else:
            how["no parent in header"] += 1
    print(f"\nprobed {probed} untagged GGUF repos, {probed_b/1e12:.2f} TB")
    print(f"  header names a parent: {named} = {100*named/max(probed,1):.1f}% by repo, {100*named_b/max(probed_b,1):.1f}% by bytes")
    for k, v in how.most_common(): print(f"    {v:4d}  {k}")
    print("  e.g.", "; ".join(f"{a[:28]} <- {b[:28]}" for a, b in ex[:4]))
    json.dump(dict(probed=probed, probed_b=probed_b, named=named, named_b=named_b, how=dict(how), untagged_tot=tot),
              open(f"{B}/qgq/ggufsrc.json", "w"), indent=1)
    print("GGUFSRC_DONE")

if __name__ == "__main__":
    main()
