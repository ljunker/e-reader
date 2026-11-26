#pragma once
// Arduino API for the browser simulator (env:sim). Only what the firmware and
// its libraries use; String, Print, Stream and FS come unchanged from the
// installed arduino-esp32 core (copied into the build by sim/build.py).
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __cplusplus
#include <algorithm>
#endif

#include "esp32-hal-log.h"
#include "pgmspace.h"

#ifndef ARDUINO
#define ARDUINO 10819
#endif
#ifndef ACCESSPERMS
#define ACCESSPERMS 0777
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)

#define HIGH 0x1
#define LOW  0x0
#define INPUT          0x01
#define OUTPUT         0x03
#define PULLUP         0x04
#define INPUT_PULLUP   0x05
#define PULLDOWN       0x08
#define INPUT_PULLDOWN 0x09
#define LSBFIRST 0
#define MSBFIRST 1

#define RTC_DATA_ATTR __attribute__((section("rtc_data"), used))
#define IRAM_ATTR

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#ifdef __cplusplus
extern "C" {
#endif
unsigned long millis(void);
unsigned long micros(void);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield(void);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void* ps_malloc(size_t size);
const char* pathToFileName(const char* path);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
using std::max;
using std::min;

#include "Print.h"
#include "Stream.h"
#include "WString.h"

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// USB/UART console: lines go to the simulator's serial log.
class SimSerial : public Stream {
public:
    void begin(unsigned long) {}
    void end() {}
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buf, size_t n) override;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override;
    operator bool() const { return true; }
};
extern SimSerial Serial;
#endif
