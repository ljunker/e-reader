#include "Fonts.h"

#include <algorithm>

#include "BuiltinFont.h"
#include "Storage.h"

namespace fonts {

namespace {

constexpr const char* kDir = "/fonts";

struct FileSource : io::ByteSource {
    File f;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override { return f.seek(off) && f.read(dst, n) == n; }
};

xff::Font builtin, card;
String current = kBuiltin;
uint32_t currentId = 0;   // 0 = built-in

bool loadFamily(const String& name) {
    String path = String(kDir) + "/" + name + ".xff";
    FileSource src;
    src.f = storage::fs().open(path, FILE_READ);
    if (!src.f) return false;
    uint32_t size = src.f.size();
    bool ok = card.load(src, size);
    src.f.close();
    if (!ok) return false;
    current = name;
    currentId = storage::hash(path + ":" + String(size)) | 1;
    return true;
}

void useBuiltin() {
    card.unload();
    current = kBuiltin;
    currentId = 0;
}

}  // namespace

std::vector<String> families() {
    std::vector<String> out;
    if (storage::mounted()) {
        File d = storage::fs().open(kDir);
        if (d && d.isDirectory()) {
            for (File f = d.openNextFile(); f; f = d.openNextFile()) {
                String name = f.name();
                int slash = name.lastIndexOf('/');
                if (slash >= 0) name = name.substring(slash + 1);
                if (f.isDirectory() || name.startsWith(".") || !name.endsWith(".xff")) continue;
                out.push_back(name.substring(0, name.length() - 4));
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const String& a, const String& b) { return a.compareTo(b) < 0; });
    out.push_back(kBuiltin);
    return out;
}

void begin() {
    if (!builtin.loaded()) builtin.attach(kBuiltinFont, sizeof kBuiltinFont);
    String want = storage::mounted() ? storage::fontFamily() : String(kBuiltin);
    if (want == kBuiltin) {
        useBuiltin();
        return;
    }
    if (currentId && want == current) return;   // already loaded
    if (want.length() && storage::mounted() && loadFamily(want)) return;
    // nothing chosen yet, or the file is gone: the first family on the card
    for (const String& f : families())
        if (f != kBuiltin && f != want && loadFamily(f)) return;
    useBuiltin();
}

const String& family() { return current; }

void nextFamily() {
    std::vector<String> all = families();
    auto it = std::find(all.begin(), all.end(), current);
    for (size_t k = 0; k < all.size(); k++) {
        it = (it == all.end() || it + 1 == all.end()) ? all.begin() : it + 1;
        storage::setFontFamily(*it);
        begin();
        if (current == *it) return;   // loaded; otherwise try the next one
    }
}

uint32_t cacheKey(uint8_t size) { return (currentId & ~0xFFu) | size; }

const xff::Face* rich(uint8_t size, int slot) {
    uint8_t style = slot & 3;
    int step = size + slot / 4;
    if (!currentId) return builtin.nearest(style, 14 + 2 * step);   // 11 / 14 px regular, 16 px bold
    return card.find(style, step);
}

int leading(uint8_t size) {
    const xff::Face* f = reading(size);
    return f ? std::max(3, f->size() / 4) : 4;
}

const xff::Face* ui() { return builtin.nearest(xff::Regular, 14); }
const xff::Face* uiBold() { return builtin.nearest(xff::Bold, 16); }
const xff::Face* small() { return builtin.nearest(xff::Regular, 11); }

}  // namespace fonts
