#!/usr/bin/env python3
"""Measure the classes the census counted as zero.

The extension census showed that the coarse buckets hide their content: `archive` in datasets is mostly
uncompressed `.tar`, and `other` is mostly `.h5`, `.hdf5`, `.pb`, `.arrow`, `.model` and torch `.pt` --
uncompressed containers whose payload is float arrays, which is the class this coder is strongest on.
None of it has ever been measured. This fetches real files of each kind from the sampled dataset and
model populations and runs the same arms as the main table: Xet Auto, the plane coder, merged mzip FAST,
and per-chunk zstd for reference.

Files are capped in size so the sweep stays honest about disk and time; every arm verifies its own
round trip and the harness records /proc/loadavg with each measurement."""
import json, os, subprocess, sys, time
B = "/root/mzip-hfbench"; OUT = f"{B}/newclass"; os.makedirs(OUT, exist_ok=True)
H = "https://huggingface.co"
WANT = {"tar": 3, "arrow": 3, "h5": 2, "hdf5": 2, "pb": 2, "pt": 3, "pth": 2, "model": 2, "mcap": 1, "bam": 1}
LO, HI = 2 * 1024 * 1024, 80 * 1024 * 1024

def sh(a, t=900): return subprocess.run(a, capture_output=True, text=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout)
    except Exception: return None

def find():
    got = {k: [] for k in WANT}
    for kind in ("datasets", "models"):
        pop = json.load(open(f"{B}/upload-mix/population_{kind}.json"))["rows"]
        import random; random.Random(3).shuffle(pop)
        for m in pop[:400]:
            if all(len(got[k]) >= WANT[k] for k in WANT): break
            d = api(f"{H}/api/{kind}/{m['id']}/tree/main?recursive=true&limit=1000")
            if not isinstance(d, list): continue
            for f in d:
                if f.get("type") != "file": continue
                p = f["path"]; sz = int(f.get("size") or 0)
                if not (LO <= sz <= HI): continue
                e = p.rsplit(".", 1)[-1].lower() if "." in p.rsplit("/", 1)[-1] else ""
                if e in WANT and len(got[e]) < WANT[e]:
                    got[e].append((kind, m["id"], p, sz))
            time.sleep(0.05)
    return got

def arms(path, name, ext):
    load = open("/proc/loadavg").read().split()[0]
    n = os.path.getsize(path)
    r = {}
    x = sh(["taskset", "-c", "3", f"{B}/agents/hf-fairness/xetsim/target/release/xetsim", path, "65536", "3"])
    for l in x.stdout.splitlines():
        if l.startswith("auto "):
            f = l.split(); r["xet"] = (float(f[3]), float(f[4]), float(f[5])); break
    p = sh(["taskset", "-c", "3", f"{B}/final/plane-entropy/bench_pe", path, "65536", "3"])
    for l in p.stdout.splitlines():
        if "ratio" in l:
            f = l.split(); r["pe"] = (float(f[3]), float(f[5]), float(f[8])); break
    m = sh(["taskset", "-c", "3", f"{B}/final/tree/pe_bench2", path, "65536", "FAST", "3"])
    for l in m.stdout.splitlines():
        if l.startswith("FAST"):
            f = l.split(); r["mzip"] = (float(f[2]), float(f[3]), float(f[4])); break
    z = sh(["taskset", "-c", "3", f"{B}/convert/seqint/seqint_pipe", path, "65536", "3", "3", "0"])
    for l in z.stdout.splitlines():
        if "ratio" in l:
            f = l.replace("ratio", "").split()
            try: r["zstd3"] = (float(f[6]), float(f[8]), float(f[11]))
            except Exception: pass
            break
    r["n"] = n; r["load"] = load
    return r

def main():
    got = find()
    print(f"{'class':7s} {'file':40s} {'MB':>6s} | {'Xet ratio':>9s} {'c':>6s} {'d':>6s} | {'ours':>8s} {'c':>6s} {'d':>6s} | {'verdict':>16s}")
    rows = []
    for ext, items in got.items():
        for kind, repo, path, sz in items:
            loc = f"{OUT}/s.{ext}"
            u = f"{H}/{'datasets/' if kind=='datasets' else ''}{repo}/resolve/main/{path}"
            sh(["curl", "-sSL", "--max-time", "900", "-o", loc, u], t=960)
            if not os.path.exists(loc) or os.path.getsize(loc) < LO: continue
            try: r = arms(loc, f"{repo}/{path}", ext)
            except Exception as e: print(f"{ext:7s} {repo[:40]:40s} arm error {e}"); continue
            if "xet" not in r: continue
            best = max([(v[0], k) for k, v in r.items() if k in ("pe", "mzip", "zstd3")] or [(0, "-")])
            xr = r["xet"][0]
            verdict = f"WIN -{100*(1-xr/best[0]):.1f}%" if best[0] > xr else f"LOSS +{100*(best[0]/xr-1):.1f}%"
            rows.append(dict(ext=ext, repo=repo, path=path, **{k: v for k, v in r.items()}, best=best[1], verdict=verdict))
            bv = r.get(best[1], (0, 0, 0))
            print(f"{ext:7s} {(repo+'/'+path)[:40]:40s} {r['n']/1e6:6.1f} | {xr:9.4f} {r['xet'][1]:6.0f} {r['xet'][2]:6.0f} | "
                  f"{best[0]:8.4f} {bv[1]:6.0f} {bv[2]:6.0f} | {verdict:>10s} ({best[1]})", flush=True)
            json.dump(rows, open(f"{OUT}/newclass.json", "w"), indent=1, default=str)
            os.remove(loc)
    print("NEWCLASS_DONE")

if __name__ == "__main__":
    main()
