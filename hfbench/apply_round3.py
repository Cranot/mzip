#!/usr/bin/env python3
"""Round 3 on /root/mzip: (a) SoA forced trial W=4 only (W=2 never won; halves the
extra cost), (b) six sibling roundtrip guards via guard_siblings{,2,3}.py.
Then build mzip_fixed3 + ut_repo3, run the suite, and check:
  - 64K sizes on 3 slices identical to mzip_soa (W=4 carried every win)
  - whole-file byte-identity vs mzip_fixed2 on 4 corpora (guards must be no-ops
    where encoders are lossless)
Backups: mzip.hpp.bak-w4only (+ per-guard backups). Refuses on anchor mismatch."""
import os, shutil, subprocess, sys, hashlib
P="/root/mzip/mzip.hpp"; B="/root/mzip-hfbench/build"; H="/root/mzip-hfbench"
src=open(P,encoding="utf-8",errors="surrogateescape").read()
OLD="            for (int fc = 0; fc < 2; ++fc) {            // CANDS[0]={0,2,0}, CANDS[1]={0,4,0}\n"
NEW=("            // W=4 ONLY (2026-09-02): measured at 64K, stride-2 never beat stride-4 on any\n"
     "            // slice (F32, BF16, GGUF F16 within 46 B), and each forced trial is a full inner\n"
     "            // compress -- dropping W=2 halves the fix's cost (2.5x -> ~1.5x of stock).\n"
     "            for (int fc = 1; fc < 2; ++fc) {            // CANDS[1]={0,4,0}\n")
if "W=4 ONLY (2026-09-02)" in src: print("w4only ALREADY APPLIED")
else:
    n=src.count(OLD)
    if n!=1: print(f"REFUSING w4only: anchor x{n}"); sys.exit(1)
    shutil.copy2(P,P+".bak-w4only"); src=src.replace(OLD,NEW)
    open(P,"w",encoding="utf-8",errors="surrogateescape").write(src); print("PATCHED w4only")
for g in ("guard_siblings.py","guard_siblings2.py","guard_siblings3.py"):
    # guard_siblings.py also builds; suppress that by running only its patch part
    code=open(f"{H}/{g}",encoding="utf-8").read().split("\nB = ")[0] if g=="guard_siblings.py" else open(f"{H}/{g}",encoding="utf-8").read()
    r=subprocess.run([sys.executable,"-c",code],capture_output=True,text=True)
    print(g,"->",(r.stdout.strip().splitlines() or [r.stderr.strip()[-200:]])[-1])
    if r.returncode!=0: print(r.stderr[-500:]); sys.exit(1)
OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name,srcf,extra in (("mzip_fixed3","mzip_cli.cpp",""),("ut_repo3","mzip_unit_tests.cpp","-D_USE_MATH_DEFINES")):
    r=subprocess.run(f"cd {B} && g++ -O3 -std=c++17 -march=native {extra} -I{B} -I/root/mzip -o {B}/{name} /root/mzip/{srcf} {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}",[l for l in r.stderr.splitlines() if ' error' in l][:4])
    if r.returncode!=0: sys.exit(1)
r=subprocess.run(f"{B}/ut_repo3",capture_output=True,text=True); print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
print("\n64K size identity vs mzip_soa (W=4 carried every win):")
for tag,f in (("MiniLM F32","weights/MiniLM-L6-v2.slice"),("weights-bf16","weights/weights-bf16.bin"),("gguf-f16","weights/gguf-f16.slice")):
    raw=open(f"{H}/{f}","rb").read(); a=b=0
    for i in range(0,len(raw),65536):
        open("/tmp/r3.in","wb").write(raw[i:i+65536])
        subprocess.run([f"{B}/mzip_soa","c","/tmp/r3.in","/tmp/r3.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); a+=os.path.getsize("/tmp/r3.a")
        subprocess.run([f"{B}/mzip_fixed3","c","/tmp/r3.in","/tmp/r3.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); b+=os.path.getsize("/tmp/r3.b")
    print(f"  {tag:13s} soa(W2+W4) {a:>9,}  fixed3(W4,guards) {b:>9,}  {'IDENTICAL' if a==b else f'DIFF {b-a:+d}'}")
print("\nwhole-file byte-identity vs mzip_fixed2 (guards must be no-ops on lossless encoders):")
for f in ("corpora/code-python.bin","corpora/tabular-numeric.bin","corpora/dialogue-csv.bin","corpora/qa-gsm8k.bin"):
    subprocess.run([f"{B}/mzip_fixed2","c",f"{H}/{f}","/tmp/r3.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run([f"{B}/mzip_fixed3","c",f"{H}/{f}","/tmp/r3.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    print(f"  {f.split('/')[-1]:22s} {sha('/tmp/r3.a')}  {sha('/tmp/r3.b')}  {'IDENTICAL' if sha('/tmp/r3.a')==sha('/tmp/r3.b') else 'DIFFERENT'}")
for x in ("/tmp/r3.in","/tmp/r3.a","/tmp/r3.b"):
    if os.path.exists(x): os.remove(x)
print(subprocess.run("cd /root/mzip && git diff --stat",shell=True,capture_output=True,text=True).stdout)
print("ALLDONE_R3")
