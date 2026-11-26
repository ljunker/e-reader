#pragma once
// (simulator) single-threaded: FreeRTOS primitives are no-ops.
#include <stdint.h>
#define portMAX_DELAY 0xffffffffu
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
