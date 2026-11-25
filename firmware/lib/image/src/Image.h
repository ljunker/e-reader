#pragma once
// Image decoding for EPUB pictures: PNG and baseline JPEG to 8-bit gray,
// downscaling and dithering to the 2-bit format of .xtp pages.
// Platform independent.

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace image {

struct Gray {
    int w = 0, h = 0;
    std::vector<uint8_t> px;   // row-major, 0 = black, 255 = white
};

enum class Format { Unknown, Png, Jpeg, Gif };
Format sniff(const uint8_t* d, size_t n);

// Memory guards for the device (8 MB PSRAM): decoded gray pixels, and the
// unfiltered PNG rows. JPEGs count after scaled decoding, so large photos
// still work; very large PNGs are skipped.
constexpr size_t kMaxPixels = 4u * 1000 * 1000;
constexpr size_t kMaxRawBytes = 5u * 1000 * 1000;

// PNG: all colour types and bit depths, not interlaced. Transparency is
// composed onto white.
bool decodePng(const uint8_t* d, size_t n, Gray& out);
// Baseline JPEG (no progressive). Decodes at the smallest 1/2, 1/4 or 1/8
// scale that is still at least as large as fit(w, h, maxW, maxH) needs.
bool decodeJpeg(const uint8_t* d, size_t n, Gray& out, int maxW, int maxH);
// Dispatches on the file signature.
bool decode(const uint8_t* d, size_t n, Gray& out, int maxW, int maxH);

// Box-filter downscale so the image fits into maxW × maxH, keeping the aspect
// ratio. Never enlarges.
Gray fit(const Gray& in, int maxW, int maxH);

// Floyd–Steinberg to 4 levels, packed 4 pixels per byte, first pixel in the
// top bits, pixels numbered row-major without row padding (like .xtp pages):
// 0 = black .. 3 = white. Size (w * h + 3) / 4.
std::vector<uint8_t> pack2bpp(const Gray& g);

}  // namespace image
