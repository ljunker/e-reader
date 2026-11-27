#pragma once
#include "NetworkClient.h"

// (simulator) The browser checks certificates; these only record the wish.
class NetworkClientSecure : public NetworkClient {
public:
    void setInsecure() { insecure_ = true; }
    void useBuiltinCACertBundle() {}
    bool insecure() const { return insecure_; }

private:
    bool insecure_ = false;
};
