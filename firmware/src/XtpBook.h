#pragma once
#include <FS.h>

#include "Book.h"
#include "Xtp.h"

// Pre-rendered pages from tools/converter (PDF, comics).
class XtpBook : public Book {
public:
    ~XtpBook() override;
    bool open(const BookEntry& entry, String& error) override;
    uint32_t pageCount() const override { return reader_ ? reader_->header().pageCount : 0; }
    void render() override;
    bool fullScreen() const override { return true; }
    bool landscape() const override;

private:
    struct FileSource : xtp::ByteSource {
        File f;
        bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
            return f.seek(off) && f.read(dst, n) == n;
        }
    } src_;
    xtp::Reader* reader_ = nullptr;
    uint8_t* pageBuf_ = nullptr;
    uint8_t* scratch_ = nullptr;
    size_t scratchSize_ = 0;
};
