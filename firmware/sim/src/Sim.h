#pragma once
// Internals of the browser simulator shared by the sim/src files.
#include <stdint.h>

namespace sim {
uint32_t nowMs();             // millis() of this boot
bool cardInserted();
bool wifiInRange();
bool usbComputer();           // cable to a computer that enumerated us
void fsChanged();             // ask the page to persist /sdcard and /nvs soon
}  // namespace sim

// Calls into the page (sim/web/sim.js, `Module.sim`), defined in Bridge.cpp.
extern "C" {
void js_panel_refresh(const uint8_t* ram, int bytes, int kind, int ms);
void js_serial(const char* text, int len);
void js_led(int on);
void js_deep_sleep(const uint8_t* rtc, int len, uint32_t maskLo, uint32_t maskHi);
int js_boot_rtc(uint8_t* rtc, int len);
int js_boot_wake_cause();
uint32_t js_boot_ext1_lo();
void js_http_response(int id, int code, const char* type, const char* body, int len);
void js_web_listening(int on);
void js_usb_media(int present);
void js_fs_changed();
int js_power_cut();
}
