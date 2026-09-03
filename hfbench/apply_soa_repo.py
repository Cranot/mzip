#!/usr/bin/env python3
"""Apply the validated small-block SoA trial to /root/mzip/mzip.hpp (backup kept,
uncommitted), then build mzip_fixed2 + ut_repo from the REAL tree and run the
losslessness suite. Refuses if the anchor is not unique or already applied.
The scratch-tree copy of this exact patch measured: +8.08/+8.31/+7.19% on F32
weights at 64K, never worse, stock decoder decodes 100% of output, suite 50/50,
byte-identical above 1 MiB."""
import os, shutil, subprocess, sys
sys.path.insert(0, "/root/mzip-hfbench")
P = "/root/mzip/mzip.hpp"
src = open(P, encoding="utf-8", errors="surrogateescape").read()
if "SMALL-BLOCK SoA TRIAL (2026-09-02)" in src:
    print("ALREADY APPLIED");
else:
    # reuse the exact OLD/NEW strings from the scratch patcher
    txt = open("/root/mzip-hfbench/soa_small_fix.py", encoding="utf-8").read()
    OLD = txt.split('OLD = """',1)[1].split('"""',1)[0]
    NEW = txt.split('NEW = """',1)[1].split('"""',1)[0]
    n = src.count(OLD)
    if n != 1: print(f"REFUSING: anchor matched {n} times"); sys.exit(1)
    shutil.copy2(P, P + ".bak-soasmall")
    open(P, "w", encoding="utf-8", errors="surrogateescape").write(src.replace(OLD, NEW))
    print("PATCHED /root/mzip/mzip.hpp  (backup mzip.hpp.bak-soasmall)")
B = "/root/mzip-hfbench/build"
OBJS = f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"
LIBS = "-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name, srcf, extra in (("mzip_fixed2", "mzip_cli.cpp", ""), ("ut_repo", "mzip_unit_tests.cpp", "-D_USE_MATH_DEFINES")):
    r = subprocess.run(f"cd {B} && g++ -O3 -std=c++17 -march=native {extra} -I{B} -I/root/mzip -o {B}/{name} /root/mzip/{srcf} {OBJS} {LIBS}",
                       shell=True, capture_output=True, text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}", [l for l in r.stderr.splitlines() if ' error' in l][:3])
r = subprocess.run(f"{B}/ut_repo", capture_output=True, text=True)
print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
print(subprocess.run("cd /root/mzip && git diff --stat", shell=True, capture_output=True, text=True).stdout)
print("ALLDONE_APPLY")
