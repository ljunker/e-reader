#pragma once
#include <Arduino.h>

class MDNSResponder {
public:
    bool begin(const char* hostName) { (void)hostName; return true; }
    void end() {}
    bool addService(const char* service, const char* proto, uint16_t port) { (void)service; (void)proto; (void)port; return true; }
};
extern MDNSResponder MDNS;
