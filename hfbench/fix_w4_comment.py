#!/usr/bin/env python3
"""Comment-only correction in /root/mzip/mzip.hpp: 'W=2 never won' -> measured
figures (+42 B bf16, +643 B gguf-f16 per 8 MiB). No binary change."""
P = "/root/mzip/mzip.hpp"
s = open(P, encoding="utf-8", errors="surrogateescape").read()
old = ("            // W=4 ONLY (2026-09-02): measured at 64K, stride-2 never beat stride-4 on any\n"
       "            // slice (F32, BF16, GGUF F16 within 46 B), and each forced trial is a full inner\n"
       "            // compress -- dropping W=2 halves the fix's cost (2.5x -> ~1.5x of stock).\n")
new = ("            // W=4 ONLY (2026-09-02): measured at 64K over 8 MiB slices, keeping stride-2 as a\n"
       "            // second forced trial bought 0 B on F32, 42 B on BF16 and 643 B on GGUF F16\n"
       "            // (<= 0.012%), while each forced trial is a full inner compress -- dropping W=2\n"
       "            // halves the fix's cost (2.5x -> ~1.5x of stock). Trade accepted, measured.\n")
n = s.count(old)
assert n == 1, f"anchor matched {n}"
open(P, "w", encoding="utf-8", errors="surrogateescape").write(s.replace(old, new))
print("comment corrected")
