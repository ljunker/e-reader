#include "Opds.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Xml.h"

namespace opds {

// ---------------------------------------------------------------- helpers

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace(uint8_t(s[a]))) a++;
    while (b > a && isspace(uint8_t(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

static std::string lower(std::string s) {
    for (auto& c : s) c = char(tolower(uint8_t(c)));
    return s;
}

static bool startsWith(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

// Runs of whitespace become one space; no space at either end.
static std::string squeeze(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    bool space = false;
    for (char c : s) {
        if (isspace(uint8_t(c))) {
            space = !o.empty();
            continue;
        }
        if (space) o += ' ';
        space = false;
        o += c;
    }
    return o;
}

// Cut to at most n bytes without splitting a UTF-8 character.
static void cutUtf8(std::string& s, size_t n) {
    if (s.size() <= n) return;
    while (n > 0 && (uint8_t(s[n]) & 0xC0) == 0x80) n--;
    s.resize(n);
}

// ---------------------------------------------------------------- URLs

std::string percentDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit(uint8_t(s[i + 1])) && isxdigit(uint8_t(s[i + 2]))) {
            o += char(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            o += s[i];
        }
    }
    return o;
}

bool parseUrl(const std::string& in, Url& u) {
    std::string url = trim(in);
    size_t sep = url.find("://");
    if (sep == std::string::npos) return false;
    u = Url();
    u.scheme = lower(url.substr(0, sep));
    if (u.scheme != "http" && u.scheme != "https") return false;
    size_t start = sep + 3;
    size_t end = url.find_first_of("/?#", start);
    if (end == std::string::npos) end = url.size();
    std::string auth = url.substr(start, end - start);
    size_t at = auth.rfind('@');
    if (at != std::string::npos) {
        std::string info = auth.substr(0, at);
        auth = auth.substr(at + 1);
        size_t colon = info.find(':');
        u.user = percentDecode(info.substr(0, colon));
        if (colon != std::string::npos) u.password = percentDecode(info.substr(colon + 1));
    }
    std::string portText;
    if (!auth.empty() && auth[0] == '[') {   // IPv6 literal
        size_t close = auth.find(']');
        if (close == std::string::npos) return false;
        u.host = auth.substr(0, close + 1);
        if (close + 1 < auth.size()) {
            if (auth[close + 1] != ':') return false;
            portText = auth.substr(close + 2);
        }
    } else {
        size_t colon = auth.find(':');
        u.host = auth.substr(0, colon);
        if (colon != std::string::npos) portText = auth.substr(colon + 1);
    }
    u.host = lower(u.host);
    if (u.host.empty()) return false;
    if (!portText.empty()) {
        long p = 0;
        for (char c : portText) {
            if (!isdigit(uint8_t(c))) return false;
            p = p * 10 + (c - '0');
            if (p > 65535) return false;
        }
        if (p == 0) return false;
        u.port = uint16_t(p);
    } else {
        u.port = u.scheme == "https" ? 443 : 80;
    }
    std::string rest = url.substr(end);
    size_t hash = rest.find('#');
    if (hash != std::string::npos) rest.resize(hash);
    if (rest.empty() || rest[0] != '/') rest = "/" + rest;
    u.path = rest;
    return true;
}

std::string origin(const Url& u) {
    std::string o = u.scheme + "://" + u.host;
    bool standard = (u.scheme == "http" && u.port == 80) || (u.scheme == "https" && u.port == 443);
    if (!standard) o += ":" + std::to_string(u.port);
    return o;
}

std::string formatUrl(const Url& u) { return origin(u) + u.path; }

// RFC 3986 5.2.4, on a path without query.
static std::string removeDots(const std::string& path) {
    std::vector<std::string> out;
    size_t i = 1;   // path starts with '/'
    bool dirEnd = false;
    while (i <= path.size()) {
        size_t slash = path.find('/', i);
        std::string seg = path.substr(i, slash == std::string::npos ? std::string::npos : slash - i);
        dirEnd = false;
        if (seg == "..") {
            if (!out.empty()) out.pop_back();
            dirEnd = true;
        } else if (seg == ".") {
            dirEnd = true;
        } else {
            out.push_back(seg);
        }
        if (slash == std::string::npos) break;
        i = slash + 1;
    }
    std::string o;
    for (const auto& s : out) o += "/" + s;
    if (dirEnd || o.empty()) o += "/";
    return o;
}

std::string resolveUrl(const std::string& base, const std::string& hrefIn) {
    std::string href = trim(hrefIn);
    Url b;
    if (!parseUrl(base, b)) return href;
    // has a scheme of its own ("http:", "mailto:" ...)
    size_t colon = href.find(':');
    if (colon != std::string::npos && colon > 0 && href.find_first_of("/?#") > colon) {
        bool scheme = isalpha(uint8_t(href[0]));
        for (size_t i = 0; i < colon && scheme; i++) scheme = isalnum(uint8_t(href[i])) || strchr("+-.", href[i]);
        if (scheme) {
            Url u;
            return parseUrl(href, u) ? formatUrl(u) : href;
        }
    }
    if (startsWith(href, "//")) {
        Url u;
        return parseUrl(b.scheme + ":" + href, u) ? formatUrl(u) : href;
    }
    std::string path = b.path, query;
    size_t q = path.find('?');
    if (q != std::string::npos) {
        query = path.substr(q);
        path.resize(q);
    }
    if (href.empty() || href[0] == '#') return origin(b) + path + query;
    if (href[0] == '?') return origin(b) + path + href;
    std::string hq;
    q = href.find_first_of("?#");
    if (q != std::string::npos) {
        hq = href.substr(q);
        href.resize(q);
        size_t h = hq.find('#');
        if (h != std::string::npos) hq.resize(h);
    }
    std::string merged = href[0] == '/' ? href : path.substr(0, path.rfind('/') + 1) + href;
    return origin(b) + removeDots(merged) + hq;
}

// ---------------------------------------------------------------- catalogs

std::vector<Catalog> parseCatalogList(const std::string& text) {
    std::vector<Catalog> out;
    size_t start = 0;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = trim(text.substr(start, nl - start));
        start = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        Catalog c;
        c.name = trim(line.substr(0, eq));
        std::string rest = trim(line.substr(eq + 1));
        size_t sp = rest.find_first_of(" \t");
        std::string url = rest.substr(0, sp);
        if (sp != std::string::npos) {
            std::string flags = lower(trim(rest.substr(sp)));
            if (flags == "insecure") c.insecure = true;
            else continue;   // unknown words: rather skip than guess
        }
        Url u;
        if (c.name.empty() || !parseUrl(url, u)) continue;
        c.user = u.user;
        c.password = u.password;
        c.url = formatUrl(u);
        out.push_back(c);
    }
    return out;
}

// ---------------------------------------------------------------- feeds

static std::string stripTags(const std::string& html) {
    std::string o;
    bool tag = false;
    for (char c : html) {
        if (c == '<') {
            tag = true;
            o += ' ';
        } else if (c == '>') {
            tag = false;
        } else if (!tag) {
            o += c;
        }
    }
    return epub::decodeEntities(o);
}

static std::string mimeOf(const std::string* type) {
    if (!type) return "";
    std::string t = lower(*type);
    size_t semi = t.find(';');
    return trim(semi == std::string::npos ? t : t.substr(0, semi));
}

struct FeedReader::Impl : epub::XmlHandler {
    explicit Impl(const std::string& base) : base(base), parser(*this) {}

    std::string base;
    epub::XmlParser parser;
    Feed feed;
    bool isAtom = false;
    std::vector<std::string> stack;   // open elements

    bool inEntry = false;
    Entry cur;
    int rank = 0;                      // of cur.download: 2 EPUB, 1 text
    std::string summaryType;
    enum class Field { None, FeedTitle, Title, Author, Summary } field = Field::None;
    size_t fieldDepth = 0;
    std::string buf;   // text of the open field

    const std::string& parent() const {
        static const std::string none;
        return stack.empty() ? none : stack.back();
    }

    void link(const epub::Attrs& a) {
        const std::string* href = epub::findAttr(a, "href");
        if (!href || href->empty()) return;
        const std::string* relAttr = epub::findAttr(a, "rel");
        std::string rel = relAttr ? lower(trim(*relAttr)) : std::string();
        std::string type = mimeOf(epub::findAttr(a, "type"));
        std::string url = resolveUrl(base, *href);
        if (!inEntry) {
            if (rel == "next" && feed.next.empty()) feed.next = url;
            return;
        }
        if (startsWith(rel, "http://opds-spec.org/acquisition")) {
            int r = type == "application/epub+zip" ? 2 : type == "text/plain" ? 1 : 0;
            if (r == 0) {
                if (rank == 0) cur.otherFormat = true;
                return;
            }
            if (r <= rank) return;
            rank = r;
            cur.download = url;
            cur.type = type;
            cur.otherFormat = false;
            const std::string* len = epub::findAttr(a, "length");
            cur.size = len ? uint32_t(strtoul(len->c_str(), nullptr, 10)) : 0;
            return;
        }
        bool atom = type == "application/atom+xml";
        static const char* const notNav[] = {"self", "up", "start", "related", "alternate", "search",
                                             "http://opds-spec.org/facet", "http://opds-spec.org/shelf"};
        for (const char* n : notNav)
            if (rel == n) atom = false;
        if (atom && cur.nav.empty()) cur.nav = url;
    }

    void begin(Field f) {
        field = f;
        fieldDepth = stack.size();
        buf.clear();
    }

    void startTag(const std::string& name, const epub::Attrs& a, bool selfClosing) override {
        if (stack.empty()) isAtom = name == "feed";
        if (field == Field::Summary && summaryType == "xhtml") {
            if (name == "p" || name == "div" || name == "br" || name == "li") buf += ' ';
        } else if (name == "entry" && !inEntry) {
            inEntry = true;
            cur = Entry();
            rank = 0;
        } else if (name == "link") {
            link(a);
        } else if (field == Field::None) {
            const std::string& p = parent();
            if (name == "title" && p == "feed") begin(Field::FeedTitle);
            else if (name == "title" && p == "entry") begin(Field::Title);
            else if (name == "name" && p == "author" && inEntry && cur.author.empty()) begin(Field::Author);
            else if ((name == "summary" || name == "content") && p == "entry" && cur.summary.empty()) {
                const std::string* t = epub::findAttr(a, "type");
                summaryType = t ? lower(*t) : "text";
                begin(Field::Summary);
            }
        }
        if (selfClosing) {
            if (field != Field::None && fieldDepth == stack.size()) endField();
            return;
        }
        stack.push_back(name);
    }

    void endField() {
        std::string t = field == Field::Summary && summaryType == "html" ? stripTags(buf) : buf;
        t = squeeze(t);
        switch (field) {
            case Field::FeedTitle: feed.title = t; break;
            case Field::Title: cur.title = t; break;
            case Field::Author: cur.author = t; break;
            case Field::Summary: cutUtf8(t, kMaxText); cur.summary = t; break;
            case Field::None: break;
        }
        field = Field::None;
        buf.clear();
    }

    void endTag(const std::string& name) override {
        // tolerate stray end tags: close up to the matching element
        size_t i = stack.size();
        while (i > 0 && stack[i - 1] != name) i--;
        if (i == 0) return;
        stack.resize(i - 1);
        if (field != Field::None && stack.size() <= fieldDepth) endField();
        if (name == "entry" && inEntry) {
            inEntry = false;
            if (cur.title.empty()) cur.title = "(untitled)";
            if (!cur.download.empty()) cur.nav.clear();
            if (cur.download.empty() && cur.nav.empty() && !cur.otherFormat) return;   // nothing to do with it
            if (feed.entries.size() >= kMaxEntries) {
                feed.truncated = true;
                return;
            }
            feed.entries.push_back(std::move(cur));
        }
    }

    void text(const std::string& s) override {
        if (field != Field::None && buf.size() < 4 * kMaxText) buf += s;
    }
};

FeedReader::FeedReader(const std::string& baseUrl) : impl_(new Impl(baseUrl)) {}
FeedReader::~FeedReader() = default;

void FeedReader::feed(const char* data, size_t n) { impl_->parser.feed(data, n); }

bool FeedReader::finish(Feed& out) {
    impl_->parser.finish();
    out = std::move(impl_->feed);
    return impl_->isAtom;
}

bool parseFeed(const std::string& xml, const std::string& baseUrl, Feed& out) {
    FeedReader r(baseUrl);
    r.feed(xml.data(), xml.size());
    return r.finish(out);
}

// ---------------------------------------------------------------- books

std::string bookFileName(const std::string& title, const std::string& author, const std::string& type) {
    std::string raw = squeeze(title);
    std::string a = squeeze(author);
    if (!a.empty()) raw += (raw.empty() ? "" : " - ") + a;
    std::string s;
    for (char c : raw) {
        uint8_t u = uint8_t(c);
        if (u < 0x20 || u == 0x7F) s += ' ';
        else if (c == ':') s += " -";
        else if (c == '/' || c == '\\' || c == '|') s += '-';
        else if (c == '"') s += '\'';
        else if (c == '*' || c == '?' || c == '<' || c == '>') continue;
        else s += c;
    }
    s = squeeze(s);
    for (size_t p; (p = s.find("..")) != std::string::npos;) s.erase(p, 1);
    while (!s.empty() && (s[0] == '.' || s[0] == ' ' || s[0] == '-')) s.erase(0, 1);
    const char* ext = lower(type) == "text/plain" ? ".txt" : ".epub";
    cutUtf8(s, 120 - strlen(ext));
    while (!s.empty() && (s.back() == ' ' || s.back() == '.' || s.back() == '-')) s.pop_back();
    if (s.empty()) s = "Book";
    return s + ext;
}

// ---------------------------------------------------------------- auth

bool parseChallenge(const std::string& h, Challenge& out) {
    Challenge basic, digest;
    Challenge* cur = nullptr;
    size_t i = 0, n = h.size();
    while (i < n) {
        while (i < n && (isspace(uint8_t(h[i])) || h[i] == ',')) i++;
        size_t t = i;
        while (i < n && !isspace(uint8_t(h[i])) && h[i] != '=' && h[i] != ',') i++;
        std::string token = h.substr(t, i - t);
        if (token.empty()) break;
        size_t j = i;
        while (j < n && isspace(uint8_t(h[j]))) j++;
        if (j < n && h[j] == '=') {   // a parameter
            i = j + 1;
            while (i < n && isspace(uint8_t(h[i]))) i++;
            std::string value;
            if (i < n && h[i] == '"') {
                for (i++; i < n && h[i] != '"'; i++) {
                    if (h[i] == '\\' && i + 1 < n) i++;
                    value += h[i];
                }
                i++;
            } else {
                size_t v = i;
                while (i < n && h[i] != ',' && !isspace(uint8_t(h[i]))) i++;
                value = h.substr(v, i - v);
            }
            if (!cur) continue;
            std::string key = lower(token);
            if (key == "realm") cur->realm = value;
            else if (key == "nonce") cur->nonce = value;
            else if (key == "opaque") cur->opaque = value;
            else if (key == "algorithm") cur->algorithm = value;
            else if (key == "stale") cur->stale = lower(value) == "true";
            else if (key == "qop") {
                std::string q = lower(value);
                size_t p = 0;
                while (p <= q.size()) {
                    size_t c = q.find(',', p);
                    if (trim(q.substr(p, c == std::string::npos ? std::string::npos : c - p)) == "auth") cur->qopAuth = true;
                    if (c == std::string::npos) break;
                    p = c + 1;
                }
            }
        } else {   // a scheme name
            std::string s = lower(token);
            if (s == "basic") { basic.scheme = Challenge::Basic; cur = &basic; }
            else if (s == "digest") { digest.scheme = Challenge::Digest; cur = &digest; }
            else cur = nullptr;
        }
    }
    if (digest.scheme == Challenge::Digest && !digest.nonce.empty()) {
        std::string alg = lower(digest.algorithm);
        if (alg.empty() || alg == "md5" || alg == "md5-sess") {
            out = digest;
            return true;
        }
    }
    if (basic.scheme == Challenge::Basic) {
        out = basic;
        return true;
    }
    return false;
}

static std::string quoted(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o + "\"";
}

std::string authorization(const Challenge& c, const std::string& user, const std::string& password,
                          const std::string& method, const std::string& uri, uint32_t nc, const std::string& cnonce) {
    if (c.scheme == Challenge::Basic) return "Basic " + base64(user + ":" + password);
    if (c.scheme != Challenge::Digest) return "";
    std::string ha1 = md5Hex(user + ":" + c.realm + ":" + password);
    bool sess = lower(c.algorithm) == "md5-sess";
    if (sess) ha1 = md5Hex(ha1 + ":" + c.nonce + ":" + cnonce);
    std::string ha2 = md5Hex(method + ":" + uri);
    char ncText[9];
    snprintf(ncText, sizeof ncText, "%08x", unsigned(nc));
    std::string response = c.qopAuth ? md5Hex(ha1 + ":" + c.nonce + ":" + ncText + ":" + cnonce + ":auth:" + ha2)
                                     : md5Hex(ha1 + ":" + c.nonce + ":" + ha2);
    std::string h = "Digest username=" + quoted(user) + ", realm=" + quoted(c.realm) + ", nonce=" + quoted(c.nonce) +
                    ", uri=" + quoted(uri);
    if (!c.algorithm.empty()) h += ", algorithm=" + c.algorithm;
    h += ", response=" + quoted(response);
    if (!c.opaque.empty()) h += ", opaque=" + quoted(c.opaque);
    if (c.qopAuth) h += ", qop=auth, nc=" + std::string(ncText) + ", cnonce=" + quoted(cnonce);
    return h;
}

std::string base64(const std::string& s) {
    static const char* abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    size_t i = 0;
    for (; i + 2 < s.size(); i += 3) {
        uint32_t v = uint8_t(s[i]) << 16 | uint8_t(s[i + 1]) << 8 | uint8_t(s[i + 2]);
        o += abc[v >> 18];
        o += abc[(v >> 12) & 63];
        o += abc[(v >> 6) & 63];
        o += abc[v & 63];
    }
    if (i < s.size()) {
        uint32_t v = uint8_t(s[i]) << 16 | (i + 1 < s.size() ? uint8_t(s[i + 1]) << 8 : 0);
        o += abc[v >> 18];
        o += abc[(v >> 12) & 63];
        o += i + 1 < s.size() ? abc[(v >> 6) & 63] : '=';
        o += '=';
    }
    return o;
}

// MD5 (RFC 1321), only for Digest authentication.
std::string md5Hex(const std::string& msg) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const uint8_t R[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    std::string m = msg;
    uint64_t bits = uint64_t(msg.size()) * 8;
    m += char(0x80);
    while (m.size() % 64 != 56) m += char(0);
    for (int i = 0; i < 8; i++) m += char(bits >> (8 * i));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++)
            w[i] = uint32_t(uint8_t(m[off + 4 * i])) | uint32_t(uint8_t(m[off + 4 * i + 1])) << 8 |
                   uint32_t(uint8_t(m[off + 4 * i + 2])) << 16 | uint32_t(uint8_t(m[off + 4 * i + 3])) << 24;
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t s = R[(i / 16) * 4 + i % 4];
            uint32_t t = a + f + K[i] + w[g];
            a = d;
            d = c;
            c = b;
            b = b + ((t << s) | (t >> (32 - s)));
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    char out[33];
    for (int i = 0; i < 16; i++) snprintf(out + 2 * i, 3, "%02x", unsigned(h[i / 4] >> (8 * (i % 4))) & 0xFF);
    return out;
}

}  // namespace opds
