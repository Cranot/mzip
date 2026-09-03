#!/usr/bin/env python3
"""Twin delta: what a REAL coder achieves, not just the entropy bound.

RESULTs 104/111 price a perturbed twin at h0(delta) -- an order-0 bound on the integer bf16 difference.
A store needs a coder that actually gets there. Three candidates on the same delta, one full tensor from
the largest cluster: zstd -19 on the raw int16 delta; zstd on the delta split into byte planes (Fano's
move); and the bound itself. Also the trivially-coded alternative a store might reach for first: zstd on
the twin's raw bf16 bytes (what BG4+LZ4 roughly does today)."""
import sys, subprocess, tempfile, os
import numpy as np
sys.path.insert(0, "/root/mzip-hfbench")
from pertcost import tensor_slice, h0

def zstd(b, lvl=19):
    with tempfile.NamedTemporaryFile(delete=False) as f: f.write(b); p = f.name
    try:
        r = subprocess.run(["zstd", f"-{lvl}", "--long=27", "-q", "-f", p, "-c"], capture_output=True, timeout=300)
        return len(r.stdout) if r.returncode == 0 else None
    finally: os.unlink(p)

def main():
    pairs = [("cryptoDev23/albedo-qwen3.6-35b", "isomsom/Affine-5cdqb2dugt-g2"),
             ("fiveflow/rq_8b_32", "fiveflow/rq_8b_224"),
             ("youngseok12/AX-3.1-Light", "jwg0830/AX-3.1-Light-sft")]
    print(f"{'pair':52s} {'raw zstd':>9s} {'delta zstd':>11s} {'planes zstd':>12s} {'h0 bound':>9s}   (bits/weight; Xet ~14.0)")
    for a, b in pairs:
        A = tensor_slice(a, nel_cap=8_000_000)
        if not A: print(a, "unreadable"); continue
        k, wa = A
        Bt = tensor_slice(b, want_key=k, nel_cap=8_000_000)
        if not Bt: print(b, "no", k); continue
        _, wb = Bt
        n = min(wa.size, wb.size); wa, wb = wa[:n], wb[:n]
        d = (wa - wb).astype(np.int16)
        raw = wa.astype(np.uint16).tobytes()
        z_raw = zstd(raw)
        z_d = zstd(d.tobytes())
        db = d.view(np.uint8).reshape(-1, 2)
        planes = np.concatenate([db[:, 0], db[:, 1]]).tobytes()
        z_p = zstd(planes)
        bpw = lambda z: 8.0 * z / n if z else float("nan")
        print(f"{(a[:24]+' ~ '+b[:24]):52s} {bpw(z_raw):9.3f} {bpw(z_d):11.3f} {bpw(z_p):12.3f} {h0(d):9.3f}")
    print("\n  'delta zstd' and 'planes zstd' are what a store would actually pay per weight for the twin; the bound")
    print("  is what RESULTs 104/111 quoted. Xet holds the twin whole at ~14.0 bits/weight today.")
    print("PERTCODE_DONE")

if __name__ == "__main__":
    main()
