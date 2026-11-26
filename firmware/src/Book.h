#pragma once
#include <Arduino.h>

#include <memory>
#include <vector>

#include "Storage.h"

// Table-of-contents entry; `pos` is a value for Book::restore().
struct TocEntry {
    uint32_t pos;
    uint8_t level;   // 0 = top level
    String title;
};

// A readable document. Pages are numbered from 0.
class Book {
public:
    virtual ~Book() = default;
    virtual bool open(const BookEntry& entry, String& error) = 0;
    virtual uint32_t pageCount() const = 0;
    uint32_t page() const { return page_; }
    void goTo(int32_t p) { page_ = p < 0 ? 0 : (uint32_t(p) >= pageCount() ? pageCount() - 1 : uint32_t(p)); }
    bool next() { if (page_ + 1 >= pageCount()) return false; page_++; return true; }
    bool prev() { if (page_ == 0) return false; page_--; return true; }

    // Draw the current page into the display buffer (already cleared).
    virtual void render() = 0;
    // Page images use the whole screen; text books leave room for a status line.
    virtual bool fullScreen() const { return false; }
    virtual bool landscape() const { return false; }

    // Position that survives re-layout (byte offset for text, page for images).
    virtual uint32_t position() const { return page_; }
    virtual void restore(uint32_t pos) { page_ = pageOf(pos); }
    // Page that shows `pos` (for chapter starts in the status line).
    virtual uint32_t pageOf(uint32_t pos) const {
        return pageCount() == 0 ? 0 : pos < pageCount() ? pos : pageCount() - 1;
    }

    // Font size changed (text books re-paginate).
    virtual void relayout() {}

    // Chapters, if the format has them.
    virtual const std::vector<TocEntry>& toc() const {
        static const std::vector<TocEntry> none;
        return none;
    }

    const String& title() const { return title_; }
    const String& path() const { return path_; }

protected:
    uint32_t page_ = 0;
    String title_, path_;
};

std::unique_ptr<Book> openBook(const BookEntry& entry, String& error);
