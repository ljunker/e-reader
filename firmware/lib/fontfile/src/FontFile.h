#pragma once
// Anti-aliased bitmap fonts in the .xff format written by 'xtconvert font'
// (format: tools/converter/src/xtconvert/font.py): several faces (style ×
// pixel size), glyphs with 4 ink levels. Platform independent; the reader
// keeps a family from the SD card in PSRAM and one built-in family in flash.

#include <stddef.h>
#include <stdint.h>

#include "ByteSource.h"

namespace xff {

enum Style : uint8_t { Regular = 0, Bold = 1, Italic = 2, BoldItalic = 3 };

struct Glyph {
    uint16_t cp;
    uint16_t advance;   // 1/16 pixel
    int8_t left;        // x of the bitmap from the pen
    int8_t top;         // rows of the bitmap above the baseline
    uint8_t width, height;
    uint32_t offset;    // of the bitmap, from the face's bitmap data
};
static_assert(sizeof(Glyph) == 12, "Glyph must match the file layout");

// Called for every pixel with ink: 1 = light .. 3 = full.
using PlotFn = void (*)(void* ctx, int x, int y, uint8_t ink);

// Next code point of UTF-8 text; a byte that does not start a valid sequence
// is taken as a Latin-1 character, so Latin-1 text files still show.
uint32_t nextCodePoint(const char*& p, const char* end);

class Face {
public:
    uint8_t style() const { return style_; }
    uint8_t size() const { return size_; }
    int ascent() const { return ascent_; }
    int descent() const { return descent_; }

    // The glyph of a code point, '?' if the face has none.
    const Glyph* glyph(uint32_t cp) const;
    const uint8_t* bitmap(const Glyph& g) const { return bitmaps_ + g.offset; }

    // Width in pixels of the UTF-8 text s[0..n).
    int width(const char* s, size_t n) const;
    int width(const char* s) const;
    // Draw with the pen at (x, baseline); returns the advance (= width()).
    int draw(int x, int baseline, const char* s, size_t n, PlotFn plot, void* ctx) const;

private:
    friend class Font;
    const Glyph* find(uint32_t cp) const;
    const Glyph* glyphs_ = nullptr;
    const uint8_t* bitmaps_ = nullptr;
    uint16_t count_ = 0;
    uint8_t style_ = 0, size_ = 0, ascent_ = 0, descent_ = 0;
    const Glyph* fallback_ = nullptr;
};

class Font {
public:
    static constexpr int kMaxFaces = 32;

    Font() = default;
    ~Font() { unload(); }
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Reads the whole file into one buffer (PSRAM on the device).
    bool load(io::ByteSource& src, uint32_t size);
    // Uses data that stays in place (the built-in font in flash).
    bool attach(const uint8_t* data, uint32_t size);
    void unload();
    bool loaded() const { return data_ != nullptr; }

    const char* name() const { return name_; }
    int faceCount() const { return faceCount_; }
    const Face& face(int i) const { return faces_[i]; }

    // Number of sizes of a style, and the face with the index-th smallest
    // size (clamped). A missing style falls back: bold italic → italic →
    // regular, bold → regular. Null if the font has no faces.
    int sizeCount(uint8_t style) const;
    const Face* find(uint8_t style, int index) const;
    // The face of a style whose size is closest to px.
    const Face* nearest(uint8_t style, int px) const;

private:
    bool parse(const uint8_t* d, uint32_t size);
    uint8_t available(uint8_t style) const;
    const uint8_t* data_ = nullptr;
    bool owned_ = false;
    char name_[25] = {};
    Face faces_[kMaxFaces];
    int faceCount_ = 0;
};

}  // namespace xff
