#include "Package.h"

#include <string.h>

#include "Text.h"
#include "Xml.h"

namespace epub {

namespace {

bool parseEntry(zip::Archive& zip, const std::string& path, XmlHandler& h, uint32_t maxSize = 4u << 20) {
    const zip::Entry* e = zip.find(path);
    if (!e || e->size > maxSize) return false;
    XmlParser p(h);
    zip::Error err = zip.stream(*e, [&](const uint8_t* d, size_t n) {
        p.feed((const char*)d, n);
        return true;
    });
    p.finish();
    return err == zip::Error::None;
}

struct ContainerHandler : XmlHandler {
    std::string rootfile;
    void startTag(const std::string& name, const Attrs& a, bool) override {
        if (name == "rootfile" && rootfile.empty())
            if (const std::string* p = findAttr(a, "full-path")) rootfile = *p;
    }
    void endTag(const std::string&) override {}
    void text(const std::string&) override {}
};

struct EncryptionHandler : XmlHandler {
    bool drm = false;
    void startTag(const std::string& name, const Attrs& a, bool) override {
        if (name != "encryptionmethod") return;
        const std::string* alg = findAttr(a, "algorithm");
        // font obfuscation is harmless: fonts are not used anyway
        if (alg && *alg != "http://www.idpf.org/2008/embedding" && *alg != "http://ns.adobe.com/pdf/enc#RC")
            drm = true;
    }
    void endTag(const std::string&) override {}
    void text(const std::string&) override {}
};

struct OpfHandler : XmlHandler {
    Package& pkg;
    std::string dir;
    std::vector<std::string> spineIds;
    std::string tocId, coverId, title;
    bool inTitle = false, haveTitle = false;
    explicit OpfHandler(Package& p, std::string d) : pkg(p), dir(std::move(d)) {}

    void startTag(const std::string& name, const Attrs& a, bool selfClosing) override {
        if (name == "title" && !haveTitle) {
            inTitle = !selfClosing;
        } else if (name == "item") {
            ManifestItem it;
            if (const std::string* v = findAttr(a, "id")) it.id = *v;
            if (const std::string* v = findAttr(a, "href")) it.path = resolvePath(dir, *v);
            if (const std::string* v = findAttr(a, "media-type")) it.mediaType = *v;
            if (const std::string* v = findAttr(a, "properties")) it.properties = *v;
            pkg.manifest.push_back(std::move(it));
        } else if (name == "itemref") {
            const std::string* id = findAttr(a, "idref");
            const std::string* linear = findAttr(a, "linear");
            if (id && !(linear && *linear == "no")) spineIds.push_back(*id);
        } else if (name == "spine") {
            if (const std::string* v = findAttr(a, "toc")) tocId = *v;
        } else if (name == "meta") {
            const std::string* n = findAttr(a, "name");
            const std::string* c = findAttr(a, "content");
            if (n && c && *n == "cover") coverId = *c;
        }
    }
    void endTag(const std::string& name) override {
        if (name == "title" && inTitle) {
            inTitle = false;
            haveTitle = true;
        }
    }
    void text(const std::string& s) override {
        if (inTitle) title += s;
    }
};

// EPUB 3 navigation document: <nav epub:type="toc"><ol><li><a href>...
struct NavHandler : XmlHandler {
    std::string dir;
    std::vector<TocTarget> toc, firstNav;
    int navDepth = 0;   // > 0 while inside a <nav>
    bool isToc = false, sawToc = false, firstDone = false;
    int ol = 0;
    bool inA = false;
    std::string aText, aHref;
    explicit NavHandler(std::string d) : dir(std::move(d)) {}

    void startTag(const std::string& name, const Attrs& a, bool selfClosing) override {
        if (name == "nav") {
            if (navDepth++ == 0) {
                const std::string* t = findAttr(a, "type");
                isToc = t && strstr(t->c_str(), "toc");
                if (isToc) sawToc = true;
                ol = 0;
            }
        } else if (navDepth > 0 && name == "ol" && !selfClosing) {
            ol++;
        } else if (navDepth > 0 && name == "a" && !selfClosing) {
            inA = true;
            aText.clear();
            const std::string* h = findAttr(a, "href");
            aHref = h ? *h : "";
        }
    }
    void endTag(const std::string& name) override {
        if (name == "nav" && navDepth > 0) {
            if (--navDepth == 0) {
                if (!firstNav.empty()) firstDone = true;
                isToc = false;
            }
        } else if (name == "ol" && navDepth > 0 && ol > 0) {
            ol--;
        } else if (name == "a" && inA) {
            inA = false;
            std::string title = foldText(aText);
            std::string path = resolvePath(dir, aHref);
            if (title.empty() || path.empty()) return;
            TocTarget t{title, ol > 0 ? ol - 1 : 0, path, fragmentOf(aHref)};
            if (isToc) toc.push_back(t);
            if (!firstDone) firstNav.push_back(t);
        }
    }
    void text(const std::string& s) override {
        if (inA) aText += s;
    }
};

// EPUB 2 NCX: <navPoint><navLabel><text>..</text></navLabel><content src/>
struct NcxHandler : XmlHandler {
    std::string dir;
    std::vector<TocTarget> toc;
    int depth = 0;
    bool inLabel = false, inText = false;
    std::string label;
    explicit NcxHandler(std::string d) : dir(std::move(d)) {}

    void startTag(const std::string& name, const Attrs& a, bool selfClosing) override {
        if (name == "navpoint" && !selfClosing) {
            depth++;
            label.clear();
        } else if (name == "navlabel" && depth > 0) {
            inLabel = true;
        } else if (name == "text" && inLabel) {
            inText = true;
        } else if (name == "content" && depth > 0) {
            const std::string* src = findAttr(a, "src");
            std::string title = foldText(label);
            if (src && !title.empty()) {
                std::string path = resolvePath(dir, *src);
                if (!path.empty()) toc.push_back({title, depth - 1, path, fragmentOf(*src)});
            }
        }
    }
    void endTag(const std::string& name) override {
        if (name == "navpoint" && depth > 0) depth--;
        else if (name == "navlabel") inLabel = false;
        else if (name == "text") inText = false;
    }
    void text(const std::string& s) override {
        if (inText) label += s;
    }
};

const ManifestItem* byId(const Package& p, const std::string& id) {
    for (const auto& m : p.manifest)
        if (m.id == id) return &m;
    return nullptr;
}

bool hasProperty(const std::string& props, const char* want) {
    size_t i = 0;
    while (i < props.size()) {
        size_t sp = props.find(' ', i);
        if (sp == std::string::npos) sp = props.size();
        if (props.compare(i, sp - i, want) == 0) return true;
        i = sp + 1;
    }
    return false;
}

}  // namespace

bool loadPackage(zip::Archive& zip, Package& pkg, std::string& error) {
    pkg = Package();
    if (zip.find("META-INF/encryption.xml")) {
        EncryptionHandler eh;
        parseEntry(zip, "META-INF/encryption.xml", eh);
        if (eh.drm) {
            error = "This book is DRM-protected and cannot be read.";
            return false;
        }
    }
    ContainerHandler ch;
    parseEntry(zip, "META-INF/container.xml", ch);
    pkg.opfPath = ch.rootfile;
    if (pkg.opfPath.empty() || !zip.find(pkg.opfPath)) {   // broken container: look for any .opf
        pkg.opfPath.clear();
        for (const auto& e : zip.entries())
            if (e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, ".opf") == 0) {
                pkg.opfPath = e.name;
                break;
            }
    }
    if (pkg.opfPath.empty()) {
        error = "Not an EPUB (no package document).";
        return false;
    }
    OpfHandler oh(pkg, dirOf(pkg.opfPath));
    if (!parseEntry(zip, pkg.opfPath, oh)) {
        error = "The package document is damaged.";
        return false;
    }
    pkg.title = foldText(oh.title);
    for (const auto& id : oh.spineIds)
        if (const ManifestItem* m = byId(pkg, id))
            if (!m->path.empty() && zip.find(m->path)) pkg.spine.push_back(m->path);
    if (pkg.spine.empty()) {
        error = "The book has no readable content.";
        return false;
    }

    for (const auto& m : pkg.manifest)
        if (hasProperty(m.properties, "cover-image")) pkg.coverPath = m.path;
    if (pkg.coverPath.empty() && !oh.coverId.empty())
        if (const ManifestItem* m = byId(pkg, oh.coverId))
            if (m->mediaType.compare(0, 6, "image/") == 0) pkg.coverPath = m->path;
    if (!pkg.coverPath.empty() && !zip.find(pkg.coverPath)) pkg.coverPath.clear();

    for (const auto& m : pkg.manifest)
        if (hasProperty(m.properties, "nav")) {
            NavHandler nh(dirOf(m.path));
            parseEntry(zip, m.path, nh);
            pkg.toc = nh.sawToc ? nh.toc : nh.firstNav;
            break;
        }
    if (pkg.toc.empty()) {
        const ManifestItem* ncx = oh.tocId.empty() ? nullptr : byId(pkg, oh.tocId);
        if (!ncx)
            for (const auto& m : pkg.manifest)
                if (m.mediaType == "application/x-dtbncx+xml") ncx = &m;
        if (ncx) {
            NcxHandler nh(dirOf(ncx->path));
            parseEntry(zip, ncx->path, nh);
            pkg.toc = nh.toc;
        }
    }
    return true;
}

}  // namespace epub
