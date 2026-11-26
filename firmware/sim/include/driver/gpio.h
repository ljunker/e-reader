#pragma once
// (simulator) pad holds only matter in real deep sleep: no-ops.
typedef int gpio_num_t;
typedef int esp_err_t;
static inline esp_err_t gpio_hold_en(gpio_num_t) { return 0; }
static inline esp_err_t gpio_hold_dis(gpio_num_t) { return 0; }
static inline void gpio_deep_sleep_hold_en(void) {}
static inline void gpio_deep_sleep_hold_dis(void) {}
