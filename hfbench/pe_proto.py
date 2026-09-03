#!/usr/bin/env python3
"""PROTOTYPE (scratch tree only, never /root/mzip): a PLANE_ENTROPY block type in mzip.
Byte-group by stride 2|4 (best of), code each plane as its own zstd-1 frame or store
raw if that does not help, tail raw. Trial-and-keep + per-block roundtrip verify.
In FAST mode, adopt when within 5% of the incumbent (the fast-decode path). Then
build mzip_pe (CLI), mzipx_pe (mode CLI), ut_pe; run suite; measure at 64K:
BALANCED byte-identity vs mzip_fixed4, FAST adoption + decode CPU."""
import os, shutil, subprocess, sys, hashlib, resource
SRC="/root/mzip"; T="/root/mzip-hfbench/petree"; B="/root/mzip-hfbench/build"; W="/root/mzip-hfbench/weights"
if os.path.exists(T): shutil.rmtree(T)
os.makedirs(f"{T}/ppmd"); os.makedirs(f"{T}/zstd_release/zstd-v1.5.6-win64/include")
for f in os.listdir(SRC):
    if f.endswith((".hpp",".h",".cpp",".c")): shutil.copy2(f"{SRC}/{f}",f"{T}/{f}")
for f in os.listdir(f"{SRC}/ppmd"): shutil.copy2(f"{SRC}/ppmd/{f}",f"{T}/ppmd/{f}")
os.symlink("/usr/include/zstd.h",f"{T}/zstd_release/zstd-v1.5.6-win64/include/zstd.h")
shutil.copy2("/root/mzip-hfbench/modeswp/mzipx.cpp",f"{T}/mzipx.cpp")
P=f"{T}/mzip.hpp"; s=open(P,encoding="utf-8",errors="surrogateescape").read()
def rep(old,new,name,first=False):
    global s
    if first:
        i=s.find(old); assert i>=0, name; s=s[:i]+new+s[i+len(old):]; return
    c=s.count(old); assert c==1, f"{name}: anchor x{c}"; s=s.replace(old,new)

# 0) self-contained codec, inserted before the first `namespace mzip {`
CODEC = r'''
// ============================================================================
// PLANE_ENTROPY (prototype 2026-09-02): byte-group by stride k in {2,4}, entropy-code
// each plane as its own zstd-1 frame (own literal table) or store it raw when coding
// does not help; tail (< k bytes) raw. Measured at HuggingFace's 64 KiB chunk size on
// 9 real weight inputs: +16% over Xet's bg4-lz4 with zstd-1 planes (+18% with huff0),
// within 0.7% of the order-0 bound; ~GB/s both ways. On weights, BWT+CM buys <=0.6%
// more at ~5000x the cost, so this is the FAST-mode path for numeric data.
// Payload: [k:1][flags:1 (bit i = plane i zstd-coded)][clen varint per plane][planes][tail]
// ============================================================================
namespace mzpe {
inline size_t wv(uint8_t* o, uint64_t v){ size_t n=0; while (v>=0x80){ o[n++]=(uint8_t)(v|0x80); v>>=7; } o[n++]=(uint8_t)v; return n; }
inline bool rv(const uint8_t*& p, const uint8_t* e, uint64_t& v){ v=0; int sh=0; while (p<e && sh<64){ uint8_t b=*p++; v|=(uint64_t)(b&0x7f)<<sh; if(!(b&0x80)) return true; sh+=7; } return false; }
inline std::vector<uint8_t> encode_k(const uint8_t* d, size_t n, unsigned k) {
    size_t L = n / k, tail = n - L*k;
    std::vector<uint8_t> plane(L), out; out.reserve(n + 64);
    std::vector<std::vector<uint8_t>> pay(k); uint8_t flags = 0;
    std::vector<uint8_t> cb(ZSTD_compressBound(L) + 16);
    for (unsigned j = 0; j < k; j++) {
        for (size_t i = 0; i < L; i++) plane[i] = d[i*k + j];
        size_t z = ZSTD_compress(cb.data(), cb.size(), plane.data(), L, 1);
        if (!ZSTD_isError(z) && z < L) { flags |= (uint8_t)(1u << j); pay[j].assign(cb.begin(), cb.begin() + z); }
        else pay[j] = plane;
    }
    out.push_back((uint8_t)k); out.push_back(flags);
    uint8_t vb[16];
    for (unsigned j = 0; j < k; j++) { size_t m = wv(vb, pay[j].size()); out.insert(out.end(), vb, vb + m); }
    for (unsigned j = 0; j < k; j++) out.insert(out.end(), pay[j].begin(), pay[j].end());
    out.insert(out.end(), d + L*k, d + n);
    return out;
}
inline std::vector<uint8_t> encode(const uint8_t* d, size_t n) {
    if (n < 64) return {};
    auto a = encode_k(d, n, 2), b = encode_k(d, n, 4);
    return (b.size() < a.size()) ? b : a;
}
inline std::vector<uint8_t> decode(const uint8_t* src, size_t sz, size_t n) {
    const uint8_t* p = src; const uint8_t* e = src + sz;
    if (sz < 2) return {};
    unsigned k = *p++; uint8_t flags = *p++;
    if (k != 2 && k != 4) return {};
    size_t L = n / k, tail = n - L*k;
    std::vector<uint64_t> cl(k);
    for (unsigned j = 0; j < k; j++) if (!rv(p, e, cl[j])) return {};
    std::vector<uint8_t> out(n), plane(L);
    for (unsigned j = 0; j < k; j++) {
        if (cl[j] > (uint64_t)(e - p)) return {};
        if (flags & (1u << j)) {
            size_t r = ZSTD_decompress(plane.data(), L, p, (size_t)cl[j]);
            if (ZSTD_isError(r) || r != L) return {};
        } else { if (cl[j] != L) return {}; memcpy(plane.data(), p, L); }
        p += cl[j];
        for (size_t i = 0; i < L; i++) out[i*k + j] = plane[i];
    }
    if ((size_t)(e - p) != tail) return {};
    memcpy(out.data() + L*k, p, tail);
    return out;
}
} // namespace mzpe
'''
rep("namespace mzip {\n", CODEC + "\nnamespace mzip {\n", "codec insert", first=True)

# 1) enum
rep("    PPMD = 0x3B,", "    PLANE_ENTROPY = 0x3C,       // byte-plane split + per-plane zstd-1/raw (numeric fast path; see mzpe)\n    PPMD = 0x3B,", "enum")
# 2) self-contained payload list
rep("                           // backstop blocks are also self-contained (payload = full stream) — MU saves ~9B/file\n",
    "                           block_type == static_cast<uint8_t>(BlockType::PLANE_ENTROPY) ||\n                           // backstop blocks are also self-contained (payload = full stream) — MU saves ~9B/file\n", "payload list")
# 3) name
rep('        case BlockType::PPMD: return "PPMD";', '        case BlockType::PPMD: return "PPMD";\n        case BlockType::PLANE_ENTROPY: return "PLANE_ENTROPY";', "name")
# 4) compact decoder
rep("            case BlockType::XZLIB: {\n                // backstop xz (liblzma) stream stored whole",
    "            case BlockType::PLANE_ENTROPY:\n                output = mzpe::decode(comp_data, comp_size, orig_size);\n                break;\n            case BlockType::XZLIB: {\n                // backstop xz (liblzma) stream stored whole", "compact decoder")
# 5) main decoder
rep("        } else if (type == BlockType::BROTLI) {\n            // brotli backstop decode\n",
    "        } else if (type == BlockType::PLANE_ENTROPY) {\n            auto decoded = mzpe::decode(block_data, block_size, block_original_size);\n            if (decoded.size() != block_original_size) { res.error = \"PLANE_ENTROPY decode failed\"; if (result) *result = res; return {}; }\n            if (!out_fits(output, out_pos, decoded.size())) { res.error = \"Block write exceeds output buffer (corrupt or malicious archive)\"; if (result) *result = res; return {}; }\n            memcpy(&output[out_pos], decoded.data(), decoded.size());\n            out_pos += decoded.size();\n        } else if (type == BlockType::BROTLI) {\n            // brotli backstop decode\n", "main decoder")
# 6) encoder trial, before the brotli backstop
rep("            // brotli-11 trial (ensemble backstop) -> BROTLI. Trial BOTH generic(0) + text(1) modes, keep smaller.\n",
    """            // PLANE_ENTROPY trial (prototype 2026-09-02): the numeric fast path. Trial-and-keep;
            // in FAST mode adopt when within 5% of the incumbent (it decodes at GB/s where BWT does ~9 MB/s).
            size_t pe_lim = this_block < 4096 ? this_block : 4096, pe_pr = 0;   // same printable test PPMd uses
            for (size_t i = 0; i < pe_lim; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) pe_pr++; }
            bool pe_texty = pe_lim > 0 && pe_pr * 100 >= pe_lim * 85;
            if (this_block >= 4096 && !pe_texty) {
                auto pe = mzpe::encode(block_data, this_block);
                if (!pe.empty() && pe.size() <= cap) {
                    bool adopt = pe.size() < cur;
                    if (!adopt && mode == CompressionMode::FAST && pe.size() <= cur + cur / 20) adopt = true;
                    if (adopt) {
                        auto rt = mzpe::decode(pe.data(), pe.size(), this_block);
                        if (rt.size() == this_block && std::memcmp(rt.data(), block_data, this_block) == 0) {
                            memcpy(preprocess_data, pe.data(), pe.size());
                            preprocess_size = pe.size();
                            analysis.type = BlockType::PLANE_ENTROPY;
                            use_generator = true;
                            cur = pe.size();
                        }
                    }
                }
            }
            // brotli-11 trial (ensemble backstop) -> BROTLI. Trial BOTH generic(0) + text(1) modes, keep smaller.
""", "encoder trial")
# 7) FAST early path: on numeric blocks, trial PLANE_ENTROPY BEFORE the backstop section and skip
#    the backstops (bwt9/zstd-19/xz/brotli/PPMd) when it succeeds -- that is what makes FAST fast.
rep("        if (this_block >= 64) {\n            size_t cap = ZSTD_compressBound(this_block);\n            size_t cur = preprocess_size;\n",
    """        // FAST EARLY PATH (prototype 2026-09-02): in FAST mode, numeric blocks go straight to
        // PLANE_ENTROPY and skip the whole backstop ensemble (bwt9 alone is ~87% of block time).
        // Measured: BALANCED untouched; FAST compress on weights drops from ~0.1 MB/s to the
        // zstd-1 class while keeping +10-33% over Xet's bg4-lz4 at its 64K chunk size.
        bool pe_fast_done = false;
        if (mode == CompressionMode::FAST && this_block >= 4096) {
            size_t fl = this_block < 4096 ? this_block : 4096, fp = 0;
            for (size_t i = 0; i < fl; i++) { uint8_t c = block_data[i]; if ((c >= 32 && c < 127) || c == 9 || c == 10 || c == 13) fp++; }
            if (!(fl > 0 && fp * 100 >= fl * 85)) {
                // v3: compare against whatever the specialist path produced (use_generator or raw);
                // a specialist result (e.g. DUAL_STREAM) used to bypass this path and drop into the
                // full backstop run -- measured: bwt9 still 87.8% of FAST block time. In FAST on a
                // numeric block, once a complete encoding exists, the backstop ensemble is skipped.
                auto pe = mzpe::encode(block_data, this_block);
                if (!pe.empty() && pe.size() < preprocess_size && pe.size() <= ZSTD_compressBound(this_block)) {
                    auto rt = mzpe::decode(pe.data(), pe.size(), this_block);
                    if (rt.size() == this_block && std::memcmp(rt.data(), block_data, this_block) == 0) {
                        memcpy(preprocess_data, pe.data(), pe.size());
                        preprocess_size = pe.size();
                        analysis.type = BlockType::PLANE_ENTROPY;
                        use_generator = true;
                    }
                }
                pe_fast_done = use_generator;   // complete encoding in hand (PE or specialist) -> skip backstops
            }
        }
        if (this_block >= 64 && !pe_fast_done) {
            size_t cap = ZSTD_compressBound(this_block);
            size_t cur = preprocess_size;
""", "fast early path")
open(P,"w",encoding="utf-8",errors="surrogateescape").write(s); print("patched scratch tree with PLANE_ENTROPY + FAST early path")

OBJS=f"{B}/libsais.o {B}/Ppmd7.o {B}/Ppmd7Enc.o {B}/Ppmd7Dec.o"; LIBS="-lzstd -lbrotlienc -lbrotlidec -lbrotlicommon -llzma"
for name,srcf,extra in (("mzip_pe","mzip_cli.cpp",""),("mzipx_pe","mzipx.cpp",""),("ut_pe","mzip_unit_tests.cpp","-D_USE_MATH_DEFINES")):
    r=subprocess.run(f"cd {T} && g++ -O3 -std=c++17 -march=native {extra} -I{T} -o {B}/{name} {srcf} {OBJS} {LIBS}",shell=True,capture_output=True,text=True)
    print(f"build {name}: {'OK' if r.returncode==0 else 'FAILED'}",[l for l in r.stderr.splitlines() if ' error' in l][:6])
    if r.returncode!=0: sys.exit(1)
r=subprocess.run(f"{B}/ut_pe",capture_output=True,text=True); print("suite:",[l for l in r.stdout.splitlines() if "Results:" in l or "FAIL" in l])

def sha(p): return hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
def cpu(): r=resource.getrusage(resource.RUSAGE_CHILDREN); return r.ru_utime+r.ru_stime
print("\n64K: BALANCED byte-identity vs mzip_fixed4 | FAST adoption, size, decode CPU MB/s")
for tag,p in (("MiniLM F32",f"{W}/MiniLM-L6-v2.slice"),("wbf16 BF16",f"{W}/weights-bf16.bin"),("Qwen BF16",f"{W}/Qwen2.5-0.5B.slice"),("gguf-f16",f"{W}/gguf-f16.slice")):
    raw=open(p,"rb").read(); N=len(raw); ident=0; sb=sf=0; dcb=dcf=0.0; ccb=ccf=0.0; pe_blocks=0; okf=True
    for i in range(0,N,65536):
        c=raw[i:i+65536]; open("/tmp/pe.in","wb").write(c)
        subprocess.run([f"{B}/mzip_fixed4","c","/tmp/pe.in","/tmp/pe.a","1"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        c0=cpu(); subprocess.run([f"{B}/mzipx_pe","c","/tmp/pe.in","/tmp/pe.b","1","balanced","0"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); ccb+=cpu()-c0
        ident+=int(sha("/tmp/pe.a")==sha("/tmp/pe.b")); sb+=os.path.getsize("/tmp/pe.b")
        c0=cpu(); pr=subprocess.run([f"{B}/mzipx_pe","c","/tmp/pe.in","/tmp/pe.f","1","fast","0"],capture_output=True,text=True,env={**os.environ,"MZIP_STATS":"1"}); ccf+=cpu()-c0
        sf+=os.path.getsize("/tmp/pe.f"); pe_blocks+=int("PLANE_ENTROPY" in pr.stderr)
        c0=cpu(); subprocess.run([f"{B}/mzipx_pe","d","/tmp/pe.b","/tmp/pe.br"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); dcb+=cpu()-c0
        c0=cpu(); subprocess.run([f"{B}/mzipx_pe","d","/tmp/pe.f","/tmp/pe.fr"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); dcf+=cpu()-c0
        okf=okf and open("/tmp/pe.fr","rb").read()==c
        # cross-decode: STOCK decoder must refuse a PLANE_ENTROPY archive cleanly (unknown type), never mis-decode
    xr=subprocess.run([f"{B}/mzip_fixed4","d","/tmp/pe.f","/tmp/pe.x"],capture_output=True,text=True)
    print(f"  {tag:11s} BALANCED identical {ident}/{(N+65535)//65536} ({sb:,} B, compress {N/1048576/max(ccb,1e-9):5.2f} MB/s) | FAST {sf:,} B ({N/sf:.4f}x), PE on {pe_blocks} blocks, compress {N/1048576/max(ccf,1e-9):6.1f} MB/s, decode {N/1048576/max(dcb,1e-9):6.1f} -> {N/1048576/max(dcf,1e-9):6.1f} MB/s (CLI incl exec), rt {'ok' if okf else 'FAIL'} | stock: {(xr.stderr.strip() or 'decoded?!')[:45]}",flush=True)
for x in ("/tmp/pe.in","/tmp/pe.a","/tmp/pe.b","/tmp/pe.f","/tmp/pe.br","/tmp/pe.fr","/tmp/pe.x"):
    if os.path.exists(x): os.remove(x)
print("ALLDONE_PE")
