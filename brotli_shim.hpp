// brotli_shim.hpp — minimal extern "C" declarations for libbrotlienc/dec (no dev headers installed;
// link the DLLs directly). Lets mzip add brotli as an ensemble backstop candidate (trial-and-keep).
#ifndef BROTLI_SHIM_HPP
#define BROTLI_SHIM_HPP
#include <cstddef>
#include <cstdint>
extern "C" {
    size_t BrotliEncoderMaxCompressedSize(size_t input_size);
    int    BrotliEncoderCompress(int quality, int lgwin, int mode,
                                 size_t input_size, const uint8_t* input_buffer,
                                 size_t* encoded_size, uint8_t* encoded_buffer);
    int    BrotliDecoderDecompress(size_t encoded_size, const uint8_t* encoded_buffer,
                                   size_t* decoded_size, uint8_t* decoded_buffer);
}
#define MZ_BROTLI_QUALITY 11
#define MZ_BROTLI_WINDOW  24
#define MZ_BROTLI_GENERIC 0
#define MZ_BROTLI_DECODE_SUCCESS 1   // BROTLI_DECODER_RESULT_SUCCESS
#endif
