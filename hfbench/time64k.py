#!/usr/bin/env python3
"""SERIAL wall-clock at HF's 64 KiB granularity. Run ONLY when no other benchmark
is on the box. Pinned to one core (taskset -c 10) so the always-on
eon-liveness-deadman.mjs core does not intrude. Medians of REPS runs.

Arms per slice (all 128 chunks, summed): lz4, bg4-lz4 (HF, split cost included),
bg4-zstd-1/3/19, mzip (stock), mzip_soa (native small-block SoA), bg4+mzip.
mzip arms spawn one process per chunk, as a deployment would if calling the
CLI; exec overhead is measured separately (spawning /bin/true 128x) and reported
so a library-call estimate can be derived.
"""
import os, sys, time, json, subprocess, statistics, numpy as np, lz4.frame, zstandard as zstd
W="/root/mzip-hfbench/weights"; OUT="/root/mzip-hfbench/chunk"
MZ="/root/mzip-hfbench/build/mzip_fixed"; SOA="/root/mzip-hfbench/build/mzip_soa"
CS=64*1024; REPS=int(sys.argv[1]) if len(sys.argv)>1 else 3
SLICES=[("MiniLM-L6-v2 F32",f"{W}/MiniLM-L6-v2.slice"),("weights-bf16 BF16",f"{W}/weights-bf16.bin"),
        ("Qwen2.5-0.5B BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice")]
def bg4(d):
    a=np.frombuffer(d,dtype=np.uint8); n=(len(a)//4)*4; m=a[:n].reshape(-1,4)
    return b"".join(m[:,k].tobytes() for k in range(4))+a[n:].tobytes()
def unbg4(d, n):
    a=np.frombuffer(d,dtype=np.uint8); q=(n//4)*4; s=q//4
    m=np.empty((s,4),dtype=np.uint8)
    for k in range(4): m[:,k]=a[k*s:(k+1)*s]
    return m.tobytes()+a[q:].tobytes()
Z={L:zstd.ZstdCompressor(level=L) for L in (1,3,19)}; ZD=zstd.ZstdDecompressor()

def arm_lz4(ch):        c=[lz4.frame.compress(x,compression_level=0) for x in ch]; return c, lambda: [lz4.frame.decompress(x) for x in c]
def arm_bg4lz4(ch):     c=[lz4.frame.compress(bg4(x),compression_level=0) for x in ch]; return c, lambda: [unbg4(lz4.frame.decompress(x),CS) for x in c]
def arm_bg4z(L):
    def f(ch):
        c=[Z[L].compress(bg4(x)) for x in ch]; return c, lambda: [unbg4(ZD.decompress(x),CS) for x in c]
    return f
def arm_cli(binp, pre=None, post=None):
    """pre: transform applied before compress; post: its exact inverse applied after
    decompress (bug fix 2026-09-02: first pass compared bg4-split output to raw and
    reported FAIL on a correct round-trip)."""
    def f(ch):
        outs=[]
        for i,x in enumerate(ch):
            d=pre(x) if pre else x; ip,op=f"{OUT}/t{i}.in",f"{OUT}/t{i}.mz"; open(ip,"wb").write(d)
            subprocess.run([binp,"c",ip,op,"1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); outs.append(op)
        def dec():
            r=[]
            for i,op in enumerate(outs):
                rp=f"{OUT}/t{i}.rt"; subprocess.run([binp,"d",op,rp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                y=open(rp,"rb").read(); r.append(post(y, len(ch[i])) if post else y)
            return r
        return outs, dec
    return f
NATIVE="/root/mzip-hfbench/build/mzip_fixed3" if os.path.exists("/root/mzip-hfbench/build/mzip_fixed3") else SOA
ARMS=[("lz4",arm_lz4),("bg4-lz4 (HF)",arm_bg4lz4),("bg4-zstd-1",arm_bg4z(1)),("bg4-zstd-3",arm_bg4z(3)),("bg4-zstd-19",arm_bg4z(19)),
      ("mzip stock",arm_cli(MZ)),("mzip native",arm_cli(NATIVE)),("bg4+mzip",arm_cli(MZ,bg4,unbg4))]

# exec overhead baseline: 128 spawns of /bin/true (+ a tiny file write/read)
def exec_overhead():
    t0=time.perf_counter()
    for i in range(128): subprocess.run(["/bin/true"],stdout=subprocess.DEVNULL);
    return time.perf_counter()-t0
eo=statistics.median([exec_overhead() for _ in range(REPS)])
print(f"exec overhead for 128 spawns: {eo*1000:.0f} ms (subtract ~2x this from CLI arms for a library estimate)\n")

import resource
def cpu_now():
    """CPU seconds consumed so far by this process AND its reaped children.
    Contention-robust for single-threaded work; wall-clock on a shared box is not."""
    r=resource.getrusage(resource.RUSAGE_CHILDREN)
    return time.process_time()+r.ru_utime+r.ru_stime
def load1(): return float(open("/proc/loadavg").read().split()[0])

rows=[]
print(f"load at start: {load1():.2f}  (CPU-time columns are the contention-robust ones)")
for tag,p in SLICES:
    raw=open(p,"rb").read(); N=len(raw); ch=[raw[i:i+CS] for i in range(0,N,CS)]
    print(f"=== {tag}  {N:,} B, {len(ch)} chunks ===")
    print(f"  {'arm':16s} {'bytes':>10s} {'ratio':>6s} {'c wall':>7s} {'c CPU':>7s} {'MB/s c(CPU)':>11s} {'d wall':>7s} {'d CPU':>7s} {'MB/s d(CPU)':>11s} rt")
    for name,fn in ARMS:
        if "soa" in name and not os.path.exists(SOA): continue
        ct=[];cc=[];dt=[];dc=[];size=0;ok=True
        for _ in range(REPS):
            w0=time.perf_counter(); c0=cpu_now(); outs,dec=fn(ch); ct.append(time.perf_counter()-w0); cc.append(cpu_now()-c0)
            size=sum(os.path.getsize(o) if isinstance(o,str) else len(o) for o in outs)
            w0=time.perf_counter(); c0=cpu_now(); back=dec(); dt.append(time.perf_counter()-w0); dc.append(cpu_now()-c0)
            ok=ok and all(b==x for b,x in zip(back,ch)) and len(back)==len(ch)
            if isinstance(outs[0],str):
                for f in os.listdir(OUT):
                    if f.startswith("t") and (f.endswith(".in") or f.endswith(".mz") or f.endswith(".rt")): os.remove(f"{OUT}/{f}")
        cm=statistics.median(ct); ccm=statistics.median(cc); dm=statistics.median(dt); dcm=statistics.median(dc)
        rows.append(dict(tag=tag,arm=name,n=N,bytes=size,c_wall=cm,c_cpu=ccm,d_wall=dm,d_cpu=dcm,ok=ok,load=load1()))
        print(f"  {name:16s} {size:>10,} {N/size:>6.3f} {cm:>7.3f} {ccm:>7.3f} {N/1048576/max(ccm,1e-9):>11.2f} {dm:>7.3f} {dcm:>7.3f} {N/1048576/max(dcm,1e-9):>11.2f} {'OK' if ok else 'FAIL'}",flush=True)
json.dump(rows,open(f"{OUT}/time64k.json","w"),indent=1)
print("\nALLDONE_T64")
