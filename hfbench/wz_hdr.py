import json, struct, sys, numpy as np

for path in ["/root/mzip-hfbench/assets/minilm.safetensors",
             "/root/mzip-hfbench/assets/smollm2.safetensors"]:
    with open(path, "rb") as f:
        nh = struct.unpack("<Q", f.read(8))[0]
        hdr = json.loads(f.read(nh))
    print("=====", path, " header_bytes=", nh, " tensors=", len(hdr) - ("__metadata__" in hdr))
    ents = [(k, v) for k, v in hdr.items() if k != "__metadata__"]
    ents.sort(key=lambda kv: kv[1]["data_offsets"][0])
    tot = 0
    for k, v in ents:
        a, b = v["data_offsets"]
        tot = max(tot, b)
    print("  data region bytes:", tot, " data starts at file offset", 8 + nh)
    # which tensors fall inside the first 8 MiB slice
    print("  %-58s %-6s %-18s %12s %12s" % ("name", "dtype", "shape", "start", "bytes"))
    acc = 0
    for k, v in ents:
        a, b = v["data_offsets"]
        if a >= 8388608 and acc > 24:
            print("   ... (%d more tensors beyond the 8MiB slice)" % (len(ents) - acc))
            break
        acc += 1
        print("  %-58s %-6s %-18s %12d %12d" % (k[:58], v["dtype"], str(v["shape"]), a, b - a))
    print()
