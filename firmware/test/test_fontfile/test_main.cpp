#include <unity.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "FontFile.h"
#include "../../src/BuiltinFont.h"

using namespace xff;

struct MemSource : io::ByteSource {
    std::vector<uint8_t> d;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
        if (uint64_t(off) + n > d.size()) return false;
        memcpy(dst, d.data() + off, n);
        return true;
    }
};

// The font the reader ships (sdcard/fonts, built by 'xtconvert font').
static MemSource card;
static bool readCard() {
    std::string path = std::string(FIXTURES_DIR) + "/../../../sdcard/fonts/literata.xff";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) card.d.insert(card.d.end(), buf, buf + n);
    fclose(f);
    return true;
}

static Font lit;

struct Canvas {
    int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1, count = 0, inkSum = 0;
};
static void plot(void* ctx, int x, int y, uint8_t ink) {
    Canvas& c = *(Canvas*)ctx;
    if (x < c.minX) c.minX = x;
    if (y < c.minY) c.minY = y;
    if (x > c.maxX) c.maxX = x;
    if (y > c.maxY) c.maxY = y;
    c.count++;
    c.inkSum += ink;
    TEST_ASSERT_TRUE(ink >= 1 && ink <= 3);
}

void setUp() {}
void tearDown() {}

void test_loads_card_font() {
    TEST_ASSERT_TRUE(readCard());
    TEST_ASSERT_TRUE(lit.load(card, card.d.size()));
    TEST_ASSERT_EQUAL_STRING("Literata", lit.name());
    TEST_ASSERT_EQUAL(20, lit.faceCount());
    for (uint8_t s = 0; s < 4; s++) TEST_ASSERT_EQUAL(5, lit.sizeCount(s));
}

void test_find_and_nearest() {
    TEST_ASSERT_EQUAL(15, lit.find(Regular, 0)->size());
    TEST_ASSERT_EQUAL(17, lit.find(Regular, 1)->size());
    TEST_ASSERT_EQUAL(29, lit.find(Bold, 9)->size());     // clamped
    TEST_ASSERT_EQUAL(15, lit.find(Italic, -1)->size());
    TEST_ASSERT_EQUAL(BoldItalic, lit.find(BoldItalic, 2)->style());
    TEST_ASSERT_EQUAL(20, lit.nearest(Regular, 21)->size());
    TEST_ASSERT_EQUAL(29, lit.nearest(Bold, 40)->size());
}

// Values from the Python reader (xtconvert.font.Xff) on the same file.
void test_metrics_match_converter() {
    const Face& f = *lit.find(Regular, 1);
    TEST_ASSERT_EQUAL(17, f.ascent());
    TEST_ASSERT_EQUAL(4, f.descent());
    TEST_ASSERT_EQUAL(82, f.width("Hamburg"));
    TEST_ASSERT_EQUAL(53, f.width("Grüße"));
    TEST_ASSERT_EQUAL(31, f.width("\xE2\x80\x9E" "A" "\xE2\x80\x9C"));   // „A“
    const Glyph* h = f.glyph('H');
    TEST_ASSERT_EQUAL(240, h->advance);
    TEST_ASSERT_EQUAL(13, h->width);
    TEST_ASSERT_EQUAL(12, h->height);
    TEST_ASSERT_EQUAL(12, h->top);
}

void test_draw_glyph() {
    const Face& f = *lit.find(Regular, 1);
    Canvas c;
    int adv = f.draw(10, 50, "H", 1, plot, &c);
    TEST_ASSERT_EQUAL(15, adv);                 // 240 / 16
    TEST_ASSERT_EQUAL(76, c.count);             // pixels with ink
    TEST_ASSERT_EQUAL(185, c.inkSum);
    TEST_ASSERT_EQUAL(11, c.minX);              // pen + left bearing 1
    TEST_ASSERT_EQUAL(23, c.maxX);
    TEST_ASSERT_EQUAL(38, c.minY);              // baseline - top
    TEST_ASSERT_EQUAL(49, c.maxY);              // ends on the baseline row above
}

void test_draw_advance_equals_width() {
    const Face& f = *lit.find(Italic, 2);
    const char* s = "Typografie für 300 px";
    Canvas c;
    TEST_ASSERT_EQUAL(f.width(s), f.draw(0, 30, s, strlen(s), plot, &c));
    TEST_ASSERT_TRUE(c.count > 100);
}

void test_utf8_and_fallbacks() {
    const Face& f = *lit.find(Regular, 1);
    // Latin-1 byte that is not valid UTF-8 reads as that character
    TEST_ASSERT_EQUAL(f.width("ü"), f.width("\xFC"));
    // unknown character → '?'
    TEST_ASSERT_EQUAL(f.width("?"), f.width("\xE4\xB8\x80"));   // U+4E00
    TEST_ASSERT_EQUAL(f.width("?"), f.width("\xF0\x9F\x98\x80"));   // emoji
    // truncated sequence: each byte on its own
    TEST_ASSERT_EQUAL(f.width("\xC3"), f.width("\xC3", 1));
    const char* p = "\xE2\x80";
    const char* end = p + 2;
    TEST_ASSERT_EQUAL(0xE2, nextCodePoint(p, end));
    TEST_ASSERT_EQUAL(0x80, nextCodePoint(p, end));
    TEST_ASSERT_TRUE(p == end);
    // overlong encoding of '/' is rejected
    const char* o = "\xC0\xAF";
    TEST_ASSERT_EQUAL(0xC0, nextCodePoint(o, o + 2));
}

void test_builtin_font() {
    Font ui;
    TEST_ASSERT_TRUE(ui.attach(kBuiltinFont, sizeof kBuiltinFont));
    TEST_ASSERT_EQUAL(3, ui.faceCount());
    TEST_ASSERT_EQUAL(1, ui.sizeCount(Bold));
    TEST_ASSERT_EQUAL(16, ui.find(Bold, 0)->size());
    TEST_ASSERT_EQUAL(14, ui.nearest(Regular, 13)->size());
    TEST_ASSERT_EQUAL(11, ui.nearest(Italic, 10)->size());   // no italic: regular
    TEST_ASSERT_TRUE(ui.find(Regular, 0)->width("Library") > 20);
}

static bool loadBytes(Font& f, std::vector<uint8_t> d) {
    MemSource s;
    s.d = std::move(d);
    return f.load(s, s.d.size());
}

void test_rejects_bad_files() {
    Font f;
    std::vector<uint8_t> d = card.d;
    d[0] = 'Y';
    TEST_ASSERT_FALSE(loadBytes(f, d));
    TEST_ASSERT_FALSE(f.loaded());
    d = card.d;
    d.resize(d.size() / 2);   // bitmaps cut off
    TEST_ASSERT_FALSE(loadBytes(f, d));
    d = card.d;
    d[32 + 8] = 0xFF;          // first face: glyph table offset out of range
    d[32 + 9] = 0xFF;
    d[32 + 10] = 0xFF;
    TEST_ASSERT_FALSE(loadBytes(f, d));
    d = card.d;
    d[4] = 0;                  // no faces
    TEST_ASSERT_FALSE(loadBytes(f, d));
    TEST_ASSERT_TRUE(loadBytes(f, card.d));
    TEST_ASSERT_TRUE(f.loaded());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_loads_card_font);
    RUN_TEST(test_find_and_nearest);
    RUN_TEST(test_metrics_match_converter);
    RUN_TEST(test_draw_glyph);
    RUN_TEST(test_draw_advance_equals_width);
    RUN_TEST(test_utf8_and_fallbacks);
    RUN_TEST(test_builtin_font);
    RUN_TEST(test_rejects_bad_files);
    return UNITY_END();
}
