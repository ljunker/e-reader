#pragma once
#include <U8g2_for_Adafruit_GFX.h>

// Fonts from U8g2 with full Latin-1 (umlauts, ß, accents).
// Milestone 3 replaces these with anti-aliased fonts generated from TTFs.
namespace fonts {
constexpr int kSizes = 3;
inline const uint8_t* reading(uint8_t size) {
    switch (size) {
        case 0: return u8g2_font_ncenR12_tf;
        case 2: return u8g2_font_ncenR18_tf;
        default: return u8g2_font_ncenR14_tf;
    }
}
inline int leading(uint8_t size) { return size == 2 ? 6 : 4; }
inline const char* sizeName(uint8_t size) { return size == 0 ? "small" : size == 2 ? "large" : "medium"; }
inline const uint8_t* ui() { return u8g2_font_helvR12_tf; }
inline const uint8_t* uiBold() { return u8g2_font_helvB14_tf; }
inline const uint8_t* small() { return u8g2_font_helvR08_tf; }
}  // namespace fonts
