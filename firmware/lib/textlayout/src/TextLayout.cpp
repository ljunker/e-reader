#include "TextLayout.h"

namespace layout {

static inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

Layout::Layout(MeasureFn measure, Params p) : measure_(std::move(measure)), p_(p) {
    spaceW_ = measure_(" ", 1);
    if (spaceW_ <= 0) spaceW_ = 1;
    hyphenW_ = measure_("-", 1);
}

bool Layout::isParaStart(const char* buf, uint32_t pos, bool bufStartIsTextStart) const {
    int nl = 0;
    uint32_t i = pos;
    while (i > 0) {
        char c = buf[i - 1];
        if (c == '\n') nl++;
        else if (!isSpace(c)) return nl >= 2;
        i--;
    }
    // reached the buffer start
    return bufStartIsTextStart || nl >= 2;
}

size_t fitPrefix(const MeasureFn& m, const char* s, size_t n, int width) {
    size_t best = 0, i = 0;
    while (i < n) {
        size_t j = i + 1;
        while (j < n && (uint8_t(s[j]) & 0xC0) == 0x80) j++;   // skip continuation bytes
        if (m(s, j) > width) break;
        best = j;
        i = j;
    }
    if (best == 0) {   // not even one character fits: take one anyway
        best = 1;
        while (best < n && (uint8_t(s[best]) & 0xC0) == 0x80) best++;
    }
    return best;
}

uint32_t Layout::page(const char* buf, uint32_t len, uint32_t pos, bool eof, bool bufStartIsTextStart,
                      std::vector<Line>& out) const {
    // skip leading whitespace, noting paragraph breaks
    bool paraStart = isParaStart(buf, pos, bufStartIsTextStart);
    {
        int nl = 0;
        while (pos < len && isSpace(buf[pos])) {
            if (buf[pos] == '\n') nl++;
            pos++;
        }
        if (nl >= 2) paraStart = true;
    }

    int lines = 0;
    while (lines < p_.maxLines && pos < len) {
        Line line{pos, pos, paraStart, false};
        int x = paraStart ? p_.indent : 0;
        bool any = false;
        bool stop = false;   // ran out of buffer mid-word

        while (pos < len) {
            uint32_t we = pos;
            while (we < len && !isSpace(buf[we])) we++;
            if (we == len && !eof) {   // word may continue beyond the buffer
                stop = true;
                break;
            }
            int w = measure_(buf + pos, we - pos);
            int need = (any ? spaceW_ : 0) + w;
            if (x + need > p_.width) {
                if (p_.hyphenate) {   // the longest first part that fits, with its "-"
                    uint16_t breaks[32];
                    int sx = x + (any ? spaceW_ : 0);
                    bool done = false;
                    for (int k = p_.hyphenate(buf + pos, we - pos, breaks, 32); k-- > 0 && !done;) {
                        uint32_t b = breaks[k];
                        if (b == 0 || b >= we - pos) continue;
                        bool dash = buf[pos + b - 1] != '-';
                        if (sx + measure_(buf + pos, b) + (dash ? hyphenW_ : 0) > p_.width) continue;
                        pos += b;
                        line.end = pos;
                        line.hyphen = dash;
                        done = true;
                    }
                    if (done) {
                        any = true;
                        break;
                    }
                }
                if (any) break;   // wrap before this word
                // single word wider than the line: hard break
                size_t n = fitPrefix(measure_, buf + pos, we - pos, p_.width - x);
                pos += uint32_t(n);
                line.end = pos;
                any = true;
                break;
            }
            x += need;
            any = true;
            pos = we;
            line.end = we;
            // consume whitespace, detect paragraph end
            int nl = 0;
            while (pos < len && isSpace(buf[pos])) {
                if (buf[pos] == '\n') nl++;
                pos++;
            }
            if (nl >= 2 || (pos == len && eof)) {
                line.paraEnd = true;
                break;
            }
        }
        if (stop && !any) break;
        if (!any) break;
        out.push_back(line);
        lines++;
        paraStart = line.paraEnd;
        if (stop) break;
    }
    return pos;
}

void Layout::placeWords(const char* buf, const Line& line, bool justify, std::vector<Word>& out) const {
    size_t first = out.size();
    int x = line.paraStart ? p_.indent : 0;
    int total = 0;
    uint32_t i = line.start;
    while (i < line.end) {
        while (i < line.end && isSpace(buf[i])) i++;
        if (i >= line.end) break;
        uint32_t we = i;
        while (we < line.end && !isSpace(buf[we])) we++;
        int w = measure_(buf + i, we - i);
        out.push_back({i, we, w});   // x holds the width for now
        total += w;
        i = we;
    }
    size_t n = out.size() - first;
    if (n == 0) return;
    if (line.hyphen) {
        out.back().hyphen = true;
        out.back().x += hyphenW_;
        total += hyphenW_;
    }
    int gaps = int(n) - 1;
    int extra = 0, rem = 0;
    int avail = p_.width - x - total;
    bool just = justify && !line.paraEnd && gaps > 0 && avail > gaps * spaceW_;
    if (just) {
        extra = avail / gaps;
        rem = avail % gaps;
    }
    for (size_t k = 0; k < n; k++) {
        Word& wd = out[first + k];
        int w = wd.x;
        wd.x = x;
        x += w + (just ? extra + (int(k) < rem ? 1 : 0) : spaceW_);
    }
}

}  // namespace layout
