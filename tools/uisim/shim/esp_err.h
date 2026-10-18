/* Host shim of esp_err.h for tools/uisim. */
#pragma once
#include <stdio.h>
#include <stdlib.h>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x107

static inline const char *esp_err_to_name(esp_err_t e) { return e == ESP_OK ? "ESP_OK" : "ESP_ERR"; }

#define ESP_ERROR_CHECK(x) do { esp_err_t err_ = (x); if (err_ != ESP_OK) { \
    fprintf(stderr, "ESP_ERROR_CHECK failed: %d at %s:%d\n", err_, __FILE__, __LINE__); abort(); } } while (0)
