#pragma once
// HTTP client of the simulator: the subset of arduino-esp32's HTTPClient the
// firmware uses. GET() fetches through the simulator page and its server's
// /proxy (sim/serve.py), so any http(s) URL works without CORS; redirects and
// 401 come back to the firmware as they would on the device.
#include <Arduino.h>

#include <utility>
#include <vector>

#include "NetworkClient.h"

#define HTTPC_ERROR_CONNECTION_REFUSED (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED (-2)
#define HTTPC_ERROR_NOT_CONNECTED (-4)
#define HTTPC_ERROR_CONNECTION_LOST (-5)
#define HTTPC_ERROR_STREAM_WRITE (-10)
#define HTTPC_ERROR_READ_TIMEOUT (-11)

class HTTPClient {
public:
    ~HTTPClient() { end(); }
    bool begin(NetworkClient& client, String url);
    void end();
    void setReuse(bool) {}
    void setConnectTimeout(int32_t) {}
    void setTimeout(uint16_t) {}
    void setUserAgent(const String& ua) { addHeader("User-Agent", ua); }
    void addHeader(const String& name, const String& value, bool first = false, bool replace = true);
    void collectHeaders(const char* keys[], size_t n) { (void)keys; (void)n; }   // all are kept
    int GET();
    String header(const char* name);
    int getSize();
    int writeToStream(Stream* stream);
    static String errorToString(int error);

private:
    String url_;
    std::vector<std::pair<String, String>> headers_;
    int id_ = 0;   // response held by the page
};
