#!/usr/bin/env python3
"""bench_corpus.py — the benchmark corpus, as DATA. Importable with no side effects.

WHY THIS IS ITS OWN MODULE
--------------------------
`benchmark_types.py` has no `if __name__ == "__main__"` guard: importing it RUNS the whole
benchmark. So the corpus table could not be reused by a second measurement or reporting tool
without either running the legacy benchmark or copying the table — and a copied corpus table is a
corpus that silently diverges from the one the reports describe. There is now exactly one.

WHAT IS DECLARED HERE, AND WHY IT MATTERS
-----------------------------------------
  * CORPUS_LABEL — this is the DEVELOPMENT corpus. Encoders in this project were built, tuned and
    debugged against these exact files. It is NOT a blind set and must never be reported as one.
    Files under train_corpus/ are excluded by construction (they are the dictionary TRAINING data;
    benchmarking on them inflates wins ~2-3x).
  * real — 1 if the bytes came from real-world data, 0 if generated. Generated files are kept
    because they exercise dispatchers, but a headline computed over them is not a headline about
    the world.
  * domain — the SOURCE GROUP. Needed because a per-file mean over this corpus is not a mean over
    content types: 12 'Other-langs' files and 6 raster files each carry one type's worth of
    meaning but 12x and 6x the file weight. Domains also give leave-one-domain-out, which answers
    "is the aggregate carried by one family?" — a question a single overall ratio cannot.

Nothing here touches the filesystem at import time except `glob`, which is read-only and is what
the legacy table already did.
"""

from __future__ import annotations

import glob
import os

ROOT = os.path.dirname(os.path.abspath(__file__))

# The corpus this project develops against. Stated explicitly so no report can imply otherwise.
CORPUS_LABEL = "DEV-2026-08"
CORPUS_IS_BLIND = False
CORPUS_NOTE = ("Development corpus: mzip's encoders were built and tuned against these files. "
               "Held out from mzip's dictionary training (train_corpus/), NOT held out from "
               "encoder development. Not a blind evaluation.")

RB = "real_bench/"
SM = "samples/64k/input/"
EX = "corpus_extra/"


def _g(pattern: str):
    """Sorted glob, relative to the repo root regardless of the caller's cwd."""
    n = len(ROOT) + 1
    return sorted(p[n:].replace("\\", "/") for p in glob.glob(os.path.join(ROOT, pattern)))


OTHER = [RB + f for f in ("clojure_core.clj", "csharp_list.cs", "elixir_genserver.ex", "julia_base.jl",
                          "kotlin_stdlib.kt", "lua_neovim.lua", "php_laravel.php", "ruby_rails.rb",
                          "rust_lib.rs", "scala_list.scala", "swift_stdlib.swift", "zig_std.zig")]

# type -> (files, real)   — byte-for-byte the table benchmark_types.py has always used.
TYPES = {
 "HTML": ([RB+"dashboard.html"], 1), "CSS": ([RB+"bootstrap.css", RB+"styles.css"], 1),
 "JavaScript": ([RB+"lodash.js", RB+"webpack.config.js"], 1),
 "TypeScript": ([RB+"handlers.ts", RB+"vscode_main.ts"] + _g(EX+"ts/*"), 1),
 "JSON": ([RB+"users.json", RB+"json_github_api.json"], 1), "XML": ([RB+"xml_maven.xml"], 1),
 "YAML": ([RB+"k8s_deployments.yaml", RB+"docker-compose.yml", RB+".github_workflows_ci.yml"], 1),
 "SQL": ([RB+"sql_schema.sql", RB+"users_dump.sql"], 1), "CSV": ([RB+"events.csv"], 1),
 "Markdown": ([RB+"api_docs.md", RB+"contributing.md", RB+"readme_large.md"], 1),
 "Log": ([RB+"apache_log_sample.log", RB+"app.log", RB+"nginx_access.log"], 1),
 "C/C++": ([RB+"linux_kernel.c", RB+"cpp_vector.hpp"], 1),
 "Python": ([RB+"django_models.py", RB+"flask_app.py", RB+"services.py"], 1),
 "Java": ([RB+"java_arraylist.java"], 1),
 "Go": ([RB+"go_http.go", RB+"handlers.go"], 1), "Other-langs": (OTHER, 1),
 "Dockerfile": ([RB+"Dockerfile"], 1), "Makefile": ([RB+"Makefile", RB+"linux_makefile"], 1),
 "Terraform": ([RB+"terraform_main.tf"], 1), "Env/dotfiles": ([RB+".env.example", RB+".gitignore"], 1),
 "Metrics": ([RB+"metrics.prom"], 1),
 "Shell": (_g(EX+"shell/*"), 1),
 "Numeric-temp": ([EX+"citytemp_float.bin"], 1), "Numeric-gyro": ([EX+"phonegyro_sensor.bin"], 1),
 "Numeric-gas": ([EX+"tsgas_series.bin"], 1), "Numeric-taxi": ([EX+"nyctaxi_cols.bin"], 1),
 # real x86-64 PE binaries -> 'MB' Bra86 BCJ filter. Permissive licences only; gzip.exe is GPL and
 # is therefore EXCLUDED from the released corpus.
 "Binary-x86": ([EX+"bin/"+b for b in ("libbrotlienc.dll", "libbrotlidec.dll", "libbrotlicommon.dll",
                                       "liblzma-5.dll", "libwinpthread-1.dll")], 1),
 "TOML(synth)": ([SM+"toml_config.toml"], 0), "INI(synth)": ([SM+"ini_config.ini"], 0),
 "GraphQL(synth)": ([SM+"graphql.graphql"], 0), "Email(synth)": ([SM+"email_headers.txt"], 0),
 "Protobuf(synth)": ([SM+"protobuf_like.bin"], 0), "Base64(synth)": ([SM+"base64.txt"], 0),
 "Protobuf-schema": ([EX+"misc/descriptor.proto"], 1),
 "reStructuredText": ([EX+"misc/cpython_intro.rst"], 1),
 "TSV": ([EX+"misc/events.tsv"], 1), "SVG": ([EX+"misc/example.svg"], 1),
 "NDJSON": ([EX+"misc/users.ndjson"], 1), "Diff/patch": ([EX+"misc/changes.patch"], 1),
 "YAML-CRD (k8s, large nested)": ([EX+"yaml/prom_bundle.yaml", EX+"yaml/certmgr.yaml"], 1),
 "FASTQ (genomics)": ([EX+"fastq/reads_10k.fastq"], 1),
 "WASM": ([EX+"wasm/sql-wasm.wasm", EX+"wasm/resvg_bg.wasm"], 1),
 "Binary-ARM/PPC": (_g(EX+"binarm/*"), 1),
 "Minified-JS": (_g(EX+"minified/*.min.js"), 1),
 "Minified-CSS": (_g(EX+"minified/*.min.css"), 1),
 "SourceMap": (_g(EX+"minified/*.map"), 1),
 "Syslog": (_g(EX+"syslog/*"), 0),
 "SQL-BigIntUnsigned": (_g(EX+"sqlbig/*"), 0),
 "Audio (WAV/PCM)": ([EX+"audio/test1.wav"], 1),
 "Scientific-Matrix (sparse)": ([EX+"grids/synthetic_fem.mtx"], 0),
 "Raster (BMP/PPM/TGA uncompressed)": ([EX+"raster/baboon.bmp", EX+"raster/baboon.ppm",
                                        EX+"raster/baboon.tga", EX+"raster/board.bmp",
                                        EX+"raster/board.tga", EX+"raster/fruits.pgm"], 1),
 "Delimited (semicolon/pipe CSV)": ([EX+"delim/stocks_semi.csv", EX+"delim/stocks_pipe.csv"], 1),
}

# type -> source group. Leave-one-domain-out over these answers "is the aggregate carried by one
# family?", which no single overall ratio can. Every key in TYPES must appear here; DOMAIN_OF()
# enforces that rather than defaulting, because a silent default would put a new type in a domain
# nobody chose and quietly reweight the statistic.
DOMAINS = {
 "markup-doc":   ["HTML", "CSS", "SVG", "Markdown", "reStructuredText"],
 "source-code":  ["JavaScript", "TypeScript", "C/C++", "Python", "Java", "Go", "Other-langs", "Shell"],
 "minified-web": ["Minified-JS", "Minified-CSS", "SourceMap"],
 "config-ser":   ["JSON", "XML", "YAML", "YAML-CRD (k8s, large nested)", "TOML(synth)", "INI(synth)",
                  "GraphQL(synth)", "Env/dotfiles", "Dockerfile", "Makefile", "Terraform",
                  "Protobuf-schema"],
 "sql":          ["SQL", "SQL-BigIntUnsigned"],
 "tabular":      ["CSV", "TSV", "NDJSON", "Delimited (semicolon/pipe CSV)", "Metrics"],
 "log":          ["Log", "Syslog"],
 "numeric":      ["Numeric-temp", "Numeric-gyro", "Numeric-gas", "Numeric-taxi"],
 "binary":       ["Binary-x86", "Binary-ARM/PPC", "WASM", "Protobuf(synth)"],
 "text-misc":    ["Email(synth)", "Base64(synth)", "Diff/patch"],
 "genomics":     ["FASTQ (genomics)"],
 "media":        ["Audio (WAV/PCM)", "Raster (BMP/PPM/TGA uncompressed)"],
 "scientific":   ["Scientific-Matrix (sparse)"],
}

_DOMAIN_OF = {t: d for d, ts in DOMAINS.items() for t in ts}


def domain_of(type_name: str) -> str:
    """Source group for a content type. Raises rather than defaulting — see DOMAINS."""
    try:
        return _DOMAIN_OF[type_name]
    except KeyError:
        raise KeyError(f"{type_name!r} has no domain in bench_corpus.DOMAINS. Add it explicitly; "
                       f"a default would silently reweight every per-domain statistic.") from None


def check_partition() -> list:
    """Return the list of problems with the type<->domain partition. Empty means consistent."""
    problems = []
    for t in TYPES:
        if t not in _DOMAIN_OF:
            problems.append(f"type in TYPES but not in any domain: {t!r}")
    for d, ts in DOMAINS.items():
        for t in ts:
            if t not in TYPES:
                problems.append(f"domain {d!r} lists a type that is not in TYPES: {t!r}")
    seen = {}
    for d, ts in DOMAINS.items():
        for t in ts:
            if t in seen:
                problems.append(f"type {t!r} is in two domains: {seen[t]!r} and {d!r}")
            seen[t] = d
    return problems


def entries():
    """Yield (type, file, real, domain) for every file that EXISTS. Missing files are reported by
    `missing()`, not silently dropped — a corpus that shrinks without saying so is how a benchmark
    starts describing a different set of files than its title claims."""
    for t, (files, real) in TYPES.items():
        d = domain_of(t)
        for f in files:
            if os.path.exists(os.path.join(ROOT, f)):
                yield t, f, int(real), d


def missing():
    """[(type, file)] for declared files that are absent — these are UNKNOWN, not zero."""
    out = []
    for t, (files, _real) in TYPES.items():
        for f in files:
            if not os.path.exists(os.path.join(ROOT, f)):
                out.append((t, f))
    return out


def summary() -> dict:
    ents = list(entries())
    return {
        "corpus_label": CORPUS_LABEL,
        "is_blind": CORPUS_IS_BLIND,
        "note": CORPUS_NOTE,
        "types_declared": len(TYPES),
        "domains": len(DOMAINS),
        "files_present": len(ents),
        "files_real": sum(1 for e in ents if e[2]),
        "files_missing": len(missing()),
        "partition_problems": check_partition(),
    }


if __name__ == "__main__":
    import json
    s = summary()
    print(json.dumps(s, indent=2))
    if s["partition_problems"]:
        print("\nPARTITION PROBLEMS:")
        for p in s["partition_problems"]:
            print("  " + p)
        raise SystemExit(1)
    m = missing()
    if m:
        print(f"\n{len(m)} declared file(s) absent on this box (reported as UNKNOWN, never as 0):")
        for t, f in m:
            print(f"  {t:42s} {f}")
