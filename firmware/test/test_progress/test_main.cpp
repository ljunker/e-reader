#include <unity.h>

#include "Progress.h"

void setUp() {}
void tearDown() {}

void test_percent() {
    TEST_ASSERT_EQUAL(0, progress::percent(0, 280));
    TEST_ASSERT_EQUAL(50, progress::percent(50, 101));
    TEST_ASSERT_EQUAL(99, progress::percent(278, 280));
    TEST_ASSERT_EQUAL(100, progress::percent(279, 280));
    TEST_ASSERT_EQUAL(100, progress::percent(0, 1));
    TEST_ASSERT_EQUAL(100, progress::percent(0, 0));
}

void test_locate_basic() {
    std::vector<uint32_t> starts = {2, 10, 25};
    progress::Chapter c = progress::locate(starts, 0, 40);   // cover page, before chapter 1
    TEST_ASSERT_EQUAL(-1, c.index);
    TEST_ASSERT_EQUAL(1, c.pagesLeft);
    c = progress::locate(starts, 2, 40);
    TEST_ASSERT_EQUAL(0, c.index);
    TEST_ASSERT_EQUAL(7, c.pagesLeft);   // pages 3..9
    c = progress::locate(starts, 9, 40);
    TEST_ASSERT_EQUAL(0, c.index);
    TEST_ASSERT_EQUAL(0, c.pagesLeft);   // last page of the chapter
    c = progress::locate(starts, 30, 40);
    TEST_ASSERT_EQUAL(2, c.index);
    TEST_ASSERT_EQUAL(9, c.pagesLeft);   // until the end of the book
}

void test_locate_same_page() {
    // part title and its first chapter on the same page
    std::vector<uint32_t> starts = {0, 5, 5, 12};
    progress::Chapter c = progress::locate(starts, 6, 20);
    TEST_ASSERT_EQUAL(2, c.index);
    TEST_ASSERT_EQUAL(5, c.pagesLeft);
}

void test_locate_empty() {
    progress::Chapter c = progress::locate({}, 3, 10);
    TEST_ASSERT_EQUAL(-1, c.index);
    TEST_ASSERT_EQUAL(6, c.pagesLeft);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_percent);
    RUN_TEST(test_locate_basic);
    RUN_TEST(test_locate_same_page);
    RUN_TEST(test_locate_empty);
    return UNITY_END();
}
