#pragma once
// Greedy line breaking and pagination for plain text. Platform independent:
// glyph widths come from a caller-supplied measure function.
//
// Text model (fits hard-wrapped .txt files such as Project Gutenberg):
//   - a blank line ("\n\n", optionally with spaces or '\r') ends a paragraph
//   - a single '\n' is treated like a space
//   - runs of spaces collapse to one

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <vector>

namespace layout {

// Width in pixels of the UTF-8 text s[0..n).
using MeasureFn = std::function<int(const char* s, size_t n)>;

struct Line {
    uint32_t start;   // byte range within the buffer
    uint32_t end;
    bool paraStart;   // first line of a paragraph (gets the indent)
    bool paraEnd;     // last line of a paragraph (not justified)
};

struct Params {
    int width = 300;    // line width in pixels
    int indent = 0;     // first-line indent
    int maxLines = 20;  // lines per page
};

struct Word {
    uint32_t start, end;
    int x;
};

class Layout {
public:
    Layout(MeasureFn measure, Params p);

    // Lay out up to maxLines lines from buf[pos..len). Set eof when the buffer
    // ends where the text ends; otherwise layout stops before a word that might
    // continue past the buffer. `bufStartIsTextStart` tells whether buf[0] is
    // the beginning of the text (for paragraph detection at pos == 0).
    // Appends to `out` and returns the position where the next page starts.
    uint32_t page(const char* buf, uint32_t len, uint32_t pos, bool eof, bool bufStartIsTextStart,
                  std::vector<Line>& out) const;

    // Word positions for drawing a line, justified unless it ends a paragraph.
    void placeWords(const char* buf, const Line& line, bool justify, std::vector<Word>& out) const;

    const Params& params() const { return p_; }

private:
    bool isParaStart(const char* buf, uint32_t pos, bool bufStartIsTextStart) const;
    MeasureFn measure_;
    Params p_;
    int spaceW_;
};

// Length in bytes of the longest prefix of s[0..n) that fits into `width`,
// cut at a UTF-8 character boundary; at least one character.
size_t fitPrefix(const MeasureFn& m, const char* s, size_t n, int width);

}  // namespace layout
