#include "Book.h"

#include "EpubBook.h"
#include "TxtBook.h"
#include "XtpBook.h"

std::unique_ptr<Book> openBook(const BookEntry& entry, String& error) {
    std::unique_ptr<Book> b;
    switch (entry.kind) {
        case BookKind::Txt: b.reset(new TxtBook()); break;
        case BookKind::Xtp: b.reset(new XtpBook()); break;
        case BookKind::Epub: b.reset(new EpubBook()); break;
        default:
            error = "Unsupported file type";
            return nullptr;
    }
    if (!b->open(entry, error)) return nullptr;
    return b;
}
