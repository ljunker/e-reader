#pragma once
// USB device stack of the simulator: the "cable" switch in the browser page
// decides whether a computer has enumerated the reader.
#include <Arduino.h>

typedef const char* esp_event_base_t;
typedef void (*esp_event_handler_t)(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
extern esp_event_base_t const ARDUINO_USB_EVENTS;

#define ESP_EVENT_ANY_ID -1
typedef enum {
    ARDUINO_USB_ANY_EVENT = ESP_EVENT_ANY_ID,
    ARDUINO_USB_STARTED_EVENT = 0,
    ARDUINO_USB_STOPPED_EVENT,
    ARDUINO_USB_SUSPEND_EVENT,
    ARDUINO_USB_RESUME_EVENT,
    ARDUINO_USB_MAX_EVENT,
} arduino_usb_event_t;

class ESPUSB {
public:
    bool begin() { return true; }
    void onEvent(esp_event_handler_t callback);
    void onEvent(arduino_usb_event_t event, esp_event_handler_t callback) { (void)event; onEvent(callback); }
    operator bool() const;
};
extern ESPUSB USB;
