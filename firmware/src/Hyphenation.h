#pragma once
#include <Arduino.h>

#include "Hyphen.h"

// Hyphenation patterns on the card: /hyph/<lang>.hyp (from sdcard/hyph in the repository).
namespace hyphenation {

// Loads the patterns for `lang` ("de", "en", "" = unknown) into `h` when
// hyphenation is switched on. Returns an id for the page index caches:
// 0 = off, otherwise it changes with the language and the pattern file.
// Without a pattern file, words still break after a '-' they contain.
uint32_t load(const char* lang, hyph::Hyphenator& h);

}  // namespace hyphenation
