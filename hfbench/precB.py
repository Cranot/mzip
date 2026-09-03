#!/usr/bin/env python3
import numpy as np, json
W="/root/mzip-hfbench/weightsB"; out={}
for m in json.load(open(W+"/slicesB.json")):
    raw=np.fromfile("%s/%s.bin"%(W,m["tag"]),dtype=np.uint8)
    if m["dtype"]=="F32":
        v=raw.view(np.uint32)
        out[m["tag"]]=dict(distinct=int(len(np.unique(v))),
                           low3=float(100.0*np.mean((v&7)==0)),
                           low13=float(100.0*np.mean((v&0x1fff)==0)))
    else:
        v=raw.view(np.uint16)
        out[m["tag"]]=dict(distinct=int(len(np.unique(v))),
                           low3=float(100.0*np.mean((v&7)==0)), low13=None)
json.dump(out,open("/root/mzip-hfbench/precB.json","w"),indent=1)
print("wrote precB.json", len(out))
