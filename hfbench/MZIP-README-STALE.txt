THE PUBLISHED NUMBERS ARE STALE, AND THEY UNDERSTATE MZIP
==========================================================
measured 2026-09-01, Hetzner box, /root/mzip-hfbench/build/
subject: /root/mzip HEAD e5fe7e8, built with full backstops.
input:   first 10 MiB (10,485,760 B) of /root/mzip/enwik9 -> /root/enwik9lab/enwik9
NOTE ON UNITS: internal/CM_SAUCE.md's 2,427,131 at 1.852 bpc back-solves to
n = 10,485,760, i.e. the benchmark is 10 MiB, NOT 10,000,000. Using 10 MB gives
2,321,998 and is NOT comparable to the published figure. This trap cost one
wrong conclusion before it was caught.

RIG VALIDATION (do this before believing any of the below)
-----------------------------------------------------------
bwtcm.cpp rebuilt here on Linux and run on the same 10 MiB prefix returns
  comp = 2,427,131 B  1.8518 bpc
which is BYTE-IDENTICAL to the figure recorded in internal/CM_SAUCE.md from the
original Windows measurement. The rig reproduces the project's own historical
number exactly, so the deltas below are not instrument error.

THE FINDING
-----------
                          | README claims | MEASURED now | delta
--------------------------|---------------|--------------|--------------
mzip, enwik9 10 MiB       |     2,671,197 |    2,427,140 | -244,057 (-9.14%)
"beats brotli:11 by"      |         5.9%  |      14.16%  | understated 2.4x
"beats bzip2:9 by"        |        14.4%  |      20.57%  | understated
vs xz -9e                 |     (unstated)|      14.64%  |
vs zstd -19               |     (unstated)|      16.94%  |
vs gzip -9                |     (unstated)|      37.11%  |

Measured competitor bytes on the identical 10 MiB input:
  brotli -q11  2,827,632 | xz -9e 2,843,328 | zstd -19 2,922,193
  bzip2 -9     3,055,521 | gzip -9 3,859,579

WHY THE README IS STALE — the CM sauce SHIPPED and nobody updated the table
----------------------------------------------------------------------------
internal/CM_SAUCE.md recorded bwtcm beating "mzip 2,671,197" by 9.1% at this
size. That 2,671,197 is exactly the number the README still publishes. Since
then the CM backend WAS productized into cm_backend.hpp, and mzip now returns
2,427,140 -- nine bytes above bwtcm's 2,427,131, i.e. mzip has absorbed
essentially the entire 9.1% win. The README simply predates the adoption.
=> The public headline understates the product by 9.14% on its own flagship
   benchmark. Fixing it is free and it is the highest-value README edit
   available. It also makes the "when NOT to use mzip" section's framing of
   prose performance unnecessarily modest.

COROLLARY — "cm_backend delivers nothing" WOULD HAVE BEEN A WRONG CONCLUSION
-----------------------------------------------------------------------------
On HF wikitext, cm_backend (2,454,790), bwt9 (2,454,793), bwtcm (2,454,791) and
shipped mzip (2,454,799) agree within NINE BYTES, which reads like a backend
that earns nothing. It is the opposite: bwt9 and the CM path have CONVERGED
because the CM win was already banked. The marginal trial shows ~0 precisely
because the gain is already in the baseline. Do not "remove the redundant CM
trial" on the strength of that near-tie -- measure against pre-CM output first.

WHAT IS STILL UNBANKED
-----------------------
lpaq1 on the same 10 MiB enwik9 prefix (10 MB variant): 2,071,319 vs mzip
2,321,998 -- a direct context model is still ~10.8% ahead on this data, and
2.3-14.8% ahead across all 8 HF shapes tested. The BWT+CM line has converged;
the remaining headroom needs the direct-CM architecture, not more BWT work.

TWO MORE STALE/UNTRACKED ITEMS FOUND WHILE READING
----------------------------------------------------
- internal/MZIP_APPLY.md says mzip has "535 ZSTD call-sites". Current count in
  mzip.hpp is 86 (`grep -c ZSTD_compress`). Do not re-quote 535.
- cm_backend is referenced at only SEVEN sites in mzip.hpp and every one is
  compress_bwt/decompress_bwt -- CM is wired ONLY as the post-BWT text coder.
  The numeric/columnar/template streams MZIP_APPLY ranks highest are still
  entropy-coded by zstd. That part of the upgrade is genuinely un-applied.
  BUT: measured headroom is SMALLEST on tabular-numeric (2.28%) and largest on
  text/code (8.98%/14.77%), which inverts MZIP_APPLY's ranking. Re-derive that
  priority list against measurement before spending on it.
- internal/ is deliberately .gitignore'd (line 77, with a rationale comment).
  4.5 MB of development record incl. STATUS.md (153 KB), single copy on one
  disk, no remote. Deliberate, but it is the single-copy pattern; back it up.

SCALE BEHAVIOUR (wikitext-103, all three points mutually comparable)
---------------------------------------------------------------------
  16 MiB slice: mzip 3,693,778  lpaq 3,401,669  -> lpaq ahead 7.91%
  24 MiB slice: mzip 5,426,460  lpaq 5,034,963  -> lpaq ahead 7.22%
mzip beats 7z PPMd at both (16MB: 3,755,068; 24MB: 5,588,922).
The deficit is NOT growing across the 16 MB DEFAULT_BLOCK_SIZE boundary, so
multi-block splitting is not costing what was feared. 32 MiB point pending.
