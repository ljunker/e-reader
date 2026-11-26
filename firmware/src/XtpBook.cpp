#include "XtpBook.h"

#include "Display.h"

XtpBook::~XtpBook() {
    delete reader_;
    free(pageBuf_);
    free(scratch_);
    if (src_.f) src_.f.close();
}

bool XtpBook::open(const BookEntry& entry, String& error) {
    path_ = entry.path;
    src_.f = storage::fs().open(entry.path, FILE_READ);
    if (!src_.f) {
        error = "Cannot open file";
        return false;
    }
    reader_ = new xtp::Reader(src_);
    if (reader_->open() != xtp::Error::None) {
        error = "Not a valid .xtp file (re-convert it)";
        return false;
    }
    const auto& h = reader_->header();
    bool fitsPortrait = h.width <= 300 && h.height <= 400;
    bool fitsLandscape = h.width <= 400 && h.height <= 300;
    if (!fitsPortrait && !fitsLandscape) {
        error = "Page size too large for this screen";
        return false;
    }
    uint32_t maxComp = 0;
    reader_->maxCompressedSize(maxComp);
    pageBuf_ = (uint8_t*)ps_malloc(reader_->pageBytes());
    scratchSize_ = maxComp;
    scratch_ = (uint8_t*)ps_malloc(scratchSize_ ? scratchSize_ : 1);
    if (!pageBuf_ || !scratch_) {
        error = "Out of memory";
        return false;
    }
    title_ = h.title[0] ? String(h.title) : entry.name;
    return true;
}

bool XtpBook::landscape() const {
    return reader_ && reader_->header().width > reader_->header().height;
}

void XtpBook::render() {
    const auto& h = reader_->header();
    display.setPortrait(!landscape());
    if (reader_->readPage(page_, pageBuf_, scratch_, scratchSize_) != xtp::Error::None) {
        display.text().drawUTF8(10, 40, "Page could not be read");
        return;
    }
    int ox = (display.width() - h.width) / 2;
    int oy = (display.height() - h.height) / 2;
    for (uint16_t y = 0; y < h.height; y++)
        for (uint16_t x = 0; x < h.width; x++) {
            uint8_t v = xtp::pixel(pageBuf_, h.width, x, y);
            if (v != 3) display.grayPixel(ox + x, oy + y, v);
        }
}
