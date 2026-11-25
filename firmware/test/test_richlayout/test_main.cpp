#include <unity.h>

#include <string.h>

#include <string>
#include <vector>

#include "RichLayout.h"

using namespace rich;

// Each character is 1 px wide, bold 2 px; size class multiplies.
static int measure(int font, const char* s, size_t n) {
    int chars = 0;
    for (size_t i = 0; i < n; i++)
        if ((uint8_t(s[i]) & 0xC0) != 0x80) chars++;
    return chars * (1 + (font & etx::kBold)) * (1 + font / 4);
}

static Env env(int width, int height) {
    Env e;
    e.measure = measure;
    e.metrics = [](int font) { return Metrics{8 * (1 + font / 4), 2}; };
    e.imageSize = [](uint32_t index, int& w, int& h) {
        w = index == 7 ? 0 : 6;
        h = index == 7 ? 0 : 20;
    };
    e.width = width;
    e.height = height;
    e.indent = 2;
    e.blockIndent = 1;
    e.leading = 0;
    e.paraGap = 2;
    return e;
}

static std::string para(uint8_t kind, uint8_t align = etx::Justify, bool indent = false, bool gap = false) {
    etx::Block b;
    b.kind = kind;
    b.align = align;
    b.indent = indent;
    b.spaceBefore = gap;
    return std::string(1, etx::kPara) + char(b.encode());
}
static std::string style(uint8_t bits) { return std::string(1, etx::kStyle) + char(etx::encodeStyle(bits)); }
static std::string img(uint32_t i) {
    char p[3];
    etx::encodeImage(i, p);
    return std::string(1, etx::kImage) + std::string(p, 3);
}

static std::vector<Line> pageOf(const Layout& lay, const std::string& t, uint32_t pos, State st, uint32_t& next,
                                State& nst, bool eof = true) {
    std::vector<Line> lines;
    next = lay.page(t.data(), t.size(), pos, st, eof, lines, nst);
    return lines;
}

static std::string plain(const std::string& t, const Layout& lay, const Line& l) {
    std::vector<Run> runs;
    lay.runs(t.data(), l, runs);
    std::string s;
    for (size_t i = 0; i < runs.size(); i++) {
        if (i && runs[i].x > runs[i - 1].x + measure(runs[i - 1].font, t.data() + runs[i - 1].start,
                                                     runs[i - 1].end - runs[i - 1].start))
            s += ' ';
        s += t.substr(runs[i].start, runs[i].end - runs[i].start);
    }
    return s;
}

void setUp() {}
void tearDown() {}

void test_wraps_and_justifies() {
    Layout lay(env(10, 100));
    std::string t = para(etx::Body) + "aaa bbb ccc ddd";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL_STRING("aaa bbb", plain(t, lay, lines[0]).c_str());
    TEST_ASSERT_TRUE(lines[0].justify);
    TEST_ASSERT_FALSE(lines[1].justify);   // last line of the paragraph
    std::vector<Run> runs;
    lay.runs(t.data(), lines[0], runs);
    TEST_ASSERT_EQUAL(0, runs[0].x);
    TEST_ASSERT_EQUAL(7, runs[1].x);   // stretched to the right edge
    TEST_ASSERT_EQUAL(t.size(), next);
}

void test_bold_is_wider_and_style_changes_inside_words() {
    Layout lay(env(10, 100));
    std::string t = para(etx::Body) + style(etx::kBold) + "bbb" + style(0) + " a" + " x" + style(etx::kItalic) + "y" +
                    style(0) + "z";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(2, lines.size());   // "bbb" is 6 px, + " a" = 8, + " xyz" does not fit
    std::vector<Run> runs;
    lay.runs(t.data(), lines[1], runs);
    TEST_ASSERT_EQUAL(3, runs.size());
    TEST_ASSERT_EQUAL(0, runs[0].x);
    TEST_ASSERT_EQUAL(1, runs[1].x);
    TEST_ASSERT_EQUAL(etx::kItalic, runs[1].font);
    TEST_ASSERT_EQUAL(2, runs[2].x);
}

void test_heading_line_height_and_gap() {
    Layout lay(env(40, 100));
    std::string t = para(etx::H1, etx::Center) + "Title" + para(etx::Body, etx::Justify, true, true) + "text";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL(26, lines[0].height);   // size class 2: ascent 24 + descent 2
    TEST_ASSERT_EQUAL(26 + 2, lines[1].top);   // paragraph gap
    TEST_ASSERT_EQUAL(10, lines[1].height);
    std::vector<Run> runs;
    lay.runs(t.data(), lines[0], runs);
    TEST_ASSERT_EQUAL(etx::kBold + 8, runs[0].font);   // headings are bold
    TEST_ASSERT_EQUAL((40 - 30) / 2, runs[0].x);       // "Title" 5 × 2 × 3 = 30 px, centred
    lay.runs(t.data(), lines[1], runs = {});
    TEST_ASSERT_EQUAL(2, runs[0].x);   // first-line indent
}

void test_page_break_code() {
    Layout lay(env(40, 100));
    std::string t = para(etx::Body) + "a" + std::string(1, etx::kPage) + para(etx::Body) + "b";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL(4, next);   // just after the page break
    lines = pageOf(lay, t, next, st, next, st);
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL_STRING("b", plain(t, lay, lines[0]).c_str());
}

void test_image_moves_to_next_page() {
    Layout lay(env(40, 30));
    std::string t = para(etx::Body) + "aaa" + img(3) + img(7) + para(etx::Body) + "b";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(1, lines.size());   // 10 + gap 2 + 20 > 30
    lines = pageOf(lay, t, next, st, next, st);
    TEST_ASSERT_EQUAL(1, lines.size());   // 20 + gap 2 + 10 > 30
    TEST_ASSERT_TRUE(lines[0].image);
    TEST_ASSERT_EQUAL(3, lines[0].imageIndex);
    TEST_ASSERT_EQUAL(17, lines[0].left);   // centred
    TEST_ASSERT_EQUAL(0, lines[0].top);
    lines = pageOf(lay, t, next, st, next, st);
    TEST_ASSERT_EQUAL(1, lines.size());   // picture 7 has no size and is skipped
    TEST_ASSERT_FALSE(lines[0].image);
    TEST_ASSERT_EQUAL(t.size(), next);
}

void test_style_carries_over_page_boundary() {
    Layout lay(env(4, 20));   // two lines per page
    std::string t = para(etx::Body) + style(etx::kBold) + "aa bb cc";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL(etx::kBold, st.style);
    lines = pageOf(lay, t, next, st, next, st);
    TEST_ASSERT_EQUAL(1, lines.size());
    std::vector<Run> runs;
    lay.runs(t.data(), lines[0], runs);
    TEST_ASSERT_EQUAL(etx::kBold, runs[0].font);
}

void test_paragraph_start_goes_to_next_page_whole() {
    Layout lay(env(10, 20));
    std::string t = para(etx::Body) + "a" + para(etx::Body) + "b" + para(etx::Body, etx::Justify, true) + "c";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL(etx::kPara, t[next]);   // the third paragraph starts the next page
    lines = pageOf(lay, t, next, st, next, st);
    TEST_ASSERT_EQUAL(2, lines[0].firstIndent);
}

void test_nbsp_glues_and_break() {
    Layout lay(env(5, 100));
    std::string t = para(etx::Body) + "aa\xC2\xA0" "bb cc" + std::string(1, etx::kBreak) + "d";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(3, lines.size());
    TEST_ASSERT_EQUAL_STRING("aa\xC2\xA0" "bb", plain(t, lay, lines[0]).c_str());
    TEST_ASSERT_FALSE(lines[1].justify);   // ends with a hard break
    TEST_ASSERT_EQUAL_STRING("d", plain(t, lay, lines[2]).c_str());
}

void test_long_word_is_cut() {
    Layout lay(env(4, 100));
    std::string t = para(etx::Body) + "abcdefghij";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(3, lines.size());
    TEST_ASSERT_EQUAL_STRING("abcd", plain(t, lay, lines[0]).c_str());
    TEST_ASSERT_EQUAL_STRING("ij", plain(t, lay, lines[2]).c_str());
}

void test_stops_before_incomplete_word() {
    Layout lay(env(40, 100));
    std::string t = para(etx::Body) + "one two thr";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st, false);
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL_STRING("one two", plain(t, lay, lines[0]).c_str());
    TEST_ASSERT_EQUAL(t.size() - 3, next);
}

void test_quote_and_list_indents() {
    Layout lay(env(20, 100));
    std::string t = para(etx::Quote, etx::Left) + "q" + para(etx::List, etx::Left) + "l";
    uint32_t next;
    State st;
    auto lines = pageOf(lay, t, 0, State(), next, st);
    TEST_ASSERT_EQUAL(1, lines[0].left);
    TEST_ASSERT_EQUAL(18, lines[0].avail);
    TEST_ASSERT_EQUAL(1, lines[1].left);
    TEST_ASSERT_EQUAL(19, lines[1].avail);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wraps_and_justifies);
    RUN_TEST(test_bold_is_wider_and_style_changes_inside_words);
    RUN_TEST(test_heading_line_height_and_gap);
    RUN_TEST(test_page_break_code);
    RUN_TEST(test_image_moves_to_next_page);
    RUN_TEST(test_style_carries_over_page_boundary);
    RUN_TEST(test_paragraph_start_goes_to_next_page_whole);
    RUN_TEST(test_nbsp_glues_and_break);
    RUN_TEST(test_long_word_is_cut);
    RUN_TEST(test_stops_before_incomplete_word);
    RUN_TEST(test_quote_and_list_indents);
    return UNITY_END();
}
