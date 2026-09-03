#!/usr/bin/env python3
"""Hub upload mix: what was uploaded to the public Hub in the last 7 days, by class of bytes.
Population: every public model / dataset repo whose lastModified falls inside the window (cursor-paginated
listing, newest first, stops at the window start). Sample: uniform random repos per population, seeded.
Per sampled repo the recursive tree with expand=true gives every file's size and its lastCommit date; bytes
whose lastCommit is inside the window count as uploaded in the window, by class (extension; safetensors split
by dtype from the repo's parameter counts; GGUF by quant type from the filename). Sample -> population by
n/N scaling; bootstrap CI over repos. Bytes are 'as uploaded' (current file size, counted once). Invisible
here and named as unknowns in the report: private repos, spaces, deleted files, files rewritten more than
once inside the window, Xet dedup at rest, and repos whose tree exceeded the page cap (counted, listed)."""
import json, subprocess, sys, os, time, random, re, collections, datetime as dt
H = "https://huggingface.co"; OUT = "/root/mzip-hfbench/upload-mix"; os.makedirs(OUT, exist_ok=True)
DAYS = 7; N_SAMPLE = {"models": 3000, "datasets": 400}; PAGE_CAP_POP = 3000; PAGE_CAP_TREE = 40; PAGE_CAP_TREE_NOEXP = 10; DATE_FILE_CAP = 500; SEED = 20260902
NOW = (dt.datetime.fromisoformat(os.environ["UM_NOW"]) if os.environ.get("UM_NOW") else dt.datetime.now(dt.timezone.utc)).replace(microsecond=0)
START = NOW - dt.timedelta(days=DAYS)
DTYPE_BYTES = {"F64": 8, "I64": 8, "U64": 8, "F32": 4, "I32": 4, "U32": 4, "F16": 2, "BF16": 2, "I16": 2, "U16": 2,
               "I8": 1, "U8": 1, "BOOL": 1, "F8_E4M3": 1, "F8_E5M2": 1, "F8": 1, "I4": 0.5, "U4": 0.5, "F4": 0.5}
def iso(s): return dt.datetime.fromisoformat(s.replace("Z", "+00:00"))
def log(*a): print(*a, flush=True)
STATS = collections.Counter()
def get(url):
    """returns (json, next_url, http_status); retries on transport errors and 429"""
    for t in range(6):
        hf = f"{OUT}/h.{os.getpid()}.tmp"
        r = subprocess.run(["curl", "-sS", "-g", "--max-time", "120", "-D", hf, url], capture_output=True, text=True)
        STATS["calls"] += 1
        status = 0; link = None
        try:
            for l in open(hf):
                if l.startswith("HTTP/"): status = int(l.split()[1])
                if l.lower().startswith("link:"):
                    m = re.search(r"<([^>]+)>;\s*rel=\"next\"", l); link = m.group(1) if m else None
        except Exception: pass
        if status == 429:
            STATS["http429"] += 1; log(f"  429 rate-limited, sleeping {60*(t+1)}s"); time.sleep(60 * (t + 1)); continue
        if status in (401, 403, 404): return None, None, status
        try: return json.loads(r.stdout), link, status
        except Exception:
            STATS["json_fail"] += 1; time.sleep(5 * (t + 1))
    return None, None, -1
def population(kind):
    pf = f"{OUT}/population_{kind}.json"
    if os.path.exists(pf):
        saved = json.load(open(pf))
        if saved.get("window_end") == NOW.isoformat() and not saved.get("capped"):
            log(f"{kind}: reusing saved population ({saved['n']} repos, {saved['pages']} pages)"); return saved["rows"]
    url = f"{H}/api/{kind}?sort=lastModified&direction=-1&limit=1000"; rows = []; pages = 0; capped = False
    while url:
        d, nxt, st = get(url)
        if not isinstance(d, list): log(f"{kind}: page {pages+1} failed (status {st}); stopping enumeration EARLY"); STATS[f"{kind}_pop_incomplete"] = 1; break
        pages += 1; stop = False
        for m in d:
            lm = iso(m["lastModified"])
            if lm > NOW: continue
            if lm < START: stop = True; break
            rows.append(dict(id=m["id"], lm=m["lastModified"], created=m.get("createdAt")))
        if pages % 20 == 0 or stop or not nxt: log(f"{kind}: page {pages}, {len(rows)} in window, oldest seen {d[-1]['lastModified'] if d else '-'}")
        if stop or not nxt: break
        if pages >= PAGE_CAP_POP: capped = True; log(f"{kind}: PAGE CAP {PAGE_CAP_POP} hit -- population TRUNCATED"); STATS[f"{kind}_pop_capped"] = 1; break
        url = nxt
        time.sleep(0.2)
    json.dump(dict(kind=kind, window_start=START.isoformat(), window_end=NOW.isoformat(), pages=pages, capped=capped, n=len(rows), rows=rows), open(f"{OUT}/population_{kind}.json", "w"))
    return rows
def classify(path):
    p = path.lower(); ext = p.rsplit(".", 1)[-1] if "." in p.rsplit("/", 1)[-1] else ""
    if ext == "safetensors": return "safetensors"
    if ext == "gguf":
        m = re.search(r"[-_.](iq\d[_a-z0-9]*|q\d[_a-z0-9]*|f16|f32|bf16|tq\d[_a-z0-9]*)(?=[-_.]|$)", p)
        q = m.group(1).upper() if m else "?"
        q = re.sub(r"_L$|_S$|_M$|_XS$|_XXS$|_NL$", lambda mm: mm.group(0), q)
        return "gguf:" + q
    if ext in ("bin", "pt", "pth", "ckpt"): return "torch-pickle"
    if ext == "parquet": return "parquet"
    if ext == "arrow": return "arrow"
    if ext in ("jsonl", "json", "csv", "tsv", "txt", "md", "py", "yaml", "yml", "toml", "cfg", "xml", "html"): return "text"
    if ext in ("zip", "tar", "gz", "tgz", "xz", "zst", "7z", "bz2", "rar"): return "archive"
    if ext in ("png", "jpg", "jpeg", "webp", "gif", "bmp", "tif", "tiff"): return "image"
    if ext in ("wav", "mp3", "flac", "ogg", "m4a", "opus"): return "audio"
    if ext in ("mp4", "mkv", "webm", "mov", "avi"): return "video"
    if ext in ("onnx", "tflite", "pb", "h5", "msgpack", "npz", "npy", "mlmodel", "engine", "plan", "gguf", "ggml", "trt"): return "other-weights"
    return "other"
def tree(kind, rid, expand):
    """expand=True: 100 entries/page with lastCommit (the API's cap for expanded pages); expand=False: 1000/page,
    no commit dates -- used for repos CREATED inside the window, where every file is in-window by construction"""
    q = "&expand=true&limit=100" if expand else "&limit=1000"; cap = PAGE_CAP_TREE if expand else PAGE_CAP_TREE_NOEXP
    files = []; url = f"{H}/api/{kind}/{rid}/tree/main?recursive=true{q}"; pages = 0; capped = False
    while url:
        d, nxt, st = get(url); pages += 1
        if not isinstance(d, list): return (None if pages == 1 else files), st, capped
        files += [f for f in d if f.get("type") == "file"]
        if not nxt: break
        if pages >= cap: capped = True; break
        url = nxt
        time.sleep(0.1)
    return files, 200, capped
def dtype_split(rid):
    d, _, st = get(f"{H}/api/models/{rid}?expand[]=safetensors")
    if not isinstance(d, dict): return None
    st_ = d.get("safetensors") or {}; params = st_.get("parameters") or {}
    by = {k: v * DTYPE_BYTES.get(k, 0) for k, v in params.items()}
    tot = sum(by.values())
    return {k: v / tot for k, v in by.items()} if tot > 0 else None
def used_storage(kind, rid):
    d, _, st = get(f"{H}/api/{kind}/{rid}?expand[]=usedStorage")
    return int(d.get("usedStorage") or 0) if isinstance(d, dict) else 0
def sample_repo(kind, rid, created):
    """Cheap pass first: the unexpanded tree gives every file's size at 1000 per request. A repo created
    inside the window needs nothing more (every byte is new). Otherwise per-file commit dates are needed,
    which cost one request per 100 files, so repos with more than DATE_FILE_CAP files are recorded with
    their total size and marked undated rather than paged through -- they are counted, never guessed."""
    new_repo = bool(created and START <= iso(created) <= NOW)
    files, st, capped = tree(kind, rid, expand=False)
    if files is None: return dict(id=rid, kind=kind, status=st, ok=False, created_in_window=new_repo)
    if not new_repo:
        if len(files) > DATE_FILE_CAP or capped:
            allb = collections.Counter()
            for f in files: allb[classify(f.get("path", ""))] += int(f.get("size") or 0)
            STATS[f"{kind}_undated_large"] += 1
            return dict(id=rid, kind=kind, ok=True, created=created, created_in_window=False, undated_large=True,
                        files=len(files), files_in_window=0, tree_capped=capped, bytes_in_window={}, bytes_total=dict(allb),
                        used_storage=used_storage(kind, rid) if capped else sum(allb.values()))
        files, st, capped = tree(kind, rid, expand=True)
        if files is None: return dict(id=rid, kind=kind, status=st, ok=False, created_in_window=new_repo)
    inw = collections.Counter(); allb = collections.Counter(); n_inw = 0; n_all = 0
    for f in files:
        c = classify(f.get("path", "")); sz = int(f.get("size") or 0); n_all += 1; allb[c] += sz
        if new_repo: inw[c] += sz; n_inw += 1; continue
        lc = (f.get("lastCommit") or {}).get("date")
        if lc and START <= iso(lc) <= NOW: inw[c] += sz; n_inw += 1
    rec = dict(id=rid, kind=kind, ok=True, created=created, created_in_window=new_repo,
               files=n_all, files_in_window=n_inw, tree_capped=capped, bytes_in_window=dict(inw), bytes_total=dict(allb))
    if capped: rec["used_storage"] = used_storage(kind, rid)
    if kind == "models" and inw.get("safetensors", 0) > 0:
        sp = dtype_split(rid)
        rec["safetensors_dtype_share"] = sp
        if sp:
            del rec["bytes_in_window"]["safetensors"]
            for k, v in sp.items(): rec["bytes_in_window"][f"safetensors:{k}"] = inw["safetensors"] * v
        else: rec["bytes_in_window"]["safetensors:?"] = rec["bytes_in_window"].pop("safetensors")
    return rec
def main():
    random.seed(SEED); log(f"window {START.isoformat()} .. {NOW.isoformat()}")
    for kind in ("models", "datasets"):
        pop = population(kind); N = len(pop); n = min(N_SAMPLE[kind], N)
        log(f"{kind}: population N={N} repos modified in window; sampling n={n}")
        shuffled = list(pop); random.Random(SEED).shuffle(shuffled); samp = shuffled[:n]
        done_ids = set()
        try:
            for l in open(f"{OUT}/sample_{kind}.jsonl"): done_ids.add(json.loads(l)["id"])
        except Exception: pass
        if len(done_ids) >= n: log(f"{kind}: {len(done_ids)} rows already sampled, skipping"); continue
        if done_ids: log(f"{kind}: resuming, {len(done_ids)} rows already present")
        outf = open(f"{OUT}/sample_{kind}.jsonl", "a"); t0 = time.time()
        for i, m in enumerate(samp):
            if m["id"] in done_ids: continue
            rec = sample_repo(kind, m["id"], m.get("created")); rec["lm"] = m["lm"]
            outf.write(json.dumps(rec) + "\n"); outf.flush()
            if (i + 1) % 100 == 0: log(f"{kind}: {i+1}/{n} sampled, {time.time()-t0:.0f}s, calls {STATS['calls']}, 429s {STATS['http429']}")
            time.sleep(0.15)
        outf.close()
    json.dump(dict(STATS), open(f"{OUT}/stats.json", "w"))
    log("UPLOAD_MIX_DONE", dict(STATS))
if __name__ == "__main__": main()
