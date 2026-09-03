import sys, io
P = "/root/mzip-hfbench/scratch-prof-src/mzip.hpp"
s = io.open(P, "r", encoding="utf-8", newline="").read()

BS = chr(92)          # a single backslash
TAB = BS + "t"        # the two characters  \ t   -> a C escape in the emitted source
NL  = BS + "n"

def sub(old, new, tag):
    global s
    c = s.count(old)
    if c != 1:
        sys.exit("ANCHOR %s count=%d (want 1)" % (tag, c))
    s = s.replace(old, new)

def tr(fields):
    # build a C string literal "MZTRACE\tF1\tF2...\n"
    return '"MZTRACE' + TAB + TAB.join(fields) + NL + '"'

# ---- 1) MS gate: dump proxy numbers + the already-computed BLOCKS size -------
fmt = tr(["MS_GATE", "size=%zu", "raw_proxy=%zu", "best_proxy=%zu",
          "best_ci=%d", "proxy_gain_pct=%.3f", "blocks_bytes=%zu"])
old = "        if (best_ci >= 0 && !ZSTD_isError(raw_proxy) && best_proxy < (raw_proxy * 98) / 100) {"
new = ('        if (std::getenv("MZIP_TRACE")) std::fprintf(stderr, ' + fmt + ', '
       '(size_t)size, (size_t)raw_proxy, (size_t)best_proxy, best_ci, '
       '(raw_proxy && raw_proxy != (size_t)-1) ? '
       '(100.0*((double)raw_proxy-(double)best_proxy)/(double)raw_proxy) : 0.0, '
       '(size_t)out_pos);\n' + old)
sub(old, new, "MS_GATE")

# ---- 2) zstd-19 backstop result ---------------------------------------------
fmt = tr(["ZSTD19BS", "n=%zu", "z=%zu", "cur=%zu"])
old = "                if (!ZSTD_isError(z)) cur = z;"
new = (old + '\n                if (std::getenv("MZIP_TRACE")) std::fprintf(stderr, ' + fmt +
       ', (size_t)this_block, (size_t)(ZSTD_isError(z)?0:z), (size_t)cur);')
sub(old, new, "ZSTD19BS")

# ---- 3) bwt9 backstop candidate vs incumbent --------------------------------
fmt = tr(["BWT9BS", "n=%zu", "b9=%zu", "cur_before=%zu", "memo=%d"])
old = "                const std::vector<uint8_t>& b9 = bwt9_memo_valid ? bwt9_memo : b9_local;"
new = (old + '\n                if (std::getenv("MZIP_TRACE")) std::fprintf(stderr, ' + fmt +
       ', (size_t)this_block, (size_t)b9.size(), (size_t)cur, (int)bwt9_memo_valid);')
sub(old, new, "BWT9BS")

# ---- 4) xz -9e trial: report its size whether or not it was adopted ---------
# A scope-exit tracer, so we see the losing size too. It must NOT wrap the
# MZ_TIMED lambda, so it is declared AFTER xpos and reads it by pointer.
fmt = tr(["XZ9E", "n=%zu", "xz_bytes=%zu", "cur_after=%zu"])
old = ("                std::vector<uint8_t> xb(xbound);\n"
       "                size_t xpos = 0;")
new = (old + "\n"
       "                struct MzTraceXz { size_t* xp; size_t* cu; size_t n;\n"
       "                    ~MzTraceXz(){ if (std::getenv(\"MZIP_TRACE\")) std::fprintf(stderr, "
       + fmt + ", n, *xp, *cu); } };\n"
       "                MzTraceXz _mztxz{&xpos, &cur, (size_t)this_block};")
sub(old, new, "XZ9E")

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("TRACE PATCH OK")
