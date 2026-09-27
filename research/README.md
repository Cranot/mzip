# research/

Research code behind mzip's context-mixing work and the enwik9 / Hutter Prize experiments.
**None of it is part of the library**: `mzip.hpp` and `mzip_amalgamated.hpp` include nothing from here.

- `lpaq_x.cpp`, `lpaq_iso.cpp`, `ref_lpaq1.cpp`, `dclm_rig*.cpp` — derived from Matt Mahoney's lpaq1,
  **GPL-2.0-or-later**; `bwtcm2.cpp` is **GPL-3.0-or-later**; `bwtcm.cpp` is derived from bzip3,
  **LGPL-3.0-or-later**. Each file carries its notice; the repository-wide list is in `../NOTICE`.
- `wikifield.cpp`, `classify_enwik.cpp`, `e1_oracle.cpp`, `field_probe.py`, `gen_dict.cpp` — enwik9 analysis tools.
- `bwt5_*.cpp`, `bwt9_phase_probe.cpp`, `entropy_probe.cpp` — one-off probes of mzip's BWT and backstop coders.
  Build from the repository root, e.g. `g++ -O2 -std=c++17 research/bwt5_winner.cpp -o bwt5_winner`.
- `*.sh` — run records of past experiments. They `cd` into the original workstation checkout and call binaries
  built there, so they document what was run; they do not run as-is in a fresh clone.
