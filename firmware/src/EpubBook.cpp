#include "EpubBook.h"

#include <algorithm>
#include <memory>

#include "Display.h"
#include "Fonts.h"
#include "Package.h"
#include "Xtp.h"
#include "Zip.h"
#include "config.h"

static constexpr uint32_t WINDOW = 8192;   // bytes read per page (a page is < 3 KB)
static char window[WINDOW + 1];

namespace {

struct FileSource : io::ByteSource {
    File f;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override { return f.seek(off) && f.read(dst, n) == n; }
};

struct FileWriter : io::Writer {
    File f;
    bool write(const void* d, size_t n) override { return f.write((const uint8_t*)d, n) == n; }
};

uint8_t fontSize = 1;   // reading size the measure callbacks use

int measure(int slot, const char* s, size_t n) {
    char tmp[200];
    display.text().setFont(fonts::rich(fontSize, slot));
    int w = 0;
    while (n > 0) {
        size_t k = n < sizeof tmp - 1 ? n : sizeof tmp - 1;
        memcpy(tmp, s, k);
        tmp[k] = 0;
        w += display.text().getUTF8Width(tmp);
        s += k;
        n -= k;
    }
    return w;
}

rich::Metrics metrics(int slot) {
    display.text().setFont(fonts::rich(fontSize, slot));
    return {display.text().getFontAscent(), -display.text().getFontDescent()};
}

int textWidth() { return display.width() - 2 * MARGIN_X; }
int textHeight() { return display.height() - MARGIN_TOP - STATUS_H; }

void showProgress(const String& title, const char* phase, uint32_t done, uint32_t total) {
    static uint32_t last = 0;
    if (done != 0 && millis() - last < 2000) return;   // e-paper refreshes are slow
    last = millis();
    display.clear();
    auto& t = display.text();
    t.setFont(fonts::uiBold());
    t.drawUTF8(MARGIN_X, 60, "Preparing book ...");
    t.setFont(fonts::ui());
    String name = title;
    while (name.length() > 3 && t.getUTF8Width(name.c_str()) > textWidth()) name.remove(name.length() - 1);
    t.drawUTF8(MARGIN_X, 90, name.c_str());
    char line[48];
    snprintf(line, sizeof line, "%s %lu / %lu", phase, (unsigned long)done + 1, (unsigned long)total);
    t.drawUTF8(MARGIN_X, 120, line);
    t.setFont(fonts::small());
    t.drawUTF8(MARGIN_X, 150, "Only the first time; this can take a minute.");
    display.show();
}

}  // namespace

rich::Layout EpubBook::makeLayout() {
    fontSize = font_;
    rich::Env e;
    e.measure = measure;
    e.metrics = metrics;
    e.imageSize = [this](uint32_t i, int& w, int& h) {
        w = i < images_.size() ? images_[i].w : 0;
        h = i < images_.size() ? images_[i].h : 0;
    };
    e.width = textWidth();
    e.height = textHeight();
    e.indent = PARA_INDENT;
    e.blockIndent = PARA_INDENT;
    e.leading = fonts::leading(font_);
    e.paraGap = 8;
    return rich::Layout(e);
}

bool EpubBook::loadCache(uint32_t epubSize) {
    File f = storage::fs().open(etxPath_, FILE_READ);
    if (!f || f.size() < epub::kEtxHeaderSize) return false;
    uint8_t hdr[epub::kEtxHeaderSize];
    if (f.read(hdr, sizeof hdr) != sizeof hdr || !epub::checkEtxHeader(hdr, epubSize)) return false;
    etxSize_ = f.size();
    f.close();

    FileSource toc;
    toc.f = storage::fs().open(etcPath_, FILE_READ);
    std::string title;
    std::vector<epub::TocItem> items;
    if (!toc.f || !epub::readToc(toc, toc.f.size(), epubSize, title, items)) return false;
    toc.f.close();

    FileSource img;
    img.f = storage::fs().open(eimPath_, FILE_READ);
    if (!img.f || !epub::readImageTable(img, img.f.size(), epubSize, images_)) return false;
    img.f.close();

    if (!title.empty()) title_ = title.c_str();
    toc_.clear();
    for (const auto& t : items) toc_.push_back({t.pos, t.level, String(t.title.c_str())});
    return true;
}

bool EpubBook::buildCache(const BookEntry& entry, uint32_t epubSize, String& error) {
    FileSource src;
    src.f = storage::fs().open(entry.path, FILE_READ);
    if (!src.f) {
        error = "Cannot open file";
        return false;
    }
    zip::Archive zip(src, epubSize);
    zip::Error ze = zip.open();
    if (ze != zip::Error::None) {
        error = String("Not a valid EPUB (") + zip::errorText(ze) + ")";
        return false;
    }
    epub::Package pkg;
    std::string err;
    if (!epub::loadPackage(zip, pkg, err)) {
        error = err.c_str();
        return false;
    }
    FileWriter etx, eim, etc;
    etx.f = storage::fs().open(etxPath_, FILE_WRITE);
    eim.f = storage::fs().open(eimPath_, FILE_WRITE);
    etc.f = storage::fs().open(etcPath_, FILE_WRITE);
    if (!etx.f || !eim.f || !etc.f) {
        error = "Cannot write to the card";
        return false;
    }
    epub::BuildOptions opt;
    opt.imageMaxW = textWidth();
    opt.imageMaxH = textHeight();
    String title = pkg.title.empty() ? entry.name : String(pkg.title.c_str());
    opt.progress = [&](const char* phase, uint32_t done, uint32_t total) {
        if (total > done) showProgress(title, phase, done, total);
        yield();
    };
    bool ok = epub::buildCache(zip, pkg, epubSize, etx, eim, etc, opt, err);
    etx.f.close();
    eim.f.close();
    etc.f.close();
    if (!ok) {
        storage::fs().remove(etcPath_);   // the contents file marks a complete cache
        error = err.c_str();
    }
    return ok;
}

bool EpubBook::open(const BookEntry& entry, String& error) {
    path_ = entry.path;
    title_ = entry.name;
    etxPath_ = storage::cacheFile(path_, ".etx");
    eimPath_ = storage::cacheFile(path_, ".eim");
    etcPath_ = storage::cacheFile(path_, ".etc");
    File f = storage::fs().open(path_, FILE_READ);
    if (!f) {
        error = "Cannot open file";
        return false;
    }
    uint32_t epubSize = f.size();
    f.close();

    display.setPortrait(true);
    if (!loadCache(epubSize)) {
        if (!buildCache(entry, epubSize, error)) return false;
        if (!loadCache(epubSize)) {
            error = "Could not read the prepared book";
            return false;
        }
    }
    font_ = storage::fontSize();
    if (!loadIndex()) {
        if (!paginate()) {
            error = "Could not lay out the book";
            return false;
        }
        saveIndex();
    }
    return true;
}

uint32_t EpubBook::readWindow(uint32_t pos) {
    File f = storage::fs().open(etxPath_, FILE_READ);
    if (!f) return 0;
    f.seek(pos);
    uint32_t n = f.read((uint8_t*)window, WINDOW);
    f.close();
    window[n] = 0;
    return n;
}

bool EpubBook::paginate() {
    display.setPortrait(true);
    display.clear();
    display.text().setFont(fonts::ui());
    display.text().setCursor(MARGIN_X, 60);
    display.text().print("Laying out pages ...");
    display.show(true);

    starts_.clear();
    rich::Layout lay = makeLayout();
    std::vector<rich::Line> lines;
    File f = storage::fs().open(etxPath_, FILE_READ);
    if (!f) return false;
    uint32_t pos = epub::kEtxHeaderSize;
    rich::State st;
    while (pos < etxSize_) {
        f.seek(pos);
        uint32_t n = f.read((uint8_t*)window, WINDOW);
        if (n == 0) break;
        bool eof = pos + n >= etxSize_;
        lines.clear();
        rich::State next;
        uint32_t adv = lay.page(window, n, 0, st, eof, lines, next);
        if (!lines.empty()) starts_.push_back({pos, st.block, st.style, {0, 0}});
        if (adv == 0) break;   // safety: no progress
        pos += adv;
        st = next;
        if ((starts_.size() & 31) == 0) yield();
    }
    f.close();
    if (starts_.empty()) starts_.push_back({epub::kEtxHeaderSize, rich::State().block, 0, {0, 0}});
    return true;
}

// Index cache layout: "EIX1", etx size, font, width, height, count, starts[count]
bool EpubBook::loadIndex() {
    File f = storage::fs().open(storage::cacheFile(path_, ".eix"), FILE_READ);
    if (!f) return false;
    uint32_t hdr[6];
    if (f.read((uint8_t*)hdr, sizeof hdr) != sizeof hdr) return false;
    if (hdr[0] != 0x31584945 || hdr[1] != etxSize_ || hdr[2] != font_ || hdr[3] != uint32_t(textWidth()) ||
        hdr[4] != uint32_t(textHeight()) || hdr[5] == 0 || hdr[5] > etxSize_)
        return false;
    starts_.resize(hdr[5]);
    size_t bytes = hdr[5] * sizeof(Start);
    return f.read((uint8_t*)starts_.data(), bytes) == bytes;
}

void EpubBook::saveIndex() {
    File f = storage::fs().open(storage::cacheFile(path_, ".eix"), FILE_WRITE);
    if (!f) return;
    uint32_t hdr[6] = {0x31584945, etxSize_, font_, uint32_t(textWidth()), uint32_t(textHeight()),
                       uint32_t(starts_.size())};
    f.write((uint8_t*)hdr, sizeof hdr);
    f.write((uint8_t*)starts_.data(), starts_.size() * sizeof(Start));
    f.close();
}

uint32_t EpubBook::pageOf(uint32_t pos) const {
    auto it = std::upper_bound(starts_.begin(), starts_.end(), pos,
                               [](uint32_t p, const Start& s) { return p < s.pos; });
    return it == starts_.begin() ? 0 : uint32_t(it - starts_.begin() - 1);
}

void EpubBook::relayout() {
    uint32_t pos = position();
    font_ = storage::fontSize();
    if (!loadIndex()) {
        paginate();
        saveIndex();
    }
    restore(pos);
}

void EpubBook::drawImage(const rich::Line& line) {
    if (line.imageIndex >= images_.size()) return;
    const epub::ImageInfo& info = images_[line.imageIndex];
    size_t bytes = (size_t(info.w) * info.h + 3) / 4;
    std::unique_ptr<uint8_t, void (*)(void*)> buf((uint8_t*)malloc(bytes), free);
    if (!buf) return;
    File f = storage::fs().open(eimPath_, FILE_READ);
    if (!f || !f.seek(info.offset) || f.read(buf.get(), bytes) != bytes) return;
    f.close();
    int x0 = MARGIN_X + line.left, y0 = MARGIN_TOP + line.top;
    for (uint16_t y = 0; y < info.h; y++)
        for (uint16_t x = 0; x < info.w; x++) {
            uint8_t v = xtp::pixel(buf.get(), info.w, x, y);
            if (v != 3) display.grayPixel(x0 + x, y0 + y, v);
        }
}

void EpubBook::render() {
    display.setPortrait(true);
    const Start& s = starts_[page_];
    uint32_t n = readWindow(s.pos);
    bool eof = s.pos + n >= etxSize_;
    rich::Layout lay = makeLayout();
    std::vector<rich::Line> lines;
    rich::State st, next;
    st.block = s.block;
    st.style = s.style;
    lay.page(window, n, 0, st, eof, lines, next);
    std::vector<rich::Run> runs;
    char word[160];
    for (const auto& line : lines) {
        if (line.image) {
            drawImage(line);
            continue;
        }
        runs.clear();
        lay.runs(window, line, runs);
        int y = MARGIN_TOP + line.top + line.ascent;
        for (const auto& r : runs) {
            size_t len = std::min<size_t>(r.end - r.start, sizeof word - 1);
            memcpy(word, window + r.start, len);
            word[len] = 0;
            display.text().setFont(fonts::rich(font_, r.font));
            display.text().drawUTF8(MARGIN_X + r.x, y, word);
        }
    }
}
