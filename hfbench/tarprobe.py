#!/usr/bin/env python3
"""The archive class is transparent (ratio 1.053), and three quarters of it is .tar. So what is inside?

A tar is a chain: a 512-byte header, then the member's raw bytes padded to 512, then the next header.
Walking the head of the file gives exact names and sizes; a window from the middle, scanned for the
'ustar' magic at offset 257 of a 512-aligned block, samples members further in without downloading the
file. Both are range requests -- nothing here downloads an archive.

What we are looking for is member types whose bytes are NOT already compressed: raw PCM audio, .npy
arrays, text, uncompressed images. Those are the ones where the container being transparent is worth
something."""
import json, os, collections, urllib.request
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
TOK = os.environ.get("HF_TOKEN", "")

def req(u, rng=None):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    if rng: r.add_header("Range", f"bytes={rng}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=90) as f: return f.read()
        except Exception: pass
    return None

def api(u):
    b = req(u); return json.loads(b) if b else None

def parse_hdr(blk):
    if len(blk) < 512 or blk[257:262] != b"ustar": return None
    name = blk[0:100].split(b"\0")[0].decode("utf-8", "replace")
    pre = blk[345:500].split(b"\0")[0].decode("utf-8", "replace")
    if pre: name = pre + "/" + name
    sz = blk[124:136].split(b"\0")[0].strip()
    try: size = int(sz, 8) if sz else 0
    except ValueError: return None
    return name, size, blk[156:157]

def walk(buf, base=0):
    """walk the header chain from the start of buf"""
    out = []; p = 0
    while p + 512 <= len(buf):
        h = parse_hdr(buf[p:p+512])
        if not h:
            p += 512
            if p > 512 * 40: break
            continue
        name, size, typ = h
        if typ in (b"0", b"\0"): out.append((name, size))
        p += 512 + ((size + 511) // 512) * 512
    return out

def scan(buf):
    """find headers anywhere in a window, without a chain"""
    out = []
    for p in range(0, len(buf) - 512, 512):
        h = parse_hdr(buf[p:p+512])
        if h and h[2] in (b"0", b"\0") and h[1] >= 0: out.append((h[0], h[1]))
    return out

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
    ext = collections.Counter(); nfile = 0; nmem = 0
    per_repo = []
    for r in cand[:45]:
        rid = r["id"]; kind = r["_kind"]
        t = api(f"{H}/api/{kind}/{rid}/tree/main?recursive=1&expand=1") or []
        if not isinstance(t, list): continue
        tars = [(f["path"], int(f.get("size") or (f.get("lfs") or {}).get("size") or 0))
                for f in t if f.get("type") == "file" and f.get("path", "").lower().endswith(".tar")]
        tars = [x for x in tars if x[1] > 1 << 20]
        tars.sort(key=lambda x: -x[1])
        got = collections.Counter()
        for path, sz in tars[:2]:
            url = f"{H}/{'datasets/' if kind=='datasets' else ''}{rid}/resolve/main/{path}"
            head = req(url, "0-4194303")
            mems = walk(head) if head else []
            mid = req(url, f"{sz//2}-{sz//2 + 2097151}") if sz > 8 << 20 else None
            if mid: mems += scan(mid)
            if not mems: continue
            nfile += 1
            for nm, msz in mems:
                e = os.path.splitext(nm)[1].lower() or "(none)"
                ext[e] += msz; got[e] += msz; nmem += 1
        if got:
            top = got.most_common(3)
            per_repo.append((rid, sum(got.values()), ", ".join(f"{k} {100*v/sum(got.values()):.0f}%" for k, v in top)))
    tot = sum(ext.values())
    print(f"tar files opened: {nfile}   members seen: {nmem}   member bytes sampled: {tot/1e9:.2f} GB\n")
    print(f"{'member ext':14s} {'GB':>10s} {'% of member bytes':>19s}   {'already compressed?':>19s}")
    COMPRESSED = {".jpg", ".jpeg", ".png", ".webp", ".mp4", ".mkv", ".webm", ".mp3", ".ogg", ".opus",
                  ".flac", ".m4a", ".gz", ".zip", ".zst", ".parquet", ".jxl", ".avif", ".7z", ".xz"}
    RAW = {".wav", ".npy", ".npz", ".bin", ".pt", ".pth", ".safetensors", ".txt", ".json", ".jsonl",
           ".csv", ".tsv", ".bmp", ".tif", ".tiff", ".pcm", ".raw", ".pkl", ".xml", ".yaml", ".md"}
    rawb = compb = 0
    for e, b in ext.most_common(18):
        tag = "yes" if e in COMPRESSED else ("NO -- raw" if e in RAW else "?")
        print(f"{e:14s} {b/1e9:10.3f} {100*b/max(tot,1):18.1f}%   {tag:>19s}")
    for e, b in ext.items():
        if e in COMPRESSED: compb += b
        elif e in RAW: rawb += b
    print(f"\n  already-compressed member bytes: {100*compb/max(tot,1):.1f}%")
    print(f"  raw / uncompressed member bytes: {100*rawb/max(tot,1):.1f}%")
    print(f"  unclassified:                    {100*(tot-compb-rawb)/max(tot,1):.1f}%")
    print("\nlargest sampled repos:")
    per_repo.sort(key=lambda x: -x[1])
    for rid, b, top in per_repo[:8]:
        print(f"  {b/1e9:8.2f} GB  {rid[:44]:44s}  {top}")
    json.dump({"ext": dict(ext), "nfile": nfile, "nmem": nmem, "tot": tot,
               "comp": compb, "raw": rawb}, open(f"{B}/upload-mix/tarprobe.json", "w"), indent=1)
    print("TARPROBE_DONE")

if __name__ == "__main__":
    main()
