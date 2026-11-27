#include <unity.h>

#include <string>

#include "Opds.h"
#include "Transfer.h"

using namespace opds;

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- catalogs

void test_catalog_list() {
    auto c = parseCatalogList(
        "\xEF\xBB\xBF# my catalogs\r\n"
        "Calibre = http://lars:geh%40im@192.168.1.10:8080/opds\r\n"
        "\r\n"
        "Standard Ebooks = https://standardebooks.org/feeds/opds\n"
        "Home NAS = https://nas.local/opds insecure\n"
        "no equals sign\n"
        " = http://x/\n"
        "Bad = ftp://x/\n"
        "Odd = http://x/ please\n");
    TEST_ASSERT_EQUAL(3, int(c.size()));
    TEST_ASSERT_EQUAL_STRING("Calibre", c[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.10:8080/opds", c[0].url.c_str());
    TEST_ASSERT_EQUAL_STRING("lars", c[0].user.c_str());
    TEST_ASSERT_EQUAL_STRING("geh@im", c[0].password.c_str());
    TEST_ASSERT_FALSE(c[0].insecure);
    TEST_ASSERT_EQUAL_STRING("https://standardebooks.org/feeds/opds", c[1].url.c_str());
    TEST_ASSERT_EQUAL_STRING("", c[1].user.c_str());
    TEST_ASSERT_TRUE(c[2].insecure);
}

// ---------------------------------------------------------------- URLs

void test_parse_url() {
    Url u;
    TEST_ASSERT_TRUE(parseUrl("HTTP://Host.Example:8080/a/b?x=1#frag", u));
    TEST_ASSERT_EQUAL_STRING("http", u.scheme.c_str());
    TEST_ASSERT_EQUAL_STRING("host.example", u.host.c_str());
    TEST_ASSERT_EQUAL(8080, u.port);
    TEST_ASSERT_EQUAL_STRING("/a/b?x=1", u.path.c_str());
    TEST_ASSERT_TRUE(parseUrl("https://example.org", u));
    TEST_ASSERT_EQUAL(443, u.port);
    TEST_ASSERT_EQUAL_STRING("/", u.path.c_str());
    TEST_ASSERT_EQUAL_STRING("https://example.org/", formatUrl(u).c_str());
    TEST_ASSERT_TRUE(parseUrl("http://u:p@h:80/x", u));
    TEST_ASSERT_EQUAL_STRING("http://h/x", formatUrl(u).c_str());
    TEST_ASSERT_TRUE(parseUrl("http://[::1]:8080/", u));
    TEST_ASSERT_EQUAL_STRING("[::1]", u.host.c_str());
    TEST_ASSERT_FALSE(parseUrl("ftp://h/", u));
    TEST_ASSERT_FALSE(parseUrl("/relative", u));
    TEST_ASSERT_FALSE(parseUrl("http://h:99999/", u));
    TEST_ASSERT_FALSE(parseUrl("http:///x", u));
}

void test_resolve_url() {
    const char* base = "http://h:8080/opds/navcatalog/4f?offset=25";
    TEST_ASSERT_EQUAL_STRING("http://h:8080/get/epub/12/calibre", resolveUrl(base, "/get/epub/12/calibre").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/x", resolveUrl(base, "x").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/y", resolveUrl(base, "../y").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/4f?offset=50", resolveUrl(base, "?offset=50").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/4f?offset=25", resolveUrl(base, "#top").c_str());
    TEST_ASSERT_EQUAL_STRING("https://cdn.org/b.epub", resolveUrl(base, "https://cdn.org/b.epub").c_str());
    TEST_ASSERT_EQUAL_STRING("http://other/z", resolveUrl(base, "//other/z").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/a?b=1", resolveUrl(base, "/a?b=1#c").c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/", resolveUrl(base, "../../..").c_str());
}

// ---------------------------------------------------------------- feeds

static const char* kNavFeed = R"XML(<?xml version='1.0' encoding='utf-8'?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:dc="http://purl.org/dc/terms/" xmlns:opds="http://opds-spec.org/2010/catalog">
  <id>calibre-all:</id>
  <title>calibre Library</title>
  <author><name>calibre</name></author>
  <link rel="start" href="/opds" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  <link rel="search" href="/opds/search/{searchTerms}" type="application/atom+xml"/>
  <entry>
    <title>By Newest</title>
    <id>calibre-navcatalog:Onewest</id>
    <content type="text">Books sorted by date</content>
    <link href="/opds/navcatalog/4f6e6577657374?library_id=Calibre_Library" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>
  </entry>
  <entry>
    <title>By Author &amp; Series</title>
    <id>calibre-navcatalog:Oauthors</id>
    <link rel="subsection" href="navcatalog/4f617574686f7273" type="application/atom+xml;profile=opds-catalog;kind=navigation"/>
  </entry>
  <entry><title>Just text</title><id>x</id></entry>
</feed>)XML";

void test_navigation_feed() {
    Feed f;
    TEST_ASSERT_TRUE(parseFeed(kNavFeed, "http://h:8080/opds", f));
    TEST_ASSERT_EQUAL_STRING("calibre Library", f.title.c_str());
    TEST_ASSERT_EQUAL(2, int(f.entries.size()));   // the entry without a link is left out
    TEST_ASSERT_EQUAL_STRING("By Newest", f.entries[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/4f6e6577657374?library_id=Calibre_Library", f.entries[0].nav.c_str());
    TEST_ASSERT_EQUAL_STRING("Books sorted by date", f.entries[0].summary.c_str());
    TEST_ASSERT_EQUAL_STRING("", f.entries[0].download.c_str());
    TEST_ASSERT_EQUAL_STRING("By Author & Series", f.entries[1].title.c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/navcatalog/4f617574686f7273", f.entries[1].nav.c_str());
    TEST_ASSERT_EQUAL_STRING("", f.next.c_str());
}

static const char* kBooksFeed = R"XML(<?xml version='1.0' encoding='utf-8'?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <title>Books sorted by date</title>
  <link rel="next" title="Next" href="/opds/navcatalog/4f?offset=2&amp;library_id=L" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>
  <entry>
    <title>Der   Prozess</title>
    <author><name>Franz Kafka</name></author>
    <author><name>Somebody Else</name></author>
    <id>urn:uuid:1</id>
    <link type="application/x-mobipocket-ebook" href="/get/mobi/1/L" rel="http://opds-spec.org/acquisition" length="400000"/>
    <link type="application/epub+zip" href="/get/epub/1/L" rel="http://opds-spec.org/acquisition" length="312345"/>
    <link type="text/plain" href="/get/txt/1/L" rel="http://opds-spec.org/acquisition"/>
    <link type="image/jpeg" href="/get/cover/1/L" rel="http://opds-spec.org/cover"/>
    <link type="application/atom+xml" href="/opds/author/kafka" rel="related"/>
    <content type="xhtml"><div xmlns="http://www.w3.org/1999/xhtml">TAGS: Roman<br/>SERIES: none<p>Josef K. wird eines Morgens verhaftet.</p></div></content>
  </entry>
  <entry>
    <title>Plain</title>
    <link type="text/plain; charset=utf-8" href="plain.txt" rel="http://opds-spec.org/acquisition/open-access"/>
    <summary type="html">&lt;p&gt;Some &lt;b&gt;bold&lt;/b&gt; text&lt;/p&gt;</summary>
  </entry>
  <entry>
    <title>Only Kindle</title>
    <link type="application/x-mobipocket-ebook" href="/get/mobi/3/L" rel="http://opds-spec.org/acquisition"/>
  </entry>
</feed>)XML";

void test_acquisition_feed() {
    Feed f;
    TEST_ASSERT_TRUE(parseFeed(kBooksFeed, "http://h:8080/opds/navcatalog/4f?library_id=L", f));
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/4f?offset=2&library_id=L", f.next.c_str());
    TEST_ASSERT_EQUAL(3, int(f.entries.size()));
    const Entry& a = f.entries[0];
    TEST_ASSERT_EQUAL_STRING("Der Prozess", a.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Franz Kafka", a.author.c_str());
    TEST_ASSERT_EQUAL_STRING("http://h:8080/get/epub/1/L", a.download.c_str());
    TEST_ASSERT_EQUAL_STRING("application/epub+zip", a.type.c_str());
    TEST_ASSERT_EQUAL(312345, int(a.size));
    TEST_ASSERT_EQUAL_STRING("", a.nav.c_str());   // "related" is not a way in
    TEST_ASSERT_FALSE(a.otherFormat);
    TEST_ASSERT_EQUAL_STRING("TAGS: Roman SERIES: none Josef K. wird eines Morgens verhaftet.", a.summary.c_str());

    const Entry& b = f.entries[1];
    TEST_ASSERT_EQUAL_STRING("http://h:8080/opds/navcatalog/plain.txt", b.download.c_str());
    TEST_ASSERT_EQUAL_STRING("text/plain", b.type.c_str());
    TEST_ASSERT_EQUAL_STRING("Some bold text", b.summary.c_str());

    const Entry& c = f.entries[2];
    TEST_ASSERT_TRUE(c.otherFormat);
    TEST_ASSERT_EQUAL_STRING("", c.download.c_str());
}

void test_feed_in_small_pieces() {
    std::string xml = kBooksFeed;
    FeedReader r("http://h/opds");
    for (size_t i = 0; i < xml.size(); i += 7) r.feed(xml.data() + i, std::min<size_t>(7, xml.size() - i));
    Feed f;
    TEST_ASSERT_TRUE(r.finish(f));
    TEST_ASSERT_EQUAL(3, int(f.entries.size()));
    TEST_ASSERT_EQUAL_STRING("Der Prozess", f.entries[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("http://h/get/epub/1/L", f.entries[0].download.c_str());
}

void test_not_a_feed() {
    Feed f;
    TEST_ASSERT_FALSE(parseFeed("<html><body><title>Login</title></body></html>", "http://h/", f));
    TEST_ASSERT_FALSE(parseFeed("{\"metadata\":{}}", "http://h/", f));
    TEST_ASSERT_FALSE(parseFeed("", "http://h/", f));
}

void test_many_entries_capped() {
    std::string xml = "<feed><title>Big</title>";
    for (int i = 0; i < 150; i++)
        xml += "<entry><title>B" + std::to_string(i) +
               "</title><link rel='http://opds-spec.org/acquisition' type='application/epub+zip' href='/b'/></entry>";
    xml += "</feed>";
    Feed f;
    TEST_ASSERT_TRUE(parseFeed(xml, "http://h/", f));
    TEST_ASSERT_EQUAL(int(kMaxEntries), int(f.entries.size()));
    TEST_ASSERT_TRUE(f.truncated);
}

void test_long_summary_cut_on_utf8() {
    std::string text;
    for (int i = 0; i < 400; i++) text += "\xc3\xa4";   // ä, 2 bytes
    Feed f;
    parseFeed("<feed><entry><title>T</title><summary>" + text +
                  "</summary><link rel='http://opds-spec.org/acquisition' type='application/epub+zip' href='/b'/></entry></feed>",
              "http://h/", f);
    TEST_ASSERT_EQUAL(1, int(f.entries.size()));
    TEST_ASSERT_TRUE(f.entries[0].summary.size() <= kMaxText);
    TEST_ASSERT_EQUAL(0, int(f.entries[0].summary.size() % 2));
}

// ---------------------------------------------------------------- names

void test_book_file_names() {
    TEST_ASSERT_EQUAL_STRING("Der Prozess - Franz Kafka.epub", bookFileName("Der Prozess", "Franz Kafka", "application/epub+zip").c_str());
    TEST_ASSERT_EQUAL_STRING("Dune - Messiah.txt", bookFileName("Dune: Messiah", "", "text/plain").c_str());
    TEST_ASSERT_EQUAL_STRING("AC-DC 'Live' - Who.epub", bookFileName("AC/DC \"Live\"?", "Who", "").c_str());
    TEST_ASSERT_EQUAL_STRING("Book.epub", bookFileName("...", "", "").c_str());
    TEST_ASSERT_EQUAL_STRING("x. y - z.epub", bookFileName("..x.. y", "z", "").c_str());
    std::string longTitle;
    for (int i = 0; i < 100; i++) longTitle += "\xc3\xb6";   // ö
    std::string n = bookFileName(longTitle, "Author", "application/epub+zip");
    TEST_ASSERT_TRUE(n.size() <= 120);
    // every name made here is one the upload check accepts
    for (const char* t : {"Der Prozess", "a:b", "C:\\x\\y", "<script>", "tab\there", "...", longTitle.c_str()})
        TEST_ASSERT_EQUAL_STRING(bookFileName(t, "A", "").c_str(), transfer::safeBookName(bookFileName(t, "A", "")).c_str());
}

// ---------------------------------------------------------------- auth

void test_md5_and_base64() {
    TEST_ASSERT_EQUAL_STRING("d41d8cd98f00b204e9800998ecf8427e", md5Hex("").c_str());
    TEST_ASSERT_EQUAL_STRING("9e107d9d372bb6826bd81d3542a419d6", md5Hex("The quick brown fox jumps over the lazy dog").c_str());
    TEST_ASSERT_EQUAL_STRING("57edf4a22be3c955ac49da2e2107b67a",
                             md5Hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890").c_str());
    TEST_ASSERT_EQUAL_STRING("QWxhZGRpbjpvcGVuIHNlc2FtZQ==", base64("Aladdin:open sesame").c_str());
    TEST_ASSERT_EQUAL_STRING("YQ==", base64("a").c_str());
    TEST_ASSERT_EQUAL_STRING("YWI=", base64("ab").c_str());
    TEST_ASSERT_EQUAL_STRING("YWJj", base64("abc").c_str());
}

void test_basic_challenge() {
    Challenge c;
    TEST_ASSERT_TRUE(parseChallenge("Basic realm=\"calibre\"", c));
    TEST_ASSERT_EQUAL(Challenge::Basic, c.scheme);
    TEST_ASSERT_EQUAL_STRING("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
                             authorization(c, "Aladdin", "open sesame", "GET", "/", 1, "x").c_str());
    TEST_ASSERT_FALSE(parseChallenge("Bearer realm=\"x\"", c));
    TEST_ASSERT_FALSE(parseChallenge("", c));
}

// RFC 2617, section 3.5
void test_digest_rfc2617() {
    Challenge c;
    TEST_ASSERT_TRUE(parseChallenge(
        "Digest realm=\"testrealm@host.com\", qop=\"auth,auth-int\", nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
        "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"", c));
    TEST_ASSERT_EQUAL(Challenge::Digest, c.scheme);
    TEST_ASSERT_TRUE(c.qopAuth);
    std::string h = authorization(c, "Mufasa", "Circle Of Life", "GET", "/dir/index.html", 1, "0a4f113b");
    TEST_ASSERT_TRUE(h.find("response=\"6629fae49393a05397450978507c4ef1\"") != std::string::npos);
    TEST_ASSERT_TRUE(h.find("nc=00000001") != std::string::npos);
    TEST_ASSERT_TRUE(h.find("opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"") != std::string::npos);
    TEST_ASSERT_TRUE(h.find("uri=\"/dir/index.html\"") != std::string::npos);
}

void test_digest_preferred_and_stale() {
    Challenge c;
    TEST_ASSERT_TRUE(parseChallenge("Basic realm=\"a\", Digest realm=\"b\", nonce=\"n\", algorithm=MD5, stale=TRUE", c));
    TEST_ASSERT_EQUAL(Challenge::Digest, c.scheme);
    TEST_ASSERT_EQUAL_STRING("b", c.realm.c_str());
    TEST_ASSERT_TRUE(c.stale);
    TEST_ASSERT_FALSE(c.qopAuth);
    // no qop: the RFC 2069 form
    std::string h = authorization(c, "u", "p", "GET", "/x", 1, "c");
    std::string ha1 = md5Hex("u:b:p"), ha2 = md5Hex("GET:/x");
    TEST_ASSERT_TRUE(h.find("response=\"" + md5Hex(ha1 + ":n:" + ha2) + "\"") != std::string::npos);
    TEST_ASSERT_TRUE(h.find("qop") == std::string::npos);
    // unknown algorithm: fall back to Basic if offered
    TEST_ASSERT_TRUE(parseChallenge("Digest realm=\"b\", nonce=\"n\", algorithm=SHA-256, Basic realm=\"a\"", c));
    TEST_ASSERT_EQUAL(Challenge::Basic, c.scheme);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_list);
    RUN_TEST(test_parse_url);
    RUN_TEST(test_resolve_url);
    RUN_TEST(test_navigation_feed);
    RUN_TEST(test_acquisition_feed);
    RUN_TEST(test_feed_in_small_pieces);
    RUN_TEST(test_not_a_feed);
    RUN_TEST(test_many_entries_capped);
    RUN_TEST(test_long_summary_cut_on_utf8);
    RUN_TEST(test_book_file_names);
    RUN_TEST(test_md5_and_base64);
    RUN_TEST(test_basic_challenge);
    RUN_TEST(test_digest_rfc2617);
    RUN_TEST(test_digest_preferred_and_stale);
    return UNITY_END();
}
