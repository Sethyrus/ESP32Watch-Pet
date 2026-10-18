/* Host shim of the Waveshare BSP for tools/uisim: only the display calls the UI uses.
 * Single thread: the lock always succeeds (uisim counts the nesting to catch misuse). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#define BSP_LCD_H_RES 410
#define BSP_LCD_V_RES 502

bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

static inline esp_err_t bsp_display_brightness_set(int percent) { (void)percent; return ESP_OK; }
static inline esp_err_t bsp_display_backlight_on(void) { return ESP_OK; }
static inline esp_err_t bsp_display_backlight_off(void) { return ESP_OK; }
