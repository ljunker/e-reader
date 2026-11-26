#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "Book.h"
#include "Buttons.h"
#include "Storage.h"

// Screens: library list, reading view, menu overlay, message.
class Ui {
public:
    void begin(const String& storageError);
    void handle(const ButtonEvent& e);
    // Saves the reading position and leaves a sensible image on the panel.
    void prepareSleep();
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
    enum class Screen { Library, Reader, Menu, Toc, Message, Wifi, UsbAsk, Usb };
    struct MenuItem { String label; void (Ui::*action)(); };

    void showLibrary(bool full = false);
    void showReader(bool full = false);
    void showMenu();
    void showMessage(const String& title, const String& text);
    void showToc();
    void showWifi();
    void showUsbAsk();
    void showUsb();
    void showText(const char* title, const char* const* lines, int n, const char* hint);
    void back();   // to the open book, else the library
    // One page of a selectable list below the header; `item` fills in the
    // label, an optional left indent and an optional tag shown on the right.
    void drawList(int count, int sel, const std::function<void(int, String&, int&, const char*&)>& item);
    void drawStatus();
    void drawHeader(const char* title);

    void openSelected();
    void closeBook();
    void turn(int delta);

    // menu actions
    void actFontSmaller();
    void actFontLarger();
    void actContents();
    void actToStart();
    void actLibrary();
    void actSleep();
    void actRescan();
    void actWifi();
    void actWifiStop();
    void actUsb();
    void actUsbStop();

    Screen screen_ = Screen::Library;
    Screen beforeMenu_ = Screen::Library;
    std::vector<BookEntry> books_;
    int sel_ = 0;
    std::unique_ptr<Book> book_;
    std::vector<MenuItem> menu_;
    int menuSel_ = 0;
    int tocSel_ = 0;
    uint32_t turnsSinceSave_ = 0;
    String storageError_;
};

extern Ui ui;
