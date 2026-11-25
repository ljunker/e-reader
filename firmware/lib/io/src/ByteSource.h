#pragma once
// Minimal I/O interfaces shared by the platform-independent libraries.
// The firmware implements them on top of SD files, the host tests on memory.

#include <stddef.h>
#include <stdint.h>

namespace io {

// Random-access input.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    // Read exactly n bytes at offset; return false on short read / error.
    virtual bool readAt(uint32_t offset, uint8_t* dst, size_t n) = 0;
};

// Sequential output.
class Writer {
public:
    virtual ~Writer() = default;
    // Append n bytes; return false on error (card full, ...).
    virtual bool write(const void* data, size_t n) = 0;
};

}  // namespace io
