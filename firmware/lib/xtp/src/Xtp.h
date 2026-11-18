#pragma once
// Reader for .xtp page files (written by tools/converter). Platform independent:
// the caller supplies a ByteSource for random access reads.
// Format: see tools/converter/src/xtconvert/xtp.py

#include <stddef.h>
#include <stdint.h>

namespace xtp {

class ByteSource {
public:
    virtual ~ByteSource() = default;
    // Read exactly n bytes at offset; return false on short read / error.
    virtual bool readAt(uint32_t offset, uint8_t* dst, size_t n) = 0;
};

enum class Error { None, Io, BadMagic, Unsupported, BadPage, Corrupt };

struct Header {
    uint16_t version = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t bpp = 0;
    uint8_t flags = 0;
    uint32_t pageCount = 0;
    uint32_t indexOffset = 0;
    char title[45] = {0};
};

class Reader {
public:
    explicit Reader(ByteSource& src) : src_(src) {}

    Error open();
    const Header& header() const { return h_; }
    // Bytes one decoded page occupies (2 bpp packed).
    size_t pageBytes() const { return size_t(h_.width) * h_.height / 4; }

    // Decode page `index` into `out` (pageBytes() long). `scratch` must hold
    // the compressed page; scratchSize is its capacity.
    Error readPage(uint32_t index, uint8_t* out, uint8_t* scratch, size_t scratchSize);

    // Largest compressed page size seen in the index (for sizing scratch).
    Error maxCompressedSize(uint32_t& result);

private:
    ByteSource& src_;
    Header h_;
};

// PackBits-like RLE decode. Returns number of bytes written, or -1 on error
// (output overflow or truncated input).
long rleDecode(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap);

// Pixel value 0..3 (0 = black, 3 = white) at x,y in a packed page.
inline uint8_t pixel(const uint8_t* page, uint16_t width, uint16_t x, uint16_t y) {
    uint32_t i = uint32_t(y) * width + x;
    return (page[i >> 2] >> (6 - 2 * (i & 3))) & 3;
}

}  // namespace xtp
