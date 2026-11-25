#pragma once
// The CSS subset the reader honours. Selectors: "tag", ".class",
// "tag.class.other" and comma lists; rules with combinators, ids, attribute
// selectors or pseudo-classes are ignored, as are at-rules (@media, ...).
// Properties: font-weight, font-style, text-align, display, page-break-before /
// break-before, text-indent, margin-top / margin.

#include <stdint.h>

#include <string>
#include <vector>

namespace epub {

// -1 = not set
struct Style {
    int8_t bold = -1;
    int8_t italic = -1;
    int8_t align = -1;         // etx::Align
    int8_t hidden = -1;        // display: none
    int8_t block = -1;         // display: block (1) / inline (0)
    int8_t pageBreak = -1;
    int8_t indent = -1;        // text-indent > 0
    int8_t spaceBefore = -1;   // margin-top > 0

    void merge(const Style& o);
};

Style parseDeclarations(const std::string& decls);

class Stylesheet {
public:
    void parse(const std::string& css);
    void append(const Stylesheet& other);
    // Cascaded style of an element: by specificity, then source order.
    Style match(const std::string& tag, const std::vector<std::string>& classes) const;
    size_t ruleCount() const { return rules_.size(); }

private:
    struct Rule {
        std::string tag;                    // "" = any
        std::vector<std::string> classes;
        int specificity;
        int order;
        Style style;
    };
    std::vector<Rule> rules_;
    int next_ = 0;
};

}  // namespace epub
