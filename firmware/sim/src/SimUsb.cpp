// USB device: cable to a computer, a charger, or nothing. The computer's view
// of the drive is the page's file panel; it ejects through onStartStop.
#include <USB.h>
#include <USBMSC.h>
#include <emscripten.h>

#include <vector>

#include "Sim.h"

esp_event_base_t const ARDUINO_USB_EVENTS = "ARDUINO_USB_EVENTS";
ESPUSB USB;

enum Cable { None = 0, Computer = 1, Charger = 2 };
static int cable = None;
static std::vector<esp_event_handler_t> handlers;
static msc_start_stop_cb startStop = nullptr;
static bool mscStarted = false;

bool sim::usbComputer() { return cable == Computer; }
bool sim::usbPowered() { return cable != None; }

static void fire(int32_t id) {
    for (auto h : handlers) h(nullptr, ARDUINO_USB_EVENTS, id, nullptr);
}

void ESPUSB::onEvent(esp_event_handler_t callback) {
    handlers.push_back(callback);
    if (cable == Computer) callback(nullptr, ARDUINO_USB_EVENTS, ARDUINO_USB_STARTED_EVENT, nullptr);
}

// tinyusb_device_mounted: only a computer enumerates the device.
ESPUSB::operator bool() const { return cable == Computer; }

extern "C" EMSCRIPTEN_KEEPALIVE void sim_usb_cable(int mode) {
    if (mode == cable) return;
    bool wasComputer = cable == Computer;
    cable = mode;
    if (wasComputer) fire(ARDUINO_USB_SUSPEND_EVENT);
    if (cable == Computer) fire(ARDUINO_USB_STARTED_EVENT);
}

// The computer ejected the drive (Finder's eject button).
extern "C" EMSCRIPTEN_KEEPALIVE void sim_usb_eject() {
    if (startStop) startStop(0, false, true);
}

bool USBMSC::begin(uint32_t blockCount, uint16_t blockSize) {
    mscStarted = blockCount > 0 && blockSize > 0;
    return mscStarted;
}

void USBMSC::onStartStop(msc_start_stop_cb cb) { startStop = cb; }

void USBMSC::mediaPresent(bool present) { js_usb_media(present && mscStarted); }
