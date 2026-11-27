#pragma once
#include <GxEPD2_BW.h>

// The GDEY042T81 (SSD1683) driver from GxEPD2, plus a 4-gray full refresh.
// GxEPD2 itself drives the panel in black and white only; GxEPD2_4G can do
// grays but defines a class of the same name, so it cannot be linked as well.
class Panel : public GxEPD2_420_GDEY042T81 {
public:
    using GxEPD2_420_GDEY042T81::GxEPD2_420_GDEY042T81;

    static constexpr uint16_t kGrayRefreshMs = 5500;   // measured 5.4 s (GxEPD2_4G)

    // Show a whole frame in 4 grays. The levels buffer holds 2 bits per pixel
    // in panel order (400 × 300, row-major, leftmost pixel in the top bits),
    // 0 = black .. 3 = white. Takes about 5.5 s and flashes like a full
    // refresh. The next black-and-white update must be a full refresh.
    void showGray(const uint8_t* levels);

    // After showGray(): put the black-and-white picture `bw` (GxEPD2's
    // buffer) into both RAMs as if it had just been shown. The next partial
    // refresh then drives only the pixels that differ from it; the others,
    // grays included, stay on the glass as they are.
    void assumeShown(const uint8_t* bw) {
        _initial_write = false;   // else GxEPD2 clears the screen first
        _initial_refresh = false;
        writeImageAgain(bw, 0, 0, WIDTH, HEIGHT);
    }

private:
    void writePlane(uint8_t command, const uint8_t* levels, uint8_t setFor);
};
