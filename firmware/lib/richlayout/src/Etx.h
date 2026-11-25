#pragma once
// The .etx rich-text stream: what the EPUB converter writes and RichLayout
// reads. UTF-8 text (Latin-1 characters only, so the U8g2 fonts can show it)
// with words separated by single spaces, plus control codes:
//
//   0x1E b      paragraph start; b = Block::encode()
//   0x0F s      inline style from here on; s = 0x10 | kBold | kItalic
//               (every paragraph starts with style 0)
//   0x1F        hard line break (<br>)
//   0x0C        page break (chapter start, CSS page-break-before)
//   0x1A a b c  picture number (a & 63) << 12 | (b & 63) << 6 | (c & 63);
//               the payload bytes are 0x40..0x7F; a picture is its own block
//
// U+00A0 (no-break space) is kept and glues words together.

#include <stdint.h>

namespace etx {

constexpr char kPara = 0x1E;
constexpr char kStyle = 0x0F;
constexpr char kBreak = 0x1F;
constexpr char kPage = 0x0C;
constexpr char kImage = 0x1A;

constexpr uint8_t kBold = 1;
constexpr uint8_t kItalic = 2;

enum Kind : uint8_t { Body = 0, H1 = 1, H2 = 2, H3 = 3, Quote = 4, List = 5, Pre = 6 };
enum Align : uint8_t { Justify = 0, Left = 1, Center = 2, Right = 3 };

struct Block {
    uint8_t kind = Body;
    uint8_t align = Justify;
    bool indent = true;         // first-line indent
    bool spaceBefore = false;   // vertical gap before the paragraph

    uint8_t encode() const {
        return uint8_t(0x80 | (kind & 7) | ((align & 3) << 3) | (indent ? 0x20 : 0) | (spaceBefore ? 0x40 : 0));
    }
    static Block decode(uint8_t b) {
        Block k;
        k.kind = b & 7;
        k.align = (b >> 3) & 3;
        k.indent = b & 0x20;
        k.spaceBefore = b & 0x40;
        return k;
    }
};

inline uint8_t encodeStyle(uint8_t bits) { return uint8_t(0x10 | (bits & 3)); }

constexpr uint32_t kMaxImages = 1u << 18;
inline void encodeImage(uint32_t index, char out[3]) {
    out[0] = char(0x40 | ((index >> 12) & 63));
    out[1] = char(0x40 | ((index >> 6) & 63));
    out[2] = char(0x40 | (index & 63));
}
inline uint32_t decodeImage(const char* p) {
    return (uint32_t(p[0] & 63) << 12) | (uint32_t(p[1] & 63) << 6) | uint32_t(p[2] & 63);
}

// Font slot for a block kind and inline style: size class * 4 + style bits.
// Size class 0 = body, 1 = second-level heading, 2 = top-level heading.
// Headings are always bold.
inline int fontFor(uint8_t blockByte, uint8_t style) {
    Block b = Block::decode(blockByte);
    int size = b.kind == H1 ? 2 : b.kind == H2 ? 1 : 0;
    if (b.kind == H1 || b.kind == H2 || b.kind == H3) style |= kBold;
    return size * 4 + (style & 3);
}
constexpr int kFontSlots = 12;

}  // namespace etx
