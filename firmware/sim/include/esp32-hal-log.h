#pragma once
// ESP-IDF style log macros, routed to the simulator's serial log.
#ifdef __cplusplus
extern "C" {
#endif
void sim_log(char level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#ifdef __cplusplus
}
#endif
#define log_e(fmt, ...) sim_log('E', fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) sim_log('W', fmt, ##__VA_ARGS__)
#define log_i(fmt, ...) sim_log('I', fmt, ##__VA_ARGS__)
#define log_d(fmt, ...) do {} while (0)
#define log_v(fmt, ...) do {} while (0)
