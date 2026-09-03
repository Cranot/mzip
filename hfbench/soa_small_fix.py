#!/usr/bin/env python3
"""Make the F32 bg4 win NATIVE: below 1 MiB, trial stride-2/4 SoA with the real
coder instead of trusting the zstd-1 proxy. Applied to a COPY of the tree
(/root/mzip-hfbench/soatree), never to /root/mzip. Exact-string replace;
refuses if the anchor is not unique."""
import os, shutil, subprocess, sys

SRC_TREE = "/root/mzip"
T = "/root/mzip-hfbench/soatree"
B = "/root/mzip-hfbench/build"

# fresh scratch copy of sources (patched NUM_EXTRACT guard included, since it is in the working tree)
if os.path.exists(T): shutil.rmtree(T)
os.makedirs(f"{T}/ppmd"); os.makedirs(f"{T}/zstd_release/zstd-v1.5.6-win64/include")
for f in os.listdir(SRC_TREE):
    if f.endswith((".hpp", ".h", ".cpp", ".c")): shutil.copy2(f"{SRC_TREE}/{f}", f"{T}/{f}")
for f in os.listdir(f"{SRC_TREE}/ppmd"): shutil.copy2(f"{SRC_TREE}/ppmd/{f}", f"{T}/ppmd/{f}")
os.symlink("/usr/include/zstd.h", f"{T}/zstd_release/zstd-v1.5.6-win64/include/zstd.h")

P = f"{T}/mzip.hpp"
src = open(P, encoding="utf-8", errors="surrogateescape").read()

OLD = """        // pay for a full recursive compress only if the proxy says a transform helps by >2%
        if (best_ci >= 0 && !ZSTD_isError(raw_proxy) && best_proxy < (raw_proxy * 98) / 100) {
            const SoaCand& c = CANDS[best_ci];
            auto t = soa_apply(data, size, c.tid, c.W, c.cols);
            auto inner = compress_impl(t.data(), size, zstd_level, block_size, nullptr, mode, /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false, /*try_image=*/false);
            if (!inner.empty()) {
                std::vector<uint8_t> ms;
                ms.reserve(inner.size() + 16);
                ms.push_back('M'); ms.push_back('S');
                ms.push_back(c.tid); ms.push_back(c.W); ms.push_back(c.cols);
                uint8_t vbuf[16]; size_t vn = write_uvarint_buf(vbuf, size);
                ms.insert(ms.end(), vbuf, vbuf + vn);
                ms.insert(ms.end(), inner.begin(), inner.end());
                // safe-by-construction: only adopt if the 'MS' stream reconstructs exactly
                auto back = decompress(ms.data(), ms.size(), nullptr);
                if (back.size() == size && std::memcmp(back.data(), data, size) == 0)
                    ms_format = std::move(ms);
            }
        }
    }
"""

NEW = """        // pay for a full recursive compress only if the proxy says a transform helps by >2%
        // -- EXCEPT on small blocks. SMALL-BLOCK SoA TRIAL (2026-09-02): zstd-1 is a poor
        // proxy for BWT+CM's benefit from de-interleaving at small sizes. Measured at
        // 64 KiB chunks (HuggingFace/Xet's real compression granularity): the proxy saw
        // only +0.13..+0.37% for stride-4 on F32 weights and pruned it on 54-69 of 128
        // chunks per file, while the real coder gained ~8% from the same transform
        // (MiniLM-L6-v2 7,624,205 -> 7,006,963; gpt2 7,687,116 -> 7,047,442). At 1 MiB
        // the same proxy sees +9% and passes, so the gate is only wrong below ~1 MiB.
        // Below SOA_SMALL_TRIAL_MAX, trial stride-2 and stride-4 with the REAL coder
        // regardless of proxy and keep the smallest that round-trips. Cost: up to two
        // extra recursive compresses of a <=1 MiB block. Format unchanged (same 'MS').
        constexpr size_t SOA_SMALL_TRIAL_MAX = 1024u * 1024;
        int trial[3]; int ntrial = 0;
        if (best_ci >= 0 && !ZSTD_isError(raw_proxy) && best_proxy < (raw_proxy * 98) / 100) trial[ntrial++] = best_ci;
        if (size <= SOA_SMALL_TRIAL_MAX) {
            for (int fc = 0; fc < 2; ++fc) {            // CANDS[0]={0,2,0}, CANDS[1]={0,4,0}
                bool dup = false;
                for (int k = 0; k < ntrial; ++k) if (trial[k] == fc) dup = true;
                if (!dup) trial[ntrial++] = fc;
            }
        }
        for (int k = 0; k < ntrial; ++k) {
            const SoaCand& c = CANDS[trial[k]];
            auto t = soa_apply(data, size, c.tid, c.W, c.cols);
            auto inner = compress_impl(t.data(), size, zstd_level, block_size, nullptr, mode, /*try_soa=*/false, /*try_tabular=*/false, /*try_sql=*/false, /*try_bcj=*/false, /*try_log=*/false, /*try_yaml=*/false, /*try_fastq=*/false, /*try_image=*/false);
            if (inner.empty()) continue;
            std::vector<uint8_t> ms;
            ms.reserve(inner.size() + 16);
            ms.push_back('M'); ms.push_back('S');
            ms.push_back(c.tid); ms.push_back(c.W); ms.push_back(c.cols);
            uint8_t vbuf[16]; size_t vn = write_uvarint_buf(vbuf, size);
            ms.insert(ms.end(), vbuf, vbuf + vn);
            ms.insert(ms.end(), inner.begin(), inner.end());
            // safe-by-construction: only adopt if the 'MS' stream reconstructs exactly
            auto back = decompress(ms.data(), ms.size(), nullptr);
            if (back.size() == size && std::memcmp(back.data(), data, size) == 0) {
                if (ms_format.empty() || ms.size() < ms_format.size()) ms_format = std::move(ms);
            }
        }
    }
"""

n = src.count(OLD)
if n != 1:
    print(f"REFUSING: anchor matched {n} times"); sys.exit(1)
open(P, "w", encoding="utf-8", errors="surrogateescape").write(src.replace(OLD, NEW))
print("patched scratch copy:", P)

LIBS = "-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
OBJS = f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"
for name, srcf, extra in (("mzip_soa", "mzip_cli.cpp", ""),
                          ("ut_soa", "mzip_unit_tests.cpp", "-D_USE_MATH_DEFINES")):
    cmd = f"cd {T} && g++ -O3 -std=c++17 -march=native {extra} -I{T} -o {B}/{name} {srcf} {OBJS} {LIBS}"
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    errs = [l for l in r.stderr.splitlines() if " error" in l]
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}", errs[:3])
print("ALLDONE_SOABUILD")
