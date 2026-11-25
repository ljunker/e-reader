#pragma once
// Read-only ZIP archive access (stored and deflated entries, no ZIP64, no
// encryption). Platform independent: reads through an io::ByteSource.

#include <stdint.h>

#include <string>
#include <vector>

#include "ByteSource.h"
#include "Inflate.h"

namespace zip {

enum class Error { None, Io, NotZip, Unsupported, Corrupt, TooLarge, Stopped, NoMemory };
const char* errorText(Error e);

struct Entry {
    std::string name;
    uint32_t headerOffset = 0;   // local file header
    uint32_t compSize = 0;
    uint32_t size = 0;
    uint32_t crc = 0;
    uint16_t method = 0;         // 0 stored, 8 deflate
    bool encrypted = false;
};

uint32_t crc32(uint32_t crc, const uint8_t* data, size_t n);

class Archive {
public:
    Archive(io::ByteSource& src, uint32_t size) : src_(src), size_(size) {}

    Error open();
    const std::vector<Entry>& entries() const { return entries_; }
    // Exact name first, then case-insensitive.
    const Entry* find(const std::string& name) const;

    // Whole entry into memory; Error::TooLarge if it is bigger than maxSize.
    Error read(const Entry& e, std::vector<uint8_t>& out, uint32_t maxSize = 16u << 20);
    Error read(const Entry& e, std::string& out, uint32_t maxSize = 16u << 20);
    // Entry in pieces; the callback returns false to stop (Error::Stopped).
    // The CRC is checked when the whole entry was read.
    Error stream(const Entry& e, const OutputFn& out);

private:
    io::ByteSource& src_;
    uint32_t size_;
    std::vector<Entry> entries_;
};

}  // namespace zip
