// microSD card and NVS. The page mounts IndexedDB at /sdcard and /nvs before
// the firmware starts and saves both back now and then.
#include <Preferences.h>
#include <SD_MMC.h>
#include <emscripten.h>
#include <sys/stat.h>

#include <stdio.h>

#include "Sim.h"
#include "vfs_api.h"

static bool cardIn = true;

bool sim::cardInserted() { return cardIn; }
void sim::fsChanged() { js_fs_changed(); }

extern "C" EMSCRIPTEN_KEEPALIVE void sim_set_card(int inserted) { cardIn = inserted; }

// ---------------------------------------------------------------- SD_MMC

fs::SDMMCFS SD_MMC = fs::SDMMCFS(FSImplPtr(new VFSImpl()));

static constexpr uint64_t kCardBytes = 8ULL * 1024 * 1024 * 1024;

namespace fs {

bool SDMMCFS::begin(const char* mountpoint, bool, bool, int, uint8_t) {
    if (mounted_) return true;
    if (!cardIn) return false;
    struct stat st;
    if (stat(mountpoint, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    _impl->mountpoint(mountpoint);
    mounted_ = true;
    return true;
}

void SDMMCFS::end() {
    if (!mounted_) return;
    _impl->mountpoint(nullptr);
    mounted_ = false;
    sim::fsChanged();
}

sdcard_type_t SDMMCFS::cardType() { return mounted_ ? CARD_SDHC : CARD_NONE; }
uint64_t SDMMCFS::cardSize() { return mounted_ ? kCardBytes : 0; }
uint64_t SDMMCFS::totalBytes() { return cardSize(); }
uint64_t SDMMCFS::usedBytes() { return 0; }
int SDMMCFS::sectorSize() { return mounted_ ? 512 : 0; }
int SDMMCFS::numSectors() { return mounted_ ? int(kCardBytes / 512) : 0; }
bool SDMMCFS::readRAW(uint8_t*, uint32_t) { return false; }
bool SDMMCFS::writeRAW(uint8_t*, uint32_t) { return false; }

}  // namespace fs

// ---------------------------------------------------------------- Preferences

// File format: one "key<TAB>value" per line; values are numbers or plain text
// without tabs and line breaks (all the firmware stores).

bool Preferences::begin(const char* name, bool readOnly, const char*) {
    file_ = std::string("/nvs/") + name;
    readOnly_ = readOnly;
    open_ = true;
    load();
    return true;
}

void Preferences::end() { open_ = false; }

void Preferences::load() {
    kv_.clear();
    FILE* f = fopen(file_.c_str(), "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        char* v = tab + 1;
        v[strcspn(v, "\r\n")] = 0;
        kv_[line] = v;
    }
    fclose(f);
}

void Preferences::save() {
    FILE* f = fopen(file_.c_str(), "w");
    if (!f) return;
    for (const auto& kv : kv_) fprintf(f, "%s\t%s\n", kv.first.c_str(), kv.second.c_str());
    fclose(f);
    sim::fsChanged();
}

bool Preferences::put(const char* key, const std::string& value) {
    if (!open_ || readOnly_) return false;
    auto it = kv_.find(key);
    if (it != kv_.end() && it->second == value) return true;
    kv_[key] = value;
    save();
    return true;
}

long long Preferences::getNum(const char* key, long long defaultValue) {
    auto it = kv_.find(key);
    return it == kv_.end() ? defaultValue : strtoll(it->second.c_str(), nullptr, 10);
}

String Preferences::getString(const char* key, const String& defaultValue) {
    auto it = kv_.find(key);
    return it == kv_.end() ? defaultValue : String(it->second.c_str());
}

bool Preferences::isKey(const char* key) { return kv_.count(key) != 0; }

bool Preferences::remove(const char* key) {
    if (!open_ || readOnly_ || !kv_.erase(key)) return false;
    save();
    return true;
}

bool Preferences::clear() {
    if (!open_ || readOnly_) return false;
    kv_.clear();
    save();
    return true;
}
