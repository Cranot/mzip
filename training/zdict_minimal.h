// Minimal zdict.h - just the functions we need for dictionary training
// From zstd v1.5.6
#pragma once
#include <stddef.h>

extern "C" {

// Train a dictionary from an array of samples
// Returns: size of dictionary stored in dictBuffer, or error code
size_t ZDICT_trainFromBuffer(void* dictBuffer, size_t dictBufferCapacity,
                             const void* samplesBuffer, const size_t* samplesSizes,
                             unsigned nbSamples);

// Check if result is an error
unsigned ZDICT_isError(size_t errorCode);

// Get error name
const char* ZDICT_getErrorName(size_t errorCode);

} // extern "C"
