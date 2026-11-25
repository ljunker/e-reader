#pragma once
// Streaming DEFLATE decoder (RFC 1951), written for this project.
// Keeps a 32 KB history window on the heap (PSRAM on the device).

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <vector>

namespace zip {

// Pull compressed bytes into dst (at most cap); return 0 when the input ends.
using InputFn = std::function<size_t(uint8_t* dst, size_t cap)>;
// Receive decompressed bytes; return false to stop early.
using OutputFn = std::function<bool(const uint8_t* data, size_t n)>;

enum class InflateResult { Ok, Stopped, BadData, Truncated, NoMemory };

InflateResult inflate(const InputFn& in, const OutputFn& out);

// Raw DEFLATE (zlib = false) or zlib-wrapped data (2-byte header, Adler-32 not
// checked) from memory into a vector. Fails when the output exceeds maxOut.
InflateResult inflateBuffer(const uint8_t* data, size_t n, bool zlib, std::vector<uint8_t>& out,
                            size_t maxOut);

}  // namespace zip
