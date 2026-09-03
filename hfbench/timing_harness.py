#!/usr/bin/env python3
"""Wall-clock compress/decompress harness with mandatory round-trip verification.

Every measurement is pinned to one CPU with taskset. Timing convention matches
/root/mzip-hfbench/xet_full.py exactly: time.perf_counter() around the whole
subprocess call, MB/s = bytes / 1048576 / seconds.

Usage: timeharness.py <plan.json> <out.jsonl> [reps]
Appends one JSON object per (cfg, input, rep) so progress is pollable.
"""
import hashlib, json, os, shutil, subprocess, sys, time

WORK = "/root/mzip-hfbench/timing/work"
CPU = os.environ.get("MZT_CPU", "10")
os.makedirs(WORK, exist_ok=True)

try:
    import numpy as np
except Exception:
    np = None


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def loadavg():
    return float(open("/proc/loadavg").read().split()[0])


def run_pinned(cmd, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    full = ["taskset", "-c", CPU] + cmd
    t0 = time.perf_counter()
    r = subprocess.run(full, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=e)
    dt = time.perf_counter() - t0
    return dt, r.returncode


# ---------- bg4 (HuggingFace xet ByteGrouping4LZ4) implemented in-process ----------
def bg4_arm(src, level, tag):
    """Returns (comp_bytes, ctime, dtime, ok). Mirrors xet_full.py:time_lz4_arm."""
    raw = open(src, "rb").read()
    base = os.path.join(WORK, tag)
    t0 = time.perf_counter()
    a = np.frombuffer(raw, dtype=np.uint8)
    n = (len(a) // 4) * 4
    m = a[:n].reshape(-1, 4)
    tail = a[n:].tobytes()
    tot = 0
    for k in range(4):
        fp = f"{base}.p{k}"
        open(fp, "wb").write(m[:, k].tobytes())
        subprocess.run(["taskset", "-c", CPU, "lz4"] + ([level] if level else []) + ["-q", "-f", fp, fp + ".lz4"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        tot += os.path.getsize(fp + ".lz4")
    ct = time.perf_counter() - t0

    t0 = time.perf_counter()
    back = []
    for k in range(4):
        fp = f"{base}.p{k}"
        subprocess.run(["taskset", "-c", CPU, "lz4", "-d", "-q", "-f", fp + ".lz4", fp + ".out"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        back.append(open(fp + ".out", "rb").read())
    mm = np.empty((len(back[0]), 4), dtype=np.uint8)
    for k in range(4):
        mm[:, k] = np.frombuffer(back[k], dtype=np.uint8)
    got = mm.tobytes() + tail
    dt = time.perf_counter() - t0

    ok = (got == raw)
    for k in range(4):
        for s in (f"{base}.p{k}", f"{base}.p{k}.lz4", f"{base}.p{k}.out"):
            if os.path.exists(s):
                os.remove(s)
    return tot, ct, dt, ok


def main():
    plan = json.load(open(sys.argv[1]))
    outp = sys.argv[2]
    reps = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    fout = open(outp, "a", buffering=1)

    srcsha = {}
    for job in plan:
        src = job["inp"]
        if src not in srcsha:
            srcsha[src] = sha(src)

    total = len(plan) * reps
    done = 0
    for rep in range(reps):
        for job in plan:
            cfg, src = job["cfg"], job["inp"]
            tag = f"{cfg}__{os.path.basename(src)}"
            cf = os.path.join(WORK, tag + ".mz")
            rf = os.path.join(WORK, tag + ".out")
            for f in (cf, rf):
                if os.path.exists(f):
                    os.remove(f)
            la0 = loadavg()
            rec = dict(cfg=cfg, inp=os.path.basename(src), rep=rep,
                       raw=os.path.getsize(src), load_before=la0)

            if job.get("kind") == "bg4":
                if np is None:
                    rec.update(err="numpy missing")
                    fout.write(json.dumps(rec) + "\n"); continue
                cb, ct, dt, ok = bg4_arm(src, job.get("level"), tag)
                rec.update(comp=cb, ct=ct, dt=dt, rt="OK" if ok else "MISMATCH", crc=0, drc=0)
            else:
                ccmd = [x.replace("{IN}", src).replace("{OUT}", cf) for x in job["ccmd"]]
                ct, crc = run_pinned(ccmd, job.get("env"))
                comp = os.path.getsize(cf) if os.path.exists(cf) else 0
                dcmd = [x.replace("{IN}", cf).replace("{OUT}", rf) for x in job["dcmd"]]
                dt, drc = run_pinned(dcmd, job.get("denv", job.get("env")))
                ok = os.path.exists(rf) and sha(rf) == srcsha[src]
                rec.update(comp=comp, ct=ct, dt=dt, crc=crc, drc=drc,
                           rt="OK" if ok else ("MISMATCH" if os.path.exists(rf) else "NO_OUTPUT"))
            rec["load_after"] = loadavg()
            fout.write(json.dumps(rec) + "\n")
            for f in (cf, rf):
                if os.path.exists(f):
                    os.remove(f)
            done += 1
            print(f"[{done}/{total}] rep{rep} {cfg:28s} {rec['inp']:22s} "
                  f"c={rec.get('ct',0):8.2f}s d={rec.get('dt',0):7.3f}s "
                  f"{rec.get('comp',0):>10,} {rec.get('rt')}", flush=True)
    fout.close()
    print("HARNESS_DONE")


if __name__ == "__main__":
    main()
