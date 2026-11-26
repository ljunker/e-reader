#pragma once
#include <vector>

#include "Book.h"
#include "EpubCache.h"
#include "RichLayout.h"

// EPUB books. On first open the EPUB is converted into cache files on the
// card (rich text, pictures, contents; see lib/epub/src/EpubCache.h), then
// paginated once per font size like TxtBook. Positions are offsets into the
// rich-text cache, so they survive font changes.
class EpubBook : public Book {
public:
    bool open(const BookEntry& entry, String& error) override;
    uint32_t pageCount() const override { return starts_.size(); }
    void render() override;
    uint32_t position() const override { return starts_.empty() ? 0 : starts_[page_].pos; }
    uint32_t pageOf(uint32_t pos) const override;
    void relayout() override;
    const std::vector<TocEntry>& toc() const override { return toc_; }

private:
    struct Start {   // page start in the .etx file and the style in effect there
        uint32_t pos;
        uint8_t block, style, pad[2];
    };
    static_assert(sizeof(Start) == 8, "stored on the card");

    bool loadCache(uint32_t epubSize);
    bool buildCache(const BookEntry& entry, uint32_t epubSize, String& error);
    bool paginate();
    bool loadIndex();
    void saveIndex();
    rich::Layout makeLayout();
    uint32_t readWindow(uint32_t pos);
    void drawImage(const rich::Line& line);

    String etxPath_, eimPath_, etcPath_;
    uint32_t etxSize_ = 0;
    uint8_t font_ = 1;
    std::vector<Start> starts_;
    std::vector<epub::ImageInfo> images_;
    std::vector<TocEntry> toc_;
};
