#pragma once
#include "WiFi.h"

#include <string>
#include <vector>

class WiFiMulti {
public:
    bool addAP(const char* ssid, const char* passphrase = nullptr);
    uint8_t run(uint32_t connectTimeout = 5000);

private:
    std::vector<std::string> ssids_;
};
