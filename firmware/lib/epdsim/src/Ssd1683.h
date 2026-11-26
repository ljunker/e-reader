#pragma once
// Model of the SSD1683 controller on the Waveshare 4.2" V2 panel, as far as
// GxEPD2 and the reader's 4-gray refresh (src/Panel.cpp) drive it: two
// 400 × 300 1-bit RAMs, the RAM window and address counters, the update
// sequence (0x22/0x20), a waveform loaded with 0x32 (4 grays) and deep sleep. Used by the
// browser simulator; no Arduino or Emscripten code, so the host tests check it.
#include <stdint.h>

#include <functional>

namespace epdsim {

enum class Refresh : uint8_t { Full, Partial, Gray };

class Ssd1683 {
public:
    static constexpr int kWidth = 400;
    static constexpr int kHeight = 300;
    static constexpr int kStride = kWidth / 8;   // bytes per RAM row

    // Times the panel holds BUSY high, in milliseconds (GDEY042T81 data sheet,
    // as used by GxEPD2), before scaling by `timeScale`.
    static constexpr uint32_t kPowerOnMs = 100;
    static constexpr uint32_t kPowerOffMs = 300;
    static constexpr uint32_t kFullMs = 1200;
    static constexpr uint32_t kPartialMs = 400;
    static constexpr uint32_t kGrayMs = 5500;
    static constexpr uint32_t kResetMs = 10;

    Ssd1683() { powerLoss(); }

    // Called when the panel shows a new picture. Full and Partial: the
    // black/white RAM (1 = white, row-major, kStride bytes per row). Gray:
    // kWidth × kHeight bytes, one level per pixel, 0 = black .. 3 = white.
    std::function<void(const uint8_t* frame, Refresh kind, uint32_t durationMs)> onRefresh;

    // 1.0 = real panel timing, 0 = instant.
    float timeScale = 1.0f;

    // SPI byte with the D/C line low (command) or high (data). `nowMs` is the
    // time of the transfer, for BUSY.
    void write(bool isData, uint8_t byte, uint32_t nowMs);
    // RST pin pulled low: leaves deep sleep, registers to their defaults.
    void hardwareReset(uint32_t nowMs);
    // Module power cut (PWR pin low): RAM and registers are lost. The glass
    // keeps showing its last picture; that is up to the caller.
    void powerLoss();

    bool busy(uint32_t nowMs) const { return int32_t(busyUntil_ - nowMs) > 0; }
    bool sleeping() const { return sleeping_; }
    bool poweredOn() const { return analogOn_; }

    const uint8_t* bwRam() const { return bw_; }
    const uint8_t* redRam() const { return red_; }
    // Pixel of the black/white RAM in panel coordinates; true = white.
    bool white(int x, int y) const { return bw_[y * kStride + x / 8] & (0x80 >> (x & 7)); }
    // Gray level a 4-gray refresh shows for a pixel: RAM 0x26 holds the high
    // bit (set = black or dark gray), RAM 0x24 the low bit (set = black or
    // light gray), as the waveform from Waveshare's demo reads them.
    uint8_t level(int x, int y) const {
        uint8_t m = 0x80 >> (x & 7);
        int i = y * kStride + x / 8;
        return uint8_t(((red_[i] & m) ? 0 : 2) + ((bw_[i] & m) ? 0 : 1));
    }
    bool waveformLoaded() const { return lutLoaded_; }

    // Diagnostics for the tests.
    uint32_t refreshCount() const { return refreshes_; }
    uint32_t unknownCommands() const { return unknown_; }

private:
    void command(uint8_t c, uint32_t nowMs);
    void data(uint8_t d, uint32_t nowMs);
    void defaults();
    void ramWrite(uint8_t* ram, uint8_t d);
    void advance();
    void activate(uint32_t nowMs);
    void holdBusy(uint32_t ms, uint32_t nowMs);

    uint8_t bw_[kStride * kHeight];
    uint8_t levels_[kWidth * kHeight];
    uint8_t red_[kStride * kHeight];

    uint8_t cmd_ = 0;
    uint8_t args_[8] = {};
    int nArgs_ = 0;

    uint8_t entryMode_ = 0x03;
    int xStart_ = 0, xEnd_ = kStride - 1;      // in bytes
    int yStart_ = 0, yEnd_ = kHeight - 1;
    int xCount_ = 0, yCount_ = 0;
    uint8_t updateSeq_ = 0xff;
    uint8_t updateCtrl_ = 0;
    bool analogOn_ = false;
    bool lutLoaded_ = false;   // 0x32 since the last reset: refreshes use it
    bool sleeping_ = false;
    uint32_t busyUntil_ = 0;
    uint32_t refreshes_ = 0;
    uint32_t unknown_ = 0;
};

}  // namespace epdsim
