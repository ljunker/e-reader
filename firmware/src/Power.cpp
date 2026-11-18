#include "Power.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "Buttons.h"
#include "Display.h"
#include "config.h"
#include "pins.h"

namespace power {

void begin() {
    pinMode(PIN_VBUS_SENSE, INPUT);
    pinMode(PIN_LDO2, OUTPUT);
    digitalWrite(PIN_LDO2, LOW);   // RGB LED + second LDO off
    analogReadResolution(12);
}

int batteryMv() {
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_VBAT_SENSE);
    return int(sum / 8 * VBAT_SCALE);
}

int batteryPercent() {
    int mv = batteryMv();
    int p = (mv - VBAT_EMPTY_MV) * 100 / (VBAT_FULL_MV - VBAT_EMPTY_MV);
    return constrain(p, 0, 100);
}

bool usbPowered() { return digitalRead(PIN_VBUS_SENSE) == HIGH; }

void deepSleep() {
    display.powerOff();
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
