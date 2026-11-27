// HTTP client of the simulator (see sim/include/HTTPClient.h). The page keeps
// each response until end(); the firmware reads it in pieces.
#include <HTTPClient.h>
#include <WiFi.h>
#include <emscripten.h>

#include "Sim.h"

// Returns a response id (> 0) or a negative HTTPC_ERROR_*.
EM_ASYNC_JS(int, js_http_get, (const char* url, const char* headers), {
    return await Module.sim.httpGet(UTF8ToString(url), UTF8ToString(headers));
});
EM_JS(int, js_http_status, (int id), { return Module.sim.httpResult(id).status; });
EM_JS(int, js_http_size, (int id), { return Module.sim.httpResult(id).size; });
EM_JS(char*, js_http_header, (int id, const char* name), {
    const v = Module.sim.httpResult(id).headers.get(UTF8ToString(name));
    return v === null ? 0 : stringToNewUTF8(v);
});
EM_JS(int, js_http_body, (int id, int offset, uint8_t* buf, int len), {
    const body = Module.sim.httpResult(id).body;
    const n = Math.max(0, Math.min(len, body.length - offset));
    HEAPU8.set(body.subarray(offset, offset + n), buf);
    return n;
});
EM_JS(void, js_http_free, (int id), { Module.sim.httpFree(id); });

bool HTTPClient::begin(NetworkClient& client, String url) {
    (void)client;
    end();
    url_ = url;
    headers_.clear();
    return true;
}

void HTTPClient::end() {
    if (id_ > 0) js_http_free(id_);
    id_ = 0;
}

void HTTPClient::addHeader(const String& name, const String& value, bool first, bool replace) {
    (void)first;
    for (auto& h : headers_)
        if (replace && h.first.equalsIgnoreCase(name)) {
            h.second = value;
            return;
        }
    headers_.push_back({name, value});
}

int HTTPClient::GET() {
    end();
    if (WiFi.status() != WL_CONNECTED || !sim::wifiInRange()) return HTTPC_ERROR_CONNECTION_REFUSED;
    String lines;
    for (const auto& h : headers_) lines += h.first + "\n" + h.second + "\n";
    int r = js_http_get(url_.c_str(), lines.c_str());
    if (r < 0) return r;
    id_ = r;
    return js_http_status(id_);
}

String HTTPClient::header(const char* name) {
    if (id_ <= 0) return String();
    char* v = js_http_header(id_, name);
    if (!v) return String();
    String s(v);
    free(v);
    return s;
}

int HTTPClient::getSize() { return id_ > 0 ? js_http_size(id_) : -1; }

int HTTPClient::writeToStream(Stream* stream) {
    if (id_ <= 0) return HTTPC_ERROR_NOT_CONNECTED;
    static uint8_t buf[4096];
    int total = 0;
    for (;;) {
        int n = js_http_body(id_, total, buf, sizeof buf);
        if (n <= 0) break;
        if (int(stream->write(buf, n)) != n) return HTTPC_ERROR_STREAM_WRITE;
        total += n;
        delay(0);   // let the page breathe, like the network would
    }
    return total;
}

String HTTPClient::errorToString(int error) {
    switch (error) {
        case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
        case HTTPC_ERROR_NOT_CONNECTED: return "not connected";
        case HTTPC_ERROR_CONNECTION_LOST: return "connection lost";
        case HTTPC_ERROR_STREAM_WRITE: return "Stream write error";
        case HTTPC_ERROR_READ_TIMEOUT: return "read Timeout";
        default: return String("error ") + error;
    }
}
