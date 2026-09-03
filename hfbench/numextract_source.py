#!/usr/bin/env python3
"""Fix NUM_EXTRACT at source (SCRATCH copy): detect_num_extract declines any block
containing a digit run the encoder cannot reproduce -- a leading zero with length
> 1 ("02" -> "2"), or >= 10 digits (may exceed uint32). The adoption guard stays
as defence in depth. Builds, runs suite, checks the reproducer + 8 corpora are
byte-identical to the current native build (the guard already routed those)."""
import os, shutil, subprocess, sys, hashlib
SRC="/root/mzip"; T="/root/mzip-hfbench/netree"; B="/root/mzip-hfbench/build"; H="/root/mzip-hfbench"
if os.path.exists(T): shutil.rmtree(T)
os.makedirs(f"{T}/ppmd"); os.makedirs(f"{T}/zstd_release/zstd-v1.5.6-win64/include")
for f in os.listdir(SRC):
    if f.endswith((".hpp",".h",".cpp",".c")): shutil.copy2(f"{SRC}/{f}",f"{T}/{f}")
for f in os.listdir(f"{SRC}/ppmd"): shutil.copy2(f"{SRC}/ppmd/{f}",f"{T}/ppmd/{f}")
os.symlink("/usr/include/zstd.h",f"{T}/zstd_release/zstd-v1.5.6-win64/include/zstd.h")
P=f"{T}/mzip.hpp"; s=open(P,encoding="utf-8",errors="surrogateescape").read()
OLD="""            while (i < n && data[i] >= '0' && data[i] <= '9') {
                num = num * 10 + (data[i] - '0');
                i++;
            }
            num_count++;
            num_bytes += (i - start);
            if (sample_nums.size() < 200) {"""
NEW="""            while (i < n && data[i] >= '0' && data[i] <= '9') {
                num = num * 10 + (data[i] - '0');
                i++;
            }
            // SOURCE FIX (2026-09-02): encode_num_extract stores each digit run as a uint32
            // VALUE and re-emits the value, so it cannot reproduce a zero-padded run ("02" of
            // 0.02 -> "2"; "001" of 0.001 -> "1") or a run >= 2^32. Measured on real HF
            // config.json files (initializer_range 0.02, layer_norm_eps 1e-06): the lossy
            // stream reached the top-level verify and the whole 58 KB file fell to uRAW (28x
            // blowup). Decline the block here so the encoder is never asked to; the adoption
            // guard remains as defence in depth. Minimal reproducer: the two bytes "02".
            if ((i - start) > 1 && data[start] == '0') return false;
            if ((i - start) >= 10) return false;
            num_count++;
            num_bytes += (i - start);
            if (sample_nums.size() < 200) {"""
c=s.count(OLD)
if c!=1: print(f"REFUSING: anchor x{c}"); sys.exit(1)
open(P,"w",encoding="utf-8",errors="surrogateescape").write(s.replace(OLD,NEW)); print("patched detect_num_extract in scratch")
OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name,srcf,extra in (("mzip_ne","mzip_cli.cpp",""),("ut_ne","mzip_unit_tests.cpp","-D_USE_MATH_DEFINES")):
    r=subprocess.run(f"cd {T} && g++ -O3 -std=c++17 -march=native {extra} -I{T} -o {B}/{name} {srcf} {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}",[l for l in r.stderr.splitlines() if ' error' in l][:4])
    if r.returncode!=0: sys.exit(1)
r=subprocess.run(f"{B}/ut_ne",capture_output=True,text=True); print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
print("\nbyte-identity vs current native (guard already routed these):")
for f in ("small2/nvidia__Qwen3.6-35B-A3B-NVFP4__config.json","synth3.json","corpora/code-python.bin","corpora/tabular-numeric.bin","corpora/dialogue-csv.bin","corpora/qa-gsm8k.bin","corpora/tweets-jsonl.bin"):
    subprocess.run([f"{B}/mzip_fixed3","c",f"{H}/{f}","/tmp/ne.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run([f"{B}/mzip_ne","c",f"{H}/{f}","/tmp/ne.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    a,b=os.path.getsize("/tmp/ne.a"),os.path.getsize("/tmp/ne.b")
    print(f"  {f.split('/')[-1][:44]:44s} {a:>9,} -> {b:>9,}  {'IDENTICAL' if sha('/tmp/ne.a')==sha('/tmp/ne.b') else f'DIFF {b-a:+d}'}")
for x in ("/tmp/ne.a","/tmp/ne.b"):
    if os.path.exists(x): os.remove(x)
print("ALLDONE_NE")
