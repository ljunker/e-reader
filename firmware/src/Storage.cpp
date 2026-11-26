#include "Storage.h"

#include <Preferences.h>
#include <SD_MMC.h>

#include <algorithm>

#include "config.h"
#include "pins.h"

namespace storage {

static Preferences prefs;
static bool mounted_ = false;

bool begin(String& error) {
    prefs.begin("reader", false);
    if (PIN_SD_DET >= 0) pinMode(PIN_SD_DET, INPUT_PULLUP);
    if (!cardPresent()) {
        error = "No microSD card";
        return false;
    }
    if (!SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3)) {
        error = "SD pin setup failed";
        return false;
    }
    if (!SD_MMC.begin("/sdcard", false /* 4-bit */, false, SDMMC_FREQ_DEFAULT, 5)) {
        error = "Cannot read the microSD card (FAT32?)";
        return false;
    }
    if (!SD_MMC.exists(STATE_DIR)) SD_MMC.mkdir(STATE_DIR);
    mounted_ = true;
    return true;
}

bool mounted() { return mounted_; }

void suspend() { mounted_ = false; }

bool remount(String& error) {
    mounted_ = false;
    SD_MMC.end();
    return begin(error);
}

bool cardPresent() { return PIN_SD_DET < 0 || digitalRead(PIN_SD_DET) == SD_DET_ACTIVE; }

fs::FS& fs() { return SD_MMC; }

BookKind kindOf(const String& path) {
    String p = path;
    p.toLowerCase();
    if (p.endsWith(".txt")) return BookKind::Txt;
    if (p.endsWith(".xtp")) return BookKind::Xtp;
    if (p.endsWith(".epub")) return BookKind::Epub;
    return BookKind::Other;
}

static void scan(const char* dir, std::vector<BookEntry>& out) {
    File d = SD_MMC.open(dir);
    if (!d || !d.isDirectory()) return;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
        if (f.isDirectory()) continue;
        String name = f.name();
        if (name.startsWith(".")) continue;   // macOS "._" files etc.
        BookKind k = kindOf(name);
        if (k == BookKind::Other) continue;
        String path = String(dir) + (String(dir).endsWith("/") ? "" : "/") + name;
        int dot = name.lastIndexOf('.');
        out.push_back({path, dot > 0 ? name.substring(0, dot) : name, k, uint32_t(f.size())});
    }
}

std::vector<BookEntry> listBooks() {
    std::vector<BookEntry> out;
    if (!mounted_) return out;
    scan(BOOKS_DIR, out);
    scan("/", out);
    std::sort(out.begin(), out.end(), [](const BookEntry& a, const BookEntry& b) {
        return a.name.compareTo(b.name) < 0;
    });
    return out;
}

uint32_t hash(const String& s) {   // FNV-1a
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < s.length(); i++) h = (h ^ uint8_t(s[i])) * 16777619u;
    return h;
}

static String key(const String& path) {
    char k[12];
    snprintf(k, sizeof k, "p%08lx", (unsigned long)hash(path));
    return k;
}

uint32_t loadPosition(const String& path) { return prefs.getUInt(key(path).c_str(), 0); }
void savePosition(const String& path, uint32_t pos) {
    if (prefs.getUInt(key(path).c_str(), UINT32_MAX) != pos) prefs.putUInt(key(path).c_str(), pos);
}

uint8_t fontSize() { return prefs.getUChar("font", 1); }
void setFontSize(uint8_t s) { prefs.putUChar("font", s); }

String cacheFile(const String& bookPath, const char* suffix) {
    char name[24];
    snprintf(name, sizeof name, "/%08lx", (unsigned long)hash(bookPath));
    return String(STATE_DIR) + name + suffix;
}

void forgetBook(const String& path) {
    for (const char* suffix : {".tix", ".eix", ".etx", ".eim", ".etc"}) {
        String f = cacheFile(path, suffix);
        if (SD_MMC.exists(f)) SD_MMC.remove(f);
    }
    if (prefs.isKey(key(path).c_str())) prefs.remove(key(path).c_str());
}

}  // namespace storage
