#include "Xtp.h"

#include <string.h>

namespace xtp {

static uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

Error Reader::open() {
    uint8_t b[64];
    if (!src_.readAt(0, b, sizeof b)) return Error::Io;
    if (memcmp(b, "XTP1", 4) != 0) return Error::BadMagic;
    h_.version = le16(b + 4);
    h_.width = le16(b + 6);
    h_.height = le16(b + 8);
    h_.bpp = b[10];
    h_.flags = b[11];
    h_.pageCount = le32(b + 12);
    h_.indexOffset = le32(b + 16);
    memcpy(h_.title, b + 20, 44);
    h_.title[44] = 0;
    if (h_.version != 1 || h_.bpp != 2 || h_.width == 0 || h_.height == 0 || (h_.width & 3)) return Error::Unsupported;
    return Error::None;
}

Error Reader::maxCompressedSize(uint32_t& result) {
    result = 0;
    uint8_t e[8];
    for (uint32_t i = 0; i < h_.pageCount; i++) {
        if (!src_.readAt(h_.indexOffset + 8 * i, e, 8)) return Error::Io;
        uint32_t len = le32(e + 4);
        if (len > result) result = len;
    }
    return Error::None;
}

Error Reader::readPage(uint32_t index, uint8_t* out, uint8_t* scratch, size_t scratchSize) {
    if (index >= h_.pageCount) return Error::BadPage;
    uint8_t e[8];
    if (!src_.readAt(h_.indexOffset + 8 * index, e, 8)) return Error::Io;
    uint32_t off = le32(e), len = le32(e + 4);
    size_t need = pageBytes();
    if (!(h_.flags & 1)) {
        if (len != need) return Error::Corrupt;
        return src_.readAt(off, out, need) ? Error::None : Error::Io;
    }
    if (len > scratchSize) return Error::Corrupt;
    if (!src_.readAt(off, scratch, len)) return Error::Io;
    long n = rleDecode(scratch, len, out, need);
    return n == long(need) ? Error::None : Error::Corrupt;
}

long rleDecode(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap) {
    size_t i = 0, o = 0;
    while (i < inLen) {
        uint8_t c = in[i++];
        if (c < 128) {
            size_t n = size_t(c) + 1;
            if (i + n > inLen || o + n > outCap) return -1;
            memcpy(out + o, in + i, n);
            i += n;
            o += n;
        } else {
            size_t n = size_t(c) - 126;
            if (i >= inLen || o + n > outCap) return -1;
            memset(out + o, in[i++], n);
            o += n;
        }
    }
    return long(o);
}

}  // namespace xtp
