#!/usr/bin/env python3
"""Apply to /root/mzip (backups kept, uncommitted): DICT_HF_REAL id 13 (+ emitter
entry + train_corpus/hf_dict.bin) and the detect_num_extract source fix. Both
validated in scratch: suite 50/50, dict -28.88% on 144 held-out HF files, source
fix byte-identical on reproducer + corpora. Then build mzip_fixed4 + ut_repo4, run
suite, re-measure held-out, and byte-identity on 4 corpora + 2 weight slices."""
import os, shutil, subprocess, sys, hashlib
R="/root/mzip"; B="/root/mzip-hfbench/build"; H="/root/mzip-hfbench/hfdict"
def rd(p): return open(p,encoding="utf-8",errors="surrogateescape").read()
def wr(p,s): open(p,"w",encoding="utf-8",errors="surrogateescape").write(s)
# --- dictionary ---
P=f"{R}/mzip_dicts.h"; s=rd(P)
if "DICT_HF_REAL" in s: print("dict ALREADY APPLIED")
else:
    d=open(f"{H}/hf_dict_16384.bin","rb").read(); assert len(d)==16384
    shutil.copy2(P,P+".bak-hfdict")
    anchor="struct DictEntry { uint8_t id; const uint8_t* data; size_t size; };"; assert s.count(anchor)==1
    arr="static const uint8_t DICT_HF_REAL[16384] = {\n  "+"".join(f"0x{b:02x},"+("\n  " if (i+1)%16==0 else " ") for i,b in enumerate(d))+"\n};\nstatic const size_t DICT_HF_REAL_SIZE = 16384;\n\n"
    s=s.replace(anchor,"// ID 13: real-data dict trained on HuggingFace model-repo metadata JSON (config.json,\n// tokenizer_config.json, generation_config.json, special_tokens_map.json) from the\n// top-120 models by downloads, 2026-09-02. Held-out (144 files): mzip 60,288 -> 42,878 B.\n"+arr+anchor)
    tbl="    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\n};"; assert s.count(tbl)==1
    s=s.replace(tbl,"    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\n    {13, DICT_HF_REAL,  DICT_HF_REAL_SIZE},\n};"); wr(P,s)
    E=f"{R}/emit_dicts_header.cpp"; e=rd(E); shutil.copy2(E,E+".bak-hfdict")
    a1='        {"DICT_JSON_REAL", "train_corpus/json_dict.bin", 12},\n'; a2='    fprintf(f, "    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\\n");\n'
    assert e.count(a1)==1 and e.count(a2)==1
    e=e.replace(a1,a1+'        {"DICT_HF_REAL",   "train_corpus/hf_dict.bin",   13},\n').replace(a2,a2+'    fprintf(f, "    {13, DICT_HF_REAL,  DICT_HF_REAL_SIZE},\\n");\n'); wr(E,e)
    shutil.copy2(f"{H}/hf_dict_16384.bin",f"{R}/train_corpus/hf_dict.bin")
    print("APPLIED dict id 13: mzip_dicts.h, emit_dicts_header.cpp, train_corpus/hf_dict.bin")
# --- num_extract source fix ---
P=f"{R}/mzip.hpp"; s=rd(P)
if "SOURCE FIX (2026-09-02): encode_num_extract stores" in s: print("num_extract source fix ALREADY APPLIED")
else:
    t=rd("/root/mzip-hfbench/numextract_source.py"); OLD=t.split('OLD="""',1)[1].split('"""',1)[0]; NEW=t.split('NEW="""',1)[1].split('"""',1)[0]
    c=s.count(OLD); assert c==1, c
    shutil.copy2(P,P+".bak-nesource"); wr(P,s.replace(OLD,NEW)); print("APPLIED num_extract source fix")
# --- build + suite ---
OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name,srcf,extra in (("mzip_fixed4","mzip_cli.cpp",""),("ut_repo4","mzip_unit_tests.cpp","-D_USE_MATH_DEFINES")):
    r=subprocess.run(f"cd {B} && g++ -O3 -std=c++17 -march=native {extra} -I{B} -I{R} -o {B}/{name} {R}/{srcf} {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}",[l for l in r.stderr.splitlines() if ' error' in l][:4])
    if r.returncode!=0: sys.exit(1)
r=subprocess.run(f"{B}/ut_repo4",capture_output=True,text=True); print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
# --- held-out + identity ---
def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
t3=t4=0
for f in sorted(os.listdir(f"{H}/test")):
    p=f"{H}/test/{f}"
    subprocess.run([f"{B}/mzip_fixed3","c",p,"/tmp/r4.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); t3+=os.path.getsize("/tmp/r4.a")
    subprocess.run([f"{B}/mzip_fixed4","c",p,"/tmp/r4.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); t4+=os.path.getsize("/tmp/r4.b")
    subprocess.run([f"{B}/mzip_fixed4","d","/tmp/r4.b","/tmp/r4.rt"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    assert open("/tmp/r4.rt","rb").read()==open(p,"rb").read(), f"RT FAIL {f}"
print(f"\nheld-out 144 HF files: fixed3 {t3:,} -> fixed4 {t4:,} ({100*(t3-t4)/t3:+.2f}%), all round-trips OK")
print("byte-identity fixed3 vs fixed4 on inputs the dict trial cannot touch (>256 KB single blocks):")
for f in ("corpora/code-python.bin","corpora/tabular-numeric.bin","corpora/qa-gsm8k.bin","corpora/prose-wikitext.bin","weights/weights-fp32.bin","weights/weights-bf16.bin"):
    subprocess.run([f"{B}/mzip_fixed3","c",f"/root/mzip-hfbench/{f}","/tmp/r4.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run([f"{B}/mzip_fixed4","c",f"/root/mzip-hfbench/{f}","/tmp/r4.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    print(f"  {f.split('/')[-1]:22s} {'IDENTICAL' if sha('/tmp/r4.a')==sha('/tmp/r4.b') else 'DIFFERENT'}")
for x in ("/tmp/r4.a","/tmp/r4.b","/tmp/r4.rt"):
    if os.path.exists(x): os.remove(x)
print(subprocess.run(f"cd {R} && git status --porcelain && git diff --stat | tail -1",shell=True,capture_output=True,text=True).stdout)
print("ALLDONE_R4")
