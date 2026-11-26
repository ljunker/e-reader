#include "Covers.h"

#include <memory>
#include <new>

#include "Cover.h"
#include "Display.h"
#include "Package.h"
#include "Xtp.h"
#include "Zip.h"

namespace covers {

namespace {

struct FileSource : io::ByteSource {
    File f;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override { return f.seek(off) && f.read(dst, n) == n; }
};

String cachePath(const BookEntry& b) { return storage::cacheFile(b.path, ".cov"); }

// Opens the cache file and checks its header; `f` stays open on success.
bool open(const BookEntry& b, File& f, cover::Info& info) {
    if (b.kind != BookKind::Epub && b.kind != BookKind::Xtp) return false;
    String p = cachePath(b);
    if (!storage::fs().exists(p)) return false;
    f = storage::fs().open(p, FILE_READ);
    uint8_t hdr[cover::kHeaderSize];
    return f && f.read(hdr, sizeof hdr) == sizeof hdr && cover::readInfo(hdr, b.size, f.size(), info);
}

bool epubPicture(const BookEntry& b, image::Gray& g) {
    FileSource src;
    src.f = storage::fs().open(b.path, FILE_READ);
    if (!src.f) return false;
    zip::Archive zip(src, b.size);
    if (zip.open() != zip::Error::None) return false;
    epub::Package pkg;
    std::string err;
    if (!epub::loadPackage(zip, pkg, err) || pkg.coverPath.empty()) return false;
    const zip::Entry* e = zip.find(pkg.coverPath);
    std::vector<uint8_t> data;
    if (!e || zip.read(*e, data, 4u << 20) != zip::Error::None) return false;
    return image::decode(data.data(), data.size(), g, cover::kFullW, cover::kFullH);
}

bool xtpPicture(const BookEntry& b, image::Gray& g) {
    FileSource src;
    src.f = storage::fs().open(b.path, FILE_READ);
    if (!src.f) return false;
    xtp::Reader r(src);
    uint32_t maxComp = 0;
    if (r.open() != xtp::Error::None || r.header().pageCount == 0 || r.maxCompressedSize(maxComp) != xtp::Error::None)
        return false;
    std::unique_ptr<uint8_t, void (*)(void*)> page((uint8_t*)ps_malloc(r.pageBytes()), free);
    std::unique_ptr<uint8_t, void (*)(void*)> scratch((uint8_t*)ps_malloc(maxComp ? maxComp : 1), free);
    if (!page || !scratch || r.readPage(0, page.get(), scratch.get(), maxComp) != xtp::Error::None) return false;
    g = cover::unpack(page.get(), r.header().width, r.header().height);
    return true;
}

void drawPacked(const uint8_t* px, int w, int h, int x0, int y0) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) display.grayPixel(x0 + x, y0 + y, xtp::pixel(px, w, x, y));
}

}  // namespace

State state(const BookEntry& b) {
    if (b.kind != BookKind::Epub && b.kind != BookKind::Xtp) return State::None;
    File f;
    cover::Info info;
    if (!open(b, f, info)) return State::Missing;
    return info.present() ? State::Present : State::None;
}

State build(const BookEntry& b) {
    if (b.kind != BookKind::Epub && b.kind != BookKind::Xtp) return State::None;
    std::vector<uint8_t> file;
    try {   // a picture too big for memory means "no cover", not a crash
        image::Gray g;
        bool ok = b.kind == BookKind::Epub ? epubPicture(b, g) : xtpPicture(b, g);
        file = cover::encode(b.size, ok ? &g : nullptr);
    } catch (const std::bad_alloc&) {
        file = cover::encode(b.size, nullptr);
    }
    File f = storage::fs().open(cachePath(b), FILE_WRITE);
    if (!f || f.write(file.data(), file.size()) != file.size()) return State::Missing;
    f.close();
    return file.size() > cover::kHeaderSize ? State::Present : State::None;
}

State drawThumb(const BookEntry& b, int x, int y) {
    if (b.kind != BookKind::Epub && b.kind != BookKind::Xtp) return State::None;
    File f;
    cover::Info info;
    if (!open(b, f, info)) return State::Missing;
    if (!info.present()) return State::None;
    uint8_t px[(cover::kThumbW * cover::kThumbH + 3) / 4];
    if (!f.seek(info.thumbOffset()) || f.read(px, info.thumbBytes()) != info.thumbBytes()) return State::Missing;
    drawPacked(px, info.thumbW, info.thumbH, x + (cover::kThumbW - info.thumbW) / 2,
               y + (cover::kThumbH - info.thumbH) / 2);
    return State::Present;
}

bool drawFull(const BookEntry& b) {
    File f;
    cover::Info info;
    if (!open(b, f, info)) {
        f.close();
        if (build(b) != State::Present || !open(b, f, info)) return false;
    }
    if (!info.present()) return false;
    std::unique_ptr<uint8_t, void (*)(void*)> px((uint8_t*)ps_malloc(info.fullBytes()), free);
    if (!px || !f.seek(info.fullOffset()) || f.read(px.get(), info.fullBytes()) != info.fullBytes()) return false;
    display.setPortrait(true);
    display.clear();
    drawPacked(px.get(), info.fullW, info.fullH, (display.width() - info.fullW) / 2,
               (display.height() - info.fullH) / 2);
    return true;
}

}  // namespace covers
