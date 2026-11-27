#pragma once
#include <Arduino.h>

// HTTP(S) GET for the online library (and later progress sync): redirects,
// Basic and Digest authentication, the body streamed into a Sink.
namespace http {

// Receives the body. write() returning less than it was given stops the
// transfer (card full, too large ...); error() then says why.
class Sink : public Stream {
public:
    virtual void begin(int32_t total) { (void)total; }   // -1: size unknown
    virtual String error() const { return String(); }
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* buf, size_t n) override = 0;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
};

// User and password are only ever sent to `origin` ("http://host:8080"),
// and only after the server asked for them. `insecure`: accept any HTTPS
// certificate from `origin` (self-signed home servers).
struct Credentials {
    String origin, user, password;
    bool insecure = false;
};

struct Result {
    int status = 0;
    String contentType;
    int32_t size = -1;
    String error;   // for the screen
};

bool get(const String& url, const Credentials& cred, Sink& sink, Result& r, const char* accept = "*/*");

// Forget the cached Digest/Basic challenge (another catalog, Wi-Fi off).
void forget();

}  // namespace http
