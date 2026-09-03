#!/usr/bin/env python3
"""Make CompressionMode::FAST mean what its comment says ("prefer fast
decompression") on numeric data, in a SCRATCH copy:
 (1) allow the SoA/MS transform in FAST (it was excluded; MS is the CHEAPEST
     decode path -- 492 MB/s on F32 whole-file vs 8.6 MB/s when FAST forces BWT);
 (2) decode-aware tiebreak: in FAST, adopt the brotli backstop when it is within
     1% of a slow-decoding incumbent (BWT_TEXT / CM_TEXT / PPMD).
BALANCED must remain BYTE-IDENTICAL (checked). Builds a mode-exposing CLI from
the scratch tree and measures FAST vs BALANCED sizes + decode CPU time at 64K and
whole-file on weights. Never touches /root/mzip."""
import os, shutil, subprocess, sys, time, resource, hashlib
SRC="/root/mzip"; T="/root/mzip-hfbench/fasttree"; B="/root/mzip-hfbench/build"; W="/root/mzip-hfbench/weights"
if os.path.exists(T): shutil.rmtree(T)
os.makedirs(f"{T}/ppmd"); os.makedirs(f"{T}/zstd_release/zstd-v1.5.6-win64/include")
for f in os.listdir(SRC):
    if f.endswith((".hpp",".h",".cpp",".c")): shutil.copy2(f"{SRC}/{f}",f"{T}/{f}")
for f in os.listdir(f"{SRC}/ppmd"): shutil.copy2(f"{SRC}/ppmd/{f}",f"{T}/ppmd/{f}")
os.symlink("/usr/include/zstd.h",f"{T}/zstd_release/zstd-v1.5.6-win64/include/zstd.h")
shutil.copy2("/root/mzip-hfbench/modeswp/mzipx.cpp",f"{T}/mzipx.cpp")
P=f"{T}/mzip.hpp"; s=open(P,encoding="utf-8",errors="surrogateescape").read()
o1="    if (try_soa && mode != CompressionMode::FAST && size >= 4096 && !is_text_like(data, size)) {"
n1=("    // FAST-MODE FIX (2026-09-02): SoA/MS was EXCLUDED from FAST, yet MS is the cheapest\n"
    "    // decode path on numeric data (F32 whole-file: 492 MB/s via MS vs 8.6 MB/s when FAST\n"
    "    // forces BWT_TEXT). A mode named for fast decompression must keep it. Measured.\n"
    "    if (try_soa && size >= 4096 && !is_text_like(data, size)) {")
o2="                        && bsz < cur && bsz <= cap) {\n                        memcpy(preprocess_data, bb.data(), bsz);"
n2=("                        && bsz <= cap && (bsz < cur ||\n"
    "                            // FAST-MODE FIX (2026-09-02): decode-aware tiebreak. At 64K on BF16 the BWT\n"
    "                            // result beat brotli by 61 B (0.14%) and decodes 5.6x slower (9 vs 53 MB/s).\n"
    "                            // In FAST, prefer brotli when within 1% of a slow-decoding incumbent.\n"
    "                            (mode == CompressionMode::FAST && bsz <= cur + cur / 100 &&\n"
    "                             (analysis.type == BlockType::BWT_TEXT || analysis.type == BlockType::CM_TEXT ||\n"
    "                              analysis.type == BlockType::PPMD)))) {\n"
    "                        memcpy(preprocess_data, bb.data(), bsz);")
for o,n in ((o1,n1),(o2,n2)):
    c=s.count(o)
    if c!=1: print(f"REFUSING: anchor x{c}: {o[:60]}"); sys.exit(1)
    s=s.replace(o,n)
open(P,"w",encoding="utf-8",errors="surrogateescape").write(s); print("patched FAST semantics in scratch")
OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
r=subprocess.run(f"cd {T} && g++ -O3 -std=c++17 -march=native -I{T} -o {B}/mzipx_fast mzipx.cpp {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
print("build mzipx_fast:","OK" if r.returncode==0 else "FAILED",[l for l in r.stderr.splitlines() if " error" in l][:4])
if r.returncode!=0: sys.exit(1)
def cpu(): r=resource.getrusage(resource.RUSAGE_CHILDREN); return r.ru_utime+r.ru_stime
def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
def run(binp,mode,inp,out):
    subprocess.run([binp,"c",inp,out,"1",mode,"0"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
def dec(binp,inp,out):
    c0=cpu(); subprocess.run([binp,"d",inp,out],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); return cpu()-c0
print("\nBALANCED byte-identity (new scratch vs current repo mzipx), whole-file:")
for tag,f in (("wfp32",f"{W}/weights-fp32.bin"),("wbf16",f"{W}/weights-bf16.bin")):
    run("/root/mzip-hfbench/modeswp/mzipx","balanced",f,"/tmp/fa.old"); run(f"{B}/mzipx_fast","balanced",f,"/tmp/fa.new")
    print(f"  {tag}: old {sha('/tmp/fa.old')} new {sha('/tmp/fa.new')} {'IDENTICAL' if sha('/tmp/fa.old')==sha('/tmp/fa.new') else 'DIFFERENT'}")
print("\nFAST vs BALANCED (new build): size and decode CPU, whole-file 8 MiB and 128x64K:")
for tag,f in (("MiniLM F32",f"{W}/MiniLM-L6-v2.slice"),("wbf16 BF16",f"{W}/weights-bf16.bin"),("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice")):
    raw=open(f,"rb").read(); N=len(raw)
    for mode in ("balanced","fast"):
        run(f"{B}/mzipx_fast",mode,f,"/tmp/fa.w"); sw=os.path.getsize("/tmp/fa.w"); dw=dec(f"{B}/mzipx_fast","/tmp/fa.w","/tmp/fa.wr")
        okw=open("/tmp/fa.wr","rb").read()==raw
        sc=0; dc=0; okc=True
        for i in range(0,N,65536):
            open("/tmp/fa.c","wb").write(raw[i:i+65536]); run(f"{B}/mzipx_fast",mode,"/tmp/fa.c","/tmp/fa.cz"); sc+=os.path.getsize("/tmp/fa.cz")
            dc+=dec(f"{B}/mzipx_fast","/tmp/fa.cz","/tmp/fa.cr"); okc=okc and open("/tmp/fa.cr","rb").read()==raw[i:i+65536]
        print(f"  {tag:11s} {mode:8s} whole {sw:>9,} ({N/sw:.4f}x) dec {N/1048576/max(dw,1e-6):>7.1f} MB/s {'ok' if okw else 'RT_FAIL'} | 64K {sc:>9,} ({N/sc:.4f}x) dec {N/1048576/max(dc,1e-6):>6.1f} MB/s(incl exec) {'ok' if okc else 'RT_FAIL'}")
for x in ("/tmp/fa.old","/tmp/fa.new","/tmp/fa.w","/tmp/fa.wr","/tmp/fa.c","/tmp/fa.cz","/tmp/fa.cr"):
    if os.path.exists(x): os.remove(x)
print("ALLDONE_FAST")
