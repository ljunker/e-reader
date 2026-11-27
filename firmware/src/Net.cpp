#include "Net.h"

#include <WiFi.h>
#include <WiFiMulti.h>

#include "Storage.h"
#include "Transfer.h"
#include "config.h"

namespace net {

static WiFiMulti* multi = nullptr;
static bool joined_ = false;

bool join(String& error) {
    if (joined_ && WiFi.status() == WL_CONNECTED) return true;
    if (!storage::mounted()) { error = "The microSD card is not ready."; return false; }
    File f = storage::fs().open(WIFI_FILE, FILE_READ);
    if (!f) {
        error = "No " WIFI_FILE " on the card.\n\nAdd one line per network:\nSSID = password";
        return false;
    }
    String text = f.readString();
    f.close();
    auto nets = transfer::parseWifiList(text.c_str());
    if (nets.empty()) {
        error = WIFI_FILE " lists no network.\n\nOne line per network:\nSSID = password";
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("ereader");
    delete multi;
    multi = new WiFiMulti();
    for (const auto& n : nets) multi->addAP(n.ssid.c_str(), n.password.empty() ? nullptr : n.password.c_str());
    uint32_t t0 = millis();
    while (multi->run(8000) != WL_CONNECTED) {
        if (millis() - t0 > WIFI_CONNECT_MS) {
            leave();
            error = "None of the " + String(nets.size()) + " networks in " WIFI_FILE " answered.";
            return false;
        }
        delay(200);
    }
    joined_ = true;
    return true;
}

void leave() {
    delete multi;
    multi = nullptr;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    joined_ = false;
}

bool joined() { return joined_; }

}  // namespace net
