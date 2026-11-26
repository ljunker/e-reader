#pragma once
#include <Arduino.h>

namespace power {
// Powers up the I2C rail and finds the battery monitor (MAX17048 or LC709203F).
void begin();
int batteryMv();           // cell voltage, -1 if no monitor answers
int batteryPercent();      // 0..100, -1 if unknown
// Powers the display down and enters deep sleep; any button wakes the device.
[[noreturn]] void deepSleep();
}  // namespace power
