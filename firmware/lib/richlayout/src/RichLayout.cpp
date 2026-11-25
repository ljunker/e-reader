#include "RichLayout.h"

#include <algorithm>

namespace rich {

using etx::Block;

static inline bool endsWord(char c) { return c == ' ' || (uint8_t(c) < 0x20 && c != etx::kStyle); }

Layout::Layout(Env env) : env_(std::move(env)) {
    for (int f = 0; f < etx::kFontSlots; f++) {
        spaceW_[f] = std::max(1, env_.measure(f, " ", 1));
        metrics_[f] = env_.metrics(f);
    }
}

int Layout::wordWidth(const char* buf, uint32_t from, uint32_t to, State st) const {
    int w = 0;
    uint32_t seg = from;
    for (uint32_t p = from; p < to;) {
        if (buf[p] == etx::kStyle) {
            if (p > seg) w += env_.measure(etx::fontFor(st.block, st.style), buf + seg, p - seg);
            st.style = uint8_t(buf[p + 1]) & 3;
            p += 2;
            seg = p;
        } else {
            p++;
        }
    }
    if (to > seg) w += env_.measure(etx::fontFor(st.block, st.style), buf + seg, to - seg);
    return w;
}

Layout::End Layout::fillLine(const char* buf, uint32_t len, uint32_t& pos, State& st, bool eof, bool paraStart,
                             Line& line, bool& any) const {
    Block b = Block::decode(st.block);
    bool quote = b.kind == etx::Quote, list = b.kind == etx::List;
    line.left = quote || list ? env_.blockIndent : 0;
    line.avail = env_.width - line.left - (quote ? env_.blockIndent : 0);
    line.firstIndent = paraStart && b.indent && (b.kind == etx::Body || quote) ? env_.indent : 0;
    int x = line.firstIndent;
    int spaceW = spaceW_[etx::fontFor(st.block, 0)];
    any = false;

    for (;;) {
        if (pos >= len) return eof ? End::Para : End::Incomplete;
        char c = buf[pos];
        if (c == ' ') {
            pos++;
            continue;
        }
        if (c == etx::kStyle) {
            if (pos + 1 >= len) return eof ? End::Para : End::Incomplete;
            st.style = uint8_t(buf[pos + 1]) & 3;
            pos += 2;
            continue;
        }
        if (c == etx::kBreak) {
            pos++;
            return End::Break;
        }
        if (c == etx::kPara || c == etx::kPage || c == etx::kImage) return End::Para;
        if (uint8_t(c) < 0x20) {   // unknown control code
            pos++;
            continue;
        }

        // one word, possibly with style changes inside
        uint32_t wstart = pos;
        State ws = st, we = st;
        uint32_t p = pos;
        bool incomplete = false;
        while (p < len && !endsWord(buf[p])) {
            if (buf[p] == etx::kStyle) {
                if (p + 1 >= len) {
                    incomplete = true;
                    break;
                }
                we.style = uint8_t(buf[p + 1]) & 3;
                p += 2;
            } else {
                p++;
            }
        }
        if (p >= len && !eof) incomplete = true;
        if (incomplete) return End::Incomplete;   // pos and st still at the word start

        int w = wordWidth(buf, wstart, p, ws);
        int need = (any ? spaceW : 0) + w;
        if (x + need > line.avail) {
            if (any) return End::Width;   // wrap before this word
            // a single word wider than the line: break it between characters
            uint32_t q = wstart, good = wstart;
            State cs = ws, goodSt = ws;
            int cx = x;
            while (q < p) {
                if (buf[q] == etx::kStyle) {
                    cs.style = uint8_t(buf[q + 1]) & 3;
                    q += 2;
                    continue;
                }
                uint32_t e = q + 1;
                while (e < p && (uint8_t(buf[e]) & 0xC0) == 0x80) e++;
                int cw = env_.measure(etx::fontFor(cs.block, cs.style), buf + q, e - q);
                if (cx + cw > line.avail && good > wstart) break;
                cx += cw;
                q = e;
                good = q;
                goodSt = cs;
            }
            pos = good;
            st = goodSt;
            line.end = good;
            any = true;
            return End::Width;
        }
        x += need;
        any = true;
        pos = p;
        st = we;
        line.end = p;
    }
}

uint32_t Layout::page(const char* buf, uint32_t len, uint32_t pos, State st, bool eof, std::vector<Line>& out,
                      State& next) const {
    int y = 0;
    bool any = false, paraStart = false;
    int pendingGap = 0;
    uint32_t paraPos = pos;
    State paraSt = st;

    for (;;) {
        while (pos < len && buf[pos] == ' ') pos++;
        if (pos >= len) break;
        char c = buf[pos];

        if (c == etx::kPage) {
            pos++;
            if (any) break;
            continue;
        }
        if (c == etx::kPara) {
            if (pos + 1 >= len) {
                if (!eof) break;
                pos++;
                continue;
            }
            paraPos = pos;
            paraSt = st;
            st.block = uint8_t(buf[pos + 1]);
            st.style = 0;
            pos += 2;
            paraStart = true;
            if (Block::decode(st.block).spaceBefore) pendingGap = std::max(pendingGap, env_.paraGap);
            continue;
        }
        if (c == etx::kImage) {
            if (pos + 4 > len) {
                if (!eof) break;
                pos = len;
                break;
            }
            uint32_t idx = etx::decodeImage(buf + pos + 1);
            int w = 0, h = 0;
            if (env_.imageSize) env_.imageSize(idx, w, h);
            if (w <= 0 || h <= 0) {
                pos += 4;
                continue;
            }
            int gap = any ? env_.paraGap : 0;
            if (any && y + gap + h > env_.height) break;
            Line l;
            l.start = pos;
            l.end = pos + 4;
            l.state = st;
            l.top = y + gap;
            l.height = h;
            l.image = true;
            l.imageIndex = idx;
            l.imageW = std::min(w, env_.width);
            l.imageH = h;
            l.left = (env_.width - l.imageW) / 2;
            l.avail = env_.width;
            out.push_back(l);
            y = l.top + h;
            any = true;
            pos += 4;
            paraStart = false;
            pendingGap = env_.paraGap;
            continue;
        }

        // a text line
        int font0 = etx::fontFor(st.block, 0);
        const Metrics& m = metrics_[font0];
        int lh = m.ascent + m.descent + env_.leading;
        int gap = any ? pendingGap : 0;
        if (any && y + gap + lh > env_.height) {
            if (paraStart) {   // keep the paragraph start (indent, gap) for the next page
                pos = paraPos;
                st = paraSt;
            }
            break;
        }
        Line l;
        l.start = pos;
        l.end = pos;
        l.state = st;
        uint32_t before = pos;
        State stBefore = st;
        bool words = false;
        End e = fillLine(buf, len, pos, st, eof, paraStart, l, words);
        if (e == End::Incomplete && !words) {
            pos = before;
            st = stBefore;
            if (paraStart && any) {
                pos = paraPos;
                st = paraSt;
            }
            break;
        }
        if (!words && e != End::Break) continue;   // a control code follows; the loop handles it
        if (!words && !any) {                       // blank line at the top of a page
            paraStart = false;
            continue;
        }
        l.justify = e == End::Width && Block::decode(l.state.block).align == etx::Justify;
        l.top = y + gap;
        l.height = lh;
        l.ascent = m.ascent;
        out.push_back(l);
        y = l.top + lh;
        any = true;
        paraStart = false;
        pendingGap = 0;
        if (e == End::Incomplete) break;
    }
    next = st;
    return pos;
}

void Layout::runs(const char* buf, const Line& line, std::vector<Run>& out) const {
    struct Seg {
        uint32_t start, end;
        int font, w;
    };
    std::vector<Seg> segs;
    std::vector<size_t> wordStart;   // index of the first segment of each word
    std::vector<int> wordW;
    State st = line.state;
    uint32_t pos = line.start;
    while (pos < line.end) {
        char c = buf[pos];
        if (c == ' ') {
            pos++;
            continue;
        }
        if (c == etx::kStyle) {
            st.style = uint8_t(buf[pos + 1]) & 3;
            pos += 2;
            continue;
        }
        if (uint8_t(c) < 0x20) {
            pos++;
            continue;
        }
        wordStart.push_back(segs.size());
        int w = 0;
        uint32_t seg = pos;
        auto close = [&](uint32_t end) {
            if (end > seg) {
                int f = etx::fontFor(st.block, st.style);
                int sw = env_.measure(f, buf + seg, end - seg);
                segs.push_back({seg, end, f, sw});
                w += sw;
            }
        };
        while (pos < line.end && !endsWord(buf[pos])) {
            if (buf[pos] == etx::kStyle) {
                close(pos);
                st.style = uint8_t(buf[pos + 1]) & 3;
                pos += 2;
                seg = pos;
            } else {
                pos++;
            }
        }
        close(pos);
        wordW.push_back(w);
    }
    size_t n = wordW.size();
    if (n == 0) return;
    int total = 0;
    for (int w : wordW) total += w;
    int spaceW = spaceW_[etx::fontFor(line.state.block, 0)];
    int x = line.left + line.firstIndent;
    int avail = line.avail - line.firstIndent;
    int gap = spaceW, rem = 0;
    if (line.justify && n > 1 && avail > total) {
        gap = (avail - total) / int(n - 1);
        rem = (avail - total) % int(n - 1);
    } else {
        int natural = total + spaceW * int(n - 1);
        uint8_t align = Block::decode(line.state.block).align;
        if (align == etx::Center) x += std::max(0, (avail - natural) / 2);
        else if (align == etx::Right) x += std::max(0, avail - natural);
    }
    for (size_t k = 0; k < n; k++) {
        size_t endSeg = k + 1 < n ? wordStart[k + 1] : segs.size();
        int sx = x;
        for (size_t s = wordStart[k]; s < endSeg; s++) {
            out.push_back({segs[s].start, segs[s].end, sx, segs[s].font});
            sx += segs[s].w;
        }
        x += wordW[k] + gap + (int(k) < rem ? 1 : 0);
    }
}

}  // namespace rich
