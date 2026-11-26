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
    text_.begin(epd_);
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

void Display::show(bool forceFull) {
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
    if (ink >= 2) t->gfx_->drawPixel(x, y, t->fg_);
}

int16_t TextRenderer::drawUTF8(int16_t x, int16_t y, const char* s) {
    if (!face_ || !gfx_) return 0;
    return face_->draw(x, y, s, strlen(s), plot, this);
}
