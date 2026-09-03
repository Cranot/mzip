#!/usr/bin/env python3
"""How much of a week's uploads is an exact copy of something else?

Every figure in this programme is quoted against bytes UPLOADED, and the honest caveat has been that
those are pre-deduplication, so the petabyte totals are upper bounds on what a store actually holds.
That caveat has never been measured, and it matters in both directions:

  * it corrects the denominator -- what Xet holds is uploads minus exact copies;
  * it should IMPROVE our relative position, because the derivative levers here (one quant predicted
    from another, a fine-tune coded against its base) attack precisely the near-copies that content
    deduplication cannot see. Dedup removes the bytes we were never claiming; what remains is the part
    our levers act on.

The Hub stores a SHA-256 for every LFS file and exposes it, so exact duplication is directly countable
without downloading anything. This walks the sampled repos, collects (sha256, size) for every LFS file,
and reports what share of the bytes sits in digests that appear more than once -- within a repo, across
repos of one author, and across authors, since those are three different phenomena."""
import collections, json, os, subprocess, time
H = "https://huggingface.co"; B = "/root/mzip-hfbench"; OUT = f"{B}/dedup"
os.makedirs(OUT, exist_ok=True)
PER_KIND = 900

def sh(a, t=90): return subprocess.run(a, capture_output=True, text=True, timeout=t)
def api(u):
    for _ in range(3):
        try:
            r = sh(["curl", "-sS", "-g", "--max-time", "60", u])
            return json.loads(r.stdout)
        except Exception:
            time.sleep(3)
    return None

def main():
    files = []      # (sha, size, repo, author, kind, path)
    for kind in ("models", "datasets"):
        rows = [json.loads(l) for l in open(f"{B}/upload-mix/sample_{kind}.jsonl")]
        rows = [r for r in rows if r.get("ok")]
        rows.sort(key=lambda r: -(sum(r.get("bytes_in_window", {}).values()) or 0))
        n = 0
        for r in rows:
            if n >= PER_KIND: break
            d = api(f"{H}/api/{kind}/{r['id']}?blobs=true")
            if not isinstance(d, dict): continue
            for s in d.get("siblings", []):
                lfs = s.get("lfs") or {}
                sha = lfs.get("sha256") or lfs.get("oid")
                sz = lfs.get("size") or s.get("size") or 0
                if not sha or not sz: continue
                files.append((sha, int(sz), r["id"], r["id"].split("/")[0], kind, s.get("rfilename", "")))
            n += 1
            if n % 100 == 0: print(f"  {kind}: {n} repos, {len(files):,} LFS files", flush=True)
            time.sleep(0.05)
    json.dump(files, open(f"{OUT}/files.json", "w"))
    print(f"\n{len(files):,} LFS files across the sampled repos")
    if not files: return

    by_sha = collections.defaultdict(list)
    for sha, sz, repo, auth, kind, path in files:
        by_sha[sha].append((sz, repo, auth, kind, path))
    total = sum(sz for sha, sz, *_ in files)
    unique = sum(v[0][0] for v in by_sha.values())
    print(f"total bytes in those files      {total/1e12:8.2f} TB")
    print(f"bytes if every digest kept once {unique/1e12:8.2f} TB")
    print(f"exact duplication               {100*(1-unique/total):8.1f}% of the bytes")

    dup_same_repo = dup_same_author = dup_cross_author = 0
    n_dup_files = 0
    for sha, occ in by_sha.items():
        if len(occ) < 2: continue
        sz = occ[0][0]
        repos = {o[1] for o in occ}; auths = {o[2] for o in occ}
        extra = (len(occ) - 1) * sz
        n_dup_files += len(occ) - 1
        if len(repos) == 1: dup_same_repo += extra
        elif len(auths) == 1: dup_same_author += extra
        else: dup_cross_author += extra
    dup = total - unique
    print(f"\n{n_dup_files:,} redundant copies, {dup/1e12:.2f} TB")
    if dup > 0:
        print(f"  within one repository            {100*dup_same_repo/dup:5.1f}%  {dup_same_repo/1e12:6.2f} TB")
        print(f"  across repos of the same author  {100*dup_same_author/dup:5.1f}%  {dup_same_author/1e12:6.2f} TB")
        print(f"  across different authors         {100*dup_cross_author/dup:5.1f}%  {dup_cross_author/1e12:6.2f} TB")

    # what KIND of file gets duplicated: the answer decides whether dedup competes with our levers
    ext_dup = collections.Counter(); ext_tot = collections.Counter()
    for sha, occ in by_sha.items():
        sz = occ[0][0]
        e = occ[0][4].rsplit(".", 1)[-1].lower() if "." in occ[0][4].rsplit("/", 1)[-1] else "(none)"
        ext_tot[e] += sz * len(occ)
        if len(occ) > 1: ext_dup[e] += sz * (len(occ) - 1)
    print(f"\n{'extension':14s} {'total':>10s} {'duplicated':>12s} {'share of it':>12s}")
    for e, t in ext_tot.most_common(12):
        print(f"{e[:14]:14s} {t/1e12:9.2f}T {ext_dup[e]/1e12:11.2f}T {100*ext_dup[e]/max(t,1):11.1f}%")
    json.dump(dict(total=total, unique=unique, dup=dup, same_repo=dup_same_repo,
                   same_author=dup_same_author, cross_author=dup_cross_author,
                   by_ext={e: [ext_tot[e], ext_dup[e]] for e in ext_tot}),
              open(f"{OUT}/dedup.json", "w"), indent=1)
    print("DEDUP_DONE")

if __name__ == "__main__":
    main()
