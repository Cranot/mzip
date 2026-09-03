#!/usr/bin/env python3
"""What fraction of the Hub's fine-tunes are LOW RANK?

RESULT 65 showed that a LoRA merge's weight delta has a recoverable rank and that recovering it beats
coding the bytes by 3-7x, while a full fine-tune's delta is full rank and gains nothing. The gate is a
measurement, not a tag, so this samples real fine-tunes from the week's population, fetches the same
tensor slice from the fine-tune and from its DECLARED base, and reports the singular spectrum.

For each pair: the rank at which 99.9% of the delta's energy is captured, the fraction of BF16 elements
a rank-r reconstruction gets exactly right, and the cost of factors + residual against coding the bytes.
No tag is trusted: repos whose declared base does not have the same tensor and dtype are skipped and
counted, and the 'lora'/'peft' tags are recorded only to see whether they predict the outcome."""
import json, os, re, struct, subprocess, sys, time
import numpy as np

H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/rank-census"
os.makedirs(OUT, exist_ok=True)
SLICE = 4 * 1024 * 1024          # 4 MiB: enough rows to see any rank a LoRA would use
MAX_PAIRS = 40

def curl(url, out=None, rng=None, t=120):
    a = ["curl", "-sSL", "--max-time", str(t)]
    if rng: a += ["-r", rng]
    if out: a += ["-o", out]
    r = subprocess.run(a + [url], capture_output=True, text=True)
    return (out if out else r.stdout) if r.returncode == 0 else None

def api(url):
    s = curl(url)
    try: return json.loads(s)
    except Exception: return None

def header(repo, fn):
    tmp = f"{OUT}/hdr.tmp"
    if not curl(f"{H}/{repo}/resolve/main/{fn}", tmp, "0-8388607"): return None, None
    b = open(tmp, "rb").read()
    if len(b) < 8: return None, None
    n = struct.unpack("<Q", b[:8])[0]
    if n + 8 > len(b): return None, None
    try: return json.loads(b[8:8 + n].decode()), 8 + n
    except Exception: return None, None

def locate(repo):
    """the largest BF16 2-D tensor in the repo's first shard, with its file, offset and shape"""
    d = api(f"{H}/api/models/{repo}")
    if not d: return None
    sib = [s["rfilename"] for s in d.get("siblings", [])]
    cands = [f for f in sib if f.endswith(".safetensors")]
    if not cands: return None
    fn = "model.safetensors" if "model.safetensors" in cands else sorted(cands)[0]
    h, off = header(repo, fn)
    if not h: return None
    cands = []
    for name, v in h.items():
        if name == "__metadata__" or not isinstance(v, dict): continue
        if v.get("dtype") != "BF16": continue
        sh = v.get("shape") or []
        if len(sh) != 2 or sh[1] < 512: continue
        sz = v["data_offsets"][1] - v["data_offsets"][0]
        if sz < SLICE + 8192: continue
        cands.append((name, fn, off + v["data_offsets"][0], sz, sh))
    if not cands: return None
    # a mid-layer projection matrix if one exists, else the largest tensor
    mid = [c for c in cands if re.search(r"layers?\.(\d+)\.", c[0]) and any(t in c[0] for t in ("mlp", "ffn", "proj"))]
    if mid:
        def lyr(c):
            m = re.search(r"layers?\.(\d+)\.", c[0]); return int(m.group(1)) if m else 0
        mid.sort(key=lyr); return mid[len(mid) // 2]
    return max(cands, key=lambda c: c[3])

def fetch_slice(repo, fn, start, path):
    if os.path.exists(path) and os.path.getsize(path) == SLICE: return True
    return bool(curl(f"{H}/{repo}/resolve/main/{fn}", path, f"{start}-{start + SLICE - 1}", 300)) and os.path.getsize(path) == SLICE

def h0(a):
    a = np.ascontiguousarray(a).ravel()
    h = np.bincount(a, minlength=256).astype(np.float64); p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum()) * a.size / 8.0
def plane_cost(d):
    d = np.asarray(d).ravel()
    zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(np.uint32)
    return h0((zig & 0xFF).astype(np.uint8)) + h0(((zig >> 8) & 0xFF).astype(np.uint8))
def bf2f(u): return (u.astype(np.uint32) << 16).view(np.float32)
def f2bf(x):
    u = x.astype(np.float32).view(np.uint32); return ((u + 0x7FFF + ((u >> 16) & 1)) >> 16).astype(np.uint16)

def analyse(bu, fu, k):
    n = min(bu.size, fu.size) // k * k
    b = bf2f(bu[:n]).reshape(-1, k); f = bf2f(fu[:n]).reshape(-1, k)
    D = f - b
    if not np.any(D): return dict(identical=True)
    U, S, Vt = np.linalg.svd(D, full_matrices=False)
    e = np.cumsum(S ** 2) / max(float((S ** 2).sum()), 1e-30)
    knee = int(np.searchsorted(e, 0.999) + 1)
    d16 = (fu[:n].astype(np.int64) - bu[:n].astype(np.int64)).astype(np.int16)
    today = plane_cost(d16.astype(np.int64))
    best = None
    for r in (8, 16, 32, 64, 128):
        if r > len(S): break
        Ur = U[:, :r] * S[:r]; Vr = Vt[:r, :]
        su = np.abs(Ur).max(axis=0); su[su == 0] = 1; sv = np.abs(Vr).max(axis=1); sv[sv == 0] = 1
        R = ((np.rint(Ur / su * 32767) * (su / 32767)) @ (np.rint(Vr / sv[:, None] * 32767) * (sv[:, None] / 32767)))
        ap = f2bf(b + R); tg = fu[:n].reshape(-1, k)
        resid = (tg.astype(np.int64) - ap.astype(np.int64))
        tot = plane_cost(resid) + (Ur.size + Vr.size) * 2 + (len(su) + len(sv)) * 4
        cand = dict(rank=r, exact=float(np.mean(resid == 0)), total=tot, ratio=n * 2 / tot)
        if best is None or tot < best["total"]: best = cand
    return dict(identical=False, knee=knee, rows=D.shape[0], cols=k, today_bytes=today,
                today_ratio=n * 2 / today, nonzero=float(np.mean(D != 0)), best=best)

def main():
    tags = json.load(open(f"{B}/upload-mix/population_models_tags.json"))
    pairs = []
    for rid, v in tags.items():
        t = v.get("tags") or []
        ft = [x for x in t if x.startswith("base_model:finetune:")]
        if not ft: continue
        base = ft[0].split(":", 2)[2]
        if base == rid: continue
        pairs.append((rid, base, "lora" if ("lora" in t or "peft" in t) else "-"))
    import random; random.seed(7); random.shuffle(pairs)
    print(f"{len(pairs)} repos in the window declare base_model:finetune; probing up to {MAX_PAIRS}\n")
    print(f"{'fine-tune':44s} {'tag':5s} {'rows':>5s} {'knee':>5s} {'nz%':>6s} {'today':>7s} {'best':>7s} {'r':>4s} {'exact%':>7s}")
    done = 0; skipped = 0; results = []
    baseloc = {}
    for rid, base, tag in pairs:
        if done >= MAX_PAIRS: break
        try:
            if base not in baseloc: baseloc[base] = locate(base)
            lb = baseloc[base]
            if not lb: skipped += 1; continue
            name, bfn, boff, bsz, bshape = lb
            hf, offf = header(rid, bfn if bfn != "model.safetensors" else "model.safetensors")
            if not hf or name not in hf:
                d = api(f"{H}/api/models/{rid}")
                sib = [s["rfilename"] for s in (d or {}).get("siblings", []) if s["rfilename"].endswith(".safetensors")]
                found = False
                for cand in sorted(sib)[:3]:
                    hf, offf = header(rid, cand)
                    if hf and name in hf: bfn2 = cand; found = True; break
                if not found: skipped += 1; continue
            else: bfn2 = bfn if bfn != "model.safetensors" else "model.safetensors"
            v = hf[name]
            if v.get("dtype") != "BF16" or v.get("shape") != bshape: skipped += 1; continue
            k = bshape[1]
            mid = ((bsz - SLICE) // 2 // (k * 2)) * (k * 2)
            bp = f"{OUT}/{base.replace('/','__')}.base"; fp = f"{OUT}/{rid.replace('/','__')}.ft"
            if not fetch_slice(base, bfn, boff + mid, bp): skipped += 1; continue
            if not fetch_slice(rid, bfn2, offf + v["data_offsets"][0] + mid, fp): skipped += 1; continue
            bu = np.frombuffer(open(bp, "rb").read(), dtype=np.uint16)
            fu = np.frombuffer(open(fp, "rb").read(), dtype=np.uint16)
            r = analyse(bu, fu, k); r.update(repo=rid, base=base, tag=tag, tensor=name)
            results.append(r); done += 1
            if r.get("identical"):
                print(f"{rid[:44]:44s} {tag:5s}  identical to the base (dedup already catches it)", flush=True)
            else:
                bst = r["best"]
                print(f"{rid[:44]:44s} {tag:5s} {r['rows']:5d} {r['knee']:5d} {100*r['nonzero']:5.1f}% "
                      f"{r['today_ratio']:7.3f} {bst['ratio']:7.3f} {bst['rank']:4d} {100*bst['exact']:6.2f}%", flush=True)
            json.dump(results, open(f"{OUT}/rank_census.json", "w"), indent=1)
            time.sleep(0.5)
        except Exception as ex:
            skipped += 1; print(f"{rid[:44]:44s} ERROR {ex}", flush=True)
    ok = [r for r in results if not r.get("identical")]
    low = [r for r in ok if r["best"]["ratio"] > r["today_ratio"] * 1.15]
    print(f"\nprobed {done}, skipped {skipped}")
    if ok:
        print(f"low-rank exploitable (>15% better than coding the bytes): {len(low)}/{len(ok)} = {100*len(low)/len(ok):.0f}%")
        if low:
            g = np.array([r["best"]["ratio"] / r["today_ratio"] for r in low])
            print(f"  their gain over the byte coder: min {g.min():.2f}x  median {np.median(g):.2f}x  max {g.max():.2f}x")
            print(f"  lora/peft tagged among them: {sum(1 for r in low if r['tag']=='lora')}/{len(low)}")
        print(f"  lora/peft tagged overall: {sum(1 for r in ok if r['tag']=='lora')}/{len(ok)}")
    print("RANK_CENSUS_DONE")

if __name__ == "__main__":
    main()
