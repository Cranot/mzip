#!/usr/bin/env python3
"""Can a real .gz from the Hub be rebuilt byte-exactly from its payload plus a small recipe?

Archives are the single largest class of the week's uploads (0.381 PB) and this programme counts them
as zero. But a .gz is a deflate stream, and deflate is DETERMINISTIC given (level, strategy, memLevel,
windowBits) and a zlib version. So the parquet trick applies: decompress, store the payload with a
stronger coder, and keep the few parameters needed to reproduce the original stream bit for bit. This
tests the premise on real Hub files before any of it is built.

For each file: read the gzip header fields, decompress, then search the recompression space for a
setting whose deflate output equals the original byte for byte. Where one is found, report what the
payload costs under zstd-19 against the original .gz size -- that difference is the lever. Where none
is found, say so: the producer was not zlib (pigz, zopfli, Go, Java) and the file needs a per-member
fallback exactly as the parquet container already does per page."""
import gzip, io, json, os, re, struct, subprocess, sys, time, zlib
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/gz-transcode"
os.makedirs(OUT, exist_ok=True)
MAXBYTES = 60 * 1024 * 1024

def sh(a, **k): return subprocess.run(a, capture_output=True, **k)
def api(url):
    r = sh(["curl", "-sS", "-g", "--max-time", "90", url], text=True)
    try: return json.loads(r.stdout)
    except Exception: return None

def find_files():
    """.gz / .zip / .tgz files in the sampled dataset population, smallest first"""
    pop = json.load(open("/root/mzip-hfbench/upload-mix/population_datasets.json"))["rows"]
    import random; random.Random(11).shuffle(pop)
    out = []
    for m in pop[:260]:
        if len(out) >= 14: break
        d = api(f"{H}/api/datasets/{m['id']}/tree/main?recursive=true&limit=1000")
        if not isinstance(d, list): continue
        for f in d:
            p = f.get("path", "").lower(); sz = int(f.get("size") or 0)
            if f.get("type") != "file" or sz < 200000 or sz > MAXBYTES: continue
            if p.endswith((".gz", ".tgz", ".zip")):
                out.append((m["id"], f["path"], sz)); break
        time.sleep(0.1)
    return out

def gz_members(b):
    """split a gzip file into members; returns [(header_dict, deflate_bytes, isize, crc)] or None"""
    if len(b) < 18 or b[0] != 0x1f or b[1] != 0x8b or b[2] != 8: return None
    flg = b[3]; mtime = struct.unpack("<I", b[4:8])[0]; xfl = b[8]; os_b = b[9]
    p = 10
    extra = fname = fcomment = None
    if flg & 4:
        xlen = struct.unpack("<H", b[p:p+2])[0]; extra = b[p+2:p+2+xlen]; p += 2 + xlen
    if flg & 8:
        e = b.index(b"\0", p); fname = b[p:e]; p = e + 1
    if flg & 16:
        e = b.index(b"\0", p); fcomment = b[p:e]; p = e + 1
    if flg & 2: p += 2
    if len(b) < p + 8: return None
    deflate = b[p:-8]
    crc, isize = struct.unpack("<II", b[-8:])
    return dict(flg=flg, mtime=mtime, xfl=xfl, os=os_b, extra=extra, fname=fname,
                fcomment=fcomment, header_len=p), deflate, isize, crc

def try_recompress(payload, target):
    """search (level, strategy, memLevel) for a deflate stream equal to target"""
    for level in (6, 9, 1, 2, 3, 4, 5, 7, 8):
        for strategy in (zlib.Z_DEFAULT_STRATEGY, zlib.Z_FILTERED, zlib.Z_HUFFMAN_ONLY, zlib.Z_RLE, zlib.Z_FIXED):
            for mem in (8, 9, 7):
                c = zlib.compressobj(level, zlib.DEFLATED, -15, mem, strategy)
                out = c.compress(payload) + c.flush()
                if out == target: return dict(level=level, strategy=strategy, memLevel=mem)
    return None

def main():
    files = find_files()
    print(f"{len(files)} archive files found\n")
    print(f"{'file':52s} {'orig MB':>8s} {'kind':>6s} {'exact':>6s} {'recipe':>22s} {'zstd-19':>9s} {'saving':>8s}")
    rows = []
    for repo, path, sz in files:
        loc = f"{OUT}/f.bin"
        r = sh(["curl", "-sSL", "--max-time", "600", "-o", loc, f"{H}/datasets/{repo}/resolve/main/{path}"])
        if not os.path.exists(loc) or os.path.getsize(loc) < 1000: continue
        b = open(loc, "rb").read()
        name = f"{repo}/{path}"
        kind = "zip" if path.lower().endswith(".zip") else "gz"
        if kind == "zip":
            print(f"{name[:52]:52s} {len(b)/1e6:8.1f} {'zip':>6s} {'-':>6s} {'(zip not tested here)':>22s}")
            continue
        m = gz_members(b)
        if not m:
            print(f"{name[:52]:52s} {len(b)/1e6:8.1f} {kind:>6s} {'n/a':>6s} {'not a single-member gzip':>22s}")
            continue
        hdr, deflate, isize, crc = m
        try: payload = zlib.decompress(deflate, -15)
        except Exception as e:
            print(f"{name[:52]:52s} {len(b)/1e6:8.1f} {kind:>6s} {'ERR':>6s} {str(e)[:22]:>22s}"); continue
        rec = try_recompress(payload, deflate)
        z = sh(["zstd", "-19", "-T0", "-c", "-"], input=payload)
        zlen = len(z.stdout)
        # the container: zstd payload + the gzip header bytes + a few recipe bytes
        total = zlen + hdr["header_len"] + 8 + 4
        saving = 1 - total / len(b)
        rows.append(dict(repo=repo, path=path, orig=len(b), payload=len(payload), zstd19=zlen,
                         exact=bool(rec), recipe=rec, total=total, saving=saving, os=hdr["os"], xfl=hdr["xfl"]))
        rstr = (f"L{rec['level']} S{rec['strategy']} M{rec['memLevel']}" if rec else "NO MATCH")
        print(f"{name[:52]:52s} {len(b)/1e6:8.1f} {kind:>6s} {('yes' if rec else 'no'):>6s} {rstr:>22s} "
              f"{zlen/1e6:8.1f}M {100*saving:7.1f}%", flush=True)
        json.dump(rows, open(f"{OUT}/gz_transcode.json", "w"), indent=1)
    ok = [r for r in rows if r["exact"]]
    if rows:
        print(f"\nreproduced byte-exactly: {len(ok)}/{len(rows)}")
        if ok:
            s = [r["saving"] for r in ok]
            print(f"  their saving vs the original .gz: min {100*min(s):.1f}%  median {100*sorted(s)[len(s)//2]:.1f}%  max {100*max(s):.1f}%")
        import collections
        print("  recipes seen:", collections.Counter((r["recipe"]["level"], r["recipe"]["strategy"], r["recipe"]["memLevel"]) for r in ok))
        alls = [r["saving"] for r in rows]
        print(f"  saving over ALL files if a non-matching member is stored verbatim: "
              f"{100*sum(r['saving'] for r in ok)/len(rows):.1f}% mean")
    print("GZ_TRANSCODE_DONE")

if __name__ == "__main__":
    main()
