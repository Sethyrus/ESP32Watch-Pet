/* Host shim of esp_timer.h for tools/uisim: the simulated monotonic clock, in us. */
#pragma once
#include <stdint.h>

int64_t esp_timer_get_time(void);
