#include "Image.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "Inflate.h"
#include "tjpgd/tjpgd.h"

namespace image {

Format sniff(const uint8_t* d, size_t n) {
    if (n >= 8 && memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0) return Format::Png;
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return Format::Jpeg;
    if (n >= 4 && memcmp(d, "GIF8", 4) == 0) return Format::Gif;
    return Format::Unknown;
}

static inline uint8_t luma(int r, int g, int b) { return uint8_t((r * 77 + g * 150 + b * 29) >> 8); }
static inline uint8_t onWhite(int v, int a) { return uint8_t((v * a + 255 * (255 - a) + 127) / 255); }

// ---------- PNG ----------

static uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

static inline uint8_t paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return uint8_t(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

bool decodePng(const uint8_t* d, size_t n, Gray& out) {
    if (sniff(d, n) != Format::Png) return false;
    uint32_t w = 0, h = 0;
    int depth = 0, ctype = -1, interlace = 0;
    uint8_t pal[256][3] = {{0}};
    uint8_t palAlpha[256];
    memset(palAlpha, 255, sizeof palAlpha);
    struct Span {
        size_t off, len;
    };
    std::vector<Span> idat;   // IDAT chunks are inflated in place, not copied
    size_t pos = 8;
    while (pos + 12 <= n) {
        uint32_t len = be32(d + pos);
        const uint8_t* type = d + pos + 4;
        const uint8_t* data = d + pos + 8;
        if (len > n - pos - 12) return false;
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = be32(data);
            h = be32(data + 4);
            depth = data[8];
            ctype = data[9];
            interlace = data[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            for (uint32_t i = 0; i < len / 3 && i < 256; i++) memcpy(pal[i], data + 3 * i, 3);
        } else if (!memcmp(type, "tRNS", 4) && ctype == 3) {
            for (uint32_t i = 0; i < len && i < 256; i++) palAlpha[i] = data[i];
        } else if (!memcmp(type, "IDAT", 4)) {
            if (len) idat.push_back({pos + 8, len});
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + len;
    }
    if (w == 0 || h == 0 || uint64_t(w) * h > kMaxPixels || interlace != 0) return false;
    int ch;
    switch (ctype) {
        case 0: ch = 1; if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return false; break;
        case 3: ch = 1; if (depth != 1 && depth != 2 && depth != 4 && depth != 8) return false; break;
        case 2: ch = 3; if (depth != 8 && depth != 16) return false; break;
        case 4: ch = 2; if (depth != 8 && depth != 16) return false; break;
        case 6: ch = 4; if (depth != 8 && depth != 16) return false; break;
        default: return false;
    }
    size_t bitsPP = size_t(ch) * depth;
    size_t stride = (w * bitsPP + 7) / 8;
    size_t bpp = std::max<size_t>(1, bitsPP / 8);
    size_t need = h * (stride + 1);
    if (need > kMaxRawBytes || idat.empty()) return false;
    std::vector<uint8_t> raw;
    raw.reserve(need);
    // walk the IDAT chunks as one zlib stream, skipping its 2-byte header
    size_t chunk = 0, inChunk = 2;
    if (idat[0].len < 2 || (d[idat[0].off] & 0x0F) != 8) return false;
    auto r = zip::inflate(
        [&](uint8_t* dst, size_t cap) -> size_t {
            while (chunk < idat.size() && inChunk >= idat[chunk].len) {
                chunk++;
                inChunk = 0;
            }
            if (chunk >= idat.size()) return 0;
            size_t k = std::min(cap, idat[chunk].len - inChunk);
            memcpy(dst, d + idat[chunk].off + inChunk, k);
            inChunk += k;
            return k;
        },
        [&](const uint8_t* p, size_t k) {
            k = std::min(k, need - raw.size());
            raw.insert(raw.end(), p, p + k);
            return raw.size() < need;   // stop once all rows are in
        });
    if ((r != zip::InflateResult::Ok && r != zip::InflateResult::Stopped) || raw.size() < need) return false;

    // undo the row filters in place
    for (uint32_t y = 0; y < h; y++) {
        uint8_t* row = &raw[y * (stride + 1)];
        uint8_t f = row[0];
        uint8_t* cur = row + 1;
        const uint8_t* prev = y ? &raw[(y - 1) * (stride + 1) + 1] : nullptr;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0;
            int b = prev ? prev[i] : 0;
            int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            switch (f) {
                case 0: break;
                case 1: cur[i] += a; break;
                case 2: cur[i] += b; break;
                case 3: cur[i] += (a + b) >> 1; break;
                case 4: cur[i] += paeth(a, b, c); break;
                default: return false;
            }
        }
    }

    out.w = int(w);
    out.h = int(h);
    out.px.assign(size_t(w) * h, 255);
    int mask = (1 << std::min(depth, 8)) - 1;
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t* row = &raw[y * (stride + 1) + 1];
        uint8_t* o = &out.px[size_t(y) * w];
        for (uint32_t x = 0; x < w; x++) {
            auto sample = [&](int c) -> int {
                if (depth == 8) return row[x * ch + c];
                if (depth == 16) return row[(x * ch + c) * 2];
                size_t bit = size_t(x) * depth;
                int shift = 8 - depth - int(bit & 7);
                return (row[bit >> 3] >> shift) & mask;
            };
            switch (ctype) {
                case 0: o[x] = uint8_t(depth < 8 ? sample(0) * 255 / mask : sample(0)); break;
                case 3: {
                    int i = sample(0);
                    o[x] = onWhite(luma(pal[i][0], pal[i][1], pal[i][2]), palAlpha[i]);
                    break;
                }
                case 2: o[x] = luma(sample(0), sample(1), sample(2)); break;
                case 4: o[x] = onWhite(sample(0), sample(1)); break;
                case 6: o[x] = onWhite(luma(sample(0), sample(1), sample(2)), sample(3)); break;
            }
        }
    }
    return true;
}

// ---------- JPEG (TJpgDec) ----------

namespace {
struct JpegCtx {
    const uint8_t* d;
    size_t n, pos;
    Gray* out;
};

size_t jpegIn(JDEC* jd, uint8_t* buf, size_t len) {
    auto* c = static_cast<JpegCtx*>(jd->device);
    size_t k = std::min(len, c->n - c->pos);
    if (buf) memcpy(buf, c->d + c->pos, k);
    c->pos += k;
    return k;
}

int jpegOut(JDEC* jd, void* bitmap, JRECT* r) {
    auto* c = static_cast<JpegCtx*>(jd->device);
    Gray& g = *c->out;
    const uint8_t* src = static_cast<const uint8_t*>(bitmap);
    int rw = r->right - r->left + 1;
    for (int y = r->top; y <= r->bottom; y++) {
        if (y >= g.h) break;
        for (int x = r->left; x <= r->right; x++)
            if (x < g.w) g.px[size_t(y) * g.w + x] = src[(y - r->top) * rw + (x - r->left)];
    }
    return 1;
}

void fitSize(int w, int h, int maxW, int maxH, int& tw, int& th) {
    double f = std::min({1.0, double(maxW) / w, double(maxH) / h});
    tw = std::max(1, int(w * f));
    th = std::max(1, int(h * f));
}
}  // namespace

bool decodeJpeg(const uint8_t* d, size_t n, Gray& out, int maxW, int maxH) {
    JpegCtx ctx{d, n, 0, &out};
    JDEC jd;
    std::vector<uint8_t> pool(16384);
    if (tjd_prepare(&jd, jpegIn, pool.data(), pool.size(), &ctx) != JDR_OK) return false;
    int tw, th;
    fitSize(jd.width, jd.height, maxW, maxH, tw, th);
    int s = 3;
    while (s > 0 && ((jd.width >> s) < tw || (jd.height >> s) < th)) s--;
    out.w = jd.width >> s;
    out.h = jd.height >> s;
    if (out.w == 0 || out.h == 0 || size_t(out.w) * out.h > kMaxPixels) return false;
    out.px.assign(size_t(out.w) * out.h, 255);
    return tjd_decomp(&jd, jpegOut, uint8_t(s)) == JDR_OK;
}

bool decode(const uint8_t* d, size_t n, Gray& out, int maxW, int maxH) {
    switch (sniff(d, n)) {
        case Format::Png: return decodePng(d, n, out);
        case Format::Jpeg: return decodeJpeg(d, n, out, maxW, maxH);
        default: return false;
    }
}

// ---------- scaling and dithering ----------

Gray fit(const Gray& in, int maxW, int maxH) {
    if (in.w <= maxW && in.h <= maxH) return in;
    Gray out;
    fitSize(in.w, in.h, maxW, maxH, out.w, out.h);
    out.px.resize(size_t(out.w) * out.h);
    for (int oy = 0; oy < out.h; oy++) {
        int y0 = int(int64_t(oy) * in.h / out.h);
        int y1 = std::max(y0 + 1, int(int64_t(oy + 1) * in.h / out.h));
        for (int ox = 0; ox < out.w; ox++) {
            int x0 = int(int64_t(ox) * in.w / out.w);
            int x1 = std::max(x0 + 1, int(int64_t(ox + 1) * in.w / out.w));
            uint32_t sum = 0;
            for (int y = y0; y < y1; y++) {
                const uint8_t* row = &in.px[size_t(y) * in.w];
                for (int x = x0; x < x1; x++) sum += row[x];
            }
            out.px[size_t(oy) * out.w + ox] = uint8_t(sum / uint32_t((y1 - y0) * (x1 - x0)));
        }
    }
    return out;
}

std::vector<uint8_t> pack2bpp(const Gray& g) {
    std::vector<uint8_t> out((size_t(g.w) * g.h + 3) / 4, 0);
    std::vector<int> cur(g.w + 2, 0), next(g.w + 2, 0);
    for (int y = 0; y < g.h; y++) {
        std::fill(next.begin(), next.end(), 0);
        for (int x = 0; x < g.w; x++) {
            int v = g.px[size_t(y) * g.w + x] + cur[x + 1];
            int q = v <= 0 ? 0 : v >= 255 ? 3 : (v * 3 + 127) / 255;
            int err = v - q * 85;
            cur[x + 2] += err * 7 / 16;
            next[x] += err * 3 / 16;
            next[x + 1] += err * 5 / 16;
            next[x + 2] += err / 16;
            size_t i = size_t(y) * g.w + x;
            out[i >> 2] |= uint8_t(q << (6 - 2 * (i & 3)));
        }
        std::swap(cur, next);
    }
    return out;
}

}  // namespace image
