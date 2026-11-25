#pragma once
// Small incremental XML/XHTML tokenizer: start/end tags with attributes,
// text with entities decoded to UTF-8, CDATA; comments, doctype and
// processing instructions are skipped. Element and attribute names are
// lower-cased; element names lose their namespace prefix ("svg:image" ->
// "image"), attribute names keep it ("epub:type"). The content of <script>
// and <style> is passed through as raw text. Not validating: it accepts the
// sloppy markup found in real EPUBs.

#include <stddef.h>

#include <string>
#include <vector>

namespace epub {

struct Attr {
    std::string name, value;
};
using Attrs = std::vector<Attr>;

// Attribute by full name, or by local name ("href" also finds "xlink:href").
const std::string* findAttr(const Attrs& attrs, const char* name);

class XmlHandler {
public:
    virtual ~XmlHandler() = default;
    virtual void startTag(const std::string& name, const Attrs& attrs, bool selfClosing) = 0;
    virtual void endTag(const std::string& name) = 0;
    virtual void text(const std::string& s) = 0;
};

class XmlParser {
public:
    explicit XmlParser(XmlHandler& h) : h_(h) {}
    void feed(const char* d, size_t n);
    void finish();   // flushes trailing text

private:
    enum class Mode { Text, Tag, Comment, Cdata, Raw };
    void flushText();
    void endOfTag();
    XmlHandler& h_;
    Mode mode_ = Mode::Text;
    std::string text_, tag_, rawEnd_;
    char quote_ = 0;
};

// "&amp;" etc. and numeric references to UTF-8; unknown entities stay as they are.
std::string decodeEntities(const std::string& s);

}  // namespace epub
