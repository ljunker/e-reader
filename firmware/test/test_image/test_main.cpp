#include <unity.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "Image.h"

static std::vector<uint8_t> loadFile(const char* name) {
    std::string path = std::string(FIXTURES_DIR) + "/images/" + name;
    std::vector<uint8_t> out;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return out;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return out;
}

static int at(const image::Gray& g, int x, int y) { return g.px[size_t(y) * g.w + x]; }

void setUp() {}
void tearDown() {}

void test_png_gray() {
    for (const char* name : {"gray.png", "gray16.png", "palette.png"}) {
        auto d = loadFile(name);
        image::Gray g;
        TEST_ASSERT_TRUE_MESSAGE(image::decodePng(d.data(), d.size(), g), name);
        TEST_ASSERT_EQUAL(40, g.w);
        TEST_ASSERT_EQUAL(20, g.h);
        TEST_ASSERT_TRUE_MESSAGE(at(g, 5, 5) < 10, name);      // left half black
        TEST_ASSERT_TRUE_MESSAGE(at(g, 30, 5) > 245, name);    // right half white
    }
}

void test_png_rgb_and_alpha() {
    auto d = loadFile("rgb.png");
    image::Gray g;
    TEST_ASSERT_TRUE(image::decodePng(d.data(), d.size(), g));
    TEST_ASSERT_INT_WITHIN(2, 76, at(g, 3, 3));   // pure red

    d = loadFile("rgba.png");   // transparent outside a black band
    TEST_ASSERT_TRUE(image::decodePng(d.data(), d.size(), g));
    TEST_ASSERT_EQUAL(100, g.w);
    TEST_ASSERT_EQUAL(255, at(g, 50, 2));
    TEST_ASSERT_EQUAL(0, at(g, 50, 20));
}

void test_jpeg_scaled_decode() {
    auto d = loadFile("photo.jpg");   // 640 × 480 gradient
    image::Gray g;
    TEST_ASSERT_TRUE(image::decodeJpeg(d.data(), d.size(), g, 1000, 1000));
    TEST_ASSERT_EQUAL(640, g.w);
    TEST_ASSERT_EQUAL(480, g.h);
    // fitting into 150 wide only needs 1/4 scale (160 × 120)
    TEST_ASSERT_TRUE(image::decodeJpeg(d.data(), d.size(), g, 150, 400));
    TEST_ASSERT_EQUAL(160, g.w);
    TEST_ASSERT_EQUAL(120, g.h);
    // gradient: darker top-left than bottom-right
    TEST_ASSERT_TRUE(at(g, 2, 2) < at(g, 157, 117));
}

void test_unsupported() {
    auto d = loadFile("progressive.jpg");
    image::Gray g;
    TEST_ASSERT_FALSE(image::decode(d.data(), d.size(), g, 300, 400));
    const uint8_t gif[] = {'G', 'I', 'F', '8', '9', 'a'};
    TEST_ASSERT_EQUAL(int(image::Format::Gif), int(image::sniff(gif, sizeof gif)));
    TEST_ASSERT_FALSE(image::decode(gif, sizeof gif, g, 300, 400));
    TEST_ASSERT_FALSE(image::decode(gif, 2, g, 300, 400));
}

void test_fit_keeps_aspect_and_never_enlarges() {
    image::Gray g;
    g.w = 600;
    g.h = 200;
    g.px.assign(600 * 200, 0);
    for (int y = 0; y < 200; y++)
        for (int x = 300; x < 600; x++) g.px[y * 600 + x] = 255;
    image::Gray f = image::fit(g, 300, 400);
    TEST_ASSERT_EQUAL(300, f.w);
    TEST_ASSERT_EQUAL(100, f.h);
    TEST_ASSERT_EQUAL(0, at(f, 10, 50));
    TEST_ASSERT_EQUAL(255, at(f, 290, 50));
    image::Gray small = image::fit(f, 1000, 1000);
    TEST_ASSERT_EQUAL(300, small.w);
}

void test_pack_levels_and_dither() {
    image::Gray g;
    g.w = 8;
    g.h = 1;
    g.px = {0, 0, 0, 0, 255, 255, 255, 255};
    auto p = image::pack2bpp(g);
    TEST_ASSERT_EQUAL(2, p.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, p[1]);

    // mid gray averages out to roughly mid level
    g.w = 64;
    g.h = 64;
    g.px.assign(64 * 64, 128);
    p = image::pack2bpp(g);
    long sum = 0;
    for (int i = 0; i < 64 * 64; i++) sum += (p[i >> 2] >> (6 - 2 * (i & 3))) & 3;
    double mean = double(sum) / (64 * 64) * 85;
    TEST_ASSERT_TRUE(mean > 118 && mean < 138);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_png_gray);
    RUN_TEST(test_png_rgb_and_alpha);
    RUN_TEST(test_jpeg_scaled_decode);
    RUN_TEST(test_unsupported);
    RUN_TEST(test_fit_keeps_aspect_and_never_enlarges);
    RUN_TEST(test_pack_levels_and_dither);
    return UNITY_END();
}
