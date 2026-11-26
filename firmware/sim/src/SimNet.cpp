// Wi-Fi, mDNS and the web server. The page's service worker turns the upload
// page's requests into sim_http_request() calls; the server answers them in
// handleClient(), from the firmware's own loop.
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <emscripten.h>

#include <deque>
#include <string>
#include <vector>

#include "Sim.h"

static bool inRange = true;
static bool connected = false;
static wifi_mode_t wifiMode = WIFI_OFF;
static std::string ssid;

bool sim::wifiInRange() { return inRange; }

extern "C" EMSCRIPTEN_KEEPALIVE void sim_set_wifi_range(int on) {
    inRange = on;
    if (!on) connected = false;
}

WiFiClass WiFi;
MDNSResponder MDNS;

String IPAddress::toString() const {
    char s[16];
    snprintf(s, sizeof s, "%u.%u.%u.%u", b_[0], b_[1], b_[2], b_[3]);
    return s;
}

bool WiFiClass::mode(wifi_mode_t m) {
    wifiMode = m;
    if (m == WIFI_OFF) connected = false;
    return true;
}

bool WiFiClass::disconnect(bool, bool) {
    connected = false;
    return true;
}

String WiFiClass::SSID() const { return connected ? String(ssid.c_str()) : String(); }
IPAddress WiFiClass::localIP() const { return connected ? IPAddress(192, 168, 1, 42) : IPAddress(); }
wl_status_t WiFiClass::status() const { return connected ? WL_CONNECTED : WL_DISCONNECTED; }

bool WiFiMulti::addAP(const char* s, const char*) {
    ssids_.push_back(s ? s : "");
    return true;
}

// Joins the first listed network when "in range" (it is the strongest one);
// otherwise scans for the whole timeout and finds nothing, like the device.
uint8_t WiFiMulti::run(uint32_t connectTimeout) {
    if (connected) return WL_CONNECTED;
    if (wifiMode == WIFI_OFF || ssids_.empty()) return WL_DISCONNECTED;
    if (!inRange) {
        delay(connectTimeout);
        return WL_DISCONNECTED;
    }
    delay(1500);
    ssid = ssids_.front();
    connected = true;
    return WL_CONNECTED;
}

// ---------------------------------------------------------------- requests

namespace {
struct Request {
    int id;
    HTTPMethod method;
    std::string uri, query, fileName;
    std::vector<uint8_t> body;
};
std::deque<Request> queue;
WebServer* listening = nullptr;

String urlDecode(const std::string& s) {
    String out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '+') out += ' ';
        else if (c == '%' && i + 2 < s.size()) {
            out += char(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else out += c;
    }
    return out;
}
}  // namespace

// Returns 0 when no server listens (the page answers "unreachable" itself).
extern "C" EMSCRIPTEN_KEEPALIVE int sim_http_request(int id, int post, const char* uri, const char* query,
                                                     const char* fileName, const uint8_t* body, int len) {
    if (!listening || !connected) return 0;
    Request r;
    r.id = id;
    r.method = post ? HTTP_POST : HTTP_GET;
    r.uri = uri;
    r.query = query ? query : "";
    r.fileName = fileName ? fileName : "";
    if (body && len > 0) r.body.assign(body, body + len);
    queue.push_back(std::move(r));
    return 1;
}

WebServer::WebServer(int port) { (void)port; }

WebServer::~WebServer() { stop(); }

void WebServer::begin() {
    listening_ = true;
    listening = this;
    js_web_listening(1);
}

void WebServer::stop() {
    if (!listening_) return;
    listening_ = false;
    if (listening == this) listening = nullptr;
    for (const auto& r : queue) js_http_response(r.id, 503, "text/plain", "", 0);
    queue.clear();
    js_web_listening(0);
}

void WebServer::on(const String& uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn) {
    routes_.push_back({uri, method, fn, ufn});
}

void WebServer::send(int code, const char* contentType, const String& content) {
    if (responded_) return;
    responded_ = true;
    js_http_response(requestId_, code, contentType ? contentType : "text/plain", content.c_str(), int(content.length()));
}

String WebServer::arg(const String& name) const {
    for (const auto& a : args_)
        if (a.first == name) return a.second;
    return String();
}

bool WebServer::hasArg(const String& name) const {
    for (const auto& a : args_)
        if (a.first == name) return true;
    return false;
}

// One request per call, like the real server's one client per loop.
void WebServer::handleClient() {
    if (!listening_ || queue.empty()) return;
    Request r = std::move(queue.front());
    queue.pop_front();

    requestId_ = r.id;
    responded_ = false;
    uri_ = r.uri.c_str();
    method_ = r.method;
    args_.clear();
    size_t start = 0;
    while (start < r.query.size()) {
        size_t amp = r.query.find('&', start);
        std::string pair = r.query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        size_t eq = pair.find('=');
        args_.push_back({urlDecode(pair.substr(0, eq)), eq == std::string::npos ? String() : urlDecode(pair.substr(eq + 1))});
        if (amp == std::string::npos) break;
        start = amp + 1;
    }

    const Route* route = nullptr;
    for (const auto& rt : routes_)
        if (rt.uri == uri_ && (rt.method == HTTP_ANY || rt.method == method_)) { route = &rt; break; }

    if (!route) {
        if (notFound_) notFound_();
    } else {
        if (route->ufn && !r.fileName.empty()) {
            // multipart upload in the web server's 1436-byte pieces
            upload_.filename = r.fileName.c_str();
            upload_.name = "file";
            upload_.type = "application/octet-stream";
            upload_.totalSize = 0;
            upload_.currentSize = 0;
            upload_.status = UPLOAD_FILE_START;
            route->ufn();
            for (size_t off = 0; off < r.body.size(); off += HTTP_UPLOAD_BUFLEN) {
                size_t n = std::min(r.body.size() - off, size_t(HTTP_UPLOAD_BUFLEN));
                memcpy(upload_.buf, r.body.data() + off, n);
                upload_.currentSize = n;
                upload_.totalSize += n;
                upload_.status = UPLOAD_FILE_WRITE;
                route->ufn();
            }
            upload_.currentSize = 0;
            upload_.status = UPLOAD_FILE_END;
            route->ufn();
        }
        route->fn();
    }
    if (!responded_) send(500, "text/plain", "no response");
    sim::fsChanged();
}
