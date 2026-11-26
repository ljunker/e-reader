#pragma once
#include <Arduino.h>

// Upload page over Wi-Fi: joins the strongest network listed in /wifi.txt and
// serves http://ereader.local/ to list, upload and delete books.
namespace wifishare {
// Blocks while connecting (up to ~20 s). On failure `error` says why.
bool start(String& error);
void stop();
void loop();                 // call often while active
bool active();
String network();            // SSID we are on
String address();            // "http://192.168.1.23/"
uint32_t filesReceived();
const String& lastFile();
uint32_t lastRequestMs();    // millis() of the last page request
bool takeChanged();          // a file arrived or was deleted since the last call
}  // namespace wifishare
