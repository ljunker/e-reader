#include "Catalog.h"

#include "Http.h"
#include "Storage.h"
#include "Transfer.h"
#include "config.h"

namespace catalog {

static http::Credentials cred;

bool configured() { return storage::mounted() && storage::fs().exists(OPDS_FILE); }

bool readList(std::vector<opds::Catalog>& out, String& error) {
    out.clear();
    File f = storage::fs().open(OPDS_FILE, FILE_READ);
    if (!f) {
        error = "No " OPDS_FILE " on the card.\n\nOne catalog per line:\nName = http://host:8080/opds";
        return false;
    }
    String text = f.readString();
    f.close();
    out = opds::parseCatalogList(text.c_str());
    if (out.empty()) {
        error = OPDS_FILE " lists no catalog.\n\nOne catalog per line:\nName = http://host:8080/opds";
        return false;
    }
    return true;
}

void use(const opds::Catalog& c) {
    opds::Url u;
    opds::parseUrl(c.url, u);
    cred.origin = opds::origin(u).c_str();
    cred.user = c.user.c_str();
    cred.password = c.password.c_str();
    cred.insecure = c.insecure;
    http::forget();
}

// ---------------------------------------------------------------- feeds

namespace {
class FeedSink : public http::Sink {
public:
    explicit FeedSink(const std::string& url) : reader_(url) {}
    using http::Sink::write;
    size_t write(const uint8_t* buf, size_t n) override {
        if (total_ + n > kMax) {
            tooBig_ = true;
            return 0;
        }
        total_ += n;
        reader_.feed(reinterpret_cast<const char*>(buf), n);
        return n;
    }
    String error() const override { return tooBig_ ? "The catalog page is too large." : String(); }
    bool finish(opds::Feed& f) { return reader_.finish(f); }

private:
    static constexpr size_t kMax = 4 * 1024 * 1024;
    opds::FeedReader reader_;
    size_t total_ = 0;
    bool tooBig_ = false;
};
}  // namespace

bool load(const std::string& url, opds::Feed& out, String& error) {
    FeedSink sink(url);
    http::Result r;
    if (!http::get(url.c_str(), cred, sink, r, "application/atom+xml, application/xml;q=0.9, */*;q=0.5")) {
        error = r.error;
        return false;
    }
    if (!sink.finish(out)) {
        String type = r.contentType;
        type.toLowerCase();
        error = type.indexOf("json") >= 0 ? "This is an OPDS 2 (JSON) catalog; the reader only knows OPDS 1 (Atom)."
                                          : "This address does not lead to an OPDS catalog.";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- books

String pathFor(const opds::Entry& e) {
    std::string name = transfer::safeBookName(opds::bookFileName(e.title, e.author, e.type));
    return String(BOOKS_DIR) + "/" + (name.empty() ? "Book.epub" : name.c_str());
}

bool onCard(const opds::Entry& e) { return !e.download.empty() && storage::fs().exists(pathFor(e)); }

namespace {
// Writes through a PSRAM buffer, since the network hands over ~1.4 KB at a
// time and small SD writes are slow.
class FileSink : public http::Sink {
public:
    FileSink(File& f, bool epub, const Progress& p) : file_(f), epub_(epub), progress_(p) {}
    ~FileSink() { free(buf_); }
    using http::Sink::write;

    void begin(int32_t total) override {
        total_ = total;
        if (!buf_) buf_ = static_cast<uint8_t*>(ps_malloc(kBuf));
        if (progress_) progress_(0, total_);
    }
    size_t write(const uint8_t* p, size_t n) override {
        if (!buf_) { error_ = "Out of memory."; return 0; }
        if (done_ == 0 && epub_ && n >= 2 && (p[0] != 'P' || p[1] != 'K')) {
            error_ = "The server sent something else than an EPUB (a login page?).";
            return 0;
        }
        size_t left = n;
        while (left) {
            size_t k = std::min(left, kBuf - used_);
            memcpy(buf_ + used_, p, k);
            used_ += k;
            p += k;
            left -= k;
            if (used_ == kBuf && !save()) return 0;
        }
        done_ += n;
        if (progress_) progress_(done_, total_);
        return n;
    }
    bool save() {   // the buffer to the file
        if (!used_) return true;
        bool ok = file_.write(buf_, used_) == used_;
        used_ = 0;
        if (!ok) error_ = "Cannot write to the card (full?).";
        return ok;
    }
    String error() const override { return error_; }
    uint32_t done() const { return done_; }

private:
    static constexpr size_t kBuf = 32 * 1024;
    File& file_;
    bool epub_;
    Progress progress_;
    uint8_t* buf_ = nullptr;
    size_t used_ = 0;
    uint32_t done_ = 0;
    int32_t total_ = -1;
    String error_;
};
}  // namespace

bool download(const opds::Entry& e, String& path, String& error, const Progress& progress) {
    if (e.download.empty()) {
        error = "This book is not offered as EPUB or text.";
        return false;
    }
    auto& fs = storage::fs();
    if (!fs.exists(BOOKS_DIR)) fs.mkdir(BOOKS_DIR);
    path = pathFor(e);
    String part = path + ".part";
    File f = fs.open(part, FILE_WRITE);
    if (!f) {
        error = "Cannot write to the card.";
        return false;
    }
    FileSink sink(f, e.type == "application/epub+zip", progress);
    http::Result r;
    bool ok = http::get(e.download.c_str(), cred, sink, r);
    if (ok && !sink.save()) {
        ok = false;
        r.error = sink.error();
    }
    if (ok && r.size >= 0 && sink.done() != uint32_t(r.size)) {
        ok = false;
        r.error = "The download broke off.";
    }
    f.close();
    if (!ok) {
        fs.remove(part);
        error = r.error;
        return false;
    }
    if (fs.exists(path)) fs.remove(path);
    if (!fs.rename(part, path)) {
        fs.remove(part);
        error = "Cannot rename the file on the card.";
        return false;
    }
    storage::forgetBook(path);   // a replaced book must lay out again
    return true;
}

}  // namespace catalog
