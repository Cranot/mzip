#!/usr/bin/env python3
"""Per-shape headroom: is the CM gap prose-specific or universal?

Runs the two strong context mixers across every corpus and compares to the
mzip result already recorded in matrix.json. Answers: on which data shapes
does a direct context model beat mzip, and by how much?
"""
import json, os, subprocess, time

CORP = "/root/mzip-hfbench/corpora"
RUN  = "/root/mzip-hfbench/run2"
LPAQ = "/root/mzip-hfbench/build/lpaq_x"

mat = json.load(open(f"{RUN}/matrix.json"))
mz = {r["corpus"]: r for r in mat if r["tool"] == "mzip L19"}
best = {}
for r in mat:
    if r["comp"] > 0 and (r["corpus"] not in best or r["comp"] < best[r["corpus"]]["comp"]):
        best[r["corpus"]] = r

out = []
print(f"{'corpus':20s} {'orig':>10s} {'mzip':>10s} {'lpaq1':>10s} {'zpaq-m5':>10s} "
      f"{'best CM vs mzip':>16s}")
for f in sorted(os.listdir(CORP)):
    if not f.endswith(".bin"): continue
    tag, p = f[:-4], f"{CORP}/{f}"
    if tag not in mz: continue
    n = os.path.getsize(p)

    t0 = time.perf_counter()
    subprocess.run([LPAQ, "9", p, "/tmp/h.lp"], stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    lt = time.perf_counter() - t0
    ls = os.path.getsize("/tmp/h.lp") if os.path.exists("/tmp/h.lp") else 0
    subprocess.run([LPAQ, "d", "/tmp/h.lp", "/tmp/h.rt"], stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    lok = os.path.exists("/tmp/h.rt") and open("/tmp/h.rt","rb").read() == open(p,"rb").read()
    for x in ("/tmp/h.lp","/tmp/h.rt"):
        if os.path.exists(x): os.remove(x)

    if os.path.exists("/tmp/h.zpaq"): os.remove("/tmp/h.zpaq")
    t0 = time.perf_counter()
    subprocess.run(["zpaq","a","/tmp/h.zpaq",p,"-m5"], stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    zt = time.perf_counter() - t0
    zs = os.path.getsize("/tmp/h.zpaq") if os.path.exists("/tmp/h.zpaq") else 0
    if os.path.exists("/tmp/h.zpaq"): os.remove("/tmp/h.zpaq")

    m = mz[tag]["comp"]
    bcm = min(x for x in (ls, zs) if x > 0)
    d = 100 * (m - bcm) / m
    out.append(dict(corpus=tag, orig=n, mzip=m, lpaq=ls, lpaq_rt=lok,
                    lpaq_s=round(lt,2), zpaq=zs, zpaq_s=round(zt,2),
                    cm_vs_mzip_pct=round(d,2), overall_best=best[tag]["tool"]))
    print(f"{tag:20s} {n:>10,} {m:>10,} {ls:>10,} {zs:>10,} {d:>+15.2f}%"
          + ("" if lok else "  [lpaq RT FAIL]"))

json.dump(out, open(f"{RUN}/headroom.json","w"), indent=1)
print(f"\nwrote {RUN}/headroom.json")
print("positive % = a direct context model beats mzip on that shape")
print("ALLDONE2")
