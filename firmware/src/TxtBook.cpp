#include "TxtBook.h"

#include <algorithm>

#include "Display.h"
#include "Fonts.h"
#include "Hyphenation.h"
#include "config.h"

static constexpr uint32_t WINDOW = 8192;   // bytes read per page (a page is < 3 KB)
static constexpr uint32_t LOOKBACK = 16;    // bytes before the page start, for paragraph detection
static char window[WINDOW + 1];

static int measure(const char* s, size_t n) {
    char tmp[200];
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

static int textWidth() { return display.width() - 2 * MARGIN_X; }
static int textHeight() { return display.height() - MARGIN_TOP - STATUS_H; }

void TxtBook::applyFont() {
    display.setPortrait(true);
    display.text().setFont(fonts::reading(font_));
    ascent_ = display.text().getFontAscent();
    lineH_ = ascent_ - display.text().getFontDescent() + fonts::leading(font_);
}

layout::Layout TxtBook::makeLayout() {
    layout::Params p;
    p.width = textWidth();
    p.indent = PARA_INDENT;
    p.maxLines = std::max(1, textHeight() / lineH_);
    if (hyphId_) p.hyphenate = [this](const char* s, size_t n, uint16_t* out, int max) {
            return hyph_.breaks(s, n, out, max);
        };
    return layout::Layout(measure, p);
}

// Plain text has no language tag: guess it from a stretch a third into the file.
void TxtBook::loadHyphenation() {
    uint32_t winStart;
    uint32_t n = readWindow(std::max(textStart_, size_ / 3), winStart);
    hyphId_ = hyphenation::load(hyph::guessLanguage(window, n), hyph_);
}

bool TxtBook::open(const BookEntry& entry, String& error) {
    path_ = entry.path;
    title_ = entry.name;
    File f = storage::fs().open(path_, FILE_READ);
    if (!f) {
        error = "Cannot open file";
        return false;
    }
    size_ = f.size();
    uint8_t bom[3] = {0};
    f.read(bom, 3);
    f.close();
    textStart_ = (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) ? 3 : 0;
    font_ = storage::fontSize();
    applyFont();
    loadHyphenation();
    if (!loadIndex()) {
        if (!paginate()) {
            error = "Could not lay out the text";
            return false;
        }
        saveIndex();
    }
    return true;
}

uint32_t TxtBook::readWindow(uint32_t pos, uint32_t& winStart) {
    winStart = pos >= textStart_ + LOOKBACK ? pos - LOOKBACK : textStart_;
    File f = storage::fs().open(path_, FILE_READ);
    if (!f) return 0;
    f.seek(winStart);
    uint32_t n = f.read((uint8_t*)window, WINDOW);
    f.close();
    window[n] = 0;
    return n;
}

bool TxtBook::paginate() {
    display.clear();
    display.text().setFont(fonts::ui());
    display.text().setCursor(MARGIN_X, 60);
    display.text().print("Preparing book ...");
    display.show(true);
    applyFont();

    starts_.clear();
    layout::Layout lay = makeLayout();
    std::vector<layout::Line> lines;
    File f = storage::fs().open(path_, FILE_READ);
    if (!f) return false;
    uint32_t pos = textStart_;
    while (pos < size_) {
        uint32_t winStart = pos >= textStart_ + LOOKBACK ? pos - LOOKBACK : textStart_;
        f.seek(winStart);
        uint32_t n = f.read((uint8_t*)window, WINDOW);
        bool eof = winStart + n >= size_;
        lines.clear();
        uint32_t next = winStart + lay.page(window, n, pos - winStart, eof, winStart == textStart_, lines);
        if (lines.empty()) break;   // only whitespace left
        starts_.push_back(pos);
        if (next <= pos) break;     // safety: no progress
        pos = next;
        if ((starts_.size() & 63) == 0) yield();
    }
    f.close();
    if (starts_.empty()) starts_.push_back(textStart_);
    return true;
}

// Index cache layout: "TIX2", size, font, width, lines, hyphenation id, count, starts[count]
bool TxtBook::loadIndex() {
    File f = storage::fs().open(storage::cacheFile(path_, ".tix"), FILE_READ);
    if (!f) return false;
    uint32_t hdr[7];
    if (f.read((uint8_t*)hdr, sizeof hdr) != sizeof hdr) return false;
    layout::Layout lay = makeLayout();
    if (hdr[0] != 0x32584954 || hdr[1] != size_ || hdr[2] != font_ || hdr[3] != uint32_t(lay.params().width) ||
        hdr[4] != uint32_t(lay.params().maxLines) || hdr[5] != hyphId_ || hdr[6] == 0 || hdr[6] > size_)
        return false;
    starts_.resize(hdr[6]);
    size_t bytes = hdr[6] * sizeof(uint32_t);
    return f.read((uint8_t*)starts_.data(), bytes) == bytes;
}

void TxtBook::saveIndex() {
    File f = storage::fs().open(storage::cacheFile(path_, ".tix"), FILE_WRITE);
    if (!f) return;
    layout::Layout lay = makeLayout();
    uint32_t hdr[7] = {0x32584954, size_, font_, uint32_t(lay.params().width), uint32_t(lay.params().maxLines),
                       hyphId_, uint32_t(starts_.size())};
    f.write((uint8_t*)hdr, sizeof hdr);
    f.write((uint8_t*)starts_.data(), starts_.size() * sizeof(uint32_t));
    f.close();
}

void TxtBook::restore(uint32_t pos) {
    auto it = std::upper_bound(starts_.begin(), starts_.end(), pos);
    page_ = it == starts_.begin() ? 0 : uint32_t(it - starts_.begin() - 1);
}

void TxtBook::relayout() {
    uint32_t pos = position();
    font_ = storage::fontSize();
    applyFont();
    if (storage::hyphenate() != (hyphId_ != 0)) loadHyphenation();
    if (!loadIndex()) {
        paginate();
        saveIndex();
    }
    restore(pos);
}

void TxtBook::render() {
    applyFont();
    uint32_t pos = starts_[page_];
    uint32_t winStart;
    uint32_t n = readWindow(pos, winStart);
    bool eof = winStart + n >= size_;
    layout::Layout lay = makeLayout();
    std::vector<layout::Line> lines;
    lay.page(window, n, pos - winStart, eof, winStart == textStart_, lines);
    std::vector<layout::Word> words;
    char word[128];
    int y = MARGIN_TOP + ascent_;
    for (const auto& line : lines) {
        words.clear();
        lay.placeWords(window, line, true, words);
        for (const auto& w : words) {
            size_t len = std::min<size_t>(w.end - w.start, sizeof word - 1);
            memcpy(word, window + w.start, len);
            word[len] = 0;
            int16_t advance = display.text().drawUTF8(MARGIN_X + w.x, y, word);
            if (w.hyphen) display.text().drawUTF8(MARGIN_X + w.x + advance, y, "-");
        }
        y += lineH_;
    }
}
