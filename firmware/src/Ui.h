#pragma once
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
    const char* openPath() const;
    uint32_t openPosition() const;

private:
    enum class Screen { Library, Reader, Menu, Message };
    struct MenuItem { String label; void (Ui::*action)(); };

    void showLibrary(bool full = false);
    void showReader(bool full = false);
    void showMenu();
    void showMessage(const String& title, const String& text);
    void drawStatus();
    void drawHeader(const char* title);

    void openSelected();
    void closeBook();
    void turn(int delta);

    // menu actions
    void actFontSmaller();
    void actFontLarger();
    void actToStart();
    void actLibrary();
    void actSleep();
    void actRescan();

    Screen screen_ = Screen::Library;
    Screen beforeMenu_ = Screen::Library;
    std::vector<BookEntry> books_;
    int sel_ = 0;
    std::unique_ptr<Book> book_;
    std::vector<MenuItem> menu_;
    int menuSel_ = 0;
    uint32_t turnsSinceSave_ = 0;
    String storageError_;
};

extern Ui ui;
