#!/usr/bin/env python3
"""What is actually inside the archive class -- weighted correctly, and reading the names.

Two faults in the first pass. It weighted each tar's member mix by the BYTES SAMPLED from that tar
rather than by the tar's real size, so one repo with large members became 93% of the answer; and it
classified by file extension, which reports 97% '(none)' for a tree of ELF binaries and shared objects
that simply have no dot in their names. This weights each tar by its own size and prints real member
paths so the classification can be checked by eye rather than trusted."""
import json, os, re, collections, urllib.request
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

def members(buf, chain):
    out = []; p = 0
    while p + 512 <= len(buf):
        h = parse_hdr(buf[p:p+512])
        if not h:
            if chain: return out
            p += 512; continue
        nm, size, typ = h
        if typ in (b"0", b"\0"): out.append((nm, size))
        p += (512 + ((size + 511) // 512) * 512) if chain else 512
    return out

SO = re.compile(r"\.so(\.\d+)*$")
def kind_of(nm):
    b = os.path.basename(nm); l = nm.lower()
    e = os.path.splitext(b)[1].lower()
    if SO.search(l) or e in (".dll", ".dylib", ".a", ".o"): return "native library"
    if e in (".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp", ".tif", ".tiff", ".jxl", ".avif"): return "image"
    if e in (".mp4", ".mkv", ".webm", ".avi", ".mov"): return "video"
    if e in (".mp3", ".wav", ".flac", ".ogg", ".opus", ".m4a"): return "audio"
    if e in (".safetensors", ".bin", ".pt", ".pth", ".ckpt", ".gguf", ".onnx", ".npy", ".npz"): return "weights/arrays"
    if e in (".py", ".pyc", ".pyi", ".h", ".hpp", ".c", ".cc", ".cpp", ".js", ".ts", ".java", ".go", ".rs"): return "source"
    if e in (".json", ".jsonl", ".txt", ".md", ".yaml", ".yml", ".xml", ".csv", ".tsv", ".cfg", ".toml", ".ini"): return "text/config"
    if e in (".gz", ".zip", ".zst", ".xz", ".bz2", ".whl", ".parquet", ".7z"): return "compressed"
    if e in ("", ".1", ".2", ".3") or "/bin/" in l or "/sbin/" in l: return "binary/no-ext"
    return "other:" + (e or "?")

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
    kb = collections.Counter()          # weighted by each tar's real size
    covered = 0.0; nfile = 0; examples = []
    for r in cand[:70]:
        rid = r["id"]; kind = r["_kind"]
        t = api(f"{H}/api/{kind}/{rid}/tree/main?recursive=1&expand=1") or []
        if not isinstance(t, list): continue
        tars = [(f["path"], int(f.get("size") or (f.get("lfs") or {}).get("size") or 0))
                for f in t if f.get("type") == "file" and f.get("path", "").lower().endswith(".tar")]
        tars = [x for x in tars if x[1] > 1 << 20]; tars.sort(key=lambda x: -x[1])
        for path, sz in tars[:2]:
            url = f"{H}/{'datasets/' if kind=='datasets' else ''}{rid}/resolve/main/{path}"
            head = req(url, "0-4194303")
            ms = members(head, True) if head else []
            if sz > 8 << 20:
                mid = req(url, f"{sz//2}-{sz//2 + 2097151}")
                if mid: ms += members(mid, False)
            ms = [(n, s) for n, s in ms if s > 0]
            if not ms: continue
            nfile += 1; covered += sz
            loc = collections.Counter()
            for nm, msz in ms: loc[kind_of(nm)] += msz
            tl = sum(loc.values()) or 1
            for k, v in loc.items(): kb[k] += sz * v / tl      # scale this tar's mix to its real size
            if len(examples) < 14:
                big = sorted(ms, key=lambda x: -x[1])[:3]
                examples.append((rid, path.split("/")[-1][:28], [n[:52] for n, _ in big]))
    tot = sum(kb.values()) or 1
    print(f"tar files opened: {nfile}   real tar bytes represented: {covered/1e9:.1f} GB\n")
    print(f"{'member kind':18s} {'% of tar bytes':>15s}")
    for k, v in kb.most_common(12):
        print(f"{k:18s} {100*v/tot:14.1f}%")
    print("\nwhat the members actually look like:")
    for rid, fn, big in examples:
        print(f"  {rid[:34]:34s} {fn:28s} {' | '.join(big)[:104]}")
    json.dump({"kinds": dict(kb), "covered": covered, "nfile": nfile},
              open(f"{B}/upload-mix/tarprobe2.json", "w"), indent=1)
    print("TARPROBE2_DONE")

if __name__ == "__main__":
    main()
