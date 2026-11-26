#include "FontFile.h"

#include <stdlib.h>
#include <string.h>

namespace xff {

static uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
static uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

uint32_t nextCodePoint(const char*& p, const char* end) {
    const uint8_t* s = (const uint8_t*)p;
    uint8_t c = s[0];
    int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (len > 1 && end - p >= len) {
        uint32_t cp = c & (0xFF >> (len + 1));
        bool ok = true;
        for (int i = 1; i < len && ok; i++) {
            ok = (s[i] & 0xC0) == 0x80;
            cp = cp << 6 | (s[i] & 0x3F);
        }
        static const uint32_t kMin[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (ok && cp >= kMin[len] && cp <= 0x10FFFF) {
            p += len;
            return cp;
        }
    }
    p++;
    return c;   // ASCII, or a stray byte read as Latin-1
}

const Glyph* Face::find(uint32_t cp) const {
    if (cp > 0xFFFF) return nullptr;
    int lo = 0, hi = count_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t c = glyphs_[mid].cp;
        if (c == cp) return &glyphs_[mid];
        if (c < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return nullptr;
}

const Glyph* Face::glyph(uint32_t cp) const {
    const Glyph* g = find(cp);
    return g ? g : fallback_;
}

int Face::width(const char* s, size_t n) const {
    const char* end = s + n;
    uint32_t pen = 0;
    while (s < end) {
        const Glyph* g = glyph(nextCodePoint(s, end));
        if (g) pen += g->advance;
    }
    return int((pen + 8) >> 4);
}

int Face::width(const char* s) const { return width(s, strlen(s)); }

int Face::draw(int x, int baseline, const char* s, size_t n, PlotFn plot, void* ctx) const {
    const char* end = s + n;
    uint32_t pen = 0;
    while (s < end) {
        const Glyph* g = glyph(nextCodePoint(s, end));
        if (!g) continue;
        int gx = x + int((pen + 8) >> 4) + g->left;
        int gy = baseline - g->top;
        const uint8_t* bm = bitmap(*g);
        int stride = (g->width + 3) / 4;
        for (int r = 0; r < g->height; r++) {
            const uint8_t* row = bm + r * stride;
            for (int c = 0; c < g->width; c++) {
                uint8_t ink = (row[c >> 2] >> (6 - 2 * (c & 3))) & 3;
                if (ink) plot(ctx, gx + c, gy + r, ink);
            }
        }
        pen += g->advance;
    }
    return int((pen + 8) >> 4);
}

void Font::unload() {
    if (owned_) free((void*)data_);
    data_ = nullptr;
    owned_ = false;
    faceCount_ = 0;
    name_[0] = 0;
}

bool Font::load(io::ByteSource& src, uint32_t size) {
    unload();
    if (size < 32) return false;
    uint8_t* d = (uint8_t*)malloc(size);
    if (!d) return false;
    if (!src.readAt(0, d, size) || !parse(d, size)) {
        free(d);
        faceCount_ = 0;
        return false;
    }
    data_ = d;
    owned_ = true;
    return true;
}

bool Font::attach(const uint8_t* d, uint32_t size) {
    unload();
    if (!parse(d, size)) {
        faceCount_ = 0;
        return false;
    }
    data_ = d;
    return true;
}

bool Font::parse(const uint8_t* d, uint32_t size) {
    if (size < 32 || memcmp(d, "XFF1", 4) != 0 || ((uintptr_t)d & 3)) return false;
    int count = d[4];
    if (count == 0 || count > kMaxFaces || 32 + 16u * count > size) return false;
    for (int i = 0; i < count; i++) {
        const uint8_t* e = d + 32 + 16 * i;
        uint32_t n = u16(e + 4), goff = u32(e + 8), boff = u32(e + 12);
        if (e[0] > 3 || e[1] == 0 || (goff & 3) || uint64_t(goff) + 12ull * n > boff || boff > size) return false;
        const Glyph* gs = (const Glyph*)(d + goff);
        uint32_t prev = 0;
        for (uint32_t k = 0; k < n; k++) {
            const Glyph& g = gs[k];
            uint64_t end = uint64_t(boff) + g.offset + uint64_t((g.width + 3) / 4) * g.height;
            if (end > size || (k && g.cp <= prev)) return false;
            prev = g.cp;
        }
        if (i > 0) {   // sorted by style, then size
            const uint8_t* p = e - 16;
            if (p[0] > e[0] || (p[0] == e[0] && p[1] >= e[1])) return false;
        }
        Face& f = faces_[i];
        f.glyphs_ = gs;
        f.bitmaps_ = d + boff;
        f.count_ = uint16_t(n);
        f.style_ = e[0];
        f.size_ = e[1];
        f.ascent_ = e[2];
        f.descent_ = e[3];
        f.fallback_ = f.find('?');
    }
    faceCount_ = count;
    memcpy(name_, d + 8, 24);
    name_[24] = 0;
    return true;
}

uint8_t Font::available(uint8_t style) const {
    if (style > 3) style = Regular;
    if (style == Regular || sizeCount(style) > 0) return style;
    if (style == BoldItalic) return sizeCount(Italic) > 0 ? Italic : Regular;
    return Regular;
}

int Font::sizeCount(uint8_t style) const {
    int n = 0;
    for (int i = 0; i < faceCount_; i++) n += faces_[i].style_ == style;
    return n;
}

const Face* Font::find(uint8_t style, int index) const {
    style = available(style);
    int n = sizeCount(style);
    if (n == 0) return faceCount_ ? &faces_[0] : nullptr;
    if (index < 0) index = 0;
    if (index >= n) index = n - 1;
    for (int i = 0; i < faceCount_; i++)
        if (faces_[i].style_ == style && index-- == 0) return &faces_[i];
    return nullptr;
}

const Face* Font::nearest(uint8_t style, int px) const {
    style = available(style);
    const Face* best = nullptr;
    for (int i = 0; i < faceCount_; i++) {
        const Face& f = faces_[i];
        if (f.style_ != style) continue;
        if (!best || abs(f.size_ - px) < abs(best->size_ - px)) best = &f;
    }
    return best ? best : (faceCount_ ? &faces_[0] : nullptr);
}

}  // namespace xff
