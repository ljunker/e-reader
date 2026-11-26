#include "Text.h"

#include <vector>

namespace epub {

void appendUtf8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

uint32_t nextCodepoint(const std::string& s, size_t& i) {
    uint8_t c = uint8_t(s[i]);
    // number of continuation bytes for a valid lead byte
    int n = c >= 0xF0 && c < 0xF5 ? 3 : c >= 0xE0 && c < 0xF0 ? 2 : c >= 0xC2 && c < 0xE0 ? 1 : 0;
    if (n > 0) {   // the loop checks the bounds
        uint32_t cp = c & (0x3F >> n);
        bool ok = true;
        for (int k = 1; k <= n; k++) {
            if (i + k >= s.size() || (uint8_t(s[i + k]) & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (uint8_t(s[i + k]) & 0x3F);
        }
        if (ok) {
            i += n + 1;
            return cp;
        }
    }
    i++;
    return c;   // ASCII, or a stray byte read as Latin-1
}

bool isSpace(uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' ||
           (cp >= 0x2000 && cp <= 0x200A && cp != 0x2007) || cp == 0x205F || cp == 0x3000 || cp == 0x1680;
}

// Base letters of U+0100..U+017F; '#' marks the ligatures handled separately.
static const char kLatinExtA[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIi"
    "##"
    "JjKkk"
    "LlLlLlLlLl"
    "NnNnNnnNn"
    "OoOoOo"
    "##"
    "RrRrRr"
    "SsSsSsSs"
    "TtTtTt"
    "UuUuUuUuUuUu"
    "Ww"
    "YyY"
    "ZzZzZz"
    "s";
static_assert(sizeof kLatinExtA - 1 == 128, "one letter per code point");

void appendFolded(uint32_t cp, std::string& out) {
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || cp == 0xAD) return;   // controls, soft hyphen
    if (cp <= 0xFF) {
        appendUtf8(cp, out);
        return;
    }
    if (cp >= 0x100 && cp < 0x180) {
        switch (cp) {
            case 0x132: out += "IJ"; return;
            case 0x133: out += "ij"; return;
            case 0x152: out += "OE"; return;
            case 0x153: out += "oe"; return;
            default: out += kLatinExtA[cp - 0x100]; return;
        }
    }
    if ((cp >= 0x300 && cp < 0x370) || (cp >= 0x200B && cp <= 0x200F) || cp == 0x2060 || cp == 0xFEFF ||
        (cp >= 0xFE00 && cp <= 0xFE0F))
        return;   // combining marks, zero-width and direction marks
    const char* s = nullptr;
    switch (cp) {
        case 0x2007: case 0x202F: s = "\xC2\xA0"; break;   // no-break spaces
        case 0x2010: case 0x2011: case 0x2012: case 0x2212: case 0x2043: s = "-"; break;
        // dashes, quotes, ellipsis, bullet, euro, trade mark, daggers: the
        // .xff fonts have these (tools/converter/src/xtconvert/font.py, EXTRA)
        case 0x2013: case 0x2014: case 0x2018: case 0x2019: case 0x201A: case 0x201C: case 0x201D: case 0x201E:
        case 0x2026: case 0x2022: case 0x20AC: case 0x2122: case 0x2020: case 0x2021: case 0x2039: case 0x203A:
            appendUtf8(cp, out);
            return;
        case 0x2015: appendUtf8(0x2014, out); return;
        case 0x201B: case 0x2032: case 0x02BC: case 0x02B9: s = "'"; break;
        case 0x201F: case 0x2033: case 0x02BA: s = "\""; break;
        case 0x2023: case 0x25CF: case 0x25AA: case 0x25E6: case 0x2219: appendUtf8(0x2022, out); return;
        case 0x2116: s = "No"; break;
        case 0x2044: case 0x2215: s = "/"; break;
        case 0x2190: s = "<-"; break;
        case 0x2192: s = "->"; break;
        case 0x2030: s = "o/oo"; break;
        case 0x02C6: s = "^"; break;
        case 0x02DC: s = "~"; break;
        case 0x1E9E: s = "SS"; break;
        case 0x0192: s = "f"; break;
        case 0x0218: s = "S"; break;
        case 0x0219: s = "s"; break;
        case 0x021A: s = "T"; break;
        case 0x021B: s = "t"; break;
        case 0xFB00: s = "ff"; break;
        case 0xFB01: s = "fi"; break;
        case 0xFB02: s = "fl"; break;
        case 0xFB03: s = "ffi"; break;
        case 0xFB04: s = "ffl"; break;
        default: s = "?"; break;
    }
    out += s;
}

std::string foldText(const std::string& s) {
    std::string out;
    bool space = false;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = nextCodepoint(s, i);
        if (isSpace(cp)) {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        appendFolded(cp, out);
    }
    return out;
}

std::string dirOf(const std::string& path) {
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string resolvePath(const std::string& dir, const std::string& href) {
    if (href.find("://") != std::string::npos || href.compare(0, 5, "data:") == 0 ||
        href.compare(0, 7, "mailto:") == 0)
        return "";
    std::string h = href.substr(0, href.find_first_of("#?"));
    std::string dec;
    for (size_t i = 0; i < h.size(); i++) {
        if (h[i] == '%' && i + 2 < h.size() && hexVal(h[i + 1]) >= 0 && hexVal(h[i + 2]) >= 0) {
            dec += char(hexVal(h[i + 1]) * 16 + hexVal(h[i + 2]));
            i += 2;
        } else {
            dec += h[i];
        }
    }
    std::string full = !dec.empty() && dec[0] == '/' ? dec.substr(1) : dir + dec;
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= full.size()) {
        size_t slash = full.find('/', start);
        if (slash == std::string::npos) slash = full.size();
        std::string seg = full.substr(start, slash - start);
        if (seg == "..") {
            if (!parts.empty()) parts.pop_back();
        } else if (!seg.empty() && seg != ".") {
            parts.push_back(seg);
        }
        start = slash + 1;
    }
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) out += '/';
        out += parts[i];
    }
    return out;
}

std::string fragmentOf(const std::string& href) {
    size_t hash = href.find('#');
    return hash == std::string::npos ? std::string() : href.substr(hash + 1);
}

}  // namespace epub
