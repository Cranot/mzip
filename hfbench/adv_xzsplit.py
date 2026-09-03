import hashlib,json,os,shutil,subprocess
SCR="/root/mzip-hfbench/advxzsplit"; shutil.rmtree(SCR,ignore_errors=True); os.makedirs(SCR)
R=[]
for fam,base in (("A","/root/mzip-hfbench/weights2"),("B","/root/mzip-hfbench/weightsB")):
    for tag in ("sdxl-unet","bloom-560m"):
        p=f"{base}/{tag}.bin"; raw=open(p,"rb").read()
        d=f"{SCR}/{fam}{tag}"; os.makedirs(d)
        tot=0; rec=[None,None]; ok=True
        for i in range(2):
            pl=raw[i::2]; sp=f"{d}/p{i}"; open(sp,"wb").write(pl)
            o=f"{d}/p{i}.xz"; r=f"{d}/p{i}.r"
            with open(o,"wb") as fo: subprocess.run(["xz","-9e","-T1","-c",sp],stdout=fo,stderr=subprocess.DEVNULL)
            with open(r,"wb") as fo: subprocess.run(["xz","-d","-c",o],stdout=fo,stderr=subprocess.DEVNULL)
            back=open(r,"rb").read()
            if back!=pl: ok=False; break
            tot+=os.path.getsize(o); rec[i]=back; os.remove(r); os.remove(sp)
        size=None
        if ok:
            out=bytearray(len(raw))
            for i in range(2): out[i::2]=rec[i]
            ok = hashlib.sha256(bytes(out)).hexdigest()==hashlib.sha256(raw).hexdigest()
            size=tot
        R.append(dict(fam=fam,tag=tag,coder="xz-9e",mode="split2",size=size,rt=ok))
        print(json.dumps(R[-1]),flush=True)
print("XZSPLIT_DONE")
