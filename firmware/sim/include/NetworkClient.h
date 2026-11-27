#pragma once
// (simulator) The HTTP client below does not use sockets: requests go
// through the page to the simulator's server (sim/serve.py /proxy).
#include <Arduino.h>

class NetworkClient {
public:
    virtual ~NetworkClient() = default;
};
