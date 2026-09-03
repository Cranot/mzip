#!/usr/bin/env python3
import numpy as np, json
W = "/root/mzip-hfbench/weightsB"
hdr = "%-15s %-5s %8s %9s %10s" % ("tag","dt","distinct","low3=0 %","low13=0 %")
print(hdr)
for m in json.load(open(W + "/slicesB.json")):
    raw = np.fromfile("%s/%s.bin" % (W, m["tag"]), dtype=np.uint8)
    if m["dtype"] == "F32":
        v = raw.view(np.uint32)
        l3  = 100.0 * np.mean((v & 7) == 0)
        l13 = 100.0 * np.mean((v & 0x1fff) == 0)
    else:
        v = raw.view(np.uint16)
        l3  = 100.0 * np.mean((v & 7) == 0)
        l13 = float("nan")
    print("%-15s %-5s %8d %9.2f %10.2f" % (m["tag"], m["dtype"], len(np.unique(v)), l3, l13))
