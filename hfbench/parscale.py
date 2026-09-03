#!/usr/bin/env python3
"""Chunk-level parallel scaling. Chunks are independent, so mzip's throughput at
HF's 64 KiB granularity should scale ~linearly with cores. Measure wall-clock
for compressing all 128 chunks of a slice with N workers, N in {1,2,4,6,11}.
RUN ONLY ON A QUIET BOX. Uses mzip_fixed2 (native SoA). Reports MB/s per N and
scaling efficiency vs N=1. Round-trips one chunk per worker as a sanity check."""
import os, sys, time, json, subprocess, statistics
import concurrent.futures as cf
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"; MZ="/root/mzip-hfbench/build/mzip_fixed2"
CS=64*1024; REPS=int(sys.argv[1]) if len(sys.argv)>1 else 2
SLICES=[("weights-bf16",f"{W}/weights-bf16.bin"),("MiniLM F32",f"{W}/MiniLM-L6-v2.slice")]
def one(args):
    i,b=args; ip,op=f"{OUT}/ps{i}.in",f"{OUT}/ps{i}.mz"; open(ip,"wb").write(b)
    subprocess.run([MZ,"c",ip,op,"1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    s=os.path.getsize(op); os.remove(ip); os.remove(op); return s
rows=[]
for tag,p in SLICES:
    raw=open(p,"rb").read(); N=len(raw); ch=[(i,raw[i*CS:(i+1)*CS]) for i in range(N//CS)]
    base=None
    print(f"=== {tag} {N:,} B, {len(ch)} chunks ===")
    for n in (1,2,4,6,11):
        ts=[]
        for _ in range(REPS):
            t0=time.perf_counter()
            with cf.ThreadPoolExecutor(max_workers=n) as ex: sizes=list(ex.map(one,ch))
            ts.append(time.perf_counter()-t0)
        t=statistics.median(ts); mbps=N/1048576/t
        if base is None: base=t
        eff=(base/t)/n
        rows.append(dict(tag=tag,workers=n,secs=t,mbps=mbps,speedup=base/t,efficiency=eff,bytes=sum(sizes)))
        print(f"  workers {n:>2}: {t:7.2f} s  {mbps:6.2f} MB/s  speedup {base/t:5.2f}x  efficiency {100*eff:5.1f}%  bytes {sum(sizes):,}",flush=True)
json.dump(rows,open(f"{OUT}/parscale.json","w"),indent=1)
print("ALLDONE_PS")
