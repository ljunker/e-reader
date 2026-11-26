#include "Hyphenation.h"

#include "Storage.h"

namespace hyphenation {

namespace {
struct FileSource : io::ByteSource {
    File f;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override { return f.seek(off) && f.read(dst, n) == n; }
};
}  // namespace

uint32_t load(const char* lang, hyph::Hyphenator& h) {
    h.unload();
    if (!storage::hyphenate()) return 0;
    if (!lang || !*lang) return 1;
    String path = String("/hyph/") + lang + ".hyp";
    FileSource src;
    src.f = storage::fs().open(path, FILE_READ);
    if (!src.f) return 1;
    uint32_t size = src.f.size();
    bool ok = h.load(src, size);
    src.f.close();
    if (!ok) return 1;
    return storage::hash(path + ":" + String(size)) | 2;
}

}  // namespace hyphenation
