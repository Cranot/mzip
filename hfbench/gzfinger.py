#!/usr/bin/env python3
"""Who produced the Hub's gzip files? A header fingerprint census.

Archives are 0.381 PB of the week and nothing takes a byte off them. The transcode lever -- store the
payload with a stronger coder plus the parameters needed to rebuild the original stream -- only works
where the producer is one we can reproduce, and a whole-file census is far too slow to settle that
(each file is a download plus dozens of trial recompressions).

The first ten bytes are enough to identify the producer family, and cost one tiny range request:
  XFL   2 = the producer used maximum compression, 4 = fastest, 0 = everything else
  OS    3 = Unix, 255 = unspecified (Go, and Python's gzip), 0 = FAT
  FNAME whether the original filename was stored (the gzip CLI does, most libraries do not)
  MTIME whether a timestamp was written or deliberately zeroed
Together these separate the stock gzip CLI, zlib-based libraries, Go's compress/gzip and pigz, which
tells us what share of the class the lever could ever reach."""
import collections, json, os, re, struct, subprocess, time
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/gz2"
TARGET_FILES = 400

def sh(a, t=90, **k): return subprocess.run(a, capture_output=True, timeout=t, **k)
def api(u):
    try: return json.loads(sh(["curl", "-sS", "-g", "--max-time", "60", u]).stdout)
    except Exception: return None

def head16(kind, repo, path):
    r = sh(["curl", "-sSL", "--max-time", "40", "-r", "0-63",
            f"{H}/{'datasets/' if kind=='datasets' else ''}{repo}/resolve/main/{path}"])
    b = r.stdout
    return b if len(b) >= 12 else None

def classify(b):
    if b[0] != 0x1f or b[1] != 0x8b or b[2] != 8: return None
    flg = b[3]; mtime = struct.unpack("<I", b[4:8])[0]; xfl = b[8]; osb = b[9]
    fname = bool(flg & 8)
    lvl = {2: "max", 4: "fast", 0: "default"}.get(xfl, f"xfl={xfl}")
    osn = {3: "unix", 255: "unspecified", 0: "fat", 7: "mac", 11: "ntfs"}.get(osb, f"os={osb}")
    # the shapes that identify a producer family
    if osn == "unix" and fname and mtime: fam = "gzip CLI (zlib)"
    elif osn == "unix" and not fname and mtime: fam = "zlib library"
    elif osn == "unspecified" and not fname and mtime == 0: fam = "Go compress/gzip"
    elif osn == "unspecified" and not fname: fam = "library, OS unset (Python/Go)"
    elif osn == "unix" and not fname and mtime == 0: fam = "zlib, timestamp zeroed"
    else: fam = f"other ({osn}, fname={int(fname)}, mtime={'0' if mtime==0 else 'set'})"
    return dict(level=lvl, os=osn, fname=fname, mtime0=(mtime == 0), family=fam)

def main():
    seen = 0; fam = collections.Counter(); lvl = collections.Counter(); byfam_bytes = collections.Counter()
    rows = []
    for kind in ("datasets", "models"):
        pop = [json.loads(l) for l in open(f"{B}/upload-mix/sample_{kind}.jsonl")]
        pop = [r for r in pop if r.get("ok")]
        pop.sort(key=lambda r: -(sum(r.get("bytes_in_window", {}).values()) or 0))
        for r in pop:
            if seen >= TARGET_FILES: break
            t = api(f"{H}/api/{kind}/{r['id']}/tree/main?recursive=true&limit=1000")
            if not isinstance(t, list): continue
            gz = [(f["path"], int(f.get("size") or 0)) for f in t
                  if f.get("type") == "file" and f["path"].lower().endswith((".gz", ".tgz"))
                  and int(f.get("size") or 0) > 50_000]
            for path, sz in gz[:6]:
                if seen >= TARGET_FILES: break
                b = head16(kind, r["id"], path)
                if not b: continue
                c = classify(b)
                if not c: continue
                seen += 1; fam[c["family"]] += 1; lvl[c["level"]] += 1; byfam_bytes[c["family"]] += sz
                rows.append(dict(kind=kind, repo=r["id"], path=path, size=sz, **c))
                if seen % 50 == 0: print(f"  {seen}/{TARGET_FILES}", flush=True)
            time.sleep(0.05)
        if seen >= TARGET_FILES: break
    json.dump(rows, open(f"{OUT}/gzfinger.json", "w"), indent=1)
    tb = max(sum(byfam_bytes.values()), 1)
    print(f"\n{seen} gzip files fingerprinted, {tb/1e9:.1f} GB of them\n")
    print(f"{'producer family':34s} {'files':>7s} {'share':>7s} {'bytes':>10s} {'byte share':>11s}")
    for f_, n in fam.most_common():
        print(f"{f_:34s} {n:7d} {100*n/seen:6.1f}% {byfam_bytes[f_]/1e9:9.1f}G {100*byfam_bytes[f_]/tb:10.1f}%")
    print(f"\ncompression level hint: " + ", ".join(f"{k} {100*v/seen:.0f}%" for k, v in lvl.most_common()))
    repro = sum(v for k, v in byfam_bytes.items() if "zlib" in k or "gzip CLI" in k)
    print(f"\nbyte share whose producer is a zlib family we can reproduce: {100*repro/tb:.1f}%")
    print("GZFINGER_DONE")

if __name__ == "__main__":
    main()
