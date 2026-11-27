#include "Http.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <memory>

#include "Opds.h"

namespace http {

// The last challenge, so later requests send the answer right away instead
// of being refused once each.
static opds::Challenge challenge;
static String challengeOrigin;
static uint32_t nonceCount = 0;

void forget() {
    challenge = opds::Challenge();
    challengeOrigin = "";
    nonceCount = 0;
}

static std::string cnonce() {
    static uint32_t n = 0;
    uint32_t x = uint32_t(micros()) * 2654435761u ^ uint32_t(millis()) << 7 ^ ++n * 40503u;
    char s[9];
    snprintf(s, sizeof s, "%08lx", (unsigned long)x);
    return s;
}

static String describe(int code) {
    switch (code) {
        case 401: return "The server wants a user and password (add them to the URL in opds.txt).";
        case 403: return "The server refused access (403).";
        case 404: return "Not found on the server (404).";
        default: return "The server answered " + String(code) + ".";
    }
}

bool get(const String& url0, const Credentials& cred, Sink& sink, Result& r, const char* accept) {
    std::string url = url0.c_str();
    bool askedForAuth = false;
    for (int hop = 0; hop < 6; hop++) {
        opds::Url u;
        if (!opds::parseUrl(url, u)) {
            r.error = "Not a web address: " + String(url.c_str());
            return false;
        }
        String org = opds::origin(u).c_str();
        bool ours = cred.user.length() && org == cred.origin;

        std::unique_ptr<NetworkClient> client;
        if (u.scheme == "https") {
            auto* s = new NetworkClientSecure();
            if (cred.insecure && org == cred.origin) s->setInsecure();
            else s->useBuiltinCACertBundle();
            client.reset(s);
        } else {
            client.reset(new NetworkClient());
        }
        HTTPClient h;   // declared after the client: goes first
        h.setReuse(false);
        h.setConnectTimeout(8000);
        h.setTimeout(15000);
        h.setUserAgent("ereader");
        if (!h.begin(*client, String(opds::formatUrl(u).c_str()))) {
            r.error = "Cannot open " + org;
            return false;
        }
        h.addHeader("Accept", accept);
        if (ours && challenge.scheme != opds::Challenge::None && challengeOrigin == org)
            h.addHeader("Authorization", opds::authorization(challenge, cred.user.c_str(), cred.password.c_str(), "GET",
                                                             u.path, ++nonceCount, cnonce())
                                             .c_str());
        const char* keys[] = {"WWW-Authenticate", "Location", "Content-Type"};
        h.collectHeaders(keys, 3);

        int code = h.GET();
        if (code < 0) {
            r.error = "No answer from " + org + " (" + HTTPClient::errorToString(code) + ")";
            h.end();
            return false;
        }
        r.status = code;
        if (code == 401 && ours && !askedForAuth) {
            opds::Challenge c;
            if (opds::parseChallenge(h.header("WWW-Authenticate").c_str(), c)) {
                challenge = c;
                challengeOrigin = org;
                nonceCount = 0;
                askedForAuth = true;
                h.end();
                hop--;   // same request again, now with the answer
                continue;
            }
        }
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            String loc = h.header("Location");
            h.end();
            if (loc.isEmpty()) {
                r.error = "The server sent a redirect without an address.";
                return false;
            }
            url = opds::resolveUrl(url, loc.c_str());
            continue;
        }
        if (code != 200) {
            r.error = code == 401 && askedForAuth ? "Wrong user or password." : describe(code);
            h.end();
            return false;
        }
        r.contentType = h.header("Content-Type");
        r.size = h.getSize();
        sink.begin(r.size);
        int n = h.writeToStream(&sink);
        h.end();
        if (n < 0) {
            r.error = sink.error();
            if (r.error.isEmpty()) r.error = "The connection broke off (" + HTTPClient::errorToString(n) + ").";
            return false;
        }
        return true;
    }
    r.error = "Too many redirects.";
    return false;
}

}  // namespace http
