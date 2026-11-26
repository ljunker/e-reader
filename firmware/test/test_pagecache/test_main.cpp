#include <unity.h>

#include "PageCache.h"

void setUp() {}
void tearDown() {}

void test_empty() {
    pagecache::Slots s;
    TEST_ASSERT_EQUAL(-1, s.find(0));
    uint32_t p = 99;
    TEST_ASSERT_TRUE(s.wanted(0, 10, p));
    TEST_ASSERT_EQUAL(1, p);   // next page first
}

void test_wanted_order() {
    pagecache::Slots s;
    s.claim(5, 5);   // on screen
    uint32_t p = 0;
    TEST_ASSERT_TRUE(s.wanted(5, 10, p));
    TEST_ASSERT_EQUAL(6, p);
    s.claim(6, 5);
    TEST_ASSERT_TRUE(s.wanted(5, 10, p));
    TEST_ASSERT_EQUAL(4, p);   // then the previous one
    s.claim(4, 5);
    TEST_ASSERT_FALSE(s.wanted(5, 10, p));
}

void test_book_edges() {
    pagecache::Slots s;
    uint32_t p = 0;
    TEST_ASSERT_FALSE(s.wanted(0, 1, p));   // one-page book
    s.claim(9, 9);
    TEST_ASSERT_TRUE(s.wanted(9, 10, p));   // last page: only the previous one
    TEST_ASSERT_EQUAL(8, p);
    s.claim(8, 9);
    TEST_ASSERT_FALSE(s.wanted(9, 10, p));
}

void test_turning_forward_evicts_the_oldest() {
    pagecache::Slots s;
    s.claim(4, 5);
    s.claim(5, 5);
    s.claim(6, 5);
    // turned to 6: page 7 replaces page 4, 5 stays for PREV
    int slot4 = s.find(4);
    TEST_ASSERT_EQUAL(slot4, s.claim(7, 6));
    TEST_ASSERT_EQUAL(-1, s.find(4));
    TEST_ASSERT_TRUE(s.find(5) >= 0);
    TEST_ASSERT_TRUE(s.find(6) >= 0);
    TEST_ASSERT_TRUE(s.find(7) >= 0);
}

void test_claim_same_page_reuses_slot() {
    pagecache::Slots s;
    int a = s.claim(3, 3);
    TEST_ASSERT_EQUAL(a, s.claim(3, 3));
    s.claim(4, 3);
    s.claim(2, 3);
    TEST_ASSERT_EQUAL(a, s.find(3));
}

void test_release_and_clear() {
    pagecache::Slots s;
    int a = s.claim(1, 1);
    s.release(a);
    TEST_ASSERT_EQUAL(-1, s.find(1));
    s.claim(1, 1);
    s.claim(2, 1);
    s.clear();
    TEST_ASSERT_EQUAL(-1, s.find(1));
    TEST_ASSERT_EQUAL(-1, s.find(2));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty);
    RUN_TEST(test_wanted_order);
    RUN_TEST(test_book_edges);
    RUN_TEST(test_turning_forward_evicts_the_oldest);
    RUN_TEST(test_claim_same_page_reuses_slot);
    RUN_TEST(test_release_and_clear);
    return UNITY_END();
}
