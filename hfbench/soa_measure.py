#!/usr/bin/env python3
"""Does the native small-block SoA trial (build/mzip_soa) reproduce the external
bg4+mzip win at 64K? Sizes only, every chunk round-trip verified, with a
sha256 cross-check that the STOCK decoder decodes mzip_soa output (format
must be unchanged)."""
import os, json, subprocess, hashlib
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"
SOA="/root/mzip-hfbench/build/mzip_soa"; STOCK="/root/mzip-hfbench/build/mzip_fixed"
CS=64*1024
INPUTS=[("MiniLM-L6-v2",f"{W}/MiniLM-L6-v2.slice"),("gpt2",f"{W}/gpt2.slice"),("bert-base",f"{W}/bert-base.slice"),
 ("bge-small",f"{W}/bge-small.slice"),("weights-fp32",f"{W}/weights-fp32.bin"),("weights-bf16",f"{W}/weights-bf16.bin"),
 ("Qwen2.5-0.5B",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice"),("gguf-q8_0",f"{W}/gguf-q8_0.slice")]
cb={r["tag"].split(" ")[0]:r for r in json.load(open(f"{OUT}/chunkbench.json")) if r["chunk"]==CS}
bm={r["tag"]:r for r in json.load(open(f"{OUT}/bg4_mzip.json"))}
rows=[]
print(f"{'slice':13s} {'plain mzip':>11s} {'ext bg4+mzip':>12s} {'NATIVE soa':>11s} | native vs plain  native vs ext  stock-decodes")
for tag,p in INPUTS:
    if not os.path.exists(p): continue
    raw=open(p,"rb").read(); N=len(raw); tot=0; bad=0; xdec_ok=0; nch=0
    for i in range(0,N,CS):
        b=raw[i:i+CS]; ip,op,rp,xp=f"{OUT}/s.in",f"{OUT}/s.mz",f"{OUT}/s.rt",f"{OUT}/s.x"
        open(ip,"wb").write(b)
        subprocess.run([SOA,"c",ip,op,"1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        s=os.path.getsize(op) if os.path.exists(op) else -1; tot+=max(s,0)
        subprocess.run([SOA,"d",op,rp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        ok=os.path.exists(rp) and open(rp,"rb").read()==b; bad+=int(not ok or s<0)
        subprocess.run([STOCK,"d",op,xp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)   # stock decoder on new output
        xdec_ok+=int(os.path.exists(xp) and open(xp,"rb").read()==b); nch+=1
        for f in (ip,op,rp,xp):
            if os.path.exists(f): os.remove(f)
    plain=cb.get(tag,{}).get("mzip"); ext=bm.get(tag,{}).get("bg4_mzip")
    rows.append(dict(tag=tag,n=N,soa=tot,bad=bad,stock_decodes=xdec_ok,nchunks=nch,plain=plain,ext=ext))
    vp=f"{100*(plain-tot)/plain:+6.2f}%" if plain else "   n/a"; ve=f"{100*(ext-tot)/ext:+6.2f}%" if ext else "   n/a"
    print(f"{tag:13s} {plain if plain else 0:>11,} {ext if ext else 0:>12,} {tot:>11,} | {vp:>15s}  {ve:>13s}  {xdec_ok}/{nch}{'  RT_FAIL x%d'%bad if bad else ''}",flush=True)
json.dump(rows,open(f"{OUT}/soa_measure.json","w"),indent=1)
print("ALLDONE_SOAM")
