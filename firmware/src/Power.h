#pragma once
#include <Arduino.h>

namespace power {
// Sets up the on-board pins and finds the MAX17048 fuel gauge.
void begin();
int batteryMv();           // cell voltage, -1 if no monitor answers
int batteryPercent();      // 0..100, -1 if unknown
bool usbPowered();         // 5 V on USB-C (computer or charger)
// Powers the display down and enters deep sleep; any button wakes the device.
[[noreturn]] void deepSleep();
}  // namespace power
