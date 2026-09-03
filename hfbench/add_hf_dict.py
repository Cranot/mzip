#!/usr/bin/env python3
"""Add DICT_HF_REAL (id 13), trained on real HuggingFace config/tokenizer JSON,
to a SCRATCH copy of the tree; build; measure held-out; run the suite.
Measured before this change: zstd-19 + this dict = 44,150 B on 144 held-out files
vs native mzip 60,288 (26.8% smaller) vs brotli 63,528.
Changes (scratch only): mzip_dicts.h gets the array + ALL_DICTS entry;
emit_dicts_header.cpp gets the news[] entry + table line; train_corpus/hf_dict.bin
is dropped in so a regeneration reproduces it. Never touches /root/mzip."""
import os, shutil, subprocess, sys
SRC="/root/mzip"; T="/root/mzip-hfbench/dicttree"; B="/root/mzip-hfbench/build"; H="/root/mzip-hfbench/hfdict"
DICT=f"{H}/hf_dict_16384.bin"
if os.path.exists(T): shutil.rmtree(T)
os.makedirs(f"{T}/ppmd"); os.makedirs(f"{T}/zstd_release/zstd-v1.5.6-win64/include"); os.makedirs(f"{T}/train_corpus")
for f in os.listdir(SRC):
    if f.endswith((".hpp",".h",".cpp",".c")): shutil.copy2(f"{SRC}/{f}",f"{T}/{f}")
for f in os.listdir(f"{SRC}/ppmd"): shutil.copy2(f"{SRC}/ppmd/{f}",f"{T}/ppmd/{f}")
os.symlink("/usr/include/zstd.h",f"{T}/zstd_release/zstd-v1.5.6-win64/include/zstd.h")
shutil.copy2(DICT,f"{T}/train_corpus/hf_dict.bin")
d=open(DICT,"rb").read(); assert len(d)==16384

# 1) mzip_dicts.h: array + table entry
P=f"{T}/mzip_dicts.h"; s=open(P,encoding="utf-8",errors="surrogateescape").read()
anchor="struct DictEntry { uint8_t id; const uint8_t* data; size_t size; };"
assert s.count(anchor)==1
arr="static const uint8_t DICT_HF_REAL[16384] = {\n  "+"".join(f"0x{b:02x},"+("\n  " if (i+1)%16==0 else " ") for i,b in enumerate(d))+"\n};\nstatic const size_t DICT_HF_REAL_SIZE = 16384;\n\n"
s=s.replace(anchor, "// ID 13: real-data dict trained on HuggingFace model-repo metadata JSON (config.json,\n// tokenizer_config.json, generation_config.json, special_tokens_map.json) from the\n// top-120 models by downloads, 2026-09-02. Held-out: zstd-19+dict 44,150 B vs mzip 60,288.\n"+arr+anchor)
tbl="    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\n};"
assert s.count(tbl)==1
s=s.replace(tbl,"    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\n    {13, DICT_HF_REAL,  DICT_HF_REAL_SIZE},\n};")
open(P,"w",encoding="utf-8",errors="surrogateescape").write(s); print("mzip_dicts.h: +DICT_HF_REAL id 13")

# 2) emitter source, for reproducibility
E=f"{T}/emit_dicts_header.cpp"; e=open(E,encoding="utf-8",errors="surrogateescape").read()
a1='        {"DICT_JSON_REAL", "train_corpus/json_dict.bin", 12},\n'
a2='    fprintf(f, "    {12, DICT_JSON_REAL,DICT_JSON_REAL_SIZE},\\n");\n'
assert e.count(a1)==1 and e.count(a2)==1, (e.count(a1),e.count(a2))
e=e.replace(a1,a1+'        {"DICT_HF_REAL",   "train_corpus/hf_dict.bin",   13},\n')
e=e.replace(a2,a2+'    fprintf(f, "    {13, DICT_HF_REAL,  DICT_HF_REAL_SIZE},\\n");\n')
open(E,"w",encoding="utf-8",errors="surrogateescape").write(e); print("emit_dicts_header.cpp: +entry 13")

# 3) build + suite
OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name,srcf,extra in (("mzip_dict","mzip_cli.cpp",""),("ut_dict","mzip_unit_tests.cpp","-D_USE_MATH_DEFINES")):
    r=subprocess.run(f"cd {T} && g++ -O3 -std=c++17 -march=native {extra} -I{T} -o {B}/{name} {srcf} {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}",[l for l in r.stderr.splitlines() if ' error' in l][:4])
    if r.returncode!=0: sys.exit(1)
r=subprocess.run(f"{B}/ut_dict",capture_output=True,text=True); print([l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])

# 4) held-out measurement: new vs current native vs brotli; count how often dict 13 is chosen
def tot(binp):
    t=0; wins13=0
    for f in sorted(os.listdir(f"{H}/test")):
        p=f"{H}/test/{f}"; o="/tmp/hd.mz"
        pr=subprocess.run([binp,"c",p,o,"1"],capture_output=True,text=True,env={**os.environ,"MZIP_STATS":"1"})
        t+=os.path.getsize(o); wins13+=int("ZSTD_DICT" in pr.stderr)
        subprocess.run([binp,"d",o,"/tmp/hd.rt"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        assert open("/tmp/hd.rt","rb").read()==open(p,"rb").read(), f"ROUNDTRIP FAIL {f}"
    return t,wins13
old,_=tot(f"{B}/mzip_fixed3"); new,w=tot(f"{B}/mzip_dict")
print(f"\nheld-out 144 files: mzip native {old:,}  ->  with DICT_HF_REAL {new:,}  ({100*(old-new)/old:+.2f}%); ZSTD_DICT chosen on {w}/144; brotli-11 63,528; zstd-19+dict standalone 44,150")
# 5) cross-decode: STOCK decoder must REJECT id 13 cleanly (unknown dict) -- expected, format-versioning note
pr=subprocess.run([f"{B}/mzip_fixed3","d","/tmp/hd.mz","/tmp/hd.x"],capture_output=True,text=True)
print("stock decoder on a dict-13 archive:", (pr.stderr.strip() or pr.stdout.strip() or "decoded?!")[:100])
print("ALLDONE_DICT")
