#!/usr/bin/env python3
"""Recover the round-4 verification that tail -22 discarded: builds exist, suite,
held-out fixed3 vs fixed4, byte-identity on inputs the dict trial cannot touch."""
import os, subprocess, hashlib, time
B="/root/mzip-hfbench/build"; H="/root/mzip-hfbench/hfdict"
for n in ("mzip_fixed4","ut_repo4"):
    p=f"{B}/{n}"; print(f"{n}: {'present' if os.path.exists(p) else 'MISSING'} {time.strftime('%H:%M', time.localtime(os.path.getmtime(p))) if os.path.exists(p) else ''}")
r=subprocess.run(f"{B}/ut_repo4",capture_output=True,text=True); print("suite:",[l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])
def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
t3=t4=0; ok=True
for f in sorted(os.listdir(f"{H}/test")):
    p=f"{H}/test/{f}"
    subprocess.run([f"{B}/mzip_fixed3","c",p,"/tmp/v4.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); t3+=os.path.getsize("/tmp/v4.a")
    subprocess.run([f"{B}/mzip_fixed4","c",p,"/tmp/v4.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); t4+=os.path.getsize("/tmp/v4.b")
    subprocess.run([f"{B}/mzip_fixed4","d","/tmp/v4.b","/tmp/v4.rt"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    ok=ok and open("/tmp/v4.rt","rb").read()==open(p,"rb").read()
print(f"held-out 144: fixed3 {t3:,} -> fixed4 {t4:,} ({100*(t3-t4)/t3:+.2f}%)  round-trips {'ALL OK' if ok else 'FAIL'}")
print("byte-identity fixed3 vs fixed4 (>256 KB single blocks, dict trial cannot fire):")
for f in ("corpora/qa-gsm8k.bin","corpora/tabular-numeric.bin","weights/weights-bf16.bin"):
    subprocess.run([f"{B}/mzip_fixed3","c",f"/root/mzip-hfbench/{f}","/tmp/v4.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run([f"{B}/mzip_fixed4","c",f"/root/mzip-hfbench/{f}","/tmp/v4.b","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    print(f"  {f.split('/')[-1]:22s} {'IDENTICAL' if sha('/tmp/v4.a')==sha('/tmp/v4.b') else 'DIFFERENT'}")
for x in ("/tmp/v4.a","/tmp/v4.b","/tmp/v4.rt"):
    if os.path.exists(x): os.remove(x)
print(subprocess.run("cd /root/mzip && git diff --stat && ls -l train_corpus/hf_dict.bin mzip.hpp.bak-* mzip_dicts.h.bak-* emit_dicts_header.cpp.bak-* 2>/dev/null | wc -l | xargs echo backups:",shell=True,capture_output=True,text=True).stdout)
print("ALLDONE_V4")
