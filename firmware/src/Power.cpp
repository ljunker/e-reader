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

static bool gaugeFound = false;

// Outputs that must stay low through deep sleep (the pads float otherwise).
static const int kHeldLow[] = {PIN_LDO2, PIN_LED, PIN_EPD_PWR};

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

void begin() {
    for (int pin : kHeldLow)
        if (pin >= 0) gpio_hold_dis(gpio_num_t(pin));
    gpio_deep_sleep_hold_dis();

    pinMode(PIN_LDO2, OUTPUT);
    digitalWrite(PIN_LDO2, LOW);        // RGB LED + second STEMMA QT off
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_RF_SWITCH, OUTPUT);
    digitalWrite(PIN_RF_SWITCH, LOW);   // on-board antenna
    pinMode(PIN_VBUS_SENSE, INPUT);

    // The gauge and its pull-ups sit on LDO1, which is always on.
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    gaugeFound = present(kMax17048Addr);
    if (!gaugeFound) log_w("no MAX17048 fuel gauge found on I2C");
}

int batteryMv() {
    uint16_t v;
    return gaugeFound && maxRead(kMaxRegVcell, v) ? max17048Millivolts(v) : -1;
}

int batteryPercent() {
    uint16_t v;
    return gaugeFound && maxRead(kMaxRegSoc, v) ? max17048Percent(v) : -1;
}

bool usbPowered() { return digitalRead(PIN_VBUS_SENSE) == HIGH; }

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
