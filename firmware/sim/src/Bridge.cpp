// JavaScript side of the HAL: each function forwards to a hook on
// `Module.sim`, which sim/web/sim.js fills in before the firmware starts.
#include <emscripten.h>

#include "Sim.h"

EM_JS(void, js_panel_refresh, (const uint8_t* ram, int bytes, int kind, int ms), {
    Module.sim.refresh(HEAPU8.slice(ram, ram + bytes), kind, ms);
});

EM_JS(void, js_serial, (const char* text, int len), {
    Module.sim.serial(UTF8ToString(text, len));
});

EM_JS(void, js_led, (int on), { Module.sim.led(!!on); });

EM_JS(void, js_deep_sleep, (const uint8_t* rtc, int len, uint32_t maskLo, uint32_t maskHi), {
    Module.sim.deepSleep(HEAPU8.slice(rtc, rtc + len), maskLo >>> 0, maskHi >>> 0);
});

EM_JS(int, js_boot_rtc, (uint8_t* rtc, int len), {
    const b = Module.sim.boot.rtc;
    if (!b || b.length !== len) return 0;
    HEAPU8.set(b, rtc);
    return 1;
});

EM_JS(int, js_boot_wake_cause, (), { return Module.sim.boot.wakeCause | 0; });

EM_JS(uint32_t, js_boot_ext1_lo, (), { return Module.sim.boot.ext1 >>> 0; });

EM_JS(void, js_http_response, (int id, int code, const char* type, const char* body, int len), {
    Module.sim.httpResponse(id, code, UTF8ToString(type), HEAPU8.slice(body, body + len));
});

EM_JS(void, js_web_listening, (int on), { Module.sim.webListening(!!on); });

EM_JS(void, js_usb_media, (int present), { Module.sim.usbMedia(!!present); });

EM_JS(void, js_fs_changed, (), { Module.sim.fsChanged(); });

EM_JS(int, js_power_cut, (), { return Module.sim.powerCut ? 1 : 0; });
