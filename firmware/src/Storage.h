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
    uint32_t lastRead = 0;   // order of opening, 0 = never opened
    uint8_t percent = 0;     // read so far, 100 = finished
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
// Books in /books and the root. Recently read first if `recentFirst`
// (books never opened follow by name), else by name.
std::vector<BookEntry> listBooks(bool recentFirst = false);
BookKind kindOf(const String& path);

// Per-book reading position and percent read (survive power-off), in NVS.
uint32_t loadPosition(const String& path);
void savePosition(const String& path, uint32_t pos, uint8_t percent);
// The book was opened now: it moves to the top of "recently read".
void markOpened(const String& path);

// Settings
uint8_t fontSize();
void setFontSize(uint8_t);
bool sortRecent();              // library order: recently read first
void setSortRecent(bool);
bool sleepCover();              // sleep screen: the book's cover instead of the page
void setSleepCover(bool);

// "/.reader/<hash><suffix>" for cached data belonging to a book.
String cacheFile(const String& bookPath, const char* suffix);
uint32_t hash(const String& s);
// A book was replaced or deleted: drop its cache files and saved position.
void forgetBook(const String& path);
}  // namespace storage
