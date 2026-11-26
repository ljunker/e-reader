#include <unity.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "Hyphen.h"

using namespace hyph;

struct MemSource : io::ByteSource {
    std::vector<uint8_t> d;
    bool readAt(uint32_t off, uint8_t* dst, size_t n) override {
        if (uint64_t(off) + n > d.size()) return false;
        memcpy(dst, d.data() + off, n);
        return true;
    }
};

// The pattern files the reader ships (sdcard/hyph, built by `xtconvert hyph`).
static bool loadCard(Hyphenator& h, const char* name) {
    std::string path = std::string(FIXTURES_DIR) + "/../../../sdcard/hyph/" + name;
    MemSource src;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) src.d.insert(src.d.end(), buf, buf + n);
    fclose(f);
    return h.load(src, src.d.size());
}

// The word with '-' at every break, '|' where the word already has a '-'.
static std::string show(const Hyphenator& h, const char* w) {
    uint16_t b[32];
    int nb = h.breaks(w, strlen(w), b, 32);
    std::string out;
    int k = 0;
    for (size_t i = 0; w[i]; i++) {
        if (k < nb && b[k] == i) {
            out += i > 0 && w[i - 1] == '-' ? "|" : "-";
            k++;
        }
        out += w[i];
    }
    return out;
}

static Hyphenator de, en;

void setUp() {}
void tearDown() {}

void test_loads_card_files() {
    TEST_ASSERT_TRUE(loadCard(de, "de.hyp"));
    TEST_ASSERT_TRUE(loadCard(en, "en.hyp"));
}

void test_rejects_bad_files() {
    Hyphenator h;
    MemSource s;
    s.d = {'H', 'Y', 'P', '2', 2, 2, 1, 0};
    TEST_ASSERT_FALSE(h.load(s, s.d.size()));
    s.d = {'H', 'Y', 'P', '1', 2, 2, 3, 0};   // directory missing
    TEST_ASSERT_FALSE(h.load(s, s.d.size()));
    TEST_ASSERT_FALSE(h.loaded());
}

void test_german() {
    TEST_ASSERT_EQUAL_STRING("Sil-ben-tren-nung", show(de, "Silbentrennung").c_str());
    TEST_ASSERT_EQUAL_STRING("Do-nau-dampf-schiff", show(de, "Donaudampfschiff").c_str());
    TEST_ASSERT_EQUAL_STRING("\xC3\x9C" "ber-set-zung", show(de, "\xC3\x9C" "bersetzung").c_str());
    TEST_ASSERT_EQUAL_STRING("Stra-\xC3\x9F" "e", show(de, "Stra\xC3\x9F" "e").c_str());
}

void test_english_and_exceptions() {
    TEST_ASSERT_EQUAL_STRING("hy-phen-ation", show(en, "hyphenation").c_str());
    TEST_ASSERT_EQUAL_STRING("ta-ble", show(en, "table").c_str());
    TEST_ASSERT_EQUAL_STRING("project", show(en, "project").c_str());
    TEST_ASSERT_EQUAL_STRING("Ta-ble", show(en, "Table").c_str());
}

void test_punctuation_and_short_words() {
    TEST_ASSERT_EQUAL_STRING("\xE2\x80\x9ESil-ben-tren-nung,\xE2\x80\x9C",
                             show(de, "\xE2\x80\x9ESilbentrennung,\xE2\x80\x9C").c_str());
    TEST_ASSERT_EQUAL_STRING("(Do-nau).", show(de, "(Donau).").c_str());
    TEST_ASSERT_EQUAL_STRING("ab", show(de, "ab").c_str());
    TEST_ASSERT_EQUAL_STRING("abc", show(en, "abc").c_str());   // left 2 + right 3
    TEST_ASSERT_EQUAL_STRING("1234567", show(de, "1234567").c_str());
}

void test_explicit_hyphens() {
    TEST_ASSERT_EQUAL_STRING("E-|Mail-|Adres-se", show(de, "E-Mail-Adresse").c_str());
    TEST_ASSERT_EQUAL_STRING("--", show(de, "--").c_str());
    TEST_ASSERT_EQUAL_STRING("3-4", show(de, "3-4").c_str());
    Hyphenator none;   // no patterns: only the explicit ones
    TEST_ASSERT_EQUAL_STRING("E-|Mail-|Adresse", show(none, "E-Mail-Adresse").c_str());
    TEST_ASSERT_EQUAL_STRING("Silbentrennung", show(none, "Silbentrennung").c_str());
}

void test_breaks_respects_max_and_unload() {
    uint16_t b[2];
    TEST_ASSERT_EQUAL(2, de.breaks("Silbentrennung", 14, b, 2));
    TEST_ASSERT_EQUAL(3, b[0]);
    TEST_ASSERT_EQUAL(6, b[1]);
    Hyphenator h;
    TEST_ASSERT_TRUE(loadCard(h, "en.hyp"));
    h.unload();
    TEST_ASSERT_FALSE(h.loaded());
    TEST_ASSERT_EQUAL(0, h.breaks("hyphenation", 11, b, 2));
}

void test_language() {
    TEST_ASSERT_EQUAL_STRING("de", normalizeLanguage("de-DE"));
    TEST_ASSERT_EQUAL_STRING("de", normalizeLanguage("DE"));
    TEST_ASSERT_EQUAL_STRING("en", normalizeLanguage("en_GB"));
    TEST_ASSERT_EQUAL_STRING("", normalizeLanguage("deu"));
    TEST_ASSERT_EQUAL_STRING("", normalizeLanguage("fr"));
    TEST_ASSERT_EQUAL_STRING("", normalizeLanguage(""));
    const char* g = "Es war einmal ein K\xC3\xB6nig, der hatte drei T\xC3\xB6" "chter. Die j\xC3\xBCngste war "
                    "die sch\xC3\xB6nste, und das wusste sie. Der K\xC3\xB6nig ist alt und die Zeit ist knapp.";
    const char* e = "It was the best of times, it was the worst of times, it was the age of wisdom, "
                    "and the age of foolishness, and he knew that the end is near.";
    TEST_ASSERT_EQUAL_STRING("de", guessLanguage(g, strlen(g)));
    TEST_ASSERT_EQUAL_STRING("en", guessLanguage(e, strlen(e)));
    TEST_ASSERT_EQUAL_STRING("", guessLanguage("Lorem ipsum dolor sit amet", 26));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_loads_card_files);
    RUN_TEST(test_rejects_bad_files);
    RUN_TEST(test_german);
    RUN_TEST(test_english_and_exceptions);
    RUN_TEST(test_punctuation_and_short_words);
    RUN_TEST(test_explicit_hyphens);
    RUN_TEST(test_breaks_respects_max_and_unload);
    RUN_TEST(test_language);
    return UNITY_END();
}
