#pragma once
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "pins.h"

using Epd = GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>;

// Wraps the Waveshare 4.2" V2 (SSD1683) panel: power pin, SPI pins, refresh policy.
class Display {
public:
    void begin(bool coldBoot);
    void powerOff();             // hibernate the controller and cut module power

    Epd& gfx() { return epd_; }
    U8G2_FOR_ADAFRUIT_GFX& text() { return u8g2_; }

    void setPortrait(bool portrait);
    int width() { return epd_.width(); }
    int height() { return epd_.height(); }
    void clear() { epd_.fillScreen(GxEPD_WHITE); }

    // Push the buffer to the panel. Fast partial refresh, with a full refresh
    // every FULL_REFRESH_EVERY updates or when forced.
    void show(bool forceFull = false);
    void requestFullRefresh() { sinceFull_ = 1000; }

    // Draw a 4-level gray pixel (0 = black .. 3 = white) using a 2×2 ordered
    // dither, since the panel is driven in black/white mode.
    inline void grayPixel(int16_t x, int16_t y, uint8_t level) {
        static const uint8_t bayer[2][2] = {{0, 2}, {3, 1}};
        // level 0 → 4/4 black, 1 → 3/4, 2 → 1/4, 3 → 0
        static const uint8_t blackOf4[4] = {4, 3, 1, 0};
        bool black = bayer[y & 1][x & 1] < blackOf4[level];
        epd_.drawPixel(x, y, black ? GxEPD_BLACK : GxEPD_WHITE);
    }

private:
    Epd epd_{GxEPD2_420_GDEY042T81(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)};
    U8G2_FOR_ADAFRUIT_GFX u8g2_;
    int sinceFull_ = 1000;
    bool initialised_ = false;
};

extern Display display;
