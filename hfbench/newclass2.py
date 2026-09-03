#!/usr/bin/env python3
"""Byte-weighted re-run of the unmeasured-class sweep.

The first pass picked one file per repo from a repo-uniform shuffle. The dataset population is
dominated in COUNT by thousands of tiny repos (its top 12 authors hold 19.6% of repos but 0.4% of the
bytes), so that sweep measured the junk and not the mass, and its "incompressible" verdict for h5, pb
and arrow says nothing about the bytes those extensions actually carry. This selects instead from the
HEAVIEST sampled repos -- the ones holding the in-window bytes -- and measures the same arms."""
import json, os, subprocess, time
B = "/root/mzip-hfbench"; OUT = f"{B}/newclass"; H = "https://huggingface.co"
WANT = ("h5", "hdf5", "pb", "arrow", "pt", "pth", "model", "tar", "mcap", "bam", "npy", "npz")
LO, HI = 3 * 1024 * 1024, 90 * 1024 * 1024
PER_EXT = 2

def sh(a, t=1200): return subprocess.run(a, capture_output=True, text=True, timeout=t)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "90", u]).stdout)
    except Exception: return None

def heavy_repos():
    out = []
    for kind in ("datasets", "models"):
        rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_{kind}.jsonl")]
        rows = [r for r in rows if r.get("ok")]
        rows.sort(key=lambda r: -(sum(r.get("bytes_in_window", {}).values()) or float(r.get("used_storage") or 0)))
        out += [(kind, r["id"], sum(r.get("bytes_in_window", {}).values())) for r in rows[:70]]
    return out

def arms(path):
    r = {"load": open("/proc/loadavg").read().split()[0], "n": os.path.getsize(path)}
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
    return r

def main():
    repos = heavy_repos()
    print(f"selecting from {len(repos)} of the heaviest sampled repos\n")
    print(f"{'ext':6s} {'file':46s} {'MB':>6s} | {'Xet':>8s} {'c':>5s} {'d':>6s} | {'ours':>8s} {'c':>5s} {'d':>6s} | verdict")
    got = {e: 0 for e in WANT}; rows = []
    for kind, rid, wb in repos:
        if all(got[e] >= PER_EXT for e in WANT): break
        d = api(f"{H}/api/{kind}/{rid}/tree/main?recursive=true&limit=1000")
        if not isinstance(d, list): continue
        cands = []
        for f in d:
            if f.get("type") != "file": continue
            p = f["path"]; sz = int(f.get("size") or 0)
            if not (LO <= sz <= HI): continue
            base = p.rsplit("/", 1)[-1].lower()
            e = base.rsplit(".", 1)[-1] if "." in base else ""
            if e in WANT and got[e] < PER_EXT: cands.append((e, p, sz))
        for e, p, sz in cands[:2]:
            if got[e] >= PER_EXT: continue
            loc = f"{OUT}/w.{e}"
            u = f"{H}/{'datasets/' if kind=='datasets' else ''}{rid}/resolve/main/{p}"
            sh(["curl", "-sSL", "--max-time", "1200", "-o", loc, u], t=1260)
            if not os.path.exists(loc) or os.path.getsize(loc) < LO: continue
            try: r = arms(loc)
            except Exception as ex: print(f"{e:6s} {rid[:46]:46s} arm error {ex}"); os.remove(loc); continue
            if "xet" not in r or "pe" not in r: os.remove(loc); continue
            best = max((r[k][0], k) for k in ("pe", "mzip") if k in r)
            xr = r["xet"][0]
            v = f"WIN -{100*(1-xr/best[0]):.1f}%" if best[0] > xr * 1.005 else (
                f"LOSS +{100*(best[0]/xr-1):.1f}%" if best[0] < xr * 0.995 else "tie")
            bv = r[best[1]]
            print(f"{e:6s} {(rid+'/'+p)[:46]:46s} {r['n']/1e6:6.1f} | {xr:8.4f} {r['xet'][1]:5.0f} {r['xet'][2]:6.0f} | "
                  f"{best[0]:8.4f} {bv[1]:5.0f} {bv[2]:6.0f} | {v} ({best[1]}) load {r['load']}", flush=True)
            rows.append(dict(ext=e, repo=rid, path=p, kind=kind, repo_bytes=wb, **r, verdict=v))
            json.dump(rows, open(f"{OUT}/newclass2.json", "w"), indent=1, default=str)
            got[e] += 1; os.remove(loc)
        time.sleep(0.1)
    print("NEWCLASS2_DONE")

if __name__ == "__main__":
    main()
