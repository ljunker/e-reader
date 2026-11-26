#pragma once
// Cover cache file (.cov) for the library list and the sleep screen.
// Platform independent.
//
//   "COV1", u32 source file size, u16 full w, u16 full h, u16 thumb w, u16 thumb h,
//   full picture (2 bpp, image::pack2bpp), thumbnail (2 bpp).
//
// A book without a cover gets a file with all sizes 0, so the reader does not
// look for one again. All integers little-endian.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "Image.h"

namespace cover {

constexpr size_t kHeaderSize = 16;
constexpr int kFullW = 300, kFullH = 400;    // the whole panel in portrait
constexpr int kThumbW = 42, kThumbH = 56;    // one row of the library list

struct Info {
    uint16_t fullW = 0, fullH = 0, thumbW = 0, thumbH = 0;
    bool present() const { return fullW && fullH && thumbW && thumbH; }
    uint32_t fullBytes() const { return (uint32_t(fullW) * fullH + 3) / 4; }
    uint32_t thumbBytes() const { return (uint32_t(thumbW) * thumbH + 3) / 4; }
    uint32_t fullOffset() const { return kHeaderSize; }
    uint32_t thumbOffset() const { return kHeaderSize + fullBytes(); }
};

// The whole file. `picture` may be null or empty for "no cover".
std::vector<uint8_t> encode(uint32_t sourceSize, const image::Gray* picture);

// Checks the header against the book's current size and the file length.
bool readInfo(const uint8_t header[kHeaderSize], uint32_t sourceSize, uint32_t fileSize, Info& out);

// 2 bpp packed pixels (0 = black .. 3 = white) back to 8-bit gray, e.g. to
// make a cover from the first page of an .xtp file.
image::Gray unpack(const uint8_t* packed, int w, int h);

}  // namespace cover
