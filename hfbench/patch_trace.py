import sys, io
P = "/root/mzip-hfbench/scratch-prof-src/mzip.hpp"
s = io.open(P, "r", encoding="utf-8", newline="").read()

def sub(old, new, tag):
    global s
    c = s.count(old)
    if c != 1:
        sys.exit("ANCHOR %s count=%d (want 1)" % (tag, c))
    s = s.replace(old, new)

# 1) MS gate: dump the proxy numbers and the already-known BLOCKS size
old = "        if (best_ci >= 0 && !ZSTD_isError(raw_proxy) && best_proxy < (raw_proxy * 98) / 100) {"
new = ('        if (std::getenv("MZIP_TRACE")) std::fprintf(stderr,'
       ' "MZTRACE\tMS_GATE\tsize=%zu\traw_proxy=%zu\tbest_proxy=%zu\tbest_ci=%d\tproxy_gain_pct=%.3f\tblocks_bytes=%zu\n",'
       ' (size_t)size, (size_t)raw_proxy, (size_t)best_proxy, best_ci,'
       ' (raw_proxy && raw_proxy != (size_t)-1) ? (100.0*((double)raw_proxy-(double)best_proxy)/(double)raw_proxy) : 0.0,'
       ' (size_t)out_pos);\n' + old)
sub(old, new, "MS_GATE")

# 2) zstd-19 backstop result
old = "                if (!ZSTD_isError(z)) cur = z;"
new = ('                if (!ZSTD_isError(z)) cur = z;\n'
       '                if (std::getenv("MZIP_TRACE")) std::fprintf(stderr, "MZTRACE\tZSTD19BS\tn=%zu\tz=%zu\tcur=%zu\n",'
       ' (size_t)this_block, (size_t)(ZSTD_isError(z)?0:z), (size_t)cur);')
sub(old, new, "ZSTD19BS")

# 3) bwt9 backstop candidate vs incumbent
old = "                const std::vector<uint8_t>& b9 = bwt9_memo_valid ? bwt9_memo : b9_local;"
new = (old + '\n                if (std::getenv("MZIP_TRACE")) std::fprintf(stderr,'
       ' "MZTRACE\tBWT9BS\tn=%zu\tb9=%zu\tcur_before=%zu\tmemo=%d\n",'
       ' (size_t)this_block, (size_t)b9.size(), (size_t)cur, (int)bwt9_memo_valid);')
sub(old, new, "BWT9BS")

# 4) xz -9e trial: report its output size whether or not it was adopted (scope-exit tracer)
old = """                std::vector<uint8_t> xb(xbound);
                size_t xpos = 0;"""
new = """                std::vector<uint8_t> xb(xbound);
                size_t xpos = 0;
                struct MzTraceXz { size_t* xp; size_t* cu; size_t n;
                    ~MzTraceXz(){ if (std::getenv("MZIP_TRACE")) std::fprintf(stderr,
                        "MZTRACE\tXZ9E\tn=%zu\txz_bytes=%zu\tcur_after=%zu\n", n, *xp, *cu); } };
                MzTraceXz _mztxz{&xpos, &cur, (size_t)this_block};"""
sub(old, new, "XZ9E")

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("TRACE PATCH OK")
