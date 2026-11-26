#include "Ui.h"

#include <algorithm>

#include "Display.h"
#include "Fonts.h"
#include "Power.h"
#include "UsbDrive.h"
#include "WifiShare.h"
#include "config.h"

Ui ui;

static constexpr int ROW_H = 26;
static constexpr int LIST_TOP = 40;

// ---------- drawing helpers ----------

static void batteryLabel(char* out, size_t n) {
    int p = power::batteryPercent();
    if (p < 0) snprintf(out, n, "--%%");
    else snprintf(out, n, "%d%%", p);
}

void Ui::drawHeader(const char* title) {
    auto& t = display.text();
    t.setFont(fonts::uiBold());
    t.drawUTF8(MARGIN_X, 24, title);
    char bat[8];
    batteryLabel(bat, sizeof bat);
    t.setFont(fonts::small());
    t.drawUTF8(display.width() - MARGIN_X - t.getUTF8Width(bat), 22, bat);
    display.gfx().drawFastHLine(MARGIN_X, 31, display.width() - 2 * MARGIN_X, GxEPD_BLACK);
}

void Ui::drawStatus() {
    auto& t = display.text();
    t.setFont(fonts::small());
    int y = display.height() - 5;
    char right[32], bat[8];
    batteryLabel(bat, sizeof bat);
    uint32_t n = book_->pageCount();
    snprintf(right, sizeof right, "%lu / %lu  %s", (unsigned long)book_->page() + 1, (unsigned long)n, bat);
    int rw = t.getUTF8Width(right);
    t.drawUTF8(display.width() - MARGIN_X - rw, y, right);
    // title, cut to the space that is left
    String title = book_->title();
    while (title.length() > 3 && t.getUTF8Width(title.c_str()) > display.width() - 3 * MARGIN_X - rw)
        title.remove(title.length() - 1);
    t.drawUTF8(MARGIN_X, y, title.c_str());
    // progress bar
    int w = display.width() - 2 * MARGIN_X;
    int filled = n > 1 ? int(int64_t(w) * book_->page() / (n - 1)) : w;
    display.gfx().drawFastHLine(MARGIN_X, display.height() - STATUS_H + 2, filled, GxEPD_BLACK);
}

// ---------- screens ----------

void Ui::begin(const String& storageError) {
    storageError_ = storageError;
    if (storageError.length()) {
        showMessage("Storage", storageError + "\n\nInsert a FAT32 card with books in /books and restart.");
        return;
    }
    books_ = storage::listBooks();
    showLibrary(true);
}

void Ui::showLibrary(bool full) {
    screen_ = Screen::Library;
    display.setPortrait(true);
    display.clear();
    drawHeader("Library");
    auto& t = display.text();
    t.setFont(fonts::ui());
    if (books_.empty()) {
        t.drawUTF8(MARGIN_X, LIST_TOP + 30, "No books found.");
        t.drawUTF8(MARGIN_X, LIST_TOP + 60, "Copy .txt, .epub or .xtp");
        t.drawUTF8(MARGIN_X, LIST_TOP + 82, "files to /books on the card.");
        display.show(full);
        return;
    }
    drawList(int(books_.size()), sel_, [this](int i, String& label, int&, const char*& tag) {
        label = books_[i].name;
        tag = books_[i].kind == BookKind::Xtp ? "PDF" : books_[i].kind == BookKind::Epub ? "EPUB" : "TXT";
    });
    display.show(full);
}

// Cut a UTF-8 string until it fits into maxW pixels in the current font.
static void fitText(String& s, int maxW) {
    auto& t = display.text();
    while (s.length() > 1 && t.getUTF8Width(s.c_str()) > maxW) {
        int n = s.length() - 1;
        while (n > 0 && (uint8_t(s[n]) & 0xC0) == 0x80) n--;   // keep characters whole
        s.remove(n);
    }
}

void Ui::drawList(int count, int sel, const std::function<void(int, String&, int&, const char*&)>& item) {
    auto& t = display.text();
    t.setFont(fonts::ui());
    int perPage = (display.height() - LIST_TOP - 24) / ROW_H;
    int first = (sel / perPage) * perPage;
    for (int i = first; i < count && i < first + perPage; i++) {
        String label;
        int indent = 0;
        const char* tag = nullptr;
        item(i, label, indent, tag);
        int y = LIST_TOP + (i - first) * ROW_H;
        if (i == sel) {
            display.gfx().fillRect(MARGIN_X - 4, y, display.width() - 2 * MARGIN_X + 8, ROW_H - 2, GxEPD_BLACK);
            t.setForegroundColor(GxEPD_WHITE);
        }
        int tagW = tag ? t.getUTF8Width(tag) + 8 : 0;
        fitText(label, display.width() - 2 * MARGIN_X - tagW - indent);
        t.drawUTF8(MARGIN_X + indent, y + ROW_H - 8, label.c_str());
        if (tag) t.drawUTF8(display.width() - MARGIN_X - tagW + 8, y + ROW_H - 8, tag);
        t.setForegroundColor(GxEPD_BLACK);
    }
    char foot[32];
    snprintf(foot, sizeof foot, "%d / %d", sel + 1, count);
    t.setFont(fonts::small());
    t.drawUTF8(MARGIN_X, display.height() - 6, foot);
}

void Ui::showToc() {
    screen_ = Screen::Toc;
    display.setPortrait(true);
    display.clear();
    drawHeader("Contents");
    const auto& toc = book_->toc();
    drawList(int(toc.size()), tocSel_, [&toc](int i, String& label, int& indent, const char*&) {
        label = toc[i].title;
        indent = std::min<int>(toc[i].level, 4) * 14;
    });
    display.show(true);
}

void Ui::actContents() {
    // preselect the chapter the reader is in
    const auto& toc = book_->toc();
    uint32_t pos = book_->position();
    tocSel_ = 0;
    for (int i = 0; i < int(toc.size()); i++)
        if (toc[i].pos <= pos) tocSel_ = i;
    showToc();
}

void Ui::showReader(bool full) {
    screen_ = Screen::Reader;
    display.clear();
    book_->render();
    if (!book_->fullScreen()) drawStatus();
    display.show(full);
}

void Ui::showMessage(const String& title, const String& text) {
    screen_ = Screen::Message;
    display.setPortrait(true);
    display.clear();
    drawHeader(title.c_str());
    auto& t = display.text();
    t.setFont(fonts::ui());
    int y = LIST_TOP + 24;
    int start = 0;
    while (start <= int(text.length())) {
        int nl = text.indexOf('\n', start);
        String line = text.substring(start, nl < 0 ? text.length() : nl);
        // naive wrap at word boundaries
        while (line.length()) {
            int cut = line.length();
            while (cut > 0 && t.getUTF8Width(line.substring(0, cut).c_str()) > display.width() - 2 * MARGIN_X) {
                int sp = line.lastIndexOf(' ', cut - 1);
                cut = sp > 0 ? sp : cut - 1;
            }
            t.drawUTF8(MARGIN_X, y, line.substring(0, cut).c_str());
            y += 20;
            line = line.substring(cut);
            line.trim();
        }
        if (nl < 0) break;
        if (nl == start) y += 8;   // blank line = paragraph gap
        start = nl + 1;
    }
    display.show(true);
}

void Ui::showMenu() {
    if (screen_ != Screen::Menu) {
        beforeMenu_ = screen_;
        menuSel_ = 0;
    }
    screen_ = Screen::Menu;
    menu_.clear();
    uint8_t fs = storage::fontSize();
    if (beforeMenu_ == Screen::Reader) {
        if (!book_->fullScreen()) {
            menu_.push_back({String("Font size: ") + fonts::sizeName(fs) + "  (smaller)", &Ui::actFontSmaller});
            menu_.push_back({String("Font size: ") + fonts::sizeName(fs) + "  (larger)", &Ui::actFontLarger});
        }
        if (!book_->toc().empty()) menu_.push_back({"Contents", &Ui::actContents});
        menu_.push_back({"Go to start", &Ui::actToStart});
        menu_.push_back({"Library", &Ui::actLibrary});
    } else {
        menu_.push_back({"Rescan card", &Ui::actRescan});
    }
    if (storageError_.isEmpty()) menu_.push_back({"Wi-Fi upload", &Ui::actWifi});
    if (usbdrive::available() && usbdrive::hostConnected()) menu_.push_back({"USB drive", &Ui::actUsb});
    menu_.push_back({"Sleep now", &Ui::actSleep});

    display.setPortrait(true);
    display.clear();
    drawHeader("Menu");
    auto& t = display.text();
    t.setFont(fonts::ui());
    for (int i = 0; i < int(menu_.size()); i++) {
        int y = LIST_TOP + i * ROW_H;
        if (i == menuSel_) {
            display.gfx().fillRect(MARGIN_X - 4, y, display.width() - 2 * MARGIN_X + 8, ROW_H - 2, GxEPD_BLACK);
            t.setForegroundColor(GxEPD_WHITE);
        }
        t.drawUTF8(MARGIN_X, y + ROW_H - 8, menu_[i].label.c_str());
        t.setForegroundColor(GxEPD_BLACK);
    }
    t.setFont(fonts::small());
    t.drawUTF8(MARGIN_X, display.height() - 6, "PREV/NEXT move, OK select, BACK close");
    display.show();
}

// ---------- actions ----------

void Ui::openSelected() {
    if (books_.empty()) return;
    String err;
    book_ = openBook(books_[sel_], err);
    if (!book_) {
        showMessage("Cannot open", books_[sel_].name + "\n\n" + err);
        return;
    }
    book_->restore(storage::loadPosition(book_->path()));
    showReader(true);
}

void Ui::closeBook() {
    if (book_) storage::savePosition(book_->path(), book_->position());
    book_.reset();
    display.setPortrait(true);
}

void Ui::turn(int delta) {
    uint32_t before = book_->page();
    book_->goTo(int32_t(before) + delta);
    if (book_->page() == before) return;
    if (++turnsSinceSave_ >= 10) {
        storage::savePosition(book_->path(), book_->position());
        turnsSinceSave_ = 0;
    }
    showReader();
}

void Ui::actFontSmaller() {
    uint8_t s = storage::fontSize();
    if (s > 0) storage::setFontSize(s - 1);
    book_->relayout();
    showReader(true);
}

void Ui::actFontLarger() {
    uint8_t s = storage::fontSize();
    if (s + 1 < fonts::kSizes) storage::setFontSize(s + 1);
    book_->relayout();
    showReader(true);
}

void Ui::actToStart() {
    book_->goTo(0);
    showReader(true);
}

void Ui::actLibrary() {
    closeBook();
    showLibrary(true);
}

void Ui::actRescan() {
    books_ = storage::listBooks();
    sel_ = 0;
    showLibrary(true);
}

void Ui::actSleep() {
    prepareSleep();
    power::deepSleep();
}

void Ui::back() {
    if (book_) showReader(true);
    else showLibrary(true);
}

void Ui::showText(const char* title, const char* const* lines, int n, const char* hint) {
    display.setPortrait(true);
    display.clear();
    drawHeader(title);
    auto& t = display.text();
    t.setFont(fonts::ui());
    int y = LIST_TOP + 24;
    for (int i = 0; i < n; i++) {
        if (!lines[i]) { y += 10; continue; }   // nullptr = gap
        String l = lines[i];
        fitText(l, display.width() - 2 * MARGIN_X);
        t.drawUTF8(MARGIN_X, y, l.c_str());
        y += 22;
    }
    t.setFont(fonts::small());
    t.drawUTF8(MARGIN_X, display.height() - 6, hint);
}

// ---------- Wi-Fi upload ----------

void Ui::actWifi() {
    closeBook();
    screen_ = Screen::Wifi;
    const char* lines[] = {"Connecting to Wi-Fi ...", nullptr, "Networks come from", WIFI_FILE " on the card."};
    showText("Wi-Fi upload", lines, 4, "");
    display.show(true);
    String err;
    if (!wifishare::start(err)) {
        screen_ = Screen::Library;
        showMessage("Wi-Fi", err);
        return;
    }
    showWifi();
}

void Ui::showWifi() {
    screen_ = Screen::Wifi;
    String net = "Network: " + wifishare::network();
    String addr = wifishare::address();
    String count = String("Received: ") + wifishare::filesReceived() + " file" + (wifishare::filesReceived() == 1 ? "" : "s");
    String last = wifishare::lastFile();
    const char* lines[] = {net.c_str(), nullptr, "Open in a browser:", addr.c_str(), "http://ereader.local/",
                           nullptr, count.c_str(), last.c_str()};
    showText("Wi-Fi upload", lines, last.length() ? 8 : 7, "BACK stops Wi-Fi");
    display.show();
}

void Ui::actWifiStop() {
    wifishare::stop();
    actRescan();
}

// ---------- USB drive ----------

void Ui::showUsbAsk() {
    screen_ = Screen::UsbAsk;
    const char* lines[] = {"Connected to a computer.", nullptr, "Show the card as a", "USB drive?"};
    showText("USB", lines, 4, "OK: USB drive   BACK: keep reading");
    display.show(true);
}

void Ui::actUsb() {
    closeBook();
    usbdrive::enable();
    showUsb();
}

void Ui::showUsb() {
    screen_ = Screen::Usb;
    const char* lines[] = {"The card is a USB drive now.", nullptr, "Copy books into /books,", "then eject the drive.",
                           nullptr, "The reader is paused", "until then."};
    showText("USB drive", lines, 7, "BACK: stop (eject first!)");
    display.show(true);
}

void Ui::actUsbStop() {
    String err;
    if (!usbdrive::disable(err)) {
        storageError_ = err;
        showMessage("Storage", err + "\n\nRestart the reader.");
        return;
    }
    actRescan();
}

void Ui::poll() {
    if (wifishare::active()) {
        wifishare::loop();
        if (screen_ == Screen::Wifi && wifishare::takeChanged()) showWifi();
        if (millis() - wifishare::lastRequestMs() > WIFI_IDLE_MS) actWifiStop();
        return;
    }
    if (usbdrive::active()) {
        if (usbdrive::ejected() || !usbdrive::hostConnected()) actUsbStop();
        return;
    }
    if (screen_ != Screen::UsbAsk && storageError_.isEmpty() && usbdrive::takeHostArrived()) showUsbAsk();
}

bool Ui::busy() const { return wifishare::active() || usbdrive::active() || screen_ == Screen::UsbAsk; }

// ---------- input ----------

void Ui::handle(const ButtonEvent& e) {
    if (e.button == Btn::None) return;
    switch (screen_) {
        case Screen::Library:
            if (books_.empty() && e.button != Btn::Menu) return;
            switch (e.button) {
                case Btn::Next: sel_ = (sel_ + (e.longPress ? 10 : 1)) % int(books_.size()); showLibrary(); break;
                case Btn::Prev: sel_ = (sel_ - (e.longPress ? 10 : 1) + 10 * int(books_.size())) % int(books_.size()); showLibrary(); break;
                case Btn::Ok: openSelected(); break;
                case Btn::Menu: showMenu(); break;
                default: break;
            }
            break;
        case Screen::Reader:
            switch (e.button) {
                case Btn::Next: turn(e.longPress ? 10 : 1); break;
                case Btn::Prev: turn(e.longPress ? -10 : -1); break;
                case Btn::Menu: case Btn::Ok: showMenu(); break;
                case Btn::Back: actLibrary(); break;
                default: break;
            }
            break;
        case Screen::Menu:
            switch (e.button) {
                case Btn::Next: menuSel_ = (menuSel_ + 1) % int(menu_.size()); showMenu(); break;
                case Btn::Prev: menuSel_ = (menuSel_ + int(menu_.size()) - 1) % int(menu_.size()); showMenu(); break;
                case Btn::Ok: (this->*menu_[menuSel_].action)(); break;
                case Btn::Back: case Btn::Menu:
                    if (beforeMenu_ == Screen::Reader && book_) showReader(true); else showLibrary(true);
                    break;
                default: break;
            }
            break;
        case Screen::Toc: {
            int n = int(book_->toc().size());
            switch (e.button) {
                case Btn::Next: tocSel_ = std::min(n - 1, tocSel_ + (e.longPress ? 10 : 1)); showToc(); break;
                case Btn::Prev: tocSel_ = std::max(0, tocSel_ - (e.longPress ? 10 : 1)); showToc(); break;
                case Btn::Ok:
                    book_->restore(book_->toc()[tocSel_].pos);
                    storage::savePosition(book_->path(), book_->position());
                    showReader(true);
                    break;
                case Btn::Back: showMenu(); break;
                case Btn::Menu: showReader(true); break;
                default: break;
            }
            break;
        }
        case Screen::Message:
            if (storageError_.length()) return;   // nothing to go back to
            back();
            break;
        case Screen::Wifi:
            if (e.button == Btn::Back) actWifiStop();
            break;
        case Screen::UsbAsk:
            if (e.button == Btn::Ok) {
                actUsb();
            } else if (e.button == Btn::Back) {
                usbdrive::decline();
                back();
            }
            break;
        case Screen::Usb:
            if (e.button == Btn::Back) actUsbStop();
            break;
    }
}

// ---------- sleep / resume ----------

void Ui::prepareSleep() {
    if (book_) {
        storage::savePosition(book_->path(), book_->position());
        if (screen_ != Screen::Reader) showReader(true);
        // small "sleeping" mark in the status line so the device does not look frozen
        if (!book_->fullScreen()) {
            display.gfx().fillRect(display.width() / 2 - 30, display.height() - STATUS_H + 4, 60, STATUS_H - 4, GxEPD_WHITE);
            display.text().setFont(fonts::small());
            display.text().drawUTF8(display.width() / 2 - 22, display.height() - 5, "sleeping");
            display.show();
        }
    } else {
        display.setPortrait(true);
        display.clear();
        display.text().setFont(fonts::uiBold());
        display.text().drawUTF8(MARGIN_X, display.height() / 2, "Sleeping");
        display.text().setFont(fonts::ui());
        display.text().drawUTF8(MARGIN_X, display.height() / 2 + 26, "Press any button to wake up.");
        display.show(true);
    }
}

void Ui::redraw() {
    if (screen_ == Screen::Reader && book_) showReader(true);
    else showLibrary(true);
}

const char* Ui::openPath() const { return book_ ? book_->path().c_str() : ""; }
uint32_t Ui::openPosition() const { return book_ ? book_->position() : 0; }

bool Ui::resume(const char* path, uint32_t position) {
    books_ = storage::listBooks();
    for (int i = 0; i < int(books_.size()); i++) {
        if (books_[i].path != path) continue;
        sel_ = i;
        String err;
        book_ = openBook(books_[i], err);
        if (!book_) return false;
        book_->restore(position);
        screen_ = Screen::Reader;
        return true;
    }
    return false;
}
