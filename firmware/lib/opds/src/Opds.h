#pragma once
// OPDS catalogs (Calibre content server and others): the catalog list in
// /opds.txt, Atom feed parsing, URLs, file names for downloaded books and
// HTTP Basic/Digest authentication. No Arduino code, so the host tests can
// check it.
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace opds {

// ---------------------------------------------------------------- catalogs

struct Catalog {
    std::string name;
    std::string url;        // without user:password
    std::string user, password;
    bool insecure = false;  // HTTPS without checking the certificate
};

// Parses /opds.txt: one catalog per line, "Name = URL", optionally followed
// by the word "insecure" (self-signed HTTPS). User and password go into the
// URL: "http://user:secret@192.168.1.10:8080/opds" (percent-encode '@', ':'
// and '/' in them). Blank lines and lines starting with '#' are skipped, as
// are lines without '=' or with a URL that is not http(s). CRLF and a UTF-8
// BOM are fine.
std::vector<Catalog> parseCatalogList(const std::string& text);

// ---------------------------------------------------------------- URLs

struct Url {
    std::string scheme;     // "http" or "https"
    std::string host;
    uint16_t port = 0;      // 80 / 443 if not given
    std::string path;       // with the query, at least "/"
    std::string user, password;   // percent-decoded
};

// Absolute http(s) URLs only; false for anything else.
bool parseUrl(const std::string& url, Url& out);
// The URL again, without user and password.
std::string formatUrl(const Url& u);
// "http://host:8080" — the part credentials are tied to.
std::string origin(const Url& u);
// A link in a feed: absolute, "//host/x", "/x", "x", "?q" or "#f".
std::string resolveUrl(const std::string& base, const std::string& href);
std::string percentDecode(const std::string& s);

// ---------------------------------------------------------------- feeds

struct Entry {
    std::string title, author, summary;
    std::string nav;        // sub-catalog to open, or ""
    std::string download;   // the book file to fetch, or ""
    std::string type;       // MIME type of `download`
    uint32_t size = 0;      // bytes, if the feed says (link length=)
    // Has a book link, but in a format the reader cannot show (MOBI, PDF ...)
    bool otherFormat = false;
};

struct Feed {
    std::string title;
    std::string next;       // next page of this feed, or ""
    std::vector<Entry> entries;
    bool truncated = false; // more than kMaxEntries entries: the rest was dropped
};

// Small strings live in internal RAM, so a page is kept to ~50 KB.
constexpr size_t kMaxEntries = 60;
constexpr size_t kMaxText = 400;     // summaries are cut to this many bytes

// Builds a Feed from Atom XML fed in pieces; links are resolved against
// `baseUrl`. Prefers EPUB over plain text; entries with neither are kept
// with otherFormat set, so the list does not look shorter than the catalog.
class FeedReader {
public:
    explicit FeedReader(const std::string& baseUrl);
    ~FeedReader();
    void feed(const char* data, size_t n);
    // Finishes parsing; false if this was not an Atom feed.
    bool finish(Feed& out);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool parseFeed(const std::string& xml, const std::string& baseUrl, Feed& out);

// ---------------------------------------------------------------- books

// File name for a downloaded book: "Title - Author.epub". Characters FAT does
// not allow become spaces or dashes, the name is cut to fit 120 bytes (on a
// UTF-8 boundary) and a name that ends up empty becomes "Book".
// `type` picks the extension: text/plain -> .txt, anything else -> .epub.
std::string bookFileName(const std::string& title, const std::string& author, const std::string& type);

// ---------------------------------------------------------------- auth

struct Challenge {
    enum Scheme { None, Basic, Digest } scheme = None;
    std::string realm, nonce, opaque, algorithm;
    bool qopAuth = false;   // server offers qop="auth"
    bool stale = false;
};

// One WWW-Authenticate header. Prefers Digest when the header offers both.
bool parseChallenge(const std::string& header, Challenge& out);
// Value for the Authorization header. `nc` counts the requests made with
// this nonce (1, 2, ...); `cnonce` is a random client string.
std::string authorization(const Challenge& c, const std::string& user, const std::string& password,
                          const std::string& method, const std::string& uri, uint32_t nc, const std::string& cnonce);

std::string base64(const std::string& s);
std::string md5Hex(const std::string& s);

}  // namespace opds
