#!/usr/bin/env python3
"""The largest class of the week (380 TB) is a container, and nothing in its repo says what is inside.

The question that decides whether it is a lever: are the members STORED or DEFLATED? A deflated member
is opaque twice over -- Xet's content-defined chunking cannot find a boundary it can reuse, so two
archives sharing 99% of their contents dedup to nothing, and a compressor sees incompressible bytes.
An archive of STORED members is already transparent and needs nothing from us.

For zip this is answerable exactly and cheaply: the central directory at the tail of the file lists
every member's compression method and both sizes. Two range requests per file, no download.
"""
import json, os, re, struct, sys, urllib.request, collections
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
TOK = os.environ.get("HF_TOKEN", "")
AEXT = (".zip", ".tar", ".tar.gz", ".tgz", ".gz", ".7z", ".rar", ".tar.bz2", ".tar.xz", ".xz", ".bz2", ".zst", ".tar.zst")

def req(u, rng=None):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    if rng: r.add_header("Range", f"bytes={rng}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=60) as f: return f.read()
        except Exception:
            pass
    return None

def api(u):
    b = req(u)
    return json.loads(b) if b else None

def ext_of(p):
    p = p.lower()
    for e in sorted(AEXT, key=len, reverse=True):
        if p.endswith(e): return e
    return None

def zip_methods(kind, rid, path, size):
    """read the zip central directory from the tail; return {method: [comp_bytes, uncomp_bytes, n]}"""
    url = f"{H}/{'datasets/' if kind=='datasets' else ''}{rid}/resolve/main/{path}"
    tail = req(url, f"{max(0,size-65600)}-{size-1}")
    if not tail: return None, "no tail"
    i = tail.rfind(b"PK\x05\x06")
    z64 = tail.rfind(b"PK\x06\x06")
    if i < 0: return None, "no EOCD"
    try:
        n_ent, cd_size, cd_off = struct.unpack("<HII", tail[i+10:i+20])
    except Exception:
        return None, "short EOCD"
    if cd_off == 0xFFFFFFFF or cd_size == 0xFFFFFFFF or n_ent == 0xFFFF:
        if z64 < 0: return None, "zip64, no locator"
        try:
            n_ent, cd_size, cd_off = struct.unpack("<QQQ", tail[z64+32:z64+56])[0:3]
        except Exception:
            return None, "zip64 parse"
    cd = req(url, f"{cd_off}-{cd_off+cd_size-1}") if cd_size < 40_000_000 else None
    if not cd: return None, "no CD"
    out = collections.defaultdict(lambda: [0, 0, 0]); p = 0
    while p + 46 <= len(cd) and cd[p:p+4] == b"PK\x01\x02":
        meth, = struct.unpack("<H", cd[p+10:p+12])
        csz, usz = struct.unpack("<II", cd[p+20:p+28])
        nl, el, cl = struct.unpack("<HHH", cd[p+28:p+34])
        e = out[meth]; e[0] += csz; e[1] += usz; e[2] += 1
        p += 46 + nl + el + cl
    return dict(out), None

def main():
    rows = []
    for fn, kind in (("sample_models.jsonl", "models"), ("sample_datasets.jsonl", "datasets")):
        try:
            for l in open(f"{B}/upload-mix/{fn}"):
                if l.strip():
                    r = json.loads(l); r["_kind"] = kind; rows.append(r)
        except OSError: pass
    cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get("archive", 0) or 0) > 0]
    cand.sort(key=lambda r: -float(r["bytes_in_window"]["archive"]))
    print(f"repos with archive bytes: {len(cand)}; probing the largest {min(60,len(cand))}\n")
    ext_bytes = collections.Counter(); meth_bytes = collections.defaultdict(lambda: [0, 0, 0])
    nz = 0; fails = collections.Counter()
    MET = {0: "stored", 8: "deflate", 9: "deflate64", 12: "bzip2", 14: "lzma", 93: "zstd", 95: "xz"}
    for r in cand[:60]:
        rid = r["id"]; kind = r["_kind"]
        t = api(f"{H}/api/{kind}/{rid}/tree/main?recursive=1&expand=1") or []
        if not isinstance(t, list): continue
        files = []
        for f in t:
            if f.get("type") != "file": continue
            e = ext_of(f.get("path", ""))
            if not e: continue
            sz = f.get("size") or (f.get("lfs") or {}).get("size") or 0
            if sz: files.append((f["path"], e, int(sz)))
        for _p, e, sz in files: ext_bytes[e] += sz
        files.sort(key=lambda x: -x[2])
        for path, e, sz in files[:2]:
            if e != ".zip" or sz < 1000: continue
            m, err = zip_methods(kind, rid, path, sz)
            if err: fails[err] += 1; continue
            nz += 1
            for k, v in m.items():
                a = meth_bytes[k]; a[0] += v[0]; a[1] += v[1]; a[2] += v[2]
    tot = sum(ext_bytes.values())
    print(f"{'extension':12s} {'GB':>10s} {'% of archive bytes':>20s}")
    for e, b in ext_bytes.most_common():
        print(f"{e:12s} {b/1e9:10.2f} {100*b/max(tot,1):19.1f}%")
    print(f"\nzip central directories read: {nz}   failures: {dict(fails)}")
    if meth_bytes:
        ct = sum(v[0] for v in meth_bytes.values()); ut = sum(v[1] for v in meth_bytes.values())
        print(f"\n{'member method':14s} {'members':>10s} {'stored GB':>12s} {'orig GB':>10s} {'% of stored':>12s} {'ratio':>7s}")
        for k, v in sorted(meth_bytes.items(), key=lambda x: -x[1][0]):
            print(f"{MET.get(k,str(k)):14s} {v[2]:10d} {v[0]/1e9:12.3f} {v[1]/1e9:10.3f}"
                  f" {100*v[0]/max(ct,1):11.1f}% {v[1]/max(v[0],1):7.3f}")
        print(f"\n  whole-archive ratio: {ut/max(ct,1):.4f}  (1.0 = nothing was compressed on the way in)")
    json.dump({"ext": dict(ext_bytes), "meth": {str(k): v for k, v in meth_bytes.items()},
               "nzip": nz, "fails": dict(fails)}, open(f"{B}/upload-mix/archprobe.json", "w"), indent=1)
    print("ARCHPROBE_DONE")

if __name__ == "__main__":
    main()
