#!/usr/bin/env python3
"""Prove the round-trip check can FAIL. Uses the same code path as the sweep:
compress, then corrupt/truncate the compressed file, decompress, cmp.
If these do not report rt=False, every rt=True in the sweep is worthless."""
import os, shutil, subprocess, random
B="/root/mzip-hfbench/build"; d="/root/mzip-hfbench/falsify"
shutil.rmtree(d,ignore_errors=True); os.makedirs(d)
random.seed(7)
src=f"{d}/f"
open(src,"wb").write(bytes(random.getrandbits(8) for _ in range(300000)))
def run(c,**k): return subprocess.run(c,capture_output=True,text=True,**k)
def cmpok(a,b): return os.path.exists(b) and run(["cmp","-s",a,b]).returncode==0

print("--- control: untouched round-trips (must be True) ---")
run([f"{B}/lpaq_x","9",src,f"{d}/c1"]); run([f"{B}/lpaq_x","d",f"{d}/c1",f"{d}/r1"])
print("lpaq1  clean :", cmpok(src,f"{d}/r1"))
run([f"{B}/mzip_fixed","c",src,f"{d}/c2","19"]); run([f"{B}/mzip_fixed","d",f"{d}/c2",f"{d}/r2"])
print("mzip   clean :", cmpok(src,f"{d}/r2"))

print("--- mutation 1: TRUNCATE the compressed stream (must be False) ---")
for nm,cf,tool in (("lpaq1",f"{d}/c1","lpaq"),("mzip",f"{d}/c2","mzip")):
    t=cf+".trunc"; data=open(cf,"rb").read(); open(t,"wb").write(data[:len(data)//2])
    r=t+".r"
    if tool=="lpaq": run([f"{B}/lpaq_x","d",t,r])
    else: run([f"{B}/mzip_fixed","d",t,r])
    print(f"{nm:6s} trunc :", cmpok(src,r))

print("--- mutation 2: FLIP A BYTE mid-stream (must be False) ---")
for nm,cf,tool in (("lpaq1",f"{d}/c1","lpaq"),("mzip",f"{d}/c2","mzip")):
    t=cf+".flip"; data=bytearray(open(cf,"rb").read())
    i=len(data)//2; data[i]^=0xFF; open(t,"wb").write(bytes(data))
    r=t+".r2"
    if tool=="lpaq": run([f"{B}/lpaq_x","d",t,r])
    else: run([f"{B}/mzip_fixed","d",t,r])
    print(f"{nm:6s} flip  :", cmpok(src,r))

print("--- mutation 3: compare against the WRONG source (must be False) ---")
open(f"{d}/other","wb").write(b"\x00"*300000)
print("wrong-src    :", cmpok(f"{d}/other",f"{d}/r1"))
shutil.rmtree(d,ignore_errors=True)
