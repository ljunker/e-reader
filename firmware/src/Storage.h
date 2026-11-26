#pragma once
#include <Arduino.h>
#include <FS.h>

#include <vector>

enum class BookKind : uint8_t { Txt, Xtp, Epub, Other };

struct BookEntry {
    String path;     // full path on the card
    String name;     // file name without extension
    BookKind kind;
    uint32_t size;
};

namespace storage {
bool begin(String& error);
bool cardPresent();
bool mounted();
// Stop using the file system while the computer owns the card (USB drive),
// then mount it again afresh: the host may have changed the FAT under us.
void suspend();
bool remount(String& error);
fs::FS& fs();
std::vector<BookEntry> listBooks();
BookKind kindOf(const String& path);

// Per-book reading position (survives power-off), stored in NVS.
uint32_t loadPosition(const String& path);
void savePosition(const String& path, uint32_t pos);

// Settings
uint8_t fontSize();
void setFontSize(uint8_t);

// "/.reader/<hash><suffix>" for cached data belonging to a book.
String cacheFile(const String& bookPath, const char* suffix);
uint32_t hash(const String& s);
// A book was replaced or deleted: drop its cache files and saved position.
void forgetBook(const String& path);
}  // namespace storage
