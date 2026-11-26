#include "Power.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "Buttons.h"
#include "Display.h"
#include "FuelGauge.h"
#include "config.h"
#include "pins.h"

namespace power {

using namespace fuelgauge;

enum class Gauge { None, Max17048, Lc709203f };
static Gauge gauge = Gauge::None;

// Outputs that must stay low through deep sleep (the pads float otherwise).
static const int kHeldLow[] = {PIN_I2C_POWER, PIN_NEOPIXEL_POWER, PIN_EPD_PWR};

static bool present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

static bool readRaw(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

static bool maxRead(uint8_t reg, uint16_t& v) {
    uint8_t b[2];
    if (!readRaw(kMax17048Addr, reg, b, 2)) return false;
    v = uint16_t(b[0] << 8 | b[1]);
    return true;
}

static bool lcRead(uint8_t reg, uint16_t& v) {
    uint8_t b[3];
    if (!readRaw(kLc709203fAddr, reg, b, 3)) return false;
    const uint8_t bus[] = {uint8_t(kLc709203fAddr << 1), reg, uint8_t(kLc709203fAddr << 1 | 1), b[0], b[1]};
    if (crc8(bus, sizeof bus) != b[2]) return false;
    v = uint16_t(b[1] << 8 | b[0]);
    return true;
}

static bool lcWrite(uint8_t reg, uint16_t v) {
    const uint8_t bus[] = {uint8_t(kLc709203fAddr << 1), reg, uint8_t(v), uint8_t(v >> 8)};
    Wire.beginTransmission(kLc709203fAddr);
    Wire.write(bus + 1, 3);
    Wire.write(crc8(bus, sizeof bus));
    return Wire.endTransmission() == 0;
}

void begin() {
    for (int pin : kHeldLow)
        if (pin >= 0) gpio_hold_dis(gpio_num_t(pin));
    gpio_deep_sleep_hold_dis();

    pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
    digitalWrite(PIN_NEOPIXEL_POWER, LOW);
    pinMode(PIN_I2C_POWER, OUTPUT);
    digitalWrite(PIN_I2C_POWER, HIGH);   // the I2C pull-ups hang on this rail
    delay(5);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

    if (present(kMax17048Addr)) {
        gauge = Gauge::Max17048;
    } else if (present(kLc709203fAddr) && lcWrite(kLcRegPowerMode, 1)) {
        gauge = Gauge::Lc709203f;
    } else {
        gauge = Gauge::None;
        log_w("no battery monitor found on I2C");
    }
}

int batteryMv() {
    uint16_t v;
    switch (gauge) {
        case Gauge::Max17048: return maxRead(kMaxRegVcell, v) ? max17048Millivolts(v) : -1;
        case Gauge::Lc709203f: return lcRead(kLcRegCellVoltage, v) ? int(v) : -1;
        default: return -1;
    }
}

int batteryPercent() {
    uint16_t v;
    if (gauge == Gauge::Max17048) return maxRead(kMaxRegSoc, v) ? max17048Percent(v) : -1;
    // The LC709203F's own estimate needs the pack configured; the voltage is enough here.
    int mv = batteryMv();
    return mv < 0 ? -1 : linearPercent(mv, VBAT_EMPTY_MV, VBAT_FULL_MV);
}

void deepSleep() {
    display.powerOff();
    Wire.end();
    for (int pin : kHeldLow) {
        if (pin < 0) continue;
        digitalWrite(pin, LOW);
        gpio_hold_en(gpio_num_t(pin));
    }
    gpio_deep_sleep_hold_en();

    uint64_t mask = Buttons::wakeMask();
    for (int pin = 0; pin < 64; pin++) {
        if (!(mask & (1ULL << pin))) continue;
        rtc_gpio_init(gpio_num_t(pin));
        rtc_gpio_set_direction(gpio_num_t(pin), RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(gpio_num_t(pin));
        rtc_gpio_pulldown_dis(gpio_num_t(pin));
    }
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   // keep pull-ups alive
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    Serial.flush();
    esp_deep_sleep_start();
}

}  // namespace power
