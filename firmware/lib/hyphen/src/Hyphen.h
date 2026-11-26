#pragma once
// Liang hyphenation (the TeX algorithm) on .hyp pattern files built by
// `xtconvert hyph` (format: tools/converter/src/xtconvert/hyph.py).
// Platform independent; text is UTF-8 within Latin-1, like the rest of the reader.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "ByteSource.h"

namespace hyph {

class Hyphenator {
public:
    // Reads the whole file into one buffer (PSRAM on the device). False if it is not a valid .hyp file.
    bool load(io::ByteSource& src, uint32_t size);
    bool loaded() const { return data_ != nullptr; }

    // Byte offsets inside the word s[0..n) where it may be broken, ascending.
    // Each run of letters is hyphenated on its own; a break right after a '-'
    // between letters is offered too (also without patterns).
    void breaks(const char* s, size_t n, std::vector<uint16_t>& out) const;

private:
    struct Len {
        uint32_t count = 0;
        const uint8_t *vectors = nullptr, *letters = nullptr, *index = nullptr;
    };
    void hyphenateRun(const uint8_t* lower, const uint16_t* offs, int n, std::vector<uint16_t>& out) const;
    const uint8_t* find(const uint8_t* s, int len) const;

    std::unique_ptr<uint8_t[]> data_;
    uint8_t left_ = 2, right_ = 2;
    std::vector<Len> lens_;   // index = pattern length - 1
};

// "de" or "en" for a language tag such as "de-DE" or "EN_gb", else "".
const char* normalizeLanguage(const char* tag);
// "de", "en" or "" from common words in a sample of the text.
const char* guessLanguage(const char* s, size_t n);

}  // namespace hyph
