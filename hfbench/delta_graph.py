#!/usr/bin/env python3
"""The derivative family as a GRAPH, not a star.

Every scheme in this programme codes a derivative against its DECLARED base: a star with the base at
the centre. But a store holds the whole family, and two fine-tunes of one base may be far closer to
each other than either is to the base (two checkpoints of a run, an SFT and its DPO, a model and its
own re-upload). The cheapest way to store N related models is a minimum spanning tree over the
pairwise delta costs, rooted at whichever node is stored in full.

This computes the full pairwise cost matrix over the community fine-tunes of one base plus the base
itself, using the same model the delta coder uses (order-0 of the zigzag byte planes of the 16-bit
difference), then compares:
  star  = every node coded against the declared base
  MST   = every node coded against its cheapest already-stored neighbour (Prim from the base)
and reports what the graph saves over the star."""
import glob, json, os
import numpy as np

P = "/root/mzip-hfbench/pairs"

def h0(a):
    h = np.bincount(a, minlength=256).astype(np.float64)
    p = h[h > 0] / h.sum()
    return float(-(p * np.log2(p)).sum()) * a.size / 8.0

def cost(x, y):
    """bytes to code x given y, as the coder models it"""
    d = x.astype(np.int64) - y.astype(np.int64)
    zig = np.where(d >= 0, 2 * d, -2 * d - 1).astype(np.uint32)
    return h0((zig & 0xFF).astype(np.uint8)) + h0(((zig >> 8) & 0xFF).astype(np.uint8))

def alone(x):
    b = x.view(np.uint8).reshape(-1, 2)
    return h0(np.ascontiguousarray(b[:, 0])) + h0(np.ascontiguousarray(b[:, 1]))

def main():
    names = ["BASE(Qwen2.5-1.5B-Instruct)"]
    slices = [np.frombuffer(open(f"{P}/qwen25-1.5b.ft.bin", "rb").read(), dtype=np.uint16)]
    for fp in sorted(glob.glob(f"{P}/community-*.ft.bin")):
        tag = os.path.basename(fp)[len("community-"):-len(".ft.bin")]
        v = np.frombuffer(open(fp, "rb").read(), dtype=np.uint16)
        if v.size != slices[0].size: continue
        names.append(tag); slices.append(v)
    N = len(names)
    print(f"{N} models, {slices[0].nbytes} B each\n")
    C = np.zeros((N, N))
    for i in range(N):
        for j in range(N):
            if i != j: C[i, j] = cost(slices[j], slices[i])   # cost of coding j given i
    A = np.array([alone(s) for s in slices])
    print("cost of coding ROW given COLUMN, in KB (diagonal = coded alone):")
    print(" " * 30 + "".join(f"{n[:9]:>10s}" for n in names))
    for j in range(N):
        row = "".join(f"{(A[j] if i == j else C[i, j]) / 1024:10.0f}" for i in range(N))
        print(f"{names[j][:30]:30s}{row}")
    # star: everything against node 0
    star = A[0] + sum(C[0, j] for j in range(1, N))
    # Prim from node 0 over "cost of coding j given i"
    intree = [0]; total = A[0]; edges = []
    while len(intree) < N:
        best = None
        for j in range(N):
            if j in intree: continue
            for i in intree:
                c = C[i, j]
                if best is None or c < best[0]: best = (c, i, j)
        c, i, j = best; intree.append(j); total += c; edges.append((names[i], names[j], c))
    print(f"\nstar (all against the declared base): {star/1e6:8.3f} MB")
    print(f"minimum spanning tree:                {total/1e6:8.3f} MB   ({100*(1-total/star):.1f}% smaller)")
    print("\ntree edges (parent -> child, cost KB):")
    for a, b, c in sorted(edges, key=lambda e: e[2]):
        print(f"  {a[:34]:34s} -> {b[:34]:34s} {c/1024:9.0f}")
    json.dump(dict(names=names, C=C.tolist(), alone=A.tolist(), star=star, mst=total,
                   edges=[(a, b, float(c)) for a, b, c in edges]), open(f"{P}/delta_graph.json", "w"), indent=1)
    print("DELTA_GRAPH_DONE")

if __name__ == "__main__":
    main()
