import json, sys, struct
for p in ["/root/mzip-hfbench/assets/smollm2.safetensors",
          "/root/mzip-hfbench/assets/minilm.safetensors"]:
    with open(p,"rb") as f:
        n = struct.unpack("<Q", f.read(8))[0]
        hdr = json.loads(f.read(n))
    base = 8 + n
    dt = {}
    for k,v in hdr.items():
        if k == "__metadata__": continue
        dt.setdefault(v["dtype"], []).append((k, base+v["data_offsets"][0], base+v["data_offsets"][1]))
    print("==", p, "hdrend=", base)
    for d, lst in dt.items():
        tot = sum(b-a for _,a,b in lst)
        print("   dtype", d, "tensors", len(lst), "bytes", tot)
        lst.sort(key=lambda x: -(x[2]-x[1]))
        for k,a,b in lst[:6]:
            print("      ", k, a, b, b-a)
