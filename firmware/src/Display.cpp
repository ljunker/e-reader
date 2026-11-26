#include "Display.h"

#include <SPI.h>

#include "config.h"
#include "pins.h"

Display display;

void Display::begin(bool coldBoot) {
    if (PIN_EPD_PWR >= 0) {
        pinMode(PIN_EPD_PWR, OUTPUT);
        digitalWrite(PIN_EPD_PWR, HIGH);
        delay(10);
    }
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
    epd_.epd2.selectSPI(SPI, SPISettings(10000000, MSBFIRST, SPI_MODE0));
    // initial = true forces a full refresh first; after deep sleep the panel
    // still shows the last image, so a partial update would be wrong anyway.
    epd_.init(0, true, 2, false);
    epd_.setRotation(DISPLAY_ROTATION_PORTRAIT);
    epd_.setTextColor(GxEPD_BLACK);
    text_.begin(epd_, *this);
    text_.setForegroundColor(GxEPD_BLACK);
    sinceFull_ = 1000;
    initialised_ = true;
    (void)coldBoot;
}

void Display::powerOff() {
    if (!initialised_) return;
    epd_.hibernate();
    if (PIN_EPD_PWR >= 0) digitalWrite(PIN_EPD_PWR, LOW);
    initialised_ = false;
}

void Display::setPortrait(bool portrait) {
    epd_.setRotation(portrait ? DISPLAY_ROTATION_PORTRAIT : DISPLAY_ROTATION_LANDSCAPE);
}

bool Display::startGray() {
    static constexpr size_t kBytes = size_t(Panel::WIDTH) * Panel::HEIGHT / 4;
    if (!levels_) levels_ = static_cast<uint8_t*>(ps_malloc(kBytes));
    if (!levels_) return false;
    memset(levels_, 0xFF, kBytes);   // white
    grayFrame_ = true;
    return true;
}

// Logical (rotated) coordinates to the panel's, as GxEPD2_BW::drawPixel does.
void Display::setLevel(int16_t x, int16_t y, uint8_t level) {
    if (x < 0 || y < 0 || x >= epd_.width() || y >= epd_.height()) return;
    int16_t t;
    switch (epd_.getRotation()) {
        case 1: t = x; x = Panel::WIDTH - y - 1; y = t; break;
        case 2: x = Panel::WIDTH - x - 1; y = Panel::HEIGHT - y - 1; break;
        case 3: t = x; x = y; y = Panel::HEIGHT - t - 1; break;
        default: break;
    }
    uint32_t i = uint32_t(y) * Panel::WIDTH + x;
    uint8_t shift = 6 - 2 * (i & 3);
    levels_[i >> 2] = uint8_t((levels_[i >> 2] & ~(3 << shift)) | (level & 3) << shift);
}

void Display::show(bool forceFull) {
    if (grayFrame_) {
        epd_.epd2.showGray(levels_);
        sinceFull_ = 1000;   // the next black-and-white update must be full
        return;
    }
    if (forceFull || sinceFull_ >= FULL_REFRESH_EVERY) {
        epd_.display(false);
        sinceFull_ = 0;
    } else {
        epd_.display(true);
        sinceFull_++;
    }
}

void TextRenderer::plot(void* ctx, int x, int y, uint8_t ink) {
    auto* t = static_cast<TextRenderer*>(ctx);
    if (t->display_ && t->display_->grayFrame()) {   // anti-aliased
        t->display_->grayPixel(x, y, t->fg_ == GxEPD_WHITE ? ink : 3 - ink);
        return;
    }
    if (ink >= 2) t->gfx_->drawPixel(x, y, t->fg_);
}

int16_t TextRenderer::drawUTF8(int16_t x, int16_t y, const char* s) {
    if (!face_ || !gfx_) return 0;
    return face_->draw(x, y, s, strlen(s), plot, this);
}
