#include "Zip.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

namespace zip {

static uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

const char* errorText(Error e) {
    switch (e) {
        case Error::None: return "ok";
        case Error::Io: return "read error";
        case Error::NotZip: return "not a ZIP archive";
        case Error::Unsupported: return "unsupported ZIP feature";
        case Error::Corrupt: return "damaged archive";
        case Error::TooLarge: return "entry too large";
        case Error::Stopped: return "stopped";
        case Error::NoMemory: return "out of memory";
    }
    return "?";
}

uint32_t crc32(uint32_t crc, const uint8_t* data, size_t n) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = true;
    }
    crc = ~crc;
    while (n--) crc = table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

Error Archive::open() {
    entries_.clear();
    if (size_ < 22) return Error::NotZip;
    // The end-of-central-directory record sits in the last 22 + 65535 bytes.
    uint32_t tail = std::min<uint32_t>(size_, 22 + 65535);
    std::vector<uint8_t> buf(tail);
    if (!src_.readAt(size_ - tail, buf.data(), tail)) return Error::Io;
    int eocd = -1;
    for (int i = int(tail) - 22; i >= 0; i--)
        if (le32(&buf[i]) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd < 0) return Error::NotZip;
    const uint8_t* e = &buf[eocd];
    uint16_t count = le16(e + 10);
    uint32_t cdSize = le32(e + 12), cdOffset = le32(e + 16);
    if (cdOffset == 0xFFFFFFFF || count == 0xFFFF) return Error::Unsupported;   // ZIP64
    if (uint64_t(cdOffset) + cdSize > size_) return Error::Corrupt;

    std::vector<uint8_t> cd(cdSize);
    if (cdSize && !src_.readAt(cdOffset, cd.data(), cdSize)) return Error::Io;
    uint32_t p = 0;
    entries_.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
        if (p + 46 > cdSize || le32(&cd[p]) != 0x02014b50) return Error::Corrupt;
        const uint8_t* h = &cd[p];
        uint16_t nameLen = le16(h + 28), extraLen = le16(h + 30), commentLen = le16(h + 32);
        if (p + 46 + nameLen > cdSize) return Error::Corrupt;
        Entry en;
        en.method = le16(h + 10);
        en.encrypted = le16(h + 8) & 1;
        en.crc = le32(h + 16);
        en.compSize = le32(h + 20);
        en.size = le32(h + 24);
        en.headerOffset = le32(h + 42);
        en.name.assign((const char*)h + 46, nameLen);
        if (en.compSize != 0xFFFFFFFF && en.size != 0xFFFFFFFF && en.headerOffset != 0xFFFFFFFF)
            entries_.push_back(std::move(en));
        p += 46 + nameLen + extraLen + commentLen;
    }
    return Error::None;
}

static bool sameNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

const Entry* Archive::find(const std::string& name) const {
    for (const auto& e : entries_)
        if (e.name == name) return &e;
    for (const auto& e : entries_)
        if (sameNoCase(e.name, name)) return &e;
    return nullptr;
}

Error Archive::stream(const Entry& e, const OutputFn& out) {
    if (e.encrypted) return Error::Unsupported;
    if (e.method != 0 && e.method != 8) return Error::Unsupported;
    uint8_t lh[30];
    if (!src_.readAt(e.headerOffset, lh, sizeof lh)) return Error::Io;
    if (le32(lh) != 0x04034b50) return Error::Corrupt;
    uint32_t data = e.headerOffset + 30 + le16(lh + 26) + le16(lh + 28);
    if (uint64_t(data) + e.compSize > size_) return Error::Corrupt;

    uint32_t crc = 0, produced = 0;
    bool stopped = false;
    auto sink = [&](const uint8_t* d, size_t n) {
        crc = crc32(crc, d, n);
        produced += n;
        if (!out(d, n)) {
            stopped = true;
            return false;
        }
        return true;
    };

    uint32_t pos = 0;
    bool ioError = false;
    if (e.method == 0) {
        uint8_t buf[2048];
        while (pos < e.compSize) {
            uint32_t k = std::min<uint32_t>(sizeof buf, e.compSize - pos);
            if (!src_.readAt(data + pos, buf, k)) return Error::Io;
            pos += k;
            if (!sink(buf, k)) return Error::Stopped;
        }
    } else {
        InflateResult r = inflate(
            [&](uint8_t* dst, size_t cap) -> size_t {
                uint32_t k = std::min<uint32_t>(uint32_t(cap), e.compSize - pos);
                if (k == 0) return 0;
                if (!src_.readAt(data + pos, dst, k)) {
                    ioError = true;
                    return 0;
                }
                pos += k;
                return k;
            },
            sink);
        if (ioError) return Error::Io;
        if (r == InflateResult::Stopped || stopped) return Error::Stopped;
        if (r == InflateResult::NoMemory) return Error::NoMemory;
        if (r != InflateResult::Ok) return Error::Corrupt;
    }
    if (produced != e.size || crc != e.crc) return Error::Corrupt;
    return Error::None;
}

Error Archive::read(const Entry& e, std::vector<uint8_t>& out, uint32_t maxSize) {
    out.clear();
    if (e.size > maxSize) return Error::TooLarge;
    out.reserve(e.size);
    return stream(e, [&](const uint8_t* d, size_t n) {
        if (out.size() + n > maxSize) return false;
        out.insert(out.end(), d, d + n);
        return true;
    });
}

Error Archive::read(const Entry& e, std::string& out, uint32_t maxSize) {
    out.clear();
    if (e.size > maxSize) return Error::TooLarge;
    out.reserve(e.size);
    return stream(e, [&](const uint8_t* d, size_t n) {
        if (out.size() + n > maxSize) return false;
        out.append((const char*)d, n);
        return true;
    });
}

}  // namespace zip
