#include "EpubCache.h"

#include <string.h>

#include <map>
#include <set>

#include "Css.h"
#include "Etx.h"
#include "Image.h"
#include "Text.h"
#include "Xml.h"

namespace epub {

namespace {

void put16(std::string& s, uint16_t v) {
    s += char(v & 0xFF);
    s += char(v >> 8);
}
void put32(std::string& s, uint32_t v) {
    for (int i = 0; i < 4; i++) s += char((v >> (8 * i)) & 0xFF);
}
uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// Buffered .etx output that knows the current file offset.
struct Emitter {
    io::Writer& w;
    std::string buf;
    uint32_t pos = kEtxHeaderSize;
    bool ok = true;
    explicit Emitter(io::Writer& wr) : w(wr) {}
    void raw(const char* d, size_t n) {
        buf.append(d, n);
        pos += uint32_t(n);
        if (buf.size() >= 4096) flush();
    }
    void raw(char c) { raw(&c, 1); }
    void raw(const std::string& s) { raw(s.data(), s.size()); }
    void flush() {
        if (!buf.empty() && !w.write(buf.data(), buf.size())) ok = false;
        buf.clear();
    }
};

bool isBlockTag(const std::string& t) {
    static const char* const tags[] = {"p",     "div",   "section", "article", "header", "footer", "h1",
                                       "h2",    "h3",    "h4",      "h5",      "h6",     "blockquote", "li",
                                       "ul",    "ol",    "dl",      "dt",      "dd",     "pre",    "figure",
                                       "figcaption", "table", "tr", "center", "aside",  "nav",    "main",
                                       "body",  "address", "caption", "hgroup", "tbody", "thead",  "tfoot"};
    for (const char* b : tags)
        if (t == b) return true;
    return false;
}

bool isVoidTag(const std::string& t) {
    static const char* const tags[] = {"meta", "input", "col", "area", "base", "wbr", "source", "param", "embed", "track"};
    for (const char* v : tags)
        if (t == v) return true;
    return false;
}

struct Frame {
    std::string tag;
    bool block = true, hidden = false, collectCss = false, pre = false;
    bool bold = false, italic = false;
    int8_t align = -1;
    uint8_t kind = etx::Body;
    int8_t indent = -1;
    bool isOl = false;
    int olCounter = 0;
};

class Converter : public XmlHandler {
public:
    Converter(zip::Archive& zip, Emitter& out, const std::set<std::string>& wanted)
        : zip_(zip), out_(out), wanted_(wanted) {}

    std::map<std::string, uint32_t> anchors;
    std::vector<std::string> images;
    std::vector<TocItem> headings;

    bool document(const std::string& path, bool first) {
        docPath_ = path;
        docDir_ = dirOf(path);
        stack_.assign(1, Frame());
        stack_[0].tag = "#root";
        sheet_ = Stylesheet();
        css_.clear();
        if (!first) pendingBreak_ = true;
        chapterStart_ = true;
        endParagraph();
        blockFrom(stack_[0]);
        if (wanted_.count(path)) pendingAnchors_.push_back(path);
        const zip::Entry* e = zip_.find(path);
        if (!e) return true;   // listed but missing: skip
        XmlParser p(*this);
        zip::Error err = zip_.stream(*e, [&](const uint8_t* d, size_t n) {
            p.feed((const char*)d, n);
            return out_.ok;
        });
        p.finish();
        endParagraph();
        return err == zip::Error::None || err == zip::Error::Corrupt;   // keep what was read
    }

    void finish() {
        endParagraph();
        resolveAnchors();
        out_.flush();
    }

    void image(const std::string& path) {
        if (path.empty()) return;
        uint32_t idx;
        auto it = imageIndex_.find(path);
        if (it != imageIndex_.end()) {
            idx = it->second;
        } else {
            if (images.size() >= etx::kMaxImages) return;
            idx = uint32_t(images.size());
            images.push_back(path);
            imageIndex_[path] = idx;
        }
        endParagraph();
        if (pendingBreak_ && !pageClean_) out_.raw(etx::kPage);
        pendingBreak_ = false;
        resolveAnchors();
        char p[3];
        etx::encodeImage(idx, p);
        out_.raw(etx::kImage);
        out_.raw(p, 3);
        pageClean_ = false;
        afterImage_ = true;
        prevHeading_ = false;
    }

    void pageBreak() { pendingBreak_ = true; }

    // ---- XmlHandler ----

    void startTag(const std::string& name, const Attrs& a, bool selfClosing) override {
        Frame& parent = stack_.back();
        if (name == "br") {
            if (!parent.hidden) lineBreak();
            return;
        }
        if (name == "img" || name == "image") {
            if (!parent.hidden) {
                const std::string* src = findAttr(a, name == "img" ? "src" : "href");
                if (src) image(resolvePath(docDir_, *src));
            }
            return;
        }
        if (name == "hr") {
            if (!parent.hidden) rule();
            return;
        }
        if (name == "link") {
            const std::string* rel = findAttr(a, "rel");
            const std::string* href = findAttr(a, "href");
            if (rel && href && rel->find("stylesheet") != std::string::npos) loadCss(resolvePath(docDir_, *href));
            return;
        }
        if (isVoidTag(name)) return;

        Frame f = parent;
        f.tag = name;
        f.block = isBlockTag(name);
        f.collectCss = false;
        f.isOl = name == "ol";
        f.olCounter = 0;
        if (name == "b" || name == "strong" || name == "th") f.bold = true;
        else if (name == "i" || name == "em" || name == "cite" || name == "dfn" || name == "var") f.italic = true;
        else if (name == "h1") f.kind = etx::H1;
        else if (name == "h2") f.kind = etx::H2;
        else if (name.size() == 2 && name[0] == 'h' && name[1] >= '3' && name[1] <= '6') f.kind = etx::H3;
        else if (name == "blockquote") f.kind = etx::Quote;
        else if (name == "li") f.kind = etx::List;
        else if (name == "pre") {
            f.kind = etx::Pre;
            f.pre = true;
        } else if (name == "center") f.align = etx::Center;
        else if (name == "head" || name == "title" || name == "script") f.hidden = true;
        else if (name == "style") {
            f.hidden = true;
            f.collectCss = true;
        } else if ((name == "td" || name == "th") && paraHasContent_) pendingSpace_ = true;

        std::vector<std::string> classes;
        if (const std::string* c = findAttr(a, "class")) {
            size_t i = 0;
            while (i < c->size()) {
                size_t sp = c->find_first_of(" \t\r\n", i);
                if (sp == std::string::npos) sp = c->size();
                if (sp > i) classes.push_back(c->substr(i, sp - i));
                i = sp + 1;
            }
        }
        Style st = sheet_.match(name, classes);
        if (const std::string* inl = findAttr(a, "style")) st.merge(parseDeclarations(*inl));
        if (st.bold >= 0) f.bold = st.bold;
        if (st.italic >= 0) f.italic = st.italic;
        if (st.align >= 0) f.align = st.align;
        if (st.hidden == 1) f.hidden = true;
        if (st.block >= 0) f.block = st.block;
        if (st.indent >= 0) f.indent = st.indent;

        if (!f.hidden) {
            const std::string* id = findAttr(a, "id");
            if (!id && name == "a") id = findAttr(a, "name");
            if (id && wanted_.count(docPath_ + "#" + *id)) pendingAnchors_.push_back(docPath_ + "#" + *id);
        }
        if (f.block && !f.hidden) {
            endParagraph();
            if (st.pageBreak == 1) pendingBreak_ = true;
            if (st.spaceBefore == 1) stickySpace_ = true;
            if (name == "li") listPrefix_ = parent.isOl ? std::to_string(++parent.olCounter) + ". " : "\xC2\xB7 ";
            blockFrom(f);
        }
        if (!selfClosing) stack_.push_back(std::move(f));
    }

    void endTag(const std::string& name) override {
        size_t i = stack_.size();
        while (i > 1 && stack_[i - 1].tag != name) i--;
        if (i <= 1) return;   // not open (or the root)
        bool block = false;
        while (stack_.size() >= i) {
            Frame& f = stack_.back();
            if (f.collectCss) {
                sheet_.parse(css_);
                css_.clear();
            }
            if (f.block && !f.hidden) block = true;
            stack_.pop_back();
        }
        if (block) {
            endParagraph();
            blockFrom(stack_.back());
        }
    }

    void text(const std::string& s) override {
        const Frame& f = stack_.back();
        if (f.collectCss) {
            css_ += s;
            return;
        }
        if (f.hidden) return;
        for (size_t i = 0; i < s.size();) {
            uint32_t cp = nextCodepoint(s, i);
            if (f.pre && cp == '\n') {
                lineBreak();
            } else if (isSpace(cp)) {
                pendingSpace_ = true;
            } else {
                emit(cp, f);
            }
        }
    }

private:
    void blockFrom(const Frame& f) {
        etx::Block b;
        b.kind = f.kind;
        bool heading = f.kind == etx::H1 || f.kind == etx::H2 || f.kind == etx::H3;
        b.align = f.align >= 0 ? uint8_t(f.align)
                               : (heading || f.kind == etx::Pre || f.kind == etx::List ? etx::Left : etx::Justify);
        b.indent = (f.kind == etx::Body || f.kind == etx::Quote) && f.indent != 0;
        b.spaceBefore = false;
        block_ = b;
    }

    void resolveAnchors() {
        for (const auto& k : pendingAnchors_) anchors.emplace(k, out_.pos);
        pendingAnchors_.clear();
    }

    void ensureParagraph() {
        if (!needBlock_) return;
        if (pendingBreak_ && !pageClean_) out_.raw(etx::kPage);
        pendingBreak_ = false;
        resolveAnchors();
        etx::Block b = block_;
        bool heading = b.kind == etx::H1 || b.kind == etx::H2 || b.kind == etx::H3;
        if (prevHeading_ || afterImage_ || chapterStart_) b.indent = false;
        if (heading || stickySpace_ || (prevHeading_ && !heading)) b.spaceBefore = true;
        uint32_t at = out_.pos;
        out_.raw(etx::kPara);
        out_.raw(char(b.encode()));
        curStyle_ = 0;
        needBlock_ = false;
        paraHasContent_ = false;
        pendingSpace_ = false;
        stickySpace_ = false;
        prevHeading_ = heading;
        afterImage_ = false;
        chapterStart_ = false;
        pageClean_ = false;
        capture_ = b.kind == etx::H1 || b.kind == etx::H2;
        if (capture_) {
            headingPos_ = at;
            headingLevel_ = b.kind == etx::H1 ? 0 : 1;
            headingText_.clear();
        }
        if (!listPrefix_.empty()) {
            out_.raw(listPrefix_);
            listPrefix_.clear();
            paraHasContent_ = true;
        }
    }

    void emit(uint32_t cp, const Frame& f) {
        ensureParagraph();
        resolveAnchors();
        if (pendingSpace_ && paraHasContent_) {
            out_.raw(' ');
            if (capture_) headingText_ += ' ';
        }
        pendingSpace_ = false;
        uint8_t want = (f.bold ? etx::kBold : 0) | (f.italic ? etx::kItalic : 0);
        if (want != curStyle_) {
            out_.raw(etx::kStyle);
            out_.raw(char(etx::encodeStyle(want)));
            curStyle_ = want;
        }
        tmp_.clear();
        appendFolded(cp, tmp_);
        out_.raw(tmp_);
        if (capture_) headingText_ += tmp_;
        paraHasContent_ = true;
    }

    void endParagraph() {
        if (capture_) {
            std::string t = foldText(headingText_);
            if (!t.empty()) headings.push_back({headingPos_, headingLevel_, t});
            capture_ = false;
        }
        needBlock_ = true;
        pendingSpace_ = false;
    }

    void lineBreak() {
        if (needBlock_ || !paraHasContent_) return;
        out_.raw(etx::kBreak);
        pendingSpace_ = false;
    }

    void rule() {
        endParagraph();
        etx::Block saved = block_;
        block_ = etx::Block();
        block_.align = etx::Center;
        block_.indent = false;
        ensureParagraph();
        out_.raw("* * *");
        paraHasContent_ = true;
        endParagraph();
        block_ = saved;
    }

    void loadCss(const std::string& path) {
        auto it = cssCache_.find(path);
        if (it == cssCache_.end()) {
            Stylesheet s;
            const zip::Entry* e = zip_.find(path);
            std::string css;
            if (e && zip_.read(*e, css, 1u << 20) == zip::Error::None) s.parse(css);
            it = cssCache_.emplace(path, std::move(s)).first;
        }
        sheet_.append(it->second);
    }

    zip::Archive& zip_;
    Emitter& out_;
    const std::set<std::string>& wanted_;
    std::string docPath_, docDir_;
    std::vector<Frame> stack_;
    Stylesheet sheet_;
    std::string css_;
    std::map<std::string, Stylesheet> cssCache_;
    std::map<std::string, uint32_t> imageIndex_;
    std::vector<std::string> pendingAnchors_;
    etx::Block block_;
    bool needBlock_ = true, paraHasContent_ = false, pendingSpace_ = false, pendingBreak_ = false;
    bool pageClean_ = true, stickySpace_ = false, prevHeading_ = false, afterImage_ = false, chapterStart_ = true;
    uint8_t curStyle_ = 0;
    std::string listPrefix_, tmp_;
    bool capture_ = false;
    uint32_t headingPos_ = 0;
    uint8_t headingLevel_ = 0;
    std::string headingText_;
};

// Does the first document already show the cover picture?
bool mentionsCover(zip::Archive& zip, const std::string& doc, const std::string& cover) {
    const zip::Entry* e = zip.find(doc);
    std::string text;
    if (!e || zip.read(*e, text, 1u << 20) != zip::Error::None) return false;
    std::string base = cover.substr(cover.rfind('/') + 1);
    if (text.find(base) != std::string::npos) return true;
    std::string encoded;   // spaces as %20
    for (char c : base) encoded += c == ' ' ? std::string("%20") : std::string(1, c);
    return text.find(encoded) != std::string::npos;
}

}  // namespace

bool buildCache(zip::Archive& zip, const Package& pkg, uint32_t epubSize, io::Writer& etx, io::Writer& eim,
                io::Writer& etc, const BuildOptions& opt, std::string& error) {
    error.clear();
    std::string hdr = "ETX1";
    put32(hdr, kCacheVersion);
    put32(hdr, epubSize);
    put32(hdr, 0);
    if (!etx.write(hdr.data(), hdr.size())) {
        error = "Cannot write to the card.";
        return false;
    }

    std::set<std::string> wanted;
    for (const auto& t : pkg.toc) {
        wanted.insert(t.path);
        if (!t.fragment.empty()) wanted.insert(t.path + "#" + t.fragment);
    }

    Emitter em(etx);
    Converter conv(zip, em, wanted);
    if (!pkg.coverPath.empty() && !mentionsCover(zip, pkg.spine[0], pkg.coverPath)) {
        conv.image(pkg.coverPath);
        conv.pageBreak();
    }
    uint32_t total = uint32_t(pkg.spine.size());
    for (uint32_t i = 0; i < total; i++) {
        if (opt.progress) opt.progress("Text", i, total);
        if (!conv.document(pkg.spine[i], i == 0) || !em.ok) break;
    }
    conv.finish();
    if (!em.ok) {
        error = "Cannot write to the card (full?).";
        return false;
    }

    // pictures
    std::string table;
    uint32_t offset = 0;
    for (uint32_t i = 0; i < conv.images.size(); i++) {
        if (opt.progress) opt.progress("Pictures", i, uint32_t(conv.images.size()));
        ImageInfo info;
        const zip::Entry* e = zip.find(conv.images[i]);
        std::vector<uint8_t> data;
        image::Gray g;
        if (e && zip.read(*e, data, 8u << 20) == zip::Error::None &&
            image::decode(data.data(), data.size(), g, opt.imageMaxW, opt.imageMaxH)) {
            data.clear();
            data.shrink_to_fit();
            g = image::fit(g, opt.imageMaxW, opt.imageMaxH);
            std::vector<uint8_t> packed = image::pack2bpp(g);
            if (!eim.write(packed.data(), packed.size())) {
                error = "Cannot write to the card (full?).";
                return false;
            }
            info.offset = offset;
            info.w = uint16_t(g.w);
            info.h = uint16_t(g.h);
            offset += uint32_t(packed.size());
        }
        put32(table, info.offset);
        put16(table, info.w);
        put16(table, info.h);
    }
    put32(table, offset);
    put32(table, uint32_t(conv.images.size()));
    put32(table, epubSize);
    table += "EIM1";
    if (!eim.write(table.data(), table.size())) {
        error = "Cannot write to the card (full?).";
        return false;
    }

    // table of contents
    std::vector<TocItem> toc;
    for (const auto& t : pkg.toc) {
        auto it = t.fragment.empty() ? conv.anchors.end() : conv.anchors.find(t.path + "#" + t.fragment);
        if (it == conv.anchors.end()) it = conv.anchors.find(t.path);
        if (it == conv.anchors.end()) continue;
        toc.push_back({it->second, uint8_t(t.level > 255 ? 255 : t.level), t.title});
    }
    if (toc.empty()) toc = conv.headings;
    std::string out = "ETC1";
    put32(out, kCacheVersion);
    put32(out, epubSize);
    std::string title = pkg.title.substr(0, 1000);
    put16(out, uint16_t(title.size()));
    out += title;
    put32(out, uint32_t(toc.size()));
    for (const auto& t : toc) {
        std::string s = t.title;
        if (s.size() > 255) {   // cut at a character boundary
            size_t n = 255;
            while (n > 0 && (uint8_t(s[n]) & 0xC0) == 0x80) n--;
            s.resize(n);
        }
        put32(out, t.pos);
        out += char(t.level);
        out += char(s.size());
        out += s;
    }
    if (!etc.write(out.data(), out.size())) {
        error = "Cannot write to the card (full?).";
        return false;
    }
    if (opt.progress) opt.progress("Done", 1, 1);
    return true;
}

bool checkEtxHeader(const uint8_t h[kEtxHeaderSize], uint32_t epubSize) {
    return memcmp(h, "ETX1", 4) == 0 && le32(h + 4) == kCacheVersion && le32(h + 8) == epubSize;
}

bool readToc(io::ByteSource& src, uint32_t fileSize, uint32_t epubSize, std::string& title,
             std::vector<TocItem>& out) {
    out.clear();
    if (fileSize < 18 || fileSize > (4u << 20)) return false;
    std::vector<uint8_t> d(fileSize);
    if (!src.readAt(0, d.data(), fileSize)) return false;
    if (memcmp(d.data(), "ETC1", 4) != 0 || le32(&d[4]) != kCacheVersion || le32(&d[8]) != epubSize) return false;
    size_t p = 12;
    uint16_t tl = le16(&d[p]);
    p += 2;
    if (p + tl + 4 > fileSize) return false;
    title.assign((const char*)&d[p], tl);
    p += tl;
    uint32_t count = le32(&d[p]);
    p += 4;
    for (uint32_t i = 0; i < count; i++) {
        if (p + 6 > fileSize) return false;
        TocItem t;
        t.pos = le32(&d[p]);
        t.level = d[p + 4];
        uint8_t n = d[p + 5];
        p += 6;
        if (p + n > fileSize) return false;
        t.title.assign((const char*)&d[p], n);
        p += n;
        out.push_back(std::move(t));
    }
    return true;
}

bool readImageTable(io::ByteSource& src, uint32_t fileSize, uint32_t epubSize, std::vector<ImageInfo>& out) {
    out.clear();
    if (fileSize < 16) return false;
    uint8_t tr[16];
    if (!src.readAt(fileSize - 16, tr, 16)) return false;
    if (memcmp(tr + 12, "EIM1", 4) != 0 || le32(tr + 8) != epubSize) return false;
    uint32_t tableOff = le32(tr), count = le32(tr + 4);
    if (uint64_t(tableOff) + uint64_t(count) * 8 + 16 != fileSize) return false;
    std::vector<uint8_t> t(count * 8);
    if (count && !src.readAt(tableOff, t.data(), t.size())) return false;
    out.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        out[i].offset = le32(&t[i * 8]);
        out[i].w = le16(&t[i * 8 + 4]);
        out[i].h = le16(&t[i * 8 + 6]);
    }
    return true;
}

}  // namespace epub
