#pragma once
// EPUB container: META-INF/container.xml -> OPF (title, manifest, spine,
// cover) and the table of contents (EPUB 3 nav document or EPUB 2 NCX).

#include <string>
#include <vector>

#include "Zip.h"

namespace epub {

struct ManifestItem {
    std::string id, path, mediaType, properties;
};

struct TocTarget {
    std::string title;   // UTF-8, whitespace collapsed
    int level = 0;       // 0 = top level
    std::string path;    // archive path of the document
    std::string fragment;
};

struct Package {
    std::string title;
    std::string language;             // dc:language as written, e.g. "de-DE"
    std::string opfPath;
    std::vector<ManifestItem> manifest;
    std::vector<std::string> spine;   // archive paths in reading order (linear items)
    std::string coverPath;            // archive path of the cover image, if any
    std::vector<TocTarget> toc;
};

// Fills `pkg`; on failure returns false with a message for the reader.
bool loadPackage(zip::Archive& zip, Package& pkg, std::string& error);

}  // namespace epub
