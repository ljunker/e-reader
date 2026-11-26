#pragma once
#include <Arduino.h>

#include <vector>

#include "FontFile.h"

// Fonts: the reading family comes from /fonts/<name>.xff on the card (built
// with 'xtconvert font', kept in PSRAM); menus and messages use the built-in
// Inter in flash, which also stands in when the card has no fonts.
namespace fonts {

// Attach the built-in font and load the family chosen in the settings.
// Call again after the card's contents may have changed.
void begin();

// Family names on the card (file names without .xff), sorted, then kBuiltin.
constexpr const char* kBuiltin = "built-in";
std::vector<String> families();
// Name of the family in use: a card family, or kBuiltin.
const String& family();
// Switch to the next family in families(); saved in the settings.
void nextFamily();
// Changes whenever the reading font does; part of the page index keys.
uint32_t cacheKey(uint8_t size);

constexpr int kSizes = 3;
inline const char* sizeName(uint8_t size) { return size == 0 ? "small" : size == 2 ? "large" : "medium"; }

// EPUB text. slot comes from etx::fontFor (size class * 4 + style bits);
// headings step up one or two sizes from the reading size.
const xff::Face* rich(uint8_t size, int slot);
// Plain text: the regular face of the reading size.
inline const xff::Face* reading(uint8_t size) { return rich(size, 0); }
int leading(uint8_t size);

const xff::Face* ui();
const xff::Face* uiBold();
const xff::Face* small();

}  // namespace fonts
