#!/usr/bin/env python3
"""Independent adversarial verification of the whisper-lv3 mzip win.
Sizes only, no timing. Every size round-trip verified with sha256."""
import hashlib, json, os, re, shutil, subprocess, sys

B="/root/mzip-hfbench/build"
SCR="/root/mzip-hfbench/advwhisper"
shutil.rmtree(SCR, ignore_errors=True); os.makedirs(SCR, exist_ok=True)

def run(c, **k): return subprocess.run(c, capture_output=True, text=True, **k)
def sha(p):
    h=hashlib.sha256()
    with open(p,"rb") as f:
        for b in iter(lambda: f.read(1<<20), b""): h.update(b)
    return h.hexdigest()

def code(coder, src, d, tagi):
    """returns (size, roundtrip_ok)"""
    o=f"{d}/{tagi}.c"; r=f"{d}/{tagi}.r"
    if coder=="mzip":
        run([f"{B}/mzip_fixed","c",src,o,"19"])
        if not os.path.exists(o): return None,False
        run([f"{B}/mzip_fixed","d",o,r])
    elif coder=="lpaq1":
        run([f"{B}/lpaq_x","9",src,o])
        if not os.path.exists(o): return None,False
        run([f"{B}/lpaq_x","d",o,r])
    elif coder=="zpaq-m5":
        a=f"{d}/{tagi}.zpaq"
        run(["zpaq","a",a,os.path.basename(src),"-m5"],cwd=d)
        if not os.path.exists(a): return None,False
        run(["zpaq","x",a,os.path.basename(src),"-to",r],cwd=d)
        o=a
    elif coder=="xz-9e":
        with open(o,"wb") as fo: subprocess.run(["xz","-9e","-T1","-c",src],stdout=fo,stderr=subprocess.DEVNULL)
        with open(r,"wb") as fo: subprocess.run(["xz","-d","-c",o],stdout=fo,stderr=subprocess.DEVNULL)
    if not os.path.exists(r): return os.path.getsize(o), False
    ok = sha(src)==sha(r)
    sz = os.path.getsize(o)
    os.remove(r)
    return sz, ok

RES=[]
for fam,path in (("A","/root/mzip-hfbench/weights2/whisper-lv3.bin"),
                 ("B","/root/mzip-hfbench/weightsB/whisper-lv3.bin")):
    d=f"{SCR}/{fam}"; os.makedirs(d,exist_ok=True)
    src=f"{d}/f"; shutil.copyfile(path,src)
    rawsha=sha(src); rawlen=os.path.getsize(src)
    # ---- native ----
    for coder in ["mzip","lpaq1","zpaq-m5","xz-9e"]:
        sz,ok=code(coder,src,d,f"nat_{coder}")
        RES.append(dict(fam=fam,mode="native",coder=coder,size=sz,rt=ok,raw=rawlen,sha=rawsha))
        print(json.dumps(RES[-1]),flush=True)
    # ---- 2-plane split (F16) ----
    blob=open(src,"rb").read()
    w=2
    planes=[blob[k::w] for k in range(w)]
    for coder in ["mzip","lpaq1","zpaq-m5","xz-9e"]:
        tot=0; recon=[None]*w; ok=True
        for i,pl in enumerate(planes):
            sp=f"{d}/sp{coder}_{i}"; open(sp,"wb").write(pl)
            o=f"{d}/spp_{coder}_{i}.c"; r=f"{d}/spp_{coder}_{i}.r"
            if coder=="mzip":
                run([f"{B}/mzip_fixed","c",sp,o,"19"])
                if not os.path.exists(o): ok=False;break
                run([f"{B}/mzip_fixed","d",o,r])
            elif coder=="lpaq1":
                run([f"{B}/lpaq_x","9",sp,o])
                if not os.path.exists(o): ok=False;break
                run([f"{B}/lpaq_x","d",o,r])
            elif coder=="zpaq-m5":
                a=f"{d}/spz_{coder}_{i}.zpaq"
                run(["zpaq","a",a,os.path.basename(sp),"-m5"],cwd=d)
                if not os.path.exists(a): ok=False;break
                run(["zpaq","x",a,os.path.basename(sp),"-to",r],cwd=d); o=a
            elif coder=="xz-9e":
                with open(o,"wb") as fo: subprocess.run(["xz","-9e","-T1","-c",sp],stdout=fo,stderr=subprocess.DEVNULL)
                with open(r,"wb") as fo: subprocess.run(["xz","-d","-c",o],stdout=fo,stderr=subprocess.DEVNULL)
            if not os.path.exists(r): ok=False;break
            back=open(r,"rb").read()
            if back!=pl: ok=False;break
            tot+=os.path.getsize(o); recon[i]=back
            os.remove(r); os.remove(sp)
        size=None
        if ok:
            out=bytearray(rawlen)
            for i in range(w): out[i::w]=recon[i]
            ok = hashlib.sha256(bytes(out)).hexdigest()==rawsha
            size=tot
        RES.append(dict(fam=fam,mode="split2",coder=coder,size=size,rt=ok,raw=rawlen,sha=rawsha))
        print(json.dumps(RES[-1]),flush=True)

json.dump(RES,open(f"{SCR}/results.json","w"),indent=1)
print("ADVWHISPER_DONE")
