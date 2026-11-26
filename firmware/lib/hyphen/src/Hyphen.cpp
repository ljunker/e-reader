#include "Hyphen.h"

#include <string.h>

#include <algorithm>
#include <new>

namespace hyph {

static constexpr int kMaxRun = 64;   // longer letter runs are left alone

static uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

bool Hyphenator::load(io::ByteSource& src, uint32_t size) {
    data_.reset();
    lens_.clear();
    if (size < 8) return false;
    std::unique_ptr<uint8_t[]> d(new (std::nothrow) uint8_t[size]);
    if (!d || !src.readAt(0, d.get(), size)) return false;
    if (memcmp(d.get(), "HYP1", 4) != 0) return false;
    uint8_t maxLen = d[6];
    if (maxLen == 0 || 8 + 16u * maxLen > size) return false;
    std::vector<Len> lens(maxLen);
    for (int l = 1; l <= maxLen; l++) {
        const uint8_t* e = d.get() + 8 + 16 * (l - 1);
        uint32_t count = u32(e), vec = u32(e + 4), let = u32(e + 8), idx = u32(e + 12);
        if (vec > size || let > size || idx > size || uint64_t(let) + uint64_t(count) * l > size ||
            uint64_t(idx) + count > size)
            return false;
        uint32_t nvec = (let - vec) / (l + 1);
        for (uint32_t k = 0; k < count; k++)
            if (d[idx + k] >= nvec) return false;
        lens[l - 1] = {count, d.get() + vec, d.get() + let, d.get() + idx};
    }
    left_ = std::max<uint8_t>(1, d[4]);
    right_ = std::max<uint8_t>(1, d[5]);
    lens_ = std::move(lens);
    data_ = std::move(d);
    return true;
}

const uint8_t* Hyphenator::find(const uint8_t* s, int len) const {
    const Len& t = lens_[len - 1];
    uint32_t lo = 0, hi = t.count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int c = memcmp(t.letters + size_t(mid) * len, s, len);
        if (c == 0) return t.vectors + size_t(t.index[mid]) * (len + 1);
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    return nullptr;
}

void Hyphenator::hyphenateRun(const uint8_t* lower, const uint16_t* offs, int n, std::vector<uint16_t>& out) const {
    if (n < left_ + right_) return;
    uint8_t w[kMaxRun + 2], pts[kMaxRun + 3] = {0};
    int m = n + 2;
    w[0] = '.';
    memcpy(w + 1, lower, n);
    w[n + 1] = '.';
    int maxLen = int(lens_.size());
    for (int i = 0; i < m; i++)
        for (int l = 1; l <= maxLen && i + l <= m; l++)
            if (const uint8_t* d = find(w + i, l))
                for (int k = 0; k <= l; k++) pts[i + k] = std::max(pts[i + k], d[k]);
    // pts[i + 1] is the value between letter i - 1 and letter i of the run
    for (int i = left_; i <= n - right_; i++)
        if (pts[i + 1] & 1) out.push_back(offs[i]);
}

static bool isLetter(uint32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7);
}

static uint8_t lowerLatin1(uint32_t c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return uint8_t(c + 32);
    return uint8_t(c);
}

void Hyphenator::breaks(const char* s, size_t n, std::vector<uint16_t>& out) const {
    out.clear();
    if (n > 0xFFFF) return;
    uint8_t run[kMaxRun];
    uint16_t offs[kMaxRun];
    int rn = 0;
    bool tooLong = false;
    uint32_t prev = 0;   // previous code point
    bool prevLetter = false, dashAfterLetter = false;
    auto flush = [&]() {
        if (data_ && !tooLong && rn > 0) hyphenateRun(run, offs, rn, out);
        rn = 0;
        tooLong = false;
    };
    size_t i = 0;
    while (i < n) {
        uint8_t b = uint8_t(s[i]);
        uint32_t c = b;
        size_t len = 1;
        if (b >= 0xC0 && b < 0xE0 && i + 1 < n && (uint8_t(s[i + 1]) & 0xC0) == 0x80) {
            c = uint32_t(b & 0x1F) << 6 | (uint8_t(s[i + 1]) & 0x3F);
            len = 2;
        } else if (b >= 0x80) {   // outside Latin-1 or broken: not a letter
            while (i + len < n && (uint8_t(s[i + len]) & 0xC0) == 0x80) len++;
            c = 0xFFFD;
        }
        if (isLetter(c)) {
            if (prev == '-' && dashAfterLetter) out.push_back(uint16_t(i));   // "E-|Mail"
            if (rn < kMaxRun) {
                run[rn] = lowerLatin1(c);
                offs[rn] = uint16_t(i);
                rn++;
            } else {
                tooLong = true;
            }
        } else {
            flush();
        }
        if (c == '-') dashAfterLetter = prevLetter;
        prevLetter = isLetter(c);
        prev = c;
        i += len;
    }
    flush();
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

const char* normalizeLanguage(const char* tag) {
    if (!tag) return "";
    char a = tag[0] | 0x20, b = tag[0] ? tag[1] | 0x20 : 0;
    char c = tag[0] && tag[1] ? tag[2] : 0;
    if (c != 0 && c != '-' && c != '_') return "";
    if (a == 'd' && b == 'e') return "de";
    if (a == 'e' && b == 'n') return "en";
    return "";
}

static int count(const char* s, size_t n, const char* word) {
    size_t wl = strlen(word);
    int k = 0;
    for (size_t i = 0; i + wl + 2 <= n; i++) {
        if (s[i] != ' ' && s[i] != '\n') continue;
        if (strncmp(s + i + 1, word, wl) != 0) continue;
        char e = s[i + 1 + wl];
        if (e == ' ' || e == '\n' || e == ',' || e == '.' || e == '\r') k++;
    }
    return k;
}

const char* guessLanguage(const char* s, size_t n) {
    static const char* const de[] = {"der", "die", "und", "das", "ist", "nicht", "ich", "sie"};
    static const char* const en[] = {"the", "and", "of", "to", "is", "that", "was", "he"};
    int d = 0, e = 0;
    for (const char* w : de) d += count(s, n, w);
    for (const char* w : en) e += count(s, n, w);
    if (d >= 5 && d > 2 * e) return "de";
    if (e >= 5 && e > 2 * d) return "en";
    return "";
}

}  // namespace hyph
