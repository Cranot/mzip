/* FSE per 64 KiB chunk on a byte-plane file: the coder Fano already vendors, pointed at deltas.
 *
 * RESULT 122: order-0 Huffman loses to zstd on sparse fine-tune deltas because it cannot spend less than
 * one bit per symbol. FSE can. This measures FSE_compress on each 64 KiB chunk of a plane, table included
 * (FSE writes its normalised-count header into the output), and prints bits per SYMBOL of that plane.
 * Two planes per weight (lo, hi), so bits/weight = lo + hi.
 *
 *   cc -O2 -I<fse dir> fsedelta.c <fse dir>/fse_compress.c <fse dir>/entropy_common.c <fse dir>/hist.c -o fsedelta
 *   ./fsedelta plane.bin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fse.h"

#define CHUNK 65536

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: fsedelta plane.bin [chunk]\n"); return 2; }
    long CH = (argc > 2) ? atol(argv[2]) : CHUNK;
    #undef CHUNK
    #define CHUNK CH
    FILE* f = fopen(argv[1], "rb"); if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* buf = malloc(n); if (fread(buf, 1, n, f) != (size_t)n) { perror("read"); return 1; }
    fclose(f);
    size_t cap = FSE_compressBound(CHUNK) + 16; unsigned char* out = malloc(cap);
    unsigned long long total = 0, raw_chunks = 0, rle_chunks = 0; long nch = 0;
    for (long off = 0; off + CHUNK <= n; off += CHUNK, nch++) {
        size_t c = FSE_compress(out, cap, buf + off, CHUNK);
        if (FSE_isError(c)) { fprintf(stderr, "FSE error: %s\n", FSE_getErrorName(c)); return 1; }
        if (c == 0) { total += CHUNK; raw_chunks++; }        /* incompressible: stored raw */
        else if (c == 1) { total += 1; rle_chunks++; }       /* single symbol: RLE */
        else total += c;
    }
    double bps = 8.0 * (double)total / ((double)nch * CHUNK);
    printf("%s: chunks %ld, raw %llu, rle %llu, FSE bits/symbol %.4f\n", argv[1], nch, raw_chunks, rle_chunks, bps);
    return 0;
}
