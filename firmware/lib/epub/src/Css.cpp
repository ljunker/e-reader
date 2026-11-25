#include "Css.h"

#include <ctype.h>
#include <stdlib.h>

#include <algorithm>

#include "Etx.h"

namespace epub {

void Style::merge(const Style& o) {
    if (o.bold >= 0) bold = o.bold;
    if (o.italic >= 0) italic = o.italic;
    if (o.align >= 0) align = o.align;
    if (o.hidden >= 0) hidden = o.hidden;
    if (o.block >= 0) block = o.block;
    if (o.pageBreak >= 0) pageBreak = o.pageBreak;
    if (o.indent >= 0) indent = o.indent;
    if (o.spaceBefore >= 0) spaceBefore = o.spaceBefore;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static std::string lower(std::string s) {
    for (auto& c : s) c = char(tolower((unsigned char)c));
    return s;
}

// First length of a value: >0, 0 or <0 (units ignored); false if not a number.
static bool sign(const std::string& v, int& out) {
    const char* p = v.c_str();
    char* end = nullptr;
    double d = strtod(p, &end);
    if (end == p) return false;
    out = d > 0 ? 1 : d < 0 ? -1 : 0;
    return true;
}

Style parseDeclarations(const std::string& decls) {
    Style s;
    size_t start = 0;
    while (start < decls.size()) {
        size_t semi = decls.find(';', start);
        if (semi == std::string::npos) semi = decls.size();
        std::string d = decls.substr(start, semi - start);
        start = semi + 1;
        size_t colon = d.find(':');
        if (colon == std::string::npos) continue;
        std::string prop = lower(trim(d.substr(0, colon)));
        std::string val = lower(trim(d.substr(colon + 1)));
        size_t imp = val.find("!important");
        if (imp != std::string::npos) val = trim(val.substr(0, imp));
        int sg;
        if (prop == "font-weight") {
            if (val == "bold" || val == "bolder") s.bold = 1;
            else if (val == "normal" || val == "lighter") s.bold = 0;
            else if (sign(val, sg)) s.bold = atoi(val.c_str()) >= 600 ? 1 : 0;
        } else if (prop == "font-style") {
            if (val == "italic" || val == "oblique") s.italic = 1;
            else if (val == "normal") s.italic = 0;
        } else if (prop == "text-align") {
            if (val == "center") s.align = etx::Center;
            else if (val == "right" || val == "end") s.align = etx::Right;
            else if (val == "left" || val == "start") s.align = etx::Left;
            else if (val == "justify") s.align = etx::Justify;
        } else if (prop == "display") {
            s.hidden = val == "none" ? 1 : 0;
            if (val == "block" || val == "list-item" || val == "table" || val == "table-row") s.block = 1;
            else if (val == "inline" || val == "inline-block") s.block = 0;
        } else if (prop == "page-break-before" || prop == "break-before") {
            s.pageBreak = (val == "always" || val == "page" || val == "left" || val == "right" || val == "recto" ||
                           val == "verso")
                              ? 1
                              : 0;
        } else if (prop == "text-indent") {
            if (sign(val, sg)) s.indent = sg > 0 ? 1 : 0;
        } else if (prop == "margin-top" || prop == "margin") {
            if (sign(val, sg)) s.spaceBefore = sg > 0 ? 1 : 0;
        }
    }
    return s;
}

static std::string stripComments(const std::string& css) {
    std::string out;
    out.reserve(css.size());
    for (size_t i = 0; i < css.size(); i++) {
        if (css.compare(i, 2, "/*") == 0) {
            size_t e = css.find("*/", i + 2);
            if (e == std::string::npos) break;
            i = e + 1;
        } else if (css.compare(i, 4, "<!--") == 0) {
            i += 3;
        } else if (css.compare(i, 3, "-->") == 0) {
            i += 2;
        } else {
            out += css[i];
        }
    }
    return out;
}

void Stylesheet::parse(const std::string& input) {
    std::string css = stripComments(input);
    size_t i = 0;
    while (i < css.size()) {
        while (i < css.size() && isspace((unsigned char)css[i])) i++;
        if (i >= css.size()) break;
        if (css[i] == '@') {   // at-rule: skip "...;" or a whole {...} block
            size_t semi = css.find(';', i), brace = css.find('{', i);
            if (brace == std::string::npos || (semi != std::string::npos && semi < brace)) {
                i = semi == std::string::npos ? css.size() : semi + 1;
                continue;
            }
            int depth = 0;
            for (i = brace; i < css.size(); i++) {
                if (css[i] == '{') depth++;
                else if (css[i] == '}' && --depth == 0) break;
            }
            i++;
            continue;
        }
        size_t open = css.find('{', i);
        if (open == std::string::npos) break;
        size_t close = css.find('}', open);
        if (close == std::string::npos) close = css.size();
        std::string selectors = css.substr(i, open - i);
        Style style = parseDeclarations(css.substr(open + 1, close - open - 1));
        i = close + 1;

        size_t s = 0;
        while (s <= selectors.size()) {
            size_t comma = selectors.find(',', s);
            if (comma == std::string::npos) comma = selectors.size();
            std::string sel = trim(selectors.substr(s, comma - s));
            s = comma + 1;
            if (sel.empty() || sel.find_first_of(" \t\r\n>+~[:#") != std::string::npos) continue;
            Rule r;
            size_t dot = sel.find('.');
            r.tag = lower(sel.substr(0, dot));
            if (r.tag == "*") r.tag.clear();
            while (dot != std::string::npos) {
                size_t nextDot = sel.find('.', dot + 1);
                std::string cls = sel.substr(dot + 1, nextDot == std::string::npos ? std::string::npos : nextDot - dot - 1);
                if (!cls.empty()) r.classes.push_back(cls);
                dot = nextDot;
            }
            r.specificity = int(r.classes.size()) * 10 + (r.tag.empty() ? 0 : 1);
            r.order = next_++;
            r.style = style;
            rules_.push_back(std::move(r));
        }
    }
}

void Stylesheet::append(const Stylesheet& other) {
    for (const auto& r : other.rules_) {
        rules_.push_back(r);
        rules_.back().order = next_++;
    }
}

Style Stylesheet::match(const std::string& tag, const std::vector<std::string>& classes) const {
    std::vector<const Rule*> hits;
    for (const auto& r : rules_) {
        if (!r.tag.empty() && r.tag != tag) continue;
        bool ok = true;
        for (const auto& c : r.classes)
            if (std::find(classes.begin(), classes.end(), c) == classes.end()) {
                ok = false;
                break;
            }
        if (ok) hits.push_back(&r);
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Rule* a, const Rule* b) {
        return a->specificity != b->specificity ? a->specificity < b->specificity : a->order < b->order;
    });
    Style s;
    for (const Rule* r : hits) s.merge(r->style);
    return s;
}

}  // namespace epub
