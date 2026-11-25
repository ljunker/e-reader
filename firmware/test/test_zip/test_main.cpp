#include <unity.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../fixtures/deflate_vectors.h"
#include "Inflate.h"
#include "Zip.h"

struct MemSource : io::ByteSource {
    std::vector<uint8_t> d;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
        if (uint64_t(off) + n > d.size()) return false;
        memcpy(dst, d.data() + off, n);
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

static std::string inflateStr(const uint8_t* d, size_t n, bool zlib, zip::InflateResult* r = nullptr) {
    std::vector<uint8_t> out;
    auto res = zip::inflateBuffer(d, n, zlib, out, 1 << 20);
    if (r) *r = res;
    return std::string(out.begin(), out.end());
}

void setUp() {}
void tearDown() {}

void test_inflate_stored() {
    TEST_ASSERT_EQUAL_STRING(std::string((const char*)kShortText, kShortText_len).c_str(),
                             inflateStr(kStored, kStored_len, false).c_str());
}

void test_inflate_fixed() {
    TEST_ASSERT_EQUAL_STRING(std::string((const char*)kShortText, kShortText_len).c_str(),
                             inflateStr(kFixed, kFixed_len, false).c_str());
}

void test_inflate_dynamic_and_zlib() {
    std::string want((const char*)kLongText, kLongText_len);
    TEST_ASSERT_TRUE(want == inflateStr(kDynamic, kDynamic_len, false));
    TEST_ASSERT_TRUE(want == inflateStr(kZlib, kZlib_len, true));
}

void test_inflate_small_input_chunks() {
    // input arrives one byte at a time
    size_t pos = 0;
    std::string out;
    auto r = zip::inflate(
        [&](uint8_t* dst, size_t) -> size_t {
            if (pos >= kDynamic_len) return 0;
            *dst = kDynamic[pos++];
            return 1;
        },
        [&](const uint8_t* d, size_t n) {
            out.append((const char*)d, n);
            return true;
        });
    TEST_ASSERT_EQUAL(int(zip::InflateResult::Ok), int(r));
    TEST_ASSERT_EQUAL(kLongText_len, out.size());
}

void test_inflate_truncated_and_limit() {
    zip::InflateResult r;
    inflateStr(kDynamic, kDynamic_len / 2, false, &r);
    TEST_ASSERT_EQUAL(int(zip::InflateResult::Truncated), int(r));
    std::vector<uint8_t> out;
    r = zip::inflateBuffer(kDynamic, kDynamic_len, false, out, 100);
    TEST_ASSERT_EQUAL(int(zip::InflateResult::Stopped), int(r));
    uint8_t bad[] = {0x07};   // final block with reserved type 3
    inflateStr(bad, 1, false, &r);
    TEST_ASSERT_EQUAL(int(zip::InflateResult::BadData), int(r));
}

void test_archive_reads_entries() {
    MemSource s;
    s.d = loadFile("sample.epub");
    TEST_ASSERT_TRUE(s.d.size() > 0);
    zip::Archive a(s, s.d.size());
    TEST_ASSERT_EQUAL(int(zip::Error::None), int(a.open()));
    const zip::Entry* mt = a.find("mimetype");
    TEST_ASSERT_NOT_NULL(mt);
    TEST_ASSERT_EQUAL(0, mt->method);
    std::string text;
    TEST_ASSERT_EQUAL(int(zip::Error::None), int(a.read(*mt, text)));
    TEST_ASSERT_EQUAL_STRING("application/epub+zip", text.c_str());

    const zip::Entry* ch2 = a.find("oebps/TEXT/ch2.xhtml");   // case-insensitive fallback
    TEST_ASSERT_NOT_NULL(ch2);
    TEST_ASSERT_EQUAL(8, ch2->method);
    TEST_ASSERT_EQUAL(int(zip::Error::None), int(a.read(*ch2, text)));
    TEST_ASSERT_EQUAL(ch2->size, text.size());
    TEST_ASSERT_TRUE(text.find("Absatz 39") != std::string::npos);
    TEST_ASSERT_NULL(a.find("nope"));
}

void test_archive_stream_stop_and_crc() {
    MemSource s;
    s.d = loadFile("sample.epub");
    zip::Archive a(s, s.d.size());
    a.open();
    const zip::Entry* ch2 = a.find("OEBPS/text/ch2.xhtml");
    TEST_ASSERT_EQUAL(int(zip::Error::Stopped), int(a.stream(*ch2, [](const uint8_t*, size_t) { return false; })));
    std::string text;
    TEST_ASSERT_EQUAL(int(zip::Error::TooLarge), int(a.read(*ch2, text, 10)));

    zip::Entry broken = *ch2;
    broken.crc ^= 1;
    TEST_ASSERT_EQUAL(int(zip::Error::Corrupt), int(a.read(broken, text)));
}

void test_not_a_zip() {
    MemSource s;
    s.d.assign(100, 'x');
    zip::Archive a(s, s.d.size());
    TEST_ASSERT_EQUAL(int(zip::Error::NotZip), int(a.open()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_inflate_stored);
    RUN_TEST(test_inflate_fixed);
    RUN_TEST(test_inflate_dynamic_and_zlib);
    RUN_TEST(test_inflate_small_input_chunks);
    RUN_TEST(test_inflate_truncated_and_limit);
    RUN_TEST(test_archive_reads_entries);
    RUN_TEST(test_archive_stream_stop_and_crc);
    RUN_TEST(test_not_a_zip);
    return UNITY_END();
}
