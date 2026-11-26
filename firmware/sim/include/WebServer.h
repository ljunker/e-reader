#pragma once
// Web server of the simulator. Requests come from the browser page (a service
// worker forwards the upload page's fetches); handlers run inside
// handleClient() exactly as on the device.
#include <Arduino.h>

#include <functional>
#include <vector>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

typedef struct {
    HTTPUploadStatus status;
    String filename;
    String name;
    String type;
    size_t totalSize;
    size_t currentSize;
    uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUpload;

class WebServer {
public:
    typedef std::function<void(void)> THandlerFunction;

    explicit WebServer(int port = 80);
    ~WebServer();

    void begin();
    void stop();
    void handleClient();

    void on(const String& uri, THandlerFunction fn) { on(uri, HTTP_ANY, fn); }
    void on(const String& uri, HTTPMethod method, THandlerFunction fn) { on(uri, method, fn, nullptr); }
    void on(const String& uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);
    void onNotFound(THandlerFunction fn) { notFound_ = fn; }

    void send(int code, const char* content_type, const String& content);
    void send(int code, const char* content_type, const char* content) { send(code, content_type, String(content)); }
    void send(int code, const String& content_type, const String& content) { send(code, content_type.c_str(), content); }
    void send(int code) { send(code, "text/plain", String()); }

    String arg(const String& name) const;
    bool hasArg(const String& name) const;
    String uri() const { return uri_; }
    HTTPMethod method() const { return method_; }
    HTTPUpload& upload() { return upload_; }

private:
    struct Route {
        String uri;
        HTTPMethod method;
        THandlerFunction fn, ufn;
    };
    std::vector<Route> routes_;
    THandlerFunction notFound_;
    bool listening_ = false;
    String uri_;
    HTTPMethod method_ = HTTP_GET;
    std::vector<std::pair<String, String>> args_;
    HTTPUpload upload_{};
    bool responded_ = false;
    int requestId_ = 0;
};
