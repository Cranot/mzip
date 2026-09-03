#!/usr/bin/env python3
"""Two classes totalling 270 TB of the week have never been opened. What are they?

'other' is 125.8 TB and 'torch-pickle' is 144.1 TB. Together that is 17% of everything uploaded, and
the only thing known about either is that the classifier could not place it. An unexamined class is
where a lever hides -- the archive class looked opaque until it turned out to be transparent, and the
U32 class looked badly encoded until it turned out to be full.

The hypothesis worth testing: some of this is the SAME weights in a different container. An ONNX export
embeds its tensors as raw protobuf and usually sits beside the safetensors it was exported from; a
pytorch_model.bin is the same tensors as model.safetensors with different framing. Both are category 2
of the frame -- an exact copy -- and both are invisible to chunk dedup because the framing differs.

File trees only. Nothing here downloads a model."""
import json, os, collections, sys, urllib.request
H = "https://huggingface.co"; B = "/root/mzip-hfbench"
TOK = os.environ.get("HF_TOKEN", "")

def req(u):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=60) as f: return f.read()
        except Exception: pass
    return None

def api(u):
    b = req(u); return json.loads(b) if b else None

KNOWN = (".safetensors", ".gguf", ".bin", ".pt", ".pth", ".ckpt", ".parquet", ".json", ".txt",
         ".md", ".yaml", ".yml", ".png", ".jpg", ".jpeg", ".mp4", ".mp3", ".wav", ".tar", ".zip",
         ".gz", ".7z", ".rar", ".csv", ".tsv", ".arrow", ".h5", ".onnx", ".npy", ".npz")

def main():
    rows = []
    for fn, kind in (("sample_models.jsonl", "models"), ("sample_datasets.jsonl", "datasets")):
        try:
            for l in open(f"{B}/upload-mix/{fn}"):
                if l.strip():
                    r = json.loads(l); r["_kind"] = kind; rows.append(r)
        except OSError: pass

    for cls_name, key in (("OTHER", "other"), ("TORCH-PICKLE", "torch-pickle")):
        cand = [r for r in rows if float((r.get("bytes_in_window") or {}).get(key, 0) or 0) > 0]
        cand.sort(key=lambda r: -float(r["bytes_in_window"][key]))
        ext = collections.Counter(); names = collections.Counter()
        with_st = 0.0; tot_cls = 0.0; nrep = 0
        examples = []
        for r in cand[:45]:
            rid = r["id"]; kd = r["_kind"]
            t = api(f"{H}/api/{kd}/{rid}/tree/main?recursive=1&expand=1")
            if not isinstance(t, list): continue
            nrep += 1
            files = []
            for f in t:
                if f.get("type") != "file": continue
                p = f.get("path", "")
                sz = int(f.get("size") or (f.get("lfs") or {}).get("size") or 0)
                if sz: files.append((p, sz))
            st_bytes = sum(s for p, s in files if p.endswith(".safetensors"))
            for p, s in files:
                low = p.lower()
                e = os.path.splitext(low)[1] or "(none)"
                if key == "torch-pickle":
                    if e not in (".bin", ".pt", ".pth", ".ckpt", ".pkl"): continue
                else:
                    if e in KNOWN: continue
                ext[e] += s
                names[os.path.basename(low)] += s
                tot_cls += s
                if st_bytes > 0: with_st += s
            if len(examples) < 12 and files:
                sel = [p for p, s in sorted(files, key=lambda x: -x[1])[:3]]
                examples.append((rid, sel))
        print(f"\n===== {cls_name}: {nrep} repos sampled, {tot_cls/1e9:.1f} GB of class bytes seen")
        print(f"  share sitting in a repo that also has safetensors: {100*with_st/max(tot_cls,1):.1f}%")
        print(f"  {'extension':14s} {'GB':>9s} {'share':>8s}")
        for e, v in ext.most_common(12):
            print(f"  {e:14s} {v/1e9:9.2f} {100*v/max(tot_cls,1):7.1f}%")
        print(f"  most common filenames by bytes:")
        for n, v in names.most_common(10):
            print(f"    {100*v/max(tot_cls,1):5.1f}%  {n[:56]}")
    print("\nOTHERPROBE_DONE")

if __name__ == "__main__":
    main()
