#!/usr/bin/env python3
# ============================================================================
# diagnose_encoders.py — SYSTEMATIC "what doesn't fire correctly" audit.
# For every corpus file it asks mzip (MZIP_STATS=1) which encoder actually fired, then flags:
#   LOSS               — mzip lost to a standard compressor.
#   MISSED-SPECIAL     — a specialized encoder was EXPECTED for this type but a general/backstop one fired
#                        (this is the TypeScript/CSV pattern: the detector didn't trigger).
#   BACKSTOP-RELIANT   — mzip won only via the brotli/xz backstop (its OWN tech lost — investigate).
#   WEAK-SPECIAL       — the right specialized encoder fired but still didn't beat the best standard.
# Re-run anytime (after adding files/encoders) to find new non-firing cases across the whole corpus.
# Prereqs: bash build_evals.sh   |   Run: python3 diagnose_encoders.py   |   Out: encoder_audit.md
# ============================================================================
import subprocess, glob, os, shutil

ROOT=os.path.dirname(os.path.abspath(__file__)); os.chdir(ROOT)
def find_xz():
    b=shutil.which("brotli"); c=[os.path.join(os.path.dirname(b),"xz.exe")] if b else []
    c+=["/mingw64/bin/xz.exe"]
    for x in c:
        try:
            if subprocess.run([x,"-9","-e","-c",__file__],capture_output=True).stdout: return x
        except Exception: pass
    return None
XZ=find_xz()
def best_std(f):
    s={}
    s["gzip-9"]=len(subprocess.run(["gzip","-9","-c",f],capture_output=True).stdout)
    s["bzip2-9"]=len(subprocess.run(["bzip2","-9","-c",f],capture_output=True).stdout)
    s["brotli-11"]=len(subprocess.run(["brotli","-q","11","-c",f],capture_output=True).stdout)
    if XZ: s["xz-9e"]=len(subprocess.run([XZ,"-9","-e","-c",f],capture_output=True).stdout)
    try: s["zstd-19"]=int(subprocess.run(["./zc.exe",f,"19"],capture_output=True,text=True).stdout)
    except: pass
    s={k:v for k,v in s.items() if v>0}
    bk=min(s,key=s.get); return bk,s[bk]
def mzip_fire(f):
    out=f".d_{os.getpid()}.mz"
    r=subprocess.run(["./mzip_cm.exe","c",f,out],capture_output=True,env={**os.environ,"MZIP_STATS":"1"})
    sz=os.path.getsize(out) if os.path.exists(out) else 0
    if os.path.exists(out): os.remove(out)
    enc={}
    for ln in r.stderr.decode("latin1").splitlines():
        if ln.startswith("MZSTATS"):
            _,name,orig,comp=ln.split("\t"); enc[name]=enc.get(name,0)+int(comp)
    chosen=max(enc,key=enc.get) if enc else "?"   # dominant encoder by bytes
    return chosen, sz, enc

GENERAL={"RAW","TEXT","STRUCTURED","BWT_TEXT","CM_TEXT","ZSTD_DICT","LZMA_RAW","LZMA_OPTIMAL","WORD_ENCODED","INCOMPRESSIBLE","UNKNOWN"}
BACKSTOP={"BROTLI","XZLIB"}
EXPECT={  # type -> specialized encoders that SHOULD fire (else MISSED-SPECIAL)
 "CSV":{"CSV_COLUMNAR","COLUMNAR"}, "Log":{"COLUMNAR","CHAR_TEMPLATE","LINE_GROUP_TPL","LINE_TEMPLATE"},
 "JSON":{"JSON_COLUMNAR"}, "HTML":{"HTML_STREAM"}, "XML":{"HTML_STREAM"},
 "YAML":{"KV_CONFIG"}, "TOML":{"KV_CONFIG"}, "INI":{"KV_CONFIG"},
 "Numeric":{"LINEAR_GEN","NUMERIC","DELTA","TIMESTAMP","LINEAR_GEN_APPROX","PERIODIC","GEOMETRIC","RECURRENCE","MODULAR","SPARSE","QUADRATIC"},
 "TypeScript":{"ML_TEMPLATE_DUAL","ML_TEMPLATE","SECTION_TEMPLATE","TEMPLATE"},
 "SQL":{"LINE_TEMPLATE","TEMPLATE"}, "Markdown":{"SECTION_TEMPLATE","TEMPLATE","LINE_GROUP_TPL"},
}
RB="real_bench/"; EX="corpus_extra/"; SM="samples/64k/input/"
CORPUS={  # type -> files (mix of real + the new real numeric/TS + a few synth for coverage)
 "HTML":[RB+"dashboard.html"], "CSS":[RB+"bootstrap.css",RB+"styles.css"],
 "JavaScript":[RB+"lodash.js"], "TypeScript":[RB+"handlers.ts",RB+"vscode_main.ts"]+sorted(glob.glob(EX+"ts/*")),
 "JSON":[RB+"users.json",RB+"json_github_api.json"], "XML":[RB+"xml_maven.xml"],
 "YAML":[RB+"k8s_deployments.yaml",RB+"docker-compose.yml"], "SQL":[RB+"sql_schema.sql",RB+"users_dump.sql"],
 "CSV":[RB+"events.csv"], "Markdown":[RB+"api_docs.md",RB+"readme_large.md"],
 "Log":[RB+"apache_log_sample.log",RB+"app.log",RB+"nginx_access.log"],
 "C/C++":[RB+"linux_kernel.c"], "Python":[RB+"django_models.py"], "Java":[RB+"java_arraylist.java"], "Go":[RB+"go_http.go"],
 "Other-langs":[RB+f for f in ("rust_lib.rs","clojure_core.clj","ruby_rails.rb","swift_stdlib.swift")],
 "Numeric":sorted(glob.glob(EX+"*.bin")),
 "TOML":[SM+"toml_config.toml"], "INI":[SM+"ini_config.ini"], "GraphQL":[SM+"graphql.graphql"], "Email":[SM+"email_headers.txt"],
}

issues=[]; ok=0; total=0
for typ,files in CORPUS.items():
    for f in files:
        if not (os.path.isfile(f) and os.path.getsize(f)>0): continue
        total+=1
        chosen,mz,enc=mzip_fire(f); bt,bs=best_std(f)
        gap=(bs-mz)/bs*100 if bs else 0     # +ve = mzip smaller (good)
        cat="BACKSTOP" if chosen in BACKSTOP else ("GENERAL" if chosen in GENERAL else "SPECIAL")
        exp=EXPECT.get(typ)
        flags=[]
        if mz>bs+32: flags.append(f"LOSS vs {bt} ({gap:+.1f}%)")
        if chosen in BACKSTOP: flags.append(f"BACKSTOP-RELIANT ({chosen})")
        if exp and chosen not in exp and cat!="SPECIAL": flags.append(f"MISSED-SPECIAL (want {'/'.join(sorted(exp))}, got {chosen})")
        if exp and chosen in exp and mz>bs+32: flags.append(f"WEAK-SPECIAL ({chosen} fired but ties/loses {bt})")
        if flags: issues.append((typ,os.path.basename(f),chosen,gap,bt," | ".join(flags)))
        else: ok+=1

# severity: LOSS > MISSED-SPECIAL > BACKSTOP > WEAK
def sev(s):
    return (0 if "LOSS" in s[5] else 1 if "MISSED-SPECIAL" in s[5] else 2 if "BACKSTOP" in s[5] else 3, -abs(s[3]))
issues.sort(key=sev)
L=["# Encoder-firing audit — what doesn't fire correctly (systematic)\n",
   f"{total} files. {ok} clean; {len(issues)} flagged. Flags: LOSS, MISSED-SPECIAL (a specialized encoder was "
   f"expected for the type but a general/backstop one fired — the TypeScript/CSV pattern), BACKSTOP-RELIANT "
   f"(won only via brotli/xz — own tech lost), WEAK-SPECIAL (right encoder fired but still ties/loses).\n",
   "| sev | type | file | fired encoder | mzip vs best-std | issue |","|---|---|---|---|--:|---|"]
for typ,fn,chosen,gap,bt,fl in issues:
    sv = "LOSS" if "LOSS" in fl else ("MISS" if "MISSED" in fl else ("BACK" if "BACKSTOP" in fl else "WEAK"))
    L.append(f"| {sv} | {typ} | {fn} | {chosen} | {gap:+.1f}% | {fl} |")
open("encoder_audit.md","w",encoding="utf-8").write("\n".join(L))
from collections import Counter
miss=Counter(t for t,_,_,_,_,fl in issues if "MISSED-SPECIAL" in fl)
back=Counter(t for t,_,_,_,_,fl in issues if "BACKSTOP" in fl)
loss=[t for t,_,_,_,_,fl in issues if "LOSS" in fl]
print(f"files={total} clean={ok} flagged={len(issues)}")
print(f"LOSS types: {loss}")
print(f"MISSED-SPECIAL by type: {dict(miss)}")
print(f"BACKSTOP-RELIANT by type: {dict(back)}")
print("full -> encoder_audit.md")
