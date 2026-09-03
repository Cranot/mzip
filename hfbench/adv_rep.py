import collections, hashlib
for fam,p in (("A-whisper","/root/mzip-hfbench/weights2/whisper-lv3.bin"),
              ("B-whisper","/root/mzip-hfbench/weightsB/whisper-lv3.bin"),
              ("A-sdxl","/root/mzip-hfbench/weights2/sdxl-unet.bin"),
              ("A-bloom","/root/mzip-hfbench/weights2/bloom-560m.bin")):
    b=open(p,"rb").read()
    # duplicate 64-byte aligned blocks
    K=64
    c=collections.Counter(b[i:i+K] for i in range(0,len(b),K))
    dup=sum(v-1 for v in c.values() if v>1)
    tot=len(b)//K
    # longest identical run of bytes
    best=0; cur=1
    for i in range(1,len(b)):
        if b[i]==b[i-1]: cur+=1
        else:
            if cur>best: best=cur
            cur=1
    if cur>best: best=cur
    print("%-12s dup64=%d/%d (%.3f%%) longest_byte_run=%d"%(fam,dup,tot,100.0*dup/tot,best))
