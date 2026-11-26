#pragma once
// Converts an EPUB once into three cache files the reader works from:
//
//   .etx  "ETX1", u32 version, u32 EPUB size, u32 0, then the rich-text
//         stream (see Etx.h). Positions are byte offsets into this file.
//   .eim  pictures: 2-bit pixel data (image::pack2bpp), then a table of
//         count × (u32 offset, u16 w, u16 h), then a trailer
//         (u32 table offset, u32 count, u32 EPUB size, "EIM1").
//         A picture that could not be decoded has w = h = 0.
//   .etc  "ETC1", u32 version, u32 EPUB size, u16 title length, title,
//         u8 language length, language (dc:language, e.g. "de-DE"),
//         u32 count, count × (u32 position, u8 level, u8 length, title).
//         Written last: when it is valid, the conversion completed.
//
// All integers little-endian, all text UTF-8 within Latin-1.

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "ByteSource.h"
#include "Package.h"
#include "Zip.h"

namespace epub {

constexpr uint32_t kCacheVersion = 2;   // 2: language in .etc
constexpr uint32_t kEtxHeaderSize = 16;

struct TocItem {
    uint32_t pos = 0;   // offset in the .etx file
    uint8_t level = 0;
    std::string title;
};

struct ImageInfo {
    uint32_t offset = 0;
    uint16_t w = 0, h = 0;
};

struct BuildOptions {
    int imageMaxW = 276;   // text area of the reader
    int imageMaxH = 374;
    // phase is "Text" or "Pictures"
    std::function<void(const char* phase, uint32_t done, uint32_t total)> progress;
};

bool buildCache(zip::Archive& zip, const Package& pkg, uint32_t epubSize, io::Writer& etx, io::Writer& eim,
                io::Writer& etc, const BuildOptions& opt, std::string& error);

bool checkEtxHeader(const uint8_t header[kEtxHeaderSize], uint32_t epubSize);
bool readToc(io::ByteSource& src, uint32_t fileSize, uint32_t epubSize, std::string& title, std::string& language,
             std::vector<TocItem>& out);
bool readImageTable(io::ByteSource& src, uint32_t fileSize, uint32_t epubSize, std::vector<ImageInfo>& out);

}  // namespace epub
