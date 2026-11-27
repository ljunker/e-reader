#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "Book.h"
#include "Buttons.h"
#include "Catalog.h"
#include "Covers.h"
#include "Display.h"
#include "PageCache.h"
#include "Storage.h"

// Screens: library list, reading view, menu overlay, message.
class Ui {
public:
    void begin(const String& storageError);
    void handle(const ButtonEvent& e);
    // Saves the reading position and leaves a sensible image on the panel.
    // Returns true when that image is a cover rather than the page.
    bool prepareSleep();
    // "Sleep now" was chosen in the menu.
    bool takeSleepRequest();
    // After deep sleep: reopen the book that was being read.
    bool resume(const char* path, uint32_t position);
    void redraw();
    // Background work: Wi-Fi requests, computer plugged in or ejected.
    void poll();
    // Wi-Fi or USB drive on: don't go to sleep.
    bool busy() const;
    const char* openPath() const;
    uint32_t openPosition() const;

private:
    enum class Screen { Library, Reader, Menu, Toc, Message, Wifi, UsbAsk, Usb,
                        CatalogPick, Catalog, CatalogBook, CatalogMsg };
    struct MenuItem { String label; void (Ui::*action)(); };

    void showLibrary(bool full = false);
    void showReader(bool full = false);
    // The current page with its status line in the buffer, not shown yet.
    void composeReader();
    // Draw the next (then the previous) page into a spare frame while the
    // reader reads, so the page turn only has to refresh the panel.
    void prerender();
    // Keep what was just drawn (the book page alone) as the frame of `page`.
    void keepFrame(uint32_t page);
    void showMenu();
    void showMessage(const String& title, const String& text);
    void showToc();
    void showWifi();
    void showUsbAsk();
    void showUsb();
    void showText(const char* title, const char* const* lines, int n, const char* hint);
    // Word-wrapped text from `y` down to `maxY` in the current font; returns the next y.
    int drawWrapped(const String& text, int y, int maxY, int lineH = 20);
    void back();   // to the open book, else the library
    // One page of a selectable list below the header; `item` fills in the
    // label, an optional left indent and an optional tag shown on the right.
    void drawList(int count, int sel, const std::function<void(int, String&, int&, const char*&)>& item);
    void drawStatus();
    void drawBattery(int right, int baseline);
    void drawHeader(const char* title);
    // One row of the library: cover, title, format and progress.
    covers::State drawBookRow(int i, int y, bool selected);

    void openSelected();
    void closeBook();
    void turn(int delta);
    void savePosition();
    // Read the card again; keep `selectPath` selected if it is still there.
    void reloadBooks(const String& selectPath);
    // After opening or re-paginating: first page of every chapter.
    void updateChapters();
    // Font, font size or hyphenation changed: lay the book out again.
    void relayoutBook();

    // menu actions
    void actFontSmaller();
    void actFontLarger();
    void actContents();
    void actToStart();
    void actLibrary();
    void actSleep();
    void actSort();
    void actSleepScreen();
    void actHyphenation();
    void actFont();
    void actRescan();
    void actWifi();
    void actWifiStop();
    void actUsb();
    void actUsbStop();

    // online library (OPDS)
    void actCatalog();
    void showCatalogPick();
    void enterCatalog(int i);
    void showCatalog();
    void showCatalogBook();
    void showCatalogMsg(const String& text);
    void catalogStatus(const String& text);   // one line at the bottom, partial refresh
    bool openFeed(const std::string& url, bool push);
    void feedBack();                           // BACK in a list: up one level
    void catalogOk();
    void downloadSelected();
    void leaveCatalog(const String& selectPath = String());
    void handleCatalog(const ButtonEvent& e);

    Screen screen_ = Screen::Library;
    Screen beforeMenu_ = Screen::Library;
    std::vector<BookEntry> books_;
    int sel_ = 0;
    std::unique_ptr<Book> book_;
    std::vector<MenuItem> menu_;
    int menuSel_ = 0;
    int tocSel_ = 0;
    uint32_t turnsSinceSave_ = 0;
    std::vector<uint32_t> chapterPages_;   // parallel to book_->toc()
    pagecache::Slots pages_;                // which page each frame holds
    Display::Frame frames_[pagecache::Slots::kSlots];
    bool sleepRequested_ = false;

    struct FeedLevel {
        std::string url;
        int sel = 0;
    };
    std::vector<opds::Catalog> catalogs_;
    int catalogSel_ = 0;
    bool catalogActive_ = false;
    uint32_t catalogInput_ = 0;      // millis() of the last button in the online library
    std::vector<FeedLevel> trail_;   // feeds opened, the current one last
    opds::Feed feed_;
    std::vector<bool> onCard_;       // parallel to feed_.entries
    bool feedLoaded_ = false;
    int feedSel_ = 0;
    String catalogNote_;             // shown on the book page (saved, error)
    String catalogSaved_;            // last book downloaded: selected in the library afterwards
    String storageError_;
};

extern Ui ui;
