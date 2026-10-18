/* Host shim of esp_log.h for tools/uisim: prints to stderr only when UISIM_LOG=1. */
#pragma once
#include <stdint.h>

void uisim_log(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define ESP_LOGE(tag, fmt, ...) uisim_log('E', tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) uisim_log('W', tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) uisim_log('I', tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) uisim_log('D', tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) uisim_log('V', tag, fmt, ##__VA_ARGS__)
