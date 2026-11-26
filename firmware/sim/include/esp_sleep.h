#pragma once
#include <stdint.h>

typedef enum {
    ESP_SLEEP_WAKEUP_UNDEFINED = 0,
    ESP_SLEEP_WAKEUP_ALL,
    ESP_SLEEP_WAKEUP_EXT0,
    ESP_SLEEP_WAKEUP_EXT1,
    ESP_SLEEP_WAKEUP_TIMER,
} esp_sleep_wakeup_cause_t;

typedef enum { ESP_PD_DOMAIN_RTC_PERIPH = 0 } esp_sleep_pd_domain_t;
typedef enum { ESP_PD_OPTION_OFF = 0, ESP_PD_OPTION_ON, ESP_PD_OPTION_AUTO } esp_sleep_pd_option_t;
typedef enum { ESP_EXT1_WAKEUP_ANY_LOW = 0, ESP_EXT1_WAKEUP_ANY_HIGH = 1 } esp_sleep_ext1_wakeup_mode_t;
typedef int esp_err_t;

#ifdef __cplusplus
extern "C" {
#endif
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
uint64_t esp_sleep_get_ext1_wakeup_status(void);
esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t domain, esp_sleep_pd_option_t option);
esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode);
// Ends this run of the firmware; the simulator boots it again on a wake-up button.
__attribute__((noreturn)) void esp_deep_sleep_start(void);
#ifdef __cplusplus
}
#endif
