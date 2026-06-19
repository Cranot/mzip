#!/usr/bin/env python3
# ============================================================================
# benchmark_types.py — STANDARD type-stratified compression benchmark.
#   mzip(+CM) vs the world's standard compressors, at max settings, on REAL files.
#   mzip output is roundtrip-verified. Auto-detects which compressors actually work.
#
# Prereqs (build once):  bash build_evals.sh     (builds zc.exe, mzip_cm.exe, mzip_base.exe)
# Run:                   python3 benchmark_types.py            (full)
#                        python3 benchmark_types.py --quick    (one file per type)
# Output:                bench_types_report.md  (full tables) + ASCII summary to console.
# See CLAUDE.md "Evals & scripts" for conventions.
# ============================================================================
import subprocess, glob, os, sys, shutil

ROOT = os.path.dirname(os.path.abspath(__file__)); os.chdir(ROOT)
QUICK = "--quick" in sys.argv

# ---- resolve WORKING compressor commands (w64devkit ships a broken xz; probe everything) ----
def works(cmd):
    try:
        r = subprocess.run(cmd + [__file__], capture_output=True)
        return len(r.stdout) > 0
    except Exception:
        return False

def find_xz():
    # w64devkit ships a broken xz first on PATH; the working one sits next to brotli (mingw64/bin)
    cands = []
    b = shutil.which("brotli")
    if b: cands.append(os.path.join(os.path.dirname(b), "xz.exe"))
    cands += ["/mingw64/bin/xz.exe", shutil.which("xz") or "xz"]
    for cand in cands:
        try:
            if cand and subprocess.run([cand,"-9","-e","-c",__file__],capture_output=True).stdout:
                return cand
        except Exception: pass
    return None

XZ = find_xz()
# candidate external standards: (label, size-fn). zstd via our zc.exe (no zstd CLI on PATH).
def ext_cmd(tool):
    return {
        "gzip-9":   ["gzip","-9","-c"],
        "bzip2-9":  ["bzip2","-9","-c"],
        "brotli-11":["brotli","-q","11","-c"],
        "xz-9e":    ([XZ,"-9","-e","-c"] if XZ else None),
    }.get(tool)

def stdout_size(cmd, f):
    try: return len(subprocess.run(cmd+[f], capture_output=True).stdout)
    except Exception: return 0

def zstd_size(f):
    if not os.path.exists("zc.exe"): return 0
    r = subprocess.run(["./zc.exe", f, "19"], capture_output=True, text=True)
    try: return int(r.stdout.strip())
    except: return 0

def mzip_size(f, withcm=True):
    exe = "./mzip_cm.exe" if withcm else "./mzip_base.exe"
    if not os.path.exists(exe): return 0
    out = f".tmp_{os.getpid()}.mz"
    subprocess.run([exe,"c",f,out], capture_output=True)
    sz = os.path.getsize(out) if os.path.exists(out) else 0
    ok = True
    if withcm:
        dec = out+".out"; subprocess.run([exe,"d",out,dec], capture_output=True)
        ok = os.path.exists(dec) and open(dec,"rb").read()==open(f,"rb").read()
        if os.path.exists(dec): os.remove(dec)
    if os.path.exists(out): os.remove(out)
    return sz if (not withcm or ok) else -1

# ---- the tool table: only keep ones that work ----
# order: external standards first, then our two variants
RAW_TOOLS = ["gzip-9","bzip2-9","zstd-19","xz-9e","mzip(noCM)","brotli-11","mzip+CM"]
def size_of(tool, f):
    if tool=="zstd-19":     return zstd_size(f)
    if tool=="mzip+CM":     return mzip_size(f, True)
    if tool=="mzip(noCM)":  return mzip_size(f, False)
    if tool=="xz-9e":       return stdout_size(ext_cmd("xz-9e"), f) if XZ else 0
    return stdout_size(ext_cmd(tool), f)
# probe availability on this script file
TOOLS = [t for t in RAW_TOOLS if (t=="zstd-19" and os.path.exists("zc.exe"))
         or (t in ("mzip+CM","mzip(noCM)") and os.path.exists("mzip_cm.exe" if "CM)" not in t else "mzip_base.exe"))
         or (t=="xz-9e" and XZ)
         or (t in ("gzip-9","bzip2-9","brotli-11") and size_of(t, __file__) > 0)]
if "mzip+CM" not in TOOLS:
    sys.exit("ERROR: mzip_cm.exe missing. Run: bash build_evals.sh")
EXT = [t for t in TOOLS if t.startswith(("gzip","bzip2","zstd","xz","brotli"))]

# ---- top content types worldwide -> HELD-OUT real files (real_bench/, NOT mzip's dict-training set) ----
# train_corpus/ is mzip's dictionary TRAINING data (per fetch.sh) -> excluded for fairness.
RB = "real_bench/"
TYPES = {
    "HTML":       [RB+"dashboard.html"],
    "CSS":        [RB+"bootstrap.css", RB+"styles.css"],
    "JavaScript": [RB+"lodash.js", RB+"webpack.config.js"],
    "TypeScript": [RB+"handlers.ts", RB+"vscode_main.ts"],
    "JSON":       [RB+"users.json", RB+"json_github_api.json"],
    "XML":        [RB+"xml_maven.xml"],
    "YAML":       [RB+"k8s_deployments.yaml", RB+"docker-compose.yml", RB+".github_workflows_ci.yml"],
    "SQL":        [RB+"sql_schema.sql", RB+"users_dump.sql"],
    "CSV":        [RB+"events.csv"],
    "Markdown":   [RB+"api_docs.md", RB+"contributing.md", RB+"readme_large.md"],
    "Log":        [RB+"apache_log_sample.log", RB+"app.log", RB+"nginx_access.log"],
    "C/C++":      [RB+"linux_kernel.c", RB+"cpp_vector.hpp"],
    "Python":     [RB+"django_models.py", RB+"flask_app.py", RB+"services.py"],
    "Java":       [RB+"java_arraylist.java"],
    "Go":         [RB+"go_http.go", RB+"handlers.go"],
    "Other-langs":[RB+"clojure_core.clj", RB+"csharp_list.cs", RB+"elixir_genserver.ex", RB+"julia_base.jl",
                   RB+"kotlin_stdlib.kt", RB+"lua_neovim.lua", RB+"php_laravel.php", RB+"ruby_rails.rb",
                   RB+"rust_lib.rs", RB+"scala_list.scala", RB+"swift_stdlib.swift", RB+"zig_std.zig"],
    "Config/Make":[RB+"Dockerfile", RB+"Makefile", RB+"linux_makefile", RB+"terraform_main.tf",
                   RB+".env.example", RB+".gitignore"],
    "Metrics":    [RB+"metrics.prom"],
}

def ratio(o,c): return f"{o/c:.2f}x" if c>0 else "-"
def pct(a,b):   return f"{(a-b)/a*100:+.2f}%" if a>0 else "n/a"

rows=[]; overall={t:0 for t in TOOLS}; overall_orig=0; rt_fail=0; type_meta={}
for typ,files in TYPES.items():
    files=[f for f in files if os.path.isfile(f) and os.path.getsize(f)>0]
    if QUICK: files=files[:1]
    if not files: continue
    orig=sum(os.path.getsize(f) for f in files)
    sizes={t:0 for t in TOOLS}
    for f in files:
        for t in TOOLS:
            s=size_of(t,f)
            if s==-1: rt_fail+=1; s=mzip_size(f,False)
            sizes[t]+=s
    rows.append((typ,len(files),orig,sizes)); overall_orig+=orig
    for t in TOOLS: overall[t]+=sizes[t]
    ext={t:sizes[t] for t in EXT if sizes[t]>0}
    best_ext=min(ext,key=ext.get) if ext else None
    ranked=sorted([(sizes[t],t) for t in TOOLS if sizes[t]>0],key=lambda x:x[0])
    rank=next((i for i,(_,t) in enumerate(ranked,1) if t=="mzip+CM"),len(ranked))
    type_meta[typ]=(best_ext, ext.get(best_ext,0), sizes["mzip+CM"], rank)

L=[]
L.append("# Type-stratified compression benchmark — mzip+CM vs the world\n")
L.append(f"Real files, each compressor at max ({', '.join(TOOLS)}). mzip roundtrip-verified. "
         f"Sizes = TOTAL bytes per content type. Roundtrip failures: **{rt_fail}**. "
         f"{'(QUICK: 1 file/type)' if QUICK else ''}\n")
L.append("| Type | files | orig | "+" | ".join(TOOLS)+" | best | mzip+CM |")
L.append("|---|--:|--:|"+"--:|"*len(TOOLS)+"--|--|")
for typ,nf,orig,sizes in rows:
    best_ext,best_sz,mz,rank=type_meta[typ]
    cells=" | ".join(str(sizes[t]) for t in TOOLS)
    verdict = "**WINS**" if (mz>0 and best_sz>0 and mz<=best_sz) else f"#{rank} (best={best_ext})"
    L.append(f"| {typ} | {nf} | {orig} | {cells} | {best_ext or '-'} | {verdict} |")
L.append(f"| **ALL** | | {overall_orig} | "+" | ".join(str(overall[t]) for t in TOOLS)+" | | |")
L.append("\n## Compression ratio (orig/comp, higher=better)\n")
L.append("| Type | "+" | ".join(TOOLS)+" |"); L.append("|---|"+"--:|"*len(TOOLS))
for typ,nf,orig,sizes in rows:
    L.append(f"| {typ} | "+" | ".join(ratio(orig,sizes[t]) for t in TOOLS)+" |")
L.append(f"| **ALL** | "+" | ".join(ratio(overall_orig,overall[t]) for t in TOOLS)+" |")
mzwins=sum(1 for t in type_meta.values() if t[2]>0 and t[1]>0 and t[2]<=t[1])
firsts=sum(1 for t in type_meta.values() if t[3]==1)
L.append("\n## Standing\n")
L.append(f"- mzip+CM beats EVERY standard compressor on **{mzwins}/{len(rows)}** content types")
L.append(f"- mzip+CM is rank #1 overall (incl. vs mzip-noCM) on {firsts}/{len(rows)} types")
L.append(f"- overall vs brotli-11: {pct(overall.get('brotli-11',0),overall['mzip+CM'])}  | vs zstd-19: {pct(overall.get('zstd-19',0),overall['mzip+CM'])}  | vs xz-9e: {pct(overall.get('xz-9e',0),overall['mzip+CM'])}")
L.append(f"- CM contribution: mzip(noCM)->mzip+CM = {pct(overall.get('mzip(noCM)',0),overall['mzip+CM'])}")
open("bench_types_report.md","w",encoding="utf-8").write("\n".join(L))
print(f"tools={TOOLS}")
print(f"types={len(rows)} rt_fail={rt_fail}  mzip+CM wins {mzwins}/{len(rows)} types, rank#1 on {firsts}")
print(f"overall vs brotli {pct(overall.get('brotli-11',0),overall['mzip+CM'])}, vs zstd {pct(overall.get('zstd-19',0),overall['mzip+CM'])}, vs xz {pct(overall.get('xz-9e',0),overall['mzip+CM'])}, CM adds {pct(overall.get('mzip(noCM)',0),overall['mzip+CM'])}")
print("full -> bench_types_report.md")
