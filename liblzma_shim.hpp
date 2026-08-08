// liblzma_shim.hpp — minimal extern "C" decls for liblzma (xz's engine). No dev header here; link the DLL.
// Lets mzip add xz-quality LZMA as an ensemble candidate (flips large-repetitive SQL vs lzma_opt2).
#ifndef LIBLZMA_SHIM_HPP
#define LIBLZMA_SHIM_HPP
#include <cstddef>
#include <cstdint>
// lzma_filter chain entry (matches liblzma's `lzma_filter`: {lzma_vli id; void *options;}).
typedef struct { uint64_t id; void* options; } mz_lzma_filter;
extern "C" {
    size_t lzma_stream_buffer_bound(size_t uncompressed_size);
    int    lzma_easy_buffer_encode(uint32_t preset, int check, void* allocator,
                                   const uint8_t* in, size_t in_size,
                                   uint8_t* out, size_t* out_pos, size_t out_size);
    int    lzma_stream_buffer_decode(uint64_t* memlimit, uint32_t flags, void* allocator,
                                     const uint8_t* in, size_t* in_pos, size_t in_size,
                                     uint8_t* out, size_t* out_pos, size_t out_size);
    // Filter-chain encode (lets us prepend an arch BCJ filter to LZMA2). Decode needs NOTHING new:
    // lzma_stream_buffer_decode reads the filter chain from the .xz header and inverts BCJ automatically.
    int    lzma_stream_buffer_encode(mz_lzma_filter* filters, int check, void* allocator,
                                     const uint8_t* in, size_t in_size,
                                     uint8_t* out, size_t* out_pos, size_t out_size);
    // Fills an lzma_options_lzma from a preset. Returns lzma_bool: nonzero = ERROR, 0 = OK.
    // Caller passes a byte buffer >= sizeof(lzma_options_lzma) (~112 B on x64; we over-allocate 256).
    unsigned char lzma_lzma_preset(void* options, uint32_t preset);
}
#define MZ_LZMA_PRESET_EXTREME 0x80000000u
#define MZ_LZMA_CHECK_NONE 0
#define MZ_LZMA_OK 0
// Filter IDs (liblzma lzma12.h / bcj.h). ARM64 needs liblzma >=5.4, RISCV >=5.6 (bundled DLL is 5.8.3).
#define MZ_LZMA_FILTER_LZMA2    0x21ULL
#define MZ_LZMA_FILTER_POWERPC  0x05ULL
#define MZ_LZMA_FILTER_ARM      0x07ULL
#define MZ_LZMA_FILTER_ARMTHUMB 0x08ULL
#define MZ_LZMA_FILTER_ARM64    0x0AULL
#define MZ_LZMA_FILTER_RISCV    0x0BULL
#define MZ_LZMA_VLI_UNKNOWN     (~0ULL)
#endif
