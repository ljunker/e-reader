#pragma once
#include <GxEPD2_BW.h>

#include "FontFile.h"
#include "Panel.h"

#include "pins.h"

using Epd = GxEPD2_BW<Panel, Panel::HEIGHT>;

// Draws text in the .xff fonts (see Fonts.h) into the display buffer. The
// interface follows the U8g2 calls the reader used before. Anti-aliased
// glyphs are thresholded: ink levels 2 and 3 become the foreground colour.
class TextRenderer {
public:
    void begin(Adafruit_GFX& gfx, class Display& d) {
        gfx_ = &gfx;
        display_ = &d;
    }
    void setFont(const xff::Face* face) { face_ = face; }
    const xff::Face* font() const { return face_; }
    void setForegroundColor(uint16_t c) { fg_ = c; }

    // Pen at (x, baseline y); returns the advance.
    int16_t drawUTF8(int16_t x, int16_t y, const char* s);
    int16_t getUTF8Width(const char* s) const { return face_ ? face_->width(s) : 0; }
    int16_t getFontAscent() const { return face_ ? face_->ascent() : 0; }
    int16_t getFontDescent() const { return face_ ? -face_->descent() : 0; }   // negative, as in U8g2

private:
    static void plot(void* ctx, int x, int y, uint8_t ink);
    Adafruit_GFX* gfx_ = nullptr;
    class Display* display_ = nullptr;
    const xff::Face* face_ = nullptr;
    uint16_t fg_ = GxEPD_BLACK;
};

// Wraps the Waveshare 4.2" V2 (SSD1683) panel: power pin, SPI pins, refresh policy.
class Display {
public:
    void begin(bool coldBoot);
    void powerOff();             // hibernate the controller and cut module power

    Epd& gfx() { return epd_; }
    TextRenderer& text() { return text_; }

    void setPortrait(bool portrait);
    int width() { return epd_.width(); }
    int height() { return epd_.height(); }
    // Start a new frame in black and white.
    void clear() {
        epd_.fillScreen(GxEPD_WHITE);
        grayFrame_ = false;
        grayPictures_ = false;
    }
    // Make the frame just cleared a 4-gray one: grayPixel() and text land in
    // true grays and show() does a 4-gray refresh (about 5.5 s). For whole-
    // screen pictures: .xtp pages and the sleep-screen cover. Returns false
    // (black and white with dithering) if the gray buffer cannot be had.
    bool startGray();
    bool grayFrame() const { return grayFrame_; }

    // Keep the frame just cleared black and white (text, lines, fast partial
    // refreshes), but show what grayPixel() draws in true grays. show() makes
    // a 4-gray refresh (about 5.5 s) when those pictures differ from the ones
    // on the panel, else a partial refresh that leaves the grays alone. For
    // the cover thumbnails in the library. Black-and-white drawing on top of a
    // gray picture pixel is lost. Returns false (dithering) without memory.
    bool startGrayPictures();

    // Push the buffer to the panel. Fast partial refresh, with a full refresh
    // every FULL_REFRESH_EVERY updates or when forced.
    void show(bool forceFull = false);
    void requestFullRefresh() { sinceFull_ = 1000; }

    // A frame drawn in advance (next or previous page), kept in PSRAM: the
    // black-and-white buffer (15 KB), the gray levels if it is a gray frame
    // (30 KB) and the rotation. Buffers are allocated on first use and reused.
    struct Frame {
        uint8_t* bw = nullptr;
        uint8_t* gray = nullptr;
        bool isGray = false;
        uint8_t rotation = 0;
    };
    // Copy what has been drawn since clear() into `f`; false if out of memory.
    bool saveFrame(Frame& f);
    // Make `f` the frame to show, as if it had just been drawn.
    void loadFrame(const Frame& f);

    // Draw a 4-level gray pixel (0 = black .. 3 = white): a true gray in a
    // gray frame, else a 2×2 ordered dither in black and white.
    inline void grayPixel(int16_t x, int16_t y, uint8_t level) {
        if (grayFrame_) {
            setLevel(x, y, level);
            return;
        }
        if (grayPictures_) setLevel(x, y, level);   // and dithered for the partial refreshes
        static const uint8_t bayer[2][2] = {{0, 2}, {3, 1}};
        // level 0 → 4/4 black, 1 → 3/4, 2 → 1/4, 3 → 0
        static const uint8_t blackOf4[4] = {4, 3, 1, 0};
        bool black = bayer[y & 1][x & 1] < blackOf4[level];
        epd_.drawPixel(x, y, black ? GxEPD_BLACK : GxEPD_WHITE);
    }

private:
    Epd epd_{Panel(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)};
    TextRenderer text_;
    void setLevel(int16_t x, int16_t y, uint8_t level);
    uint8_t* buffer();            // GxEPD2's black-and-white frame buffer
    uint8_t* levels_ = nullptr;   // gray frame, 2 bits per pixel in panel order (PSRAM)
    bool grayFrame_ = false;
    bool grayPictures_ = false;
    // Gray pictures in a black-and-white frame (startGrayPictures): the levels
    // they were last shown with, the frame composed for the 4-gray refresh,
    // and whether such a picture is on the glass, which a partial refresh of
    // another screen would leave behind.
    uint8_t* shownLevels_ = nullptr;
    uint8_t* composed_ = nullptr;
    bool grayOnGlass_ = false;
    void showGrayPictures();
    int sinceFull_ = 1000;
    bool initialised_ = false;
};

extern Display display;
