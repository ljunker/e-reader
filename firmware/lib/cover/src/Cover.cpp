#include "Cover.h"

#include <string.h>

namespace cover {

namespace {

void put16(std::vector<uint8_t>& o, uint16_t v) {
    o.push_back(uint8_t(v));
    o.push_back(uint8_t(v >> 8));
}

void put32(std::vector<uint8_t>& o, uint32_t v) {
    put16(o, uint16_t(v));
    put16(o, uint16_t(v >> 16));
}

uint16_t get16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t get32(const uint8_t* p) { return get16(p) | uint32_t(get16(p + 2)) << 16; }

}  // namespace

std::vector<uint8_t> encode(uint32_t sourceSize, const image::Gray* picture) {
    std::vector<uint8_t> out = {'C', 'O', 'V', '1'};
    put32(out, sourceSize);
    if (!picture || picture->w <= 0 || picture->h <= 0) {
        put32(out, 0);
        put32(out, 0);
        return out;
    }
    // both from the original, so the thumbnail is dithered at its own size
    image::Gray full = image::fit(*picture, kFullW, kFullH);
    image::Gray thumb = image::fit(*picture, kThumbW, kThumbH);
    put16(out, uint16_t(full.w));
    put16(out, uint16_t(full.h));
    put16(out, uint16_t(thumb.w));
    put16(out, uint16_t(thumb.h));
    std::vector<uint8_t> a = image::pack2bpp(full);
    out.insert(out.end(), a.begin(), a.end());
    std::vector<uint8_t> b = image::pack2bpp(thumb);
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

bool readInfo(const uint8_t header[kHeaderSize], uint32_t sourceSize, uint32_t fileSize, Info& out) {
    if (memcmp(header, "COV1", 4) != 0 || get32(header + 4) != sourceSize) return false;
    Info i;
    i.fullW = get16(header + 8);
    i.fullH = get16(header + 10);
    i.thumbW = get16(header + 12);
    i.thumbH = get16(header + 14);
    if (i.fullW > kFullW || i.fullH > kFullH || i.thumbW > kThumbW || i.thumbH > kThumbH) return false;
    uint32_t need = kHeaderSize + (i.present() ? i.fullBytes() + i.thumbBytes() : 0);
    if (fileSize < need) return false;
    out = i;
    return true;
}

image::Gray unpack(const uint8_t* packed, int w, int h) {
    static const uint8_t level[4] = {0, 85, 170, 255};
    image::Gray g;
    g.w = w;
    g.h = h;
    g.px.resize(size_t(w) * h);
    for (size_t i = 0; i < g.px.size(); i++) g.px[i] = level[(packed[i >> 2] >> (6 - 2 * (i & 3))) & 3];
    return g;
}

}  // namespace cover
