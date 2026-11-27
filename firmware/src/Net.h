#pragma once
#include <Arduino.h>

// Joins the strongest Wi-Fi network listed in /wifi.txt; shared by the upload
// page and the online library.
namespace net {
// Blocks while connecting (up to WIFI_CONNECT_MS). On failure `error` says why.
bool join(String& error);
void leave();   // disconnect and switch the radio off
bool joined();
}  // namespace net
