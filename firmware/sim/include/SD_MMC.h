#pragma once
// microSD card of the simulator: the browser's IndexedDB mounted at /sdcard,
// reached through the arduino-esp32 VFS layer like the real card.
#include "FS.h"

#define SDMMC_FREQ_DEFAULT 20000
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_DEFAULT

typedef enum { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC, CARD_UNKNOWN } sdcard_type_t;

namespace fs {
class SDMMCFS : public FS {
public:
    SDMMCFS(FSImplPtr impl) : FS(impl) {}
    bool setPins(int clk, int cmd, int d0) { (void)clk; (void)cmd; (void)d0; return true; }
    bool setPins(int clk, int cmd, int d0, int d1, int d2, int d3) { (void)clk; (void)cmd; (void)d0; (void)d1; (void)d2; (void)d3; return true; }
    bool begin(const char* mountpoint = "/sdcard", bool mode1bit = false, bool format_if_mount_failed = false,
               int sdmmc_frequency = BOARD_MAX_SDMMC_FREQ, uint8_t maxOpenFiles = 5);
    void end();
    sdcard_type_t cardType();
    uint64_t cardSize();
    uint64_t totalBytes();
    uint64_t usedBytes();
    int sectorSize();
    int numSectors();
    bool readRAW(uint8_t* buffer, uint32_t sector);
    bool writeRAW(uint8_t* buffer, uint32_t sector);

private:
    bool mounted_ = false;
};
}  // namespace fs

extern fs::SDMMCFS SD_MMC;
