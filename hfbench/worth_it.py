#!/usr/bin/env python3
"""Is each win worth its speed? Place every class in the tier where it actually pays.

A ratio alone does not say whether a scheme is usable. For a content store the economics are
asymmetric: a chunk is compressed once when it is uploaded and decompressed on every download, and
crucially, a smaller chunk is also a SHORTER DOWNLOAD. So a slower decoder still wins end to end as
long as the CPU time it adds is less than the network time the smaller payload saves.

For each class, per 64 KiB chunk:
  stored bytes      65536 / ratio
  decode time       65536 / (decompress MB/s)          [throughput is over uncompressed bytes]
  delta bytes       what our arm saves
  delta time        what our arm costs (negative = we are also faster)
  BREAK-EVEN LINK   delta_bytes / delta_time  -- the per-stream link speed at which the CPU cost
                    equals the network saving. Below it, our arm delivers the chunk SOONER despite
                    being slower to decode; above it, the win is storage-only.
Reference points: a home connection is 0.01-0.1 Gb/s, a cloud VM 1-25 Gb/s, an NVMe read ~25 Gb/s.
"""
CHUNK = 65536.0

# class, n, xet ratio, ours ratio, xet comp MB/s, ours comp, xet decomp MB/s, ours decomp
ROWS = [
    ("F32-nlp",        1, 1.0722, 1.2097,  762, 1171,  2572,  4530),
    ("F32-embed",      2, 1.2522, 1.5872,  591, 1131,  2273,  3539),
    ("F32-llm",        3, 1.2878, 1.4816,  865, 1178,  2724,  4032),
    ("F32-vision",     1, 1.0505, 1.1977,  763, 1100,  2484,  4443),
    ("F32-audio",      1, 1.4785, 2.3209,  571, 1210,  2285,  3277),
    ("F32-vislang",    1, 1.4920, 2.3326,  504, 1102,  2100,  2835),
    ("F32-diffusion",  1, 1.0551, 1.1997,  725, 1197,  2406,  4959),
    ("BF16-llm",       4, 1.1447, 1.4885,  495, 1078,  2275,  3446),
    ("BF16-vislang",   1, 1.1538, 1.4932,  457, 1107,  2012,  3460),
    ("F16-llm",        2, 1.0000, 1.3230, 1489,  861, 21067,  2142),
    ("F16-audio",      1, 1.0263, 1.1661, 1135,  966,  7053,  2586),
    ("F16-diffusion",  1, 1.0001, 1.1837, 1253,  990, 21203,  2997),
    ("GGUF-F16",       1, 1.0006, 1.5012, 1942,  838, 21359,  1847),
    ("GGUF-Q",         7, 1.0006, 1.0667, 1893,  722, 19853,  1288),
    ("tokenizer",      8, 2.5360, 4.3946,  480,  384,  3665,  1338),
    ("parquet",        3, 1.0068, 1.1387, 1551,  231, 12508,  1208),
    ("dataset text",   8, 1.9003, 2.7556,  401,  256,  2537,  1421),
    ("repo JSON",      1, 4.6476,12.5571,  722,  515,  1651,  2667),
    ("torch .pt cache",2, 1.0559, 1.2961, 1360,  349,  7220,  1831),
    (".npz",           1, 1.2505, 1.5095,  517,  559,  2041,  3157),
    ("tar of text",    3, 1.1127, 1.4767,  536,  830,  2160,  1451),
    ("imatrix .dat",   1, 1.4517, 1.4343,  923,  316,  2375,  1871),
]

def tier(db, dt):
    """dt in seconds; returns (break-even MB/s or None, verdict)"""
    if db <= 0:
        return None, "LOSS  bigger"
    if dt <= 0:
        return None, "FREE  smaller and faster"
    be = db / dt                      # bytes per second
    gb = be * 8 / 1e9
    if gb >= 100: return be, f"FREE below {gb:.0f} Gb/s"
    if gb >= 10:  return be, f"WIN below {gb:.0f} Gb/s"
    if gb >= 1:   return be, f"WIN below {gb:.1f} Gb/s"
    return be, f"STORAGE TIER (>{gb*1000:.0f} Mb/s and it costs time)"

print(f"{'class':16s} {'n':>2s} | {'stored B/chunk':>17s} | {'saved':>7s} | {'decode us':>13s} | {'cost us':>8s} | verdict")
print("-" * 108)
for name, n, xr, orr, xc, oc, xd, od in ROWS:
    xb, ob = CHUNK / xr, CHUNK / orr
    xt, ot = CHUNK / (xd * 1e6), CHUNK / (od * 1e6)
    db, dt = xb - ob, ot - xt
    be, v = tier(db, dt)
    print(f"{name:16s} {n:2d} | {xb:7.0f} -> {ob:6.0f} | {db:6.0f}B | {xt*1e6:5.1f} -> {ot*1e6:5.1f} | "
          f"{dt*1e6:+7.1f} | {v}")

print()
print("Upload side, the same question for the compress path (a chunk is compressed once):")
print(f"{'class':16s} | {'saved':>7s} | {'encode us':>13s} | {'cost us':>8s} | break-even link")
print("-" * 84)
for name, n, xr, orr, xc, oc, xd, od in ROWS:
    xb, ob = CHUNK / xr, CHUNK / orr
    xt, ot = CHUNK / (xc * 1e6), CHUNK / (oc * 1e6)
    db, dt = xb - ob, ot - xt
    be, v = tier(db, dt)
    print(f"{name:16s} | {db:6.0f}B | {xt*1e6:5.1f} -> {ot*1e6:5.1f} | {dt*1e6:+7.1f} | {v}")

print()
print("Beyond the chunk (whole-file or reference schemes, so the comparison is against what Xet stores):")
BEYOND = [
    ("fine-tune vs base", 1.1447, 2.94, 2275, 602),
    ("LoRA rank recovery", 1.1447, 11.78, 2275, 602),
    ("parquet transcode", 1.005, 1.749, 12508, 100),
    ("GGUF recipe", 1.0006, 1e9, 19853, 0.0),
]
for name, xr, orr, xd, od in BEYOND:
    xb, ob = CHUNK / xr, CHUNK / orr
    xt = CHUNK / (xd * 1e6)
    if od <= 0:
        print(f"{name:20s} | {xb:7.0f} -> {ob:8.2f} B/chunk | regeneration, not decompression: "
              f"8-95 s per model on one core, once")
        continue
    ot = CHUNK / (od * 1e6)
    db, dt = xb - ob, ot - xt
    be, v = tier(db, dt)
    print(f"{name:20s} | {xb:7.0f} -> {ob:6.0f} B/chunk | {db:6.0f}B saved | {dt*1e6:+7.1f} us | {v}")
