#!/usr/bin/env python3
"""Community fine-tunes: the most-downloaded third-party fine-tunes of Qwen/Qwen2.5-1.5B-Instruct
(the real derivative population, incl. LoRA merges). For each: classify from the model card
(lora/peft/unsloth vs full), fetch the tensor-aligned slice of model.layers.9.mlp.up_proj.weight,
and measure zigzag+FSE delta vs the instruct base -- same method as delta_all.py."""
import json, struct, subprocess, os, time, re, numpy as np
H = "https://huggingface.co"; P = "/root/mzip-hfbench/pairs"; FSE = "/root/mzip-hfbench/fse/fse"
BASE = "Qwen/Qwen2.5-1.5B-Instruct"; TENSOR = "model.layers.9.mlp.up_proj.weight"; BASE_SLICE = f"{P}/qwen25-1.5b.ft.bin"
def api(url):
    r = subprocess.run(["curl", "-sS", "--max-time", "60", url], capture_output=True, text=True)
    try: return json.loads(r.stdout)
    except Exception: return None
def curl(url, out, a, b):
    for t in range(3):
        subprocess.run(["curl", "-sSL", "--max-time", "300", "-r", f"{a}-{b}", "-o", out, url], capture_output=True)
        if os.path.exists(out) and os.path.getsize(out) == b - a + 1: return True
        time.sleep(4 * (t + 1))
    return False
def header(repo, fn):
    url = f"{H}/{repo}/resolve/main/{fn}"; tmp = f"{P}/chdr.tmp"
    if not curl(url, tmp, 0, 4 * 1024 * 1024 - 1): return None, None
    b = open(tmp, "rb").read(); n = struct.unpack("<Q", b[:8])[0]
    if n + 8 > len(b):
        if not curl(url, tmp, 0, n + 8 - 1): return None, None
        b = open(tmp, "rb").read()
    return json.loads(b[8:8 + n].decode()), 8 + n
def H0(x):
    h = np.bincount(x, minlength=256).astype(np.float64); p = h[h > 0] / h.sum(); return float(-(p * np.log2(p)).sum())
def fse_plane(pl):
    fi, fo, fr = f"{P}/cpl.tmp", f"{P}/cpl.tmp.fse", f"{P}/cpl.tmp.rt"; open(fi, "wb").write(pl)
    subprocess.run([FSE, "-B65536", "-f", fi, fo], capture_output=True)
    if not os.path.exists(fo): return len(pl)
    s = os.path.getsize(fo); subprocess.run([FSE, "-d", "-f", fo, fr], capture_output=True)
    assert open(fr, "rb").read() == pl, "FSE round-trip FAILED"; return min(s, len(pl))
lst = api(f"{H}/api/models?filter=base_model:finetune:{BASE}&sort=downloads&direction=-1&limit=40")
if not lst: print("list failed"); raise SystemExit
base_bytes = np.frombuffer(open(BASE_SLICE, "rb").read(), dtype=np.uint8); n = len(base_bytes)
eb = np.frombuffer(base_bytes.tobytes(), dtype=np.uint16).astype(np.int64)
done = 0
print("%-52s %-9s %7s | %-7s %-7s %-7s | %s" % ("community fine-tune of " + BASE, "kind", "dl", "alone", "zigzag", "costs", "same-hi"))
for m in lst:
    if done >= 8: break
    rid = m["id"]; dl = m.get("downloads", 0) or 0
    info = api(f"{H}/api/models/{rid}?blobs=true")
    if not info: continue
    sib = {s["rfilename"]: s.get("size") or 0 for s in info.get("siblings", [])}
    fn = "model.safetensors" if "model.safetensors" in sib else None
    if not fn:
        idx = [k for k in sib if k.endswith("model.safetensors.index.json")]
        if idx:
            r = subprocess.run(["curl", "-sSL", "--max-time", "60", f"{H}/{rid}/resolve/main/{idx[0]}"], capture_output=True, text=True)
            try: fn = json.loads(r.stdout)["weight_map"].get(TENSOR)
            except Exception: fn = None
    if not fn: print("%-52s skip: no safetensors" % rid); continue
    rd = subprocess.run(["curl", "-sSL", "--max-time", "60", f"{H}/{rid}/raw/main/README.md"], capture_output=True, text=True).stdout.lower()
    kind = "lora/peft" if re.search(r"\blora\b|\bpeft\b|adapter", rd) else ("unsloth" if "unsloth" in rd else "full/unk")
    h, off = header(rid, fn)
    if not h or TENSOR not in h: print("%-52s skip: tensor not found" % rid); continue
    mt = h[TENSOR]
    if mt["dtype"] != "BF16" or mt["shape"] != [8960, 1536]: print("%-52s skip: %s %s" % (rid, mt["dtype"], mt["shape"])); continue
    a = off + mt["data_offsets"][0] + 4096 * 2; out = f"{P}/community-{rid.replace('/', '__')}.ft.bin"
    if not curl(f"{H}/{rid}/resolve/main/{fn}", out, a, a + n - 1): print("%-52s skip: fetch failed" % rid); continue
    f = np.frombuffer(open(out, "rb").read(), dtype=np.uint8); ef = np.frombuffer(f.tobytes(), dtype=np.uint16).astype(np.int64)
    # alone: order-0 planes with FSE (same coder as the delta, for a like-for-like 'alone')
    m2 = f.reshape(-1, 2); alone = n / (fse_plane(m2[:, 0].copy().tobytes()) + fse_plane(m2[:, 1].copy().tobytes()))
    d = ef - eb; zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(np.uint16).view(np.uint8).reshape(-1, 2)
    zr = n / (fse_plane(zig[:, 0].copy().tobytes()) + fse_plane(zig[:, 1].copy().tobytes()))
    same_hi = float(np.mean((eb >> 8) == (ef >> 8)))
    print("%-52s %-9s %7d | %.4f  %.4f  %5.0f%% | %5.1f%%" % (rid[:52], kind, dl, alone, zr, 100 * alone / zr, 100 * same_hi), flush=True)
    json.dump(dict(repo=rid, kind=kind, downloads=dl, alone=alone, zigzag=zr, same_hi=same_hi), open(f"{P}/community-{rid.replace('/', '__')}.json", "w"))
    done += 1; time.sleep(1)
for t in ("cpl.tmp", "cpl.tmp.fse", "cpl.tmp.rt", "chdr.tmp"):
    try: os.remove(f"{P}/{t}")
    except FileNotFoundError: pass
print("COMMUNITY_DONE")
