#include <unity.h>

#include <string.h>
#include <string>
#include <vector>

#include "TextLayout.h"

using namespace layout;

// monospace: 1 px per character (UTF-8 aware)
static int mono(const char* s, size_t n) {
    int w = 0;
    for (size_t i = 0; i < n; i++)
        if ((uint8_t(s[i]) & 0xC0) != 0x80) w++;
    return w;
}

static std::string text(const char* buf, const Line& l) { return std::string(buf + l.start, l.end - l.start); }

void setUp() {}
void tearDown() {}

void test_wraps_words() {
    Layout lay(mono, {10, 0, 20});
    const char* t = "aaa bbb ccc ddd";
    std::vector<Line> lines;
    uint32_t next = lay.page(t, strlen(t), 0, true, true, lines);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL_STRING("aaa bbb", text(t, lines[0]).c_str());
    TEST_ASSERT_EQUAL_STRING("ccc ddd", text(t, lines[1]).c_str());
    TEST_ASSERT_TRUE(lines[0].paraStart);
    TEST_ASSERT_TRUE(lines[1].paraEnd);
    TEST_ASSERT_EQUAL(strlen(t), next);
}

void test_paragraphs_and_single_newlines() {
    Layout lay(mono, {20, 2, 20});
    const char* t = "one\ntwo\n\nthree";
    std::vector<Line> lines;
    lay.page(t, strlen(t), 0, true, true, lines);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL_STRING("one\ntwo", text(t, lines[0]).c_str());
    TEST_ASSERT_TRUE(lines[0].paraEnd);
    TEST_ASSERT_TRUE(lines[1].paraStart);
    TEST_ASSERT_EQUAL_STRING("three", text(t, lines[1]).c_str());
}

void test_pagination_continues() {
    Layout lay(mono, {5, 0, 2});
    const char* t = "aa bb cc dd ee";
    std::vector<Line> p1, p2, p3;
    uint32_t n1 = lay.page(t, strlen(t), 0, true, true, p1);
    uint32_t n2 = lay.page(t, strlen(t), n1, true, true, p2);
    lay.page(t, strlen(t), n2, true, true, p3);
    TEST_ASSERT_EQUAL(2, p1.size());
    TEST_ASSERT_EQUAL_STRING("aa bb", text(t, p1[0]).c_str());
    TEST_ASSERT_EQUAL_STRING("cc dd", text(t, p1[1]).c_str());
    TEST_ASSERT_EQUAL(1, p2.size());
    TEST_ASSERT_EQUAL_STRING("ee", text(t, p2[0]).c_str());
    TEST_ASSERT_FALSE(p2[0].paraStart);   // continues the paragraph
    TEST_ASSERT_EQUAL(0, p3.size());
}

void test_long_word_hard_break_utf8() {
    Layout lay(mono, {4, 0, 5});
    const char* t = "\xc3\xa4\xc3\xa4\xc3\xa4\xc3\xa4\xc3\xa4\xc3\xa4";   // 6 × ä
    std::vector<Line> lines;
    lay.page(t, strlen(t), 0, true, true, lines);
    TEST_ASSERT_EQUAL(2, lines.size());
    TEST_ASSERT_EQUAL(8, lines[0].end - lines[0].start);   // 4 chars, 2 bytes each
}

void test_stops_at_buffer_end_without_eof() {
    Layout lay(mono, {20, 0, 5});
    const char* t = "hello wor";   // "wor" may continue
    std::vector<Line> lines;
    uint32_t next = lay.page(t, strlen(t), 0, false, true, lines);
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL_STRING("hello", text(t, lines[0]).c_str());
    TEST_ASSERT_EQUAL(6, next);
}

void test_justify() {
    Layout lay(mono, {11, 0, 5});
    const char* t = "aa bb cc dddd";
    std::vector<Line> lines;
    lay.page(t, strlen(t), 0, true, true, lines);
    std::vector<Word> words;
    lay.placeWords(t, lines[0], true, words);   // "aa bb cc": 6 chars, 5 px extra over 2 gaps
    TEST_ASSERT_EQUAL(3, words.size());
    TEST_ASSERT_EQUAL(0, words[0].x);
    TEST_ASSERT_EQUAL(11 - 2, words[2].x);   // last word flush right
    words.clear();
    lay.placeWords(t, lines[1], true, words);   // last line: not justified
    TEST_ASSERT_EQUAL(0, words[0].x);
}

void test_mid_paragraph_start_detection() {
    Layout lay(mono, {50, 3, 5});
    const char* t = "first\n\nsecond para";
    std::vector<Line> lines;
    lay.page(t, strlen(t), 7, true, true, lines);
    TEST_ASSERT_TRUE(lines[0].paraStart);
    lines.clear();
    lay.page(t, strlen(t), 14, true, true, lines);   // "para", mid paragraph
    TEST_ASSERT_FALSE(lines[0].paraStart);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wraps_words);
    RUN_TEST(test_paragraphs_and_single_newlines);
    RUN_TEST(test_pagination_continues);
    RUN_TEST(test_long_word_hard_break_utf8);
    RUN_TEST(test_stops_at_buffer_end_without_eof);
    RUN_TEST(test_justify);
    RUN_TEST(test_mid_paragraph_start_detection);
    return UNITY_END();
}
