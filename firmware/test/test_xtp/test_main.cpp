#include <unity.h>

#include <string.h>
#include <vector>

#include "Xtp.h"

struct MemSource : xtp::ByteSource {
    std::vector<uint8_t> d;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
        if (off + n > d.size()) return false;
        memcpy(dst, d.data() + off, n);
        return true;
    }
};

static void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 0xff); v.push_back(x >> 8); }
static void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 0xff); }

// 8x2 image, 2 pages: page 0 all white (RLE run), page 1 a literal pattern.
static MemSource makeFile() {
    MemSource s;
    auto& v = s.d;
    v.insert(v.end(), {'X', 'T', 'P', '1'});
    put16(v, 1); put16(v, 8); put16(v, 2);
    v.push_back(2); v.push_back(1);
    put32(v, 2); put32(v, 64);
    const char* title = "Test";
    for (int i = 0; i < 44; i++) v.push_back(i < 4 ? title[i] : 0);
    std::vector<uint8_t> p0 = {128 + 2, 0xff};            // run of 4 bytes 0xff
    std::vector<uint8_t> p1 = {3, 0x1b, 0x1b, 0x00, 0xe4}; // 4 literal bytes
    uint32_t off = 64 + 16;
    put32(v, off); put32(v, p0.size());
    put32(v, off + p0.size()); put32(v, p1.size());
    v.insert(v.end(), p0.begin(), p0.end());
    v.insert(v.end(), p1.begin(), p1.end());
    return s;
}

void setUp() {}
void tearDown() {}

void test_header() {
    MemSource s = makeFile();
    xtp::Reader r(s);
    TEST_ASSERT_TRUE(r.open() == xtp::Error::None);
    TEST_ASSERT_EQUAL(8, r.header().width);
    TEST_ASSERT_EQUAL(2, r.header().height);
    TEST_ASSERT_EQUAL(2, r.header().pageCount);
    TEST_ASSERT_EQUAL_STRING("Test", r.header().title);
    TEST_ASSERT_EQUAL(4, r.pageBytes());
}

void test_pages() {
    MemSource s = makeFile();
    xtp::Reader r(s);
    r.open();
    uint8_t out[4], scratch[16];
    TEST_ASSERT_TRUE(r.readPage(0, out, scratch, sizeof scratch) == xtp::Error::None);
    for (uint8_t b : out) TEST_ASSERT_EQUAL_HEX8(0xff, b);
    TEST_ASSERT_TRUE(r.readPage(1, out, scratch, sizeof scratch) == xtp::Error::None);
    TEST_ASSERT_EQUAL_HEX8(0x1b, out[0]);
    TEST_ASSERT_EQUAL(0, xtp::pixel(out, 8, 0, 0));
    TEST_ASSERT_EQUAL(1, xtp::pixel(out, 8, 1, 0));
    TEST_ASSERT_EQUAL(2, xtp::pixel(out, 8, 2, 0));
    TEST_ASSERT_EQUAL(3, xtp::pixel(out, 8, 3, 0));
    TEST_ASSERT_EQUAL(0, xtp::pixel(out, 8, 0, 1));   // byte 2 = 0x00
    TEST_ASSERT_EQUAL(3, xtp::pixel(out, 8, 4, 1));   // byte 3 = 0xe4 = 11 10 01 00
    TEST_ASSERT_EQUAL(0, xtp::pixel(out, 8, 7, 1));
    TEST_ASSERT_TRUE(r.readPage(2, out, scratch, sizeof scratch) == xtp::Error::BadPage);
    uint32_t m;
    r.maxCompressedSize(m);
    TEST_ASSERT_EQUAL(5, m);
}

void test_bad_magic() {
    MemSource s = makeFile();
    s.d[0] = 'Y';
    xtp::Reader r(s);
    TEST_ASSERT_TRUE(r.open() == xtp::Error::BadMagic);
}

void test_rle_overflow() {
    uint8_t in[] = {200, 0xaa};  // run of 74
    uint8_t out[10];
    TEST_ASSERT_EQUAL(-1, xtp::rleDecode(in, 2, out, sizeof out));
    uint8_t trunc[] = {5, 1, 2};
    TEST_ASSERT_EQUAL(-1, xtp::rleDecode(trunc, 3, out, sizeof out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_header);
    RUN_TEST(test_pages);
    RUN_TEST(test_bad_magic);
    RUN_TEST(test_rle_overflow);
    return UNITY_END();
}
