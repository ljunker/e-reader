#pragma once
// Line breaking and pagination for the .etx rich-text stream (see Etx.h):
// styled words, headings with their own line height, block alignment,
// indents and pictures. Platform independent: font metrics, text widths and
// picture sizes come from the caller.

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <vector>

#include "Etx.h"

namespace rich {

// Block and inline style in effect at a position. Stored in the page index,
// because a page can start in the middle of a styled paragraph.
struct State {
    uint8_t block = etx::Block().encode();
    uint8_t style = 0;
    bool operator==(const State& o) const { return block == o.block && style == o.style; }
};

struct Metrics {
    int ascent = 0;    // above the baseline
    int descent = 0;   // below the baseline (positive)
};

struct Env {
    std::function<int(int font, const char* s, size_t n)> measure;   // width of UTF-8 text
    std::function<Metrics(int font)> metrics;
    std::function<void(uint32_t index, int& w, int& h)> imageSize;   // 0 × 0 = skip
    int width = 300;        // text area
    int height = 400;
    int indent = 16;        // first-line indent of body paragraphs
    int blockIndent = 16;   // blockquotes (both sides) and list items (left)
    int leading = 4;        // extra space between lines
    int paraGap = 8;        // before spaceBefore blocks, around pictures
};

struct Line {
    uint32_t start = 0, end = 0;   // byte range in the buffer
    State state;                    // at `start`
    int top = 0;                    // y of the line box within the page
    int height = 0;
    int ascent = 0;                 // baseline = top + ascent
    int left = 0;                   // x of the line box (block indent)
    int avail = 0;                  // line box width
    int firstIndent = 0;            // first line of a paragraph
    bool justify = false;           // stretch the gaps to `avail`
    bool image = false;
    uint32_t imageIndex = 0;
    int imageW = 0, imageH = 0;
};

struct Run {
    uint32_t start, end;   // text bytes without control codes
    int x;                 // relative to the text area
    int font;
};

class Layout {
public:
    explicit Layout(Env env);

    // Lay out one page from buf[pos..len) with `st` in effect at pos. Set eof
    // when the buffer ends where the text ends; otherwise layout stops before
    // anything that might continue past the buffer. Appends the lines to `out`,
    // returns where the next page starts and its state in `next`.
    uint32_t page(const char* buf, uint32_t len, uint32_t pos, State st, bool eof, std::vector<Line>& out,
                  State& next) const;

    // Text runs of a line with their x positions (alignment and justification
    // applied). Not for picture lines.
    void runs(const char* buf, const Line& line, std::vector<Run>& out) const;

    const Env& env() const { return env_; }

private:
    enum class End { Width, Break, Para, Incomplete };
    End fillLine(const char* buf, uint32_t len, uint32_t& pos, State& st, bool eof, bool paraStart,
                 Line& line, bool& any) const;
    int wordWidth(const char* buf, uint32_t from, uint32_t to, State st) const;
    Env env_;
    int spaceW_[etx::kFontSlots];
    Metrics metrics_[etx::kFontSlots];
};

}  // namespace rich
