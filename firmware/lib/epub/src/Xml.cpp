#include "Xml.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "Text.h"

namespace epub {

const std::string* findAttr(const Attrs& attrs, const char* name) {
    for (const auto& a : attrs)
        if (a.name == name) return &a.value;
    for (const auto& a : attrs) {
        size_t colon = a.name.find(':');
        if (colon != std::string::npos && a.name.compare(colon + 1, std::string::npos, name) == 0) return &a.value;
    }
    return nullptr;
}

static void lower(std::string& s) {
    for (auto& c : s) c = char(tolower((unsigned char)c));
}

static bool endsWithNoCase(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); i++)
        if (tolower((unsigned char)s[s.size() - suffix.size() + i]) != suffix[i]) return false;
    return true;
}

void XmlParser::flushText() {
    if (text_.empty()) return;
    h_.text(decodeEntities(text_));
    text_.clear();
}

void XmlParser::feed(const char* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = d[i];
        switch (mode_) {
            case Mode::Text:
                if (c == '<') {
                    flushText();
                    mode_ = Mode::Tag;
                    tag_.clear();
                    quote_ = 0;
                } else {
                    text_ += c;
                    // keep memory bounded; whitespace never splits an entity
                    if (text_.size() > 32768 && isspace((unsigned char)c)) flushText();
                }
                break;
            case Mode::Tag:
                if (quote_) {
                    if (c == quote_) quote_ = 0;
                    tag_ += c;
                } else if (c == '>') {
                    endOfTag();
                } else {
                    tag_ += c;
                    if ((c == '"' || c == '\'') && tag_[0] != '!' && tag_[0] != '?') quote_ = c;
                    if (tag_ == "!--") {
                        mode_ = Mode::Comment;
                        tag_.clear();
                    } else if (tag_ == "![CDATA[") {
                        mode_ = Mode::Cdata;
                        tag_.clear();
                    }
                }
                break;
            case Mode::Comment:
                tag_ += c;
                if (tag_.size() > 3) tag_.erase(0, tag_.size() - 3);
                if (tag_ == "-->") mode_ = Mode::Text;
                break;
            case Mode::Cdata:
                text_ += c;
                if (text_.size() >= 3 && text_.compare(text_.size() - 3, 3, "]]>") == 0) {
                    text_.resize(text_.size() - 3);
                    if (!text_.empty()) h_.text(text_);
                    text_.clear();
                    mode_ = Mode::Text;
                }
                break;
            case Mode::Raw:
                text_ += c;
                if (c == '>' || isspace((unsigned char)c)) {
                    // "</style>" or "</style >"
                    std::string body = text_.substr(0, text_.size() - 1);
                    if (endsWithNoCase(body, rawEnd_)) {
                        body.resize(body.size() - rawEnd_.size());
                        if (!body.empty()) h_.text(body);
                        text_.clear();
                        std::string name = rawEnd_.substr(2);
                        if (c == '>') {
                            h_.endTag(name);
                            mode_ = Mode::Text;
                        } else {
                            tag_ = "/" + name;
                            quote_ = 0;
                            mode_ = Mode::Tag;
                        }
                    }
                }
                break;
        }
    }
}

void XmlParser::finish() {
    if (mode_ == Mode::Text || mode_ == Mode::Raw) flushText();
    mode_ = Mode::Text;
}

void XmlParser::endOfTag() {
    mode_ = Mode::Text;
    const std::string& t = tag_;
    if (t.empty() || t[0] == '!' || t[0] == '?') return;   // doctype, processing instruction
    size_t i = 0;
    bool end = t[0] == '/';
    if (end) i = 1;
    size_t nameStart = i;
    while (i < t.size() && !isspace((unsigned char)t[i]) && t[i] != '/') i++;
    std::string name = t.substr(nameStart, i - nameStart);
    size_t colon = name.find(':');
    if (colon != std::string::npos) name.erase(0, colon + 1);
    lower(name);
    if (name.empty()) return;
    if (end) {
        h_.endTag(name);
        return;
    }
    Attrs attrs;
    bool selfClosing = false;
    while (i < t.size()) {
        while (i < t.size() && isspace((unsigned char)t[i])) i++;
        if (i >= t.size()) break;
        if (t[i] == '/') {
            selfClosing = true;
            i++;
            continue;
        }
        size_t a = i;
        while (i < t.size() && !isspace((unsigned char)t[i]) && t[i] != '=' && t[i] != '/') i++;
        Attr at;
        at.name = t.substr(a, i - a);
        lower(at.name);
        while (i < t.size() && isspace((unsigned char)t[i])) i++;
        if (i < t.size() && t[i] == '=') {
            i++;
            while (i < t.size() && isspace((unsigned char)t[i])) i++;
            if (i < t.size() && (t[i] == '"' || t[i] == '\'')) {
                char q = t[i++];
                size_t v = i;
                while (i < t.size() && t[i] != q) i++;
                at.value = decodeEntities(t.substr(v, i - v));
                if (i < t.size()) i++;
            } else {
                size_t v = i;
                while (i < t.size() && !isspace((unsigned char)t[i])) i++;
                at.value = decodeEntities(t.substr(v, i - v));
            }
        }
        if (!at.name.empty()) attrs.push_back(std::move(at));
        selfClosing = false;   // only a trailing '/' counts
    }
    if (!t.empty() && t.back() == '/') selfClosing = true;
    h_.startTag(name, attrs, selfClosing);
    if (!selfClosing && (name == "script" || name == "style")) {
        mode_ = Mode::Raw;
        rawEnd_ = "</" + name;
        text_.clear();
    }
}

// ---------- entities ----------

static const char* const kLatin1Names[96] = {
    "nbsp",   "iexcl",  "cent",   "pound",  "curren", "yen",    "brvbar", "sect",   "uml",    "copy",   "ordf",
    "laquo",  "not",    "shy",    "reg",    "macr",   "deg",    "plusmn", "sup2",   "sup3",   "acute",  "micro",
    "para",   "middot", "cedil",  "sup1",   "ordm",   "raquo",  "frac14", "frac12", "frac34", "iquest", "Agrave",
    "Aacute", "Acirc",  "Atilde", "Auml",   "Aring",  "AElig",  "Ccedil", "Egrave", "Eacute", "Ecirc",  "Euml",
    "Igrave", "Iacute", "Icirc",  "Iuml",   "ETH",    "Ntilde", "Ograve", "Oacute", "Ocirc",  "Otilde", "Ouml",
    "times",  "Oslash", "Ugrave", "Uacute", "Ucirc",  "Uuml",   "Yacute", "THORN",  "szlig",  "agrave", "aacute",
    "acirc",  "atilde", "auml",   "aring",  "aelig",  "ccedil", "egrave", "eacute", "ecirc",  "euml",   "igrave",
    "iacute", "icirc",  "iuml",   "eth",    "ntilde", "ograve", "oacute", "ocirc",  "otilde", "ouml",   "divide",
    "oslash", "ugrave", "uacute", "ucirc",  "uuml",   "yacute", "thorn",  "yuml"};

static const struct {
    const char* name;
    uint32_t cp;
} kOtherNames[] = {
    {"amp", '&'},       {"lt", '<'},        {"gt", '>'},       {"quot", '"'},      {"apos", '\''},
    {"ndash", 0x2013},  {"mdash", 0x2014},  {"hellip", 0x2026}, {"lsquo", 0x2018},  {"rsquo", 0x2019},
    {"sbquo", 0x201A},  {"ldquo", 0x201C},  {"rdquo", 0x201D}, {"bdquo", 0x201E},  {"lsaquo", 0x2039},
    {"rsaquo", 0x203A}, {"bull", 0x2022},   {"euro", 0x20AC},  {"trade", 0x2122},  {"ensp", 0x2002},
    {"emsp", 0x2003},   {"thinsp", 0x2009}, {"zwnj", 0x200C},  {"zwj", 0x200D},    {"OElig", 0x152},
    {"oelig", 0x153},   {"Scaron", 0x160},  {"scaron", 0x161}, {"Yuml", 0x178},    {"dagger", 0x2020},
    {"Dagger", 0x2021}, {"permil", 0x2030}, {"prime", 0x2032}, {"Prime", 0x2033},  {"minus", 0x2212},
    {"larr", 0x2190},   {"rarr", 0x2192},   {"hyphen", 0x2010}};

static bool entityCodepoint(const std::string& name, uint32_t& cp) {
    if (name.size() > 1 && name[0] == '#') {
        char* end = nullptr;
        unsigned long v = (name[1] == 'x' || name[1] == 'X') ? strtoul(name.c_str() + 2, &end, 16)
                                                             : strtoul(name.c_str() + 1, &end, 10);
        if (!end || *end || v == 0 || v > 0x10FFFF) return false;
        cp = uint32_t(v);
        return true;
    }
    for (int i = 0; i < 96; i++)
        if (name == kLatin1Names[i]) {
            cp = 0xA0 + i;
            return true;
        }
    for (const auto& e : kOtherNames)
        if (name == e.name) {
            cp = e.cp;
            return true;
        }
    return false;
}

std::string decodeEntities(const std::string& s) {
    if (s.find('&') == std::string::npos) return s;
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos && semi - i <= 10) {
                uint32_t cp;
                if (entityCodepoint(s.substr(i + 1, semi - i - 1), cp)) {
                    appendUtf8(cp, out);
                    i = semi;
                    continue;
                }
            }
        }
        out += s[i];
    }
    return out;
}

}  // namespace epub
