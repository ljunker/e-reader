#include <unity.h>

#include "Cover.h"

void setUp() {}
void tearDown() {}

static image::Gray gradient(int w, int h) {
    image::Gray g;
    g.w = w;
    g.h = h;
    g.px.resize(size_t(w) * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) g.px[size_t(y) * w + x] = uint8_t(x * 255 / (w - 1));
    return g;
}

void test_roundtrip() {
    image::Gray g = gradient(600, 800);
    std::vector<uint8_t> f = cover::encode(12345, &g);
    cover::Info i;
    TEST_ASSERT_TRUE(cover::readInfo(f.data(), 12345, uint32_t(f.size()), i));
    TEST_ASSERT_TRUE(i.present());
    TEST_ASSERT_EQUAL(300, i.fullW);
    TEST_ASSERT_EQUAL(400, i.fullH);
    TEST_ASSERT_EQUAL(42, i.thumbW);
    TEST_ASSERT_EQUAL(56, i.thumbH);
    TEST_ASSERT_EQUAL(f.size(), i.thumbOffset() + i.thumbBytes());
    // gradient: dark on the left, light on the right of the thumbnail
    image::Gray t = cover::unpack(f.data() + i.thumbOffset(), i.thumbW, i.thumbH);
    TEST_ASSERT_EQUAL(0, t.px[0]);
    TEST_ASSERT_EQUAL(255, t.px[i.thumbW - 1]);
}

void test_wide_picture_keeps_aspect() {
    image::Gray g = gradient(800, 200);
    std::vector<uint8_t> f = cover::encode(1, &g);
    cover::Info i;
    TEST_ASSERT_TRUE(cover::readInfo(f.data(), 1, uint32_t(f.size()), i));
    TEST_ASSERT_EQUAL(300, i.fullW);
    TEST_ASSERT_EQUAL(75, i.fullH);
    TEST_ASSERT_EQUAL(42, i.thumbW);
    TEST_ASSERT_TRUE(i.thumbH >= 10 && i.thumbH <= 11);
}

void test_no_cover_marker() {
    std::vector<uint8_t> f = cover::encode(77, nullptr);
    TEST_ASSERT_EQUAL(cover::kHeaderSize, f.size());
    cover::Info i;
    TEST_ASSERT_TRUE(cover::readInfo(f.data(), 77, uint32_t(f.size()), i));
    TEST_ASSERT_FALSE(i.present());
}

void test_rejects_stale_and_short() {
    image::Gray g = gradient(100, 100);
    std::vector<uint8_t> f = cover::encode(500, &g);
    cover::Info i;
    TEST_ASSERT_FALSE(cover::readInfo(f.data(), 501, uint32_t(f.size()), i));       // book replaced
    TEST_ASSERT_FALSE(cover::readInfo(f.data(), 500, uint32_t(f.size()) - 1, i));   // cut off
    f[0] = 'X';
    TEST_ASSERT_FALSE(cover::readInfo(f.data(), 500, uint32_t(f.size()), i));
}

void test_unpack_levels() {
    const uint8_t packed[] = {0x1B};   // 0, 1, 2, 3
    image::Gray g = cover::unpack(packed, 4, 1);
    TEST_ASSERT_EQUAL(0, g.px[0]);
    TEST_ASSERT_EQUAL(85, g.px[1]);
    TEST_ASSERT_EQUAL(170, g.px[2]);
    TEST_ASSERT_EQUAL(255, g.px[3]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_wide_picture_keeps_aspect);
    RUN_TEST(test_no_cover_marker);
    RUN_TEST(test_rejects_stale_and_short);
    RUN_TEST(test_unpack_levels);
    return UNITY_END();
}
