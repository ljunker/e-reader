#include <unity.h>

#include <string>

#include "Transfer.h"

using transfer::parseWifiList;
using transfer::safeBookName;

void setUp() {}
void tearDown() {}

void test_wifi_several_networks() {
    auto n = parseWifiList(
        "# home networks\n"
        "Zuhause = geheim123\n"
        "\n"
        "  Büro WLAN=pass=word#1  \n"
        "Gartenhaus =\n");
    TEST_ASSERT_EQUAL(3, int(n.size()));
    TEST_ASSERT_EQUAL_STRING("Zuhause", n[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("geheim123", n[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Büro WLAN", n[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("pass=word#1", n[1].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Gartenhaus", n[2].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", n[2].password.c_str());
}

void test_wifi_crlf_bom_and_bad_lines() {
    auto n = parseWifiList(
        "\xEF\xBB\xBFOne = a\r\n"
        "no equals sign\r\n"
        " = nossid\r\n"
        "123456789012345678901234567890123 = ssid too long\r\n"
        "Two = 1234567890123456789012345678901234567890123456789012345678901234\r\n"   // 64 chars
        "Three = b");
    TEST_ASSERT_EQUAL(2, int(n.size()));
    TEST_ASSERT_EQUAL_STRING("One", n[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("a", n[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Three", n[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("b", n[1].password.c_str());
}

void test_wifi_empty() {
    TEST_ASSERT_EQUAL(0, int(parseWifiList("").size()));
    TEST_ASSERT_EQUAL(0, int(parseWifiList("# only a comment\n\n").size()));
}

void test_book_names_accepted() {
    TEST_ASSERT_EQUAL_STRING("Moby Dick.epub", safeBookName("Moby Dick.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("paper.XTP", safeBookName("paper.XTP").c_str());
    TEST_ASSERT_EQUAL_STRING("b.txt", safeBookName("some/dir/b.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("b.txt", safeBookName("C:\\Users\\x\\b.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("Märchen.epub", safeBookName("Märchen.epub").c_str());
}

void test_book_names_rejected() {
    TEST_ASSERT_EQUAL_STRING("", safeBookName("").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("..").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("x.epub/..").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName(".hidden.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("._mac.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("a..b.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("virus.exe").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("book.epub.zip").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("dir/").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("a\nb.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName("what?.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("", safeBookName(std::string(118, 'a') + ".epub").c_str());
}

void test_book_name_folder_traversal_stripped() {
    // The folder part is dropped, so "../" cannot leave /books.
    TEST_ASSERT_EQUAL_STRING("x.epub", safeBookName("../x.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("x.epub", safeBookName("..\\..\\x.epub").c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_several_networks);
    RUN_TEST(test_wifi_crlf_bom_and_bad_lines);
    RUN_TEST(test_wifi_empty);
    RUN_TEST(test_book_names_accepted);
    RUN_TEST(test_book_names_rejected);
    RUN_TEST(test_book_name_folder_traversal_stripped);
    return UNITY_END();
}
