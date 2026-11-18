#pragma once
#include <vector>

#include "Book.h"
#include "TextLayout.h"

// Plain-text books (UTF-8). Paginates the whole file once per font size and
// caches the page start offsets on the card.
class TxtBook : public Book {
public:
    bool open(const BookEntry& entry, String& error) override;
    uint32_t pageCount() const override { return starts_.size(); }
    void render() override;
    uint32_t position() const override { return starts_.empty() ? 0 : starts_[page_]; }
    void restore(uint32_t pos) override;
    void relayout() override;

private:
    bool paginate();
    bool loadIndex();
    void saveIndex();
    uint32_t readWindow(uint32_t pos, uint32_t& winStart);   // returns bytes read
    layout::Layout makeLayout();
    void applyFont();

    uint32_t size_ = 0;
    uint32_t textStart_ = 0;   // after a UTF-8 BOM
    uint8_t font_ = 1;
    int lineH_ = 20, ascent_ = 15;
    std::vector<uint32_t> starts_;
};
