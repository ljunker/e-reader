#include <unity.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "Css.h"
#include "EpubCache.h"
#include "Etx.h"
#include "Package.h"
#include "Text.h"
#include "Xml.h"

using namespace epub;

#define ASSERT_HAS(t, s) TEST_ASSERT_TRUE_MESSAGE((t).find(s) != std::string::npos, (t).c_str())

struct MemSource : io::ByteSource {
    std::vector<uint8_t> d;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
        if (uint64_t(off) + n > d.size()) return false;
        memcpy(dst, d.data() + off, n);
        return true;
    }
};

struct VecWriter : io::Writer {
    std::vector<uint8_t> d;
    bool write(const void* p, size_t n) override {
        d.insert(d.end(), (const uint8_t*)p, (const uint8_t*)p + n);
        return true;
    }
};

static std::vector<uint8_t> loadFile(const char* name) {
    std::string path = std::string(FIXTURES_DIR) + "/" + name;
    std::vector<uint8_t> out;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return out;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return out;
}

// Readable form of an .etx stream for assertions.
static std::string dump(const std::vector<uint8_t>& etx, size_t from = kEtxHeaderSize) {
    std::string s;
    for (size_t i = from; i < etx.size(); i++) {
        char c = char(etx[i]);
        if (c == etx::kPara) {
            etx::Block b = etx::Block::decode(etx[++i]);
            static const char* kinds[] = {"p", "h1", "h2", "h3", "q", "li", "pre", "?"};
            s += std::string("[") + kinds[b.kind] + (b.align == etx::Center ? "|c" : b.align == etx::Left ? "|l" : "") +
                 (b.indent ? "|>" : "") + (b.spaceBefore ? "|^" : "") + "]";
        } else if (c == etx::kStyle) {
            uint8_t st = etx[++i] & 3;
            s += st == 0 ? "</>" : st == etx::kBold ? "<b>" : st == etx::kItalic ? "<i>" : "<bi>";
        } else if (c == etx::kBreak) {
            s += "<br>";
        } else if (c == etx::kPage) {
            s += "<page>";
        } else if (c == etx::kImage) {
            s += "<img " + std::to_string(etx::decodeImage((const char*)&etx[i + 1])) + ">";
            i += 3;
        } else {
            s += c;
        }
    }
    return s;
}

struct Built {
    MemSource src;
    Package pkg;
    VecWriter etx, eim, etc;
    std::string error;
    bool ok = false;
};

static void build(const char* name, Built& b) {
    b.src.d = loadFile(name);
    zip::Archive a(b.src, b.src.d.size());
    TEST_ASSERT_EQUAL(int(zip::Error::None), int(a.open()));
    if (!loadPackage(a, b.pkg, b.error)) return;
    BuildOptions opt;
    b.ok = buildCache(a, b.pkg, uint32_t(b.src.d.size()), b.etx, b.eim, b.etc, opt, b.error);
}

void setUp() {}
void tearDown() {}

// ---------- building blocks ----------

struct Recorder : XmlHandler {
    std::string log;
    void startTag(const std::string& n, const Attrs& a, bool self) override {
        log += "<" + n;
        for (const auto& x : a) log += " " + x.name + "=" + x.value;
        log += self ? "/>" : ">";
    }
    void endTag(const std::string& n) override { log += "</" + n + ">"; }
    void text(const std::string& s) override { log += s; }
};

void test_xml_tokenizer_in_small_chunks() {
    const char* doc =
        "<?xml version='1.0'?><!DOCTYPE html><svg:image xlink:href=\"a.png\"/>"
        "<p class='x &amp; y' data=unq>a &lt;b&gt; &#228;&#x00FC;&nbsp;&unknown;</p><!-- <p>no</p> -->"
        "<![CDATA[<raw>]]><style>p > a { x: \"</p>\" }</style ><br>";
    Recorder r;
    XmlParser p(r);
    for (size_t i = 0; i < strlen(doc); i++) p.feed(doc + i, 1);
    p.finish();
    TEST_ASSERT_EQUAL_STRING(
        "<image xlink:href=a.png/><p class=x & y data=unq>a <b> \xC3\xA4\xC3\xBC\xC2\xA0&unknown;</p><raw>"
        "<style>p > a { x: \"</p>\" }</style><br>",
        r.log.c_str());
    Attrs at = {{"xlink:href", "x"}};
    TEST_ASSERT_EQUAL_STRING("x", findAttr(at, "href")->c_str());
}

void test_folding_and_paths() {
    TEST_ASSERT_EQUAL_STRING("\"Zitat\" - ... 'a' --", foldText("\xE2\x80\x9EZitat\xE2\x80\x9C \xE2\x80\x93 \xE2\x80\xA6 \xE2\x80\x98" "a\xE2\x80\x99 \xE2\x80\x94").c_str());
    TEST_ASSERT_EQUAL_STRING("L\xC3\xB3" "dz OE fi ?", foldText("  \xC5\x81\xC3\xB3" "d\xC5\xBA\n \xC5\x92 \xEF\xAC\x81 \xCE\xB1 ").c_str());
    TEST_ASSERT_EQUAL_STRING("ab", foldText("a\xC2\xAD" "b").c_str());   // soft hyphen
    TEST_ASSERT_EQUAL_STRING("\xC3\xA4", foldText("\xE4").c_str());       // stray Latin-1 byte
    TEST_ASSERT_EQUAL_STRING("OEBPS/images/a b.png", resolvePath("OEBPS/text/", "../images/a%20b.png#x").c_str());
    TEST_ASSERT_EQUAL_STRING("x.html", resolvePath("", "./x.html?q=1").c_str());
    TEST_ASSERT_EQUAL_STRING("", resolvePath("OEBPS/", "http://example.com/a").c_str());
    TEST_ASSERT_EQUAL_STRING("sec", fragmentOf("a.html#sec").c_str());
}

void test_css_cascade() {
    Stylesheet s;
    s.parse("/* c */ p { text-align: center; font-weight: bold } .x { font-weight: normal !important }"
            "p.x.y { font-style: italic } div p, p:first-child, #id { display: none } "
            "@font-face { font-family: a; } @import url(x.css); h2, .chap { page-break-before: always }"
            ".m { margin: 1em 0 0 0; text-indent: 0 }");
    Style a = s.match("p", {"x", "y"});
    TEST_ASSERT_EQUAL(0, a.bold);   // .x beats p
    TEST_ASSERT_EQUAL(1, a.italic);
    TEST_ASSERT_EQUAL(etx::Center, a.align);
    TEST_ASSERT_EQUAL(-1, a.hidden);   // combinator, pseudo-class and id rules ignored
    TEST_ASSERT_EQUAL(1, s.match("div", {"chap"}).pageBreak);
    Style m = s.match("span", {"m"});
    TEST_ASSERT_EQUAL(1, m.spaceBefore);
    TEST_ASSERT_EQUAL(0, m.indent);
    Style inl = parseDeclarations("font-weight:700; display : none");
    TEST_ASSERT_EQUAL(1, inl.bold);
    TEST_ASSERT_EQUAL(1, inl.hidden);
}

// ---------- whole books ----------

void test_package_epub3() {
    MemSource src;
    src.d = loadFile("sample.epub");
    zip::Archive a(src, src.d.size());
    a.open();
    Package pkg;
    std::string err;
    TEST_ASSERT_TRUE(loadPackage(a, pkg, err));
    TEST_ASSERT_EQUAL_STRING("Das Testbuch & mehr", pkg.title.c_str());
    TEST_ASSERT_EQUAL_STRING("de", pkg.language.c_str());
    TEST_ASSERT_EQUAL(2, pkg.spine.size());
    TEST_ASSERT_EQUAL_STRING("OEBPS/text/ch1.xhtml", pkg.spine[0].c_str());
    TEST_ASSERT_EQUAL_STRING("OEBPS/images/cover.jpg", pkg.coverPath.c_str());
    TEST_ASSERT_EQUAL(3, pkg.toc.size());   // the landmarks nav is not the table of contents
    TEST_ASSERT_EQUAL_STRING("Kapitel 1", pkg.toc[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("Ein Abschnitt", pkg.toc[1].title.c_str());
    TEST_ASSERT_EQUAL(1, pkg.toc[1].level);
    TEST_ASSERT_EQUAL_STRING("sec", pkg.toc[1].fragment.c_str());
    TEST_ASSERT_EQUAL_STRING("Kapitel 2", pkg.toc[2].title.c_str());
    TEST_ASSERT_EQUAL_STRING("OEBPS/text/ch2.xhtml", pkg.toc[2].path.c_str());
}

void test_convert_epub3_text() {
    Built b;
    build("sample.epub", b);
    TEST_ASSERT_TRUE_MESSAGE(b.ok, b.error.c_str());
    TEST_ASSERT_TRUE(checkEtxHeader(b.etx.d.data(), uint32_t(b.src.d.size())));
    std::string t = dump(b.etx.d);
    // cover first, on its own page
    TEST_ASSERT_EQUAL(0, t.find("<img 0><page>[h1|l|^]Kapitel 1"));
    // the gap between words comes before a style change
    ASSERT_HAS(t, "Kapitel 1[p|^]Ein <i>kursives </>und <b>fettes </>Wort & \"Zitat\" -- 10\xC2\xA0km.");
    ASSERT_HAS(t, "[p|c|>]Mitte");
    TEST_ASSERT_TRUE(t.find("Unsichtbar") == std::string::npos);
    TEST_ASSERT_TRUE(t.find("not text") == std::string::npos);
    TEST_ASSERT_TRUE(t.find("var x") == std::string::npos);
    TEST_ASSERT_TRUE(t.find("Kapitel 1Kapitel") == std::string::npos);   // <title> is not text
    ASSERT_HAS(t, "<page>[h2|c|^]Abschnitt[p|^]Bild folgt:<img 1>");
    ASSERT_HAS(t, "[li|l]\xC2\xB7 Erstens[li|l]\xC2\xB7 Zweitens");
    ASSERT_HAS(t, "[p|>]<i>schr\xC3\xA4g<b>dick</>, <b>inline </>L\xC3\xB3" "dz ...<page>");
    ASSERT_HAS(t, "<page>[h1|l|^]Kapitel 2[h3|l|^]Untertitel[p|^]Zeile eins<br>Zeile zwei[p|>]Absatz 0");
}

void test_convert_epub3_toc_and_images() {
    Built b;
    build("sample.epub", b);
    TEST_ASSERT_TRUE(b.ok);
    MemSource toc;
    toc.d = b.etc.d;
    std::string title, lang;
    std::vector<TocItem> items;
    TEST_ASSERT_TRUE(readToc(toc, toc.d.size(), uint32_t(b.src.d.size()), title, lang, items));
    TEST_ASSERT_EQUAL_STRING("Das Testbuch & mehr", title.c_str());
    TEST_ASSERT_EQUAL_STRING("de", lang.c_str());
    TEST_ASSERT_EQUAL(3, items.size());
    TEST_ASSERT_EQUAL(etx::kPara, char(b.etx.d[items[0].pos]));
    TEST_ASSERT_EQUAL(0, dump(b.etx.d, items[0].pos).find("[h1|l|^]Kapitel 1"));
    TEST_ASSERT_EQUAL(0, dump(b.etx.d, items[1].pos).find("[h2|c|^]Abschnitt"));
    TEST_ASSERT_EQUAL(1, items[1].level);
    TEST_ASSERT_EQUAL(0, dump(b.etx.d, items[2].pos).find("[h1|l|^]Kapitel 2[h3"));
    TEST_ASSERT_FALSE(readToc(toc, toc.d.size(), 12345, title, lang, items));   // other EPUB size

    MemSource eim;
    eim.d = b.eim.d;
    std::vector<ImageInfo> imgs;
    TEST_ASSERT_TRUE(readImageTable(eim, eim.d.size(), uint32_t(b.src.d.size()), imgs));
    TEST_ASSERT_EQUAL(2, imgs.size());
    TEST_ASSERT_EQUAL(276, imgs[0].w);   // 600 × 800 cover fitted into 276 × 374
    TEST_ASSERT_EQUAL(368, imgs[0].h);
    TEST_ASSERT_EQUAL(100, imgs[1].w);
    TEST_ASSERT_EQUAL(50, imgs[1].h);
    TEST_ASSERT_EQUAL(imgs[0].offset + (276 * 368 + 3) / 4, imgs[1].offset);
}

void test_convert_epub2_with_ncx() {
    Built b;
    build("sample2.epub", b);
    TEST_ASSERT_TRUE_MESSAGE(b.ok, b.error.c_str());
    TEST_ASSERT_EQUAL_STRING("Zwei", b.pkg.title.c_str());
    TEST_ASSERT_TRUE(b.pkg.coverPath.empty());
    std::string t = dump(b.etx.d);
    TEST_ASSERT_EQUAL_STRING("[p]Erster Text.<page>[p]Vorher.[p|>]Ziel.", t.c_str());
    MemSource toc;
    toc.d = b.etc.d;
    std::string title, lang;
    std::vector<TocItem> items;
    TEST_ASSERT_TRUE(readToc(toc, toc.d.size(), uint32_t(b.src.d.size()), title, lang, items));
    TEST_ASSERT_EQUAL(2, items.size());
    TEST_ASSERT_TRUE(lang.empty());
    TEST_ASSERT_EQUAL_STRING("Unterpunkt", items[1].title.c_str());
    TEST_ASSERT_EQUAL(1, items[1].level);
    TEST_ASSERT_EQUAL(0, dump(b.etx.d, items[1].pos).find("[p|>]Ziel."));
}

void test_drm_is_refused() {
    Built b;
    build("drm.epub", b);
    TEST_ASSERT_FALSE(b.ok);
    TEST_ASSERT_TRUE(b.error.find("DRM") != std::string::npos);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_xml_tokenizer_in_small_chunks);
    RUN_TEST(test_folding_and_paths);
    RUN_TEST(test_css_cascade);
    RUN_TEST(test_package_epub3);
    RUN_TEST(test_convert_epub3_text);
    RUN_TEST(test_convert_epub3_toc_and_images);
    RUN_TEST(test_convert_epub2_with_ncx);
    RUN_TEST(test_drm_is_refused);
    return UNITY_END();
}
