#pragma once
#include "gpio.h"
typedef enum { RTC_GPIO_MODE_INPUT_ONLY = 0, RTC_GPIO_MODE_OUTPUT_ONLY, RTC_GPIO_MODE_INPUT_OUTPUT } rtc_gpio_mode_t;
static inline esp_err_t rtc_gpio_init(gpio_num_t) { return 0; }
static inline esp_err_t rtc_gpio_set_direction(gpio_num_t, rtc_gpio_mode_t) { return 0; }
static inline esp_err_t rtc_gpio_pullup_en(gpio_num_t) { return 0; }
static inline esp_err_t rtc_gpio_pulldown_dis(gpio_num_t) { return 0; }
