#pragma once
// Wi-Fi of the simulator: no radio, just whether a network is "in range"
// (a switch in the browser page) and a made-up address.
#include <Arduino.h>

typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;
typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_DISCONNECTED = 6,
} wl_status_t;

class IPAddress {
public:
    IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : b_{a, b, c, d} {}
    String toString() const;

private:
    uint8_t b_[4];
};

class WiFiClass {
public:
    bool mode(wifi_mode_t m);
    bool setHostname(const char* name) { (void)name; return true; }
    bool disconnect(bool wifioff = false, bool eraseap = false);
    String SSID() const;
    IPAddress localIP() const;
    wl_status_t status() const;
};
extern WiFiClass WiFi;
