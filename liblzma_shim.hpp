// liblzma_shim.hpp — minimal extern "C" decls for liblzma (xz's engine). No dev header here; link the DLL.
// Lets mzip add xz-quality LZMA as an ensemble candidate (flips large-repetitive SQL vs lzma_opt2).
#ifndef LIBLZMA_SHIM_HPP
#define LIBLZMA_SHIM_HPP
#include <cstddef>
#include <cstdint>
extern "C" {
    size_t lzma_stream_buffer_bound(size_t uncompressed_size);
    int    lzma_easy_buffer_encode(uint32_t preset, int check, void* allocator,
                                   const uint8_t* in, size_t in_size,
                                   uint8_t* out, size_t* out_pos, size_t out_size);
    int    lzma_stream_buffer_decode(uint64_t* memlimit, uint32_t flags, void* allocator,
                                     const uint8_t* in, size_t* in_pos, size_t in_size,
                                     uint8_t* out, size_t* out_pos, size_t out_size);
}
#define MZ_LZMA_PRESET_EXTREME 0x80000000u
#define MZ_LZMA_CHECK_NONE 0
#define MZ_LZMA_OK 0
#endif
