#!/usr/bin/env python3
import os, shutil, subprocess
B="/root/mzip-hfbench/build"; d="/root/mzip-hfbench/ovh"
shutil.rmtree(d,ignore_errors=True); os.makedirs(d)
def run(c,**k): return subprocess.run(c,capture_output=True,text=True,**k)
def sz(p): return os.path.getsize(p) if os.path.exists(p) else -1
for n in (1, 1024, 65536):
    p=f"{d}/f"; open(p,"wb").write(b"\x00"*n)
    for f in ("m","l","x","z.zpaq"):
        if os.path.exists(f"{d}/{f}"): os.remove(f"{d}/{f}")
    run([f"{B}/mzip_fixed","c",p,f"{d}/m","19"])
    run([f"{B}/lpaq_x","9",p,f"{d}/l"])
    run(["zpaq","a",f"{d}/z.zpaq","f","-m5"],cwd=d)
    with open(f"{d}/x","wb") as fo: subprocess.run(["xz","-9e","-T1","-c",p],stdout=fo)
    print("input %6d B (all zero) -> mzip %6d  lpaq1 %6d  zpaq-m5 %6d  xz-9e %6d"
          % (n, sz(f"{d}/m"), sz(f"{d}/l"), sz(f"{d}/z.zpaq"), sz(f"{d}/x")))
shutil.rmtree(d,ignore_errors=True)
