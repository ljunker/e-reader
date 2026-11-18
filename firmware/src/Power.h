#pragma once
#include <Arduino.h>

namespace power {
void begin();
int batteryMv();
int batteryPercent();      // 0..100, rough (linear between empty and full)
bool usbPowered();
// Powers the display down and enters deep sleep; any button wakes the device.
[[noreturn]] void deepSleep();
}  // namespace power
