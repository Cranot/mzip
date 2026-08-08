// ppmd_backend.hpp -- standalone PPMd variant H (Ppmd7 / PPMdH) codec wrapper.
//
// Wraps the public-domain LZMA-SDK Ppmd7 model + its 7z range coder
// (Ppmd7z_* enc/dec) to in-memory buffers, for use as an mzip backstop.
//
// The underlying codec files (ppmd/Ppmd7.c, ppmd/Ppmd7Enc.c, ppmd/Ppmd7Dec.c
// and their headers) are PUBLIC DOMAIN:
//   "Ppmd7.c -- PPMdH codec  2023-09-07 : Igor Pavlov : Public domain
//    This code is based on PPMd var.H (2001): Dmitry Shkarin : Public domain"
//
// Compile the C sources with:  gcc -O2 -c ppmd/Ppmd7.c ppmd/Ppmd7Enc.c ppmd/Ppmd7Dec.c
// and link the resulting .o files into the mzip g++ line.
//
// Wire format produced by compress(): the raw Ppmd7z range-coder stream and
// nothing else (it begins with a 0x00 byte, which Ppmd7z_RangeDec_Init consumes,
// and ends with a 5-byte flush).  The number of symbols to decode (orig_n) and
// the model parameters (order, memMiB) are NOT stored here -- the caller (mzip)
// must persist them in its own container and pass them back to decompress().
//
// SECURITY NOTE for untrusted streams: decode-time heap use is a pure function
// of memMiB (the model arena is Ppmd7_Alloc'd up front to memMiB<<20 bytes,
// +0..3 bytes alignment), NOT of the compressed bytes.  A caller MUST bound
// memMiB when decoding attacker-controlled data.  decompress() never reads past
// comp_n (EOF yields 0 bytes) and never writes more than orig_n bytes.

#ifndef PPMD_BACKEND_HPP
#define PPMD_BACKEND_HPP

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <vector>

extern "C" {
#include "ppmd/Ppmd7.h"
}

namespace ppmdbk {

// --- allocator (malloc/free) ------------------------------------------------
inline void* ppmd_alloc(ISzAllocPtr, size_t size) { return std::malloc(size); }
inline void  ppmd_free (ISzAllocPtr, void* p)      { std::free(p); }

// --- byte sink: appends to a std::vector<uint8_t> ---------------------------
// IByteOut MUST be the first member so a (OutBuf*) alias of the vtable pointer
// is valid.  The vtable pointer is passed const; we cast it away to reach state.
struct OutBuf {
    IByteOut vt;
    std::vector<uint8_t>* out;
};
inline void OutBuf_Write(IByteOutPtr pp, Byte b) {
    OutBuf* s = (OutBuf*)pp;            // C-style cast strips the interface's const
    s->out->push_back(b);
}

// --- byte source: reads from a fixed buffer, 0 past EOF ----------------------
struct InBuf {
    IByteIn vt;
    const uint8_t* data;
    size_t pos;
    size_t size;
};
inline Byte InBuf_Read(IByteInPtr pp) {
    InBuf* s = (InBuf*)pp;
    if (s->pos < s->size) return s->data[s->pos++];
    return 0;                            // EOF/short-read: feed zeros (memory-safe)
}

// Clamp (order, memMiB) into the codec's legal range and return the arena size
// in bytes.  Both compress and decompress MUST clamp identically so a stream is
// decoded with the exact parameters it was encoded with.
inline UInt32 clamp_params(unsigned& order, uint32_t memMiB) {
    if (order < PPMD7_MIN_ORDER) order = PPMD7_MIN_ORDER;
    if (order > PPMD7_MAX_ORDER) order = PPMD7_MAX_ORDER;
    uint64_t sz = (uint64_t)memMiB << 20;
    if (sz < (uint64_t)PPMD7_MIN_MEM_SIZE) sz = PPMD7_MIN_MEM_SIZE;
    if (sz > (uint64_t)PPMD7_MAX_MEM_SIZE) sz = PPMD7_MAX_MEM_SIZE;
    return (UInt32)sz;
}

// Compress n bytes of data with PPMd var.H at the given order and memory budget.
// Returns the raw range-coder stream, or an empty vector on allocation failure.
// (An empty input still yields the 5-byte range-coder flush -- non-empty output
// for n==0 is expected and decodes back to zero bytes.)
inline std::vector<uint8_t>
compress(const uint8_t* data, size_t n, unsigned order, uint32_t memMiB) {
    std::vector<uint8_t> out;
    UInt32 memSize = clamp_params(order, memMiB);

    static const ISzAlloc alloc = { ppmd_alloc, ppmd_free };
    CPpmd7 p;
    Ppmd7_Construct(&p);
    if (!Ppmd7_Alloc(&p, memSize, &alloc)) return out;   // OOM -> empty
    Ppmd7_Init(&p, order);

    OutBuf ob;
    ob.vt.Write = &OutBuf_Write;
    ob.out = &out;
    p.rc.enc.Stream = &ob.vt;

    Ppmd7z_Init_RangeEnc(&p);
    if (n) Ppmd7z_EncodeSymbols(&p, data, data + n);
    Ppmd7z_Flush_RangeEnc(&p);

    Ppmd7_Free(&p, &alloc);
    return out;
}

// Decompress exactly orig_n bytes from a stream produced by compress() with the
// SAME (order, memMiB).  Returns the decoded bytes; a returned size != orig_n
// signals a decode error (corrupt/truncated stream) -- the caller must check.
inline std::vector<uint8_t>
decompress(const uint8_t* comp, size_t comp_n, size_t orig_n,
           unsigned order, uint32_t memMiB) {
    std::vector<uint8_t> out;
    UInt32 memSize = clamp_params(order, memMiB);

    static const ISzAlloc alloc = { ppmd_alloc, ppmd_free };
    CPpmd7 p;
    Ppmd7_Construct(&p);
    if (!Ppmd7_Alloc(&p, memSize, &alloc)) return out;   // OOM -> empty
    Ppmd7_Init(&p, order);

    InBuf ib;
    ib.vt.Read = &InBuf_Read;
    ib.data = comp;
    ib.pos  = 0;
    ib.size = comp_n;
    p.rc.dec.Stream = &ib.vt;

    if (!Ppmd7z_RangeDec_Init(&p.rc.dec)) {              // bad leading byte / all-ones
        Ppmd7_Free(&p, &alloc);
        return out;                                      // empty -> error
    }

    out.reserve(orig_n);
    for (size_t i = 0; i < orig_n; ++i) {
        int sym = Ppmd7z_DecodeSymbol(&p);
        if (sym < 0) break;                              // SYM_END / SYM_ERROR -> short = error
        out.push_back((uint8_t)sym);
    }

    Ppmd7_Free(&p, &alloc);
    return out;
}

} // namespace ppmdbk

#endif // PPMD_BACKEND_HPP
