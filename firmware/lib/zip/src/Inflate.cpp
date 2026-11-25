#include "Inflate.h"

#include <string.h>

#include <memory>
#include <new>

namespace zip {

namespace {

constexpr int kMaxBits = 15;
constexpr int kFastBits = 9;
constexpr uint32_t kWindow = 32768;
constexpr uint16_t kNoFast = 0xFFFF;

// Canonical Huffman table: counts per length, symbols sorted by code, plus a
// lookup table for codes up to kFastBits long (indexed by the next stream bits).
struct Huffman {
    uint16_t count[kMaxBits + 1];
    uint16_t symbol[288];
    uint16_t fast[1 << kFastBits];   // (length << 9) | symbol, or kNoFast

    bool build(const uint8_t* lengths, int n) {
        memset(count, 0, sizeof count);
        for (int i = 0; i < n; i++) count[lengths[i]]++;
        if (count[0] == n) {   // no codes at all (allowed for an unused distance tree)
            for (auto& f : fast) f = kNoFast;
            return true;
        }
        int left = 1;
        for (int len = 1; len <= kMaxBits; len++) {
            left <<= 1;
            left -= count[len];
            if (left < 0) return false;   // over-subscribed
        }
        uint16_t offs[kMaxBits + 1];
        offs[1] = 0;
        for (int len = 1; len < kMaxBits; len++) offs[len + 1] = offs[len] + count[len];
        for (int i = 0; i < n; i++)
            if (lengths[i]) symbol[offs[lengths[i]]++] = uint16_t(i);

        for (auto& f : fast) f = kNoFast;
        uint32_t code = 0;
        int index = 0;
        for (int len = 1; len <= kFastBits; len++) {
            for (int k = 0; k < count[len]; k++, index++, code++) {
                uint32_t rev = 0;   // deflate sends Huffman codes MSB first
                for (int b = 0; b < len; b++) rev |= ((code >> b) & 1) << (len - 1 - b);
                for (uint32_t i = rev; i < (1u << kFastBits); i += 1u << len)
                    fast[i] = uint16_t((len << 9) | symbol[index]);
            }
            code <<= 1;
        }
        return true;
    }
};

const uint16_t kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                               31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

struct State {
    const InputFn& in;
    const OutputFn& out;
    uint8_t inBuf[2048];
    size_t inPos = 0, inLen = 0;
    bool inEnd = false;
    uint32_t bits = 0;
    int nbits = 0;
    uint8_t window[kWindow];
    uint32_t wpos = 0, flushed = 0;
    uint64_t total = 0;
    bool stopped = false;
    Huffman lit, dist;

    State(const InputFn& i, const OutputFn& o) : in(i), out(o) {}

    // Make at least n bits available if the input allows; returns false at the end.
    bool fill(int n) {
        while (nbits < n) {
            if (inPos == inLen) {
                if (inEnd) return false;
                inLen = in(inBuf, sizeof inBuf);
                inPos = 0;
                if (inLen == 0) {
                    inEnd = true;
                    return false;
                }
            }
            bits |= uint32_t(inBuf[inPos++]) << nbits;
            nbits += 8;
        }
        return true;
    }
    bool get(int n, uint32_t& v) {
        if (n == 0) {
            v = 0;
            return true;
        }
        if (!fill(n)) return false;
        v = bits & ((1u << n) - 1);
        bits >>= n;
        nbits -= n;
        return true;
    }
    // Decode one symbol; -1 on truncated input, -2 on an invalid code.
    int decode(const Huffman& h) {
        fill(kMaxBits);   // may come up short at the very end; checked below
        if (nbits > 0) {
            uint16_t f = h.fast[bits & ((1u << kFastBits) - 1)];
            if (f != kNoFast && (f >> 9) <= nbits) {
                int len = f >> 9;
                bits >>= len;
                nbits -= len;
                return f & 0x1FF;
            }
        }
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= kMaxBits; len++) {
            if (nbits == 0 && !fill(1)) return -1;
            code |= int(bits & 1);
            bits >>= 1;
            nbits--;
            int count = h.count[len];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -2;
    }

    bool flush() {
        if (wpos > flushed && !stopped) {
            if (!out(window + flushed, wpos - flushed)) stopped = true;
        }
        flushed = wpos;
        return !stopped;
    }
    inline bool put(uint8_t b) {
        window[wpos++] = b;
        total++;
        if (wpos == kWindow) {
            bool ok = flush();
            wpos = flushed = 0;
            return ok;
        }
        return true;
    }
};

InflateResult stored(State& s) {
    s.bits >>= s.nbits & 7;   // skip to the byte boundary
    s.nbits -= s.nbits & 7;
    uint32_t len, nlen;
    if (!s.get(16, len) || !s.get(16, nlen)) return InflateResult::Truncated;
    if ((len ^ 0xFFFF) != nlen) return InflateResult::BadData;
    while (len--) {
        uint32_t b;
        if (!s.get(8, b)) return InflateResult::Truncated;
        if (!s.put(uint8_t(b))) return InflateResult::Stopped;
    }
    return InflateResult::Ok;
}

InflateResult codes(State& s) {
    for (;;) {
        int sym = s.decode(s.lit);
        if (sym < 0) return sym == -1 ? InflateResult::Truncated : InflateResult::BadData;
        if (sym < 256) {
            if (!s.put(uint8_t(sym))) return InflateResult::Stopped;
            continue;
        }
        if (sym == 256) return InflateResult::Ok;
        sym -= 257;
        if (sym >= 29) return InflateResult::BadData;
        uint32_t extra;
        if (!s.get(kLenExtra[sym], extra)) return InflateResult::Truncated;
        uint32_t len = kLenBase[sym] + extra;
        int dsym = s.decode(s.dist);
        if (dsym < 0) return dsym == -1 ? InflateResult::Truncated : InflateResult::BadData;
        if (dsym >= 30) return InflateResult::BadData;
        if (!s.get(kDistExtra[dsym], extra)) return InflateResult::Truncated;
        uint32_t d = kDistBase[dsym] + extra;
        if (d > s.total || d > kWindow) return InflateResult::BadData;
        uint32_t from = (s.wpos + kWindow - d) & (kWindow - 1);
        while (len--) {
            uint8_t b = s.window[from];
            from = (from + 1) & (kWindow - 1);
            if (!s.put(b)) return InflateResult::Stopped;
        }
    }
}

InflateResult fixedBlock(State& s) {
    static uint8_t litLen[288], distLen[30];
    if (litLen[0] == 0) {
        for (int i = 0; i < 144; i++) litLen[i] = 8;
        for (int i = 144; i < 256; i++) litLen[i] = 9;
        for (int i = 256; i < 280; i++) litLen[i] = 7;
        for (int i = 280; i < 288; i++) litLen[i] = 8;
        for (int i = 0; i < 30; i++) distLen[i] = 5;
    }
    s.lit.build(litLen, 288);
    s.dist.build(distLen, 30);
    return codes(s);
}

InflateResult dynamicBlock(State& s) {
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint32_t hlit, hdist, hclen;
    if (!s.get(5, hlit) || !s.get(5, hdist) || !s.get(4, hclen)) return InflateResult::Truncated;
    hlit += 257;
    hdist += 1;
    hclen += 4;
    if (hlit > 286 || hdist > 30) return InflateResult::BadData;
    uint8_t lengths[320] = {0};
    for (uint32_t i = 0; i < hclen; i++) {
        uint32_t v;
        if (!s.get(3, v)) return InflateResult::Truncated;
        lengths[order[i]] = uint8_t(v);
    }
    Huffman& lencode = s.lit;   // reused as scratch before the literal table is built
    if (!lencode.build(lengths, 19)) return InflateResult::BadData;
    memset(lengths, 0, sizeof lengths);
    uint32_t i = 0;
    while (i < hlit + hdist) {
        int sym = s.decode(lencode);
        if (sym < 0) return sym == -1 ? InflateResult::Truncated : InflateResult::BadData;
        if (sym < 16) {
            lengths[i++] = uint8_t(sym);
            continue;
        }
        uint8_t val = 0;
        uint32_t rep;
        if (sym == 16) {
            if (i == 0) return InflateResult::BadData;
            val = lengths[i - 1];
            if (!s.get(2, rep)) return InflateResult::Truncated;
            rep += 3;
        } else if (sym == 17) {
            if (!s.get(3, rep)) return InflateResult::Truncated;
            rep += 3;
        } else {
            if (!s.get(7, rep)) return InflateResult::Truncated;
            rep += 11;
        }
        if (i + rep > hlit + hdist) return InflateResult::BadData;
        while (rep--) lengths[i++] = val;
    }
    if (lengths[256] == 0) return InflateResult::BadData;   // no end-of-block code
    if (!s.lit.build(lengths, int(hlit))) return InflateResult::BadData;
    if (!s.dist.build(lengths + hlit, int(hdist))) return InflateResult::BadData;
    return codes(s);
}

}  // namespace

InflateResult inflate(const InputFn& in, const OutputFn& out) {
    std::unique_ptr<State> s(new (std::nothrow) State(in, out));
    if (!s) return InflateResult::NoMemory;
    uint32_t last;
    do {
        uint32_t type;
        if (!s->get(1, last) || !s->get(2, type)) return InflateResult::Truncated;
        InflateResult r;
        switch (type) {
            case 0: r = stored(*s); break;
            case 1: r = fixedBlock(*s); break;
            case 2: r = dynamicBlock(*s); break;
            default: return InflateResult::BadData;
        }
        if (r != InflateResult::Ok) return r;
    } while (!last);
    if (!s->flush()) return InflateResult::Stopped;
    return InflateResult::Ok;
}

InflateResult inflateBuffer(const uint8_t* data, size_t n, bool zlib, std::vector<uint8_t>& out, size_t maxOut) {
    if (zlib) {
        if (n < 2 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0 || (data[1] & 0x20))
            return InflateResult::BadData;
        data += 2;
        n -= 2;
    }
    size_t pos = 0;
    bool tooLarge = false;
    InflateResult r = inflate(
        [&](uint8_t* dst, size_t cap) {
            size_t k = n - pos < cap ? n - pos : cap;
            memcpy(dst, data + pos, k);
            pos += k;
            return k;
        },
        [&](const uint8_t* d, size_t k) {
            if (out.size() + k > maxOut) {
                tooLarge = true;
                return false;
            }
            out.insert(out.end(), d, d + k);
            return true;
        });
    if (tooLarge) return InflateResult::Stopped;
    return r;
}

}  // namespace zip
