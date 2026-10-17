// PROBE (not the app): D1 (LVGL flush of the 288x288 scene) + R1 (passive scan with the UI up).
// Use: in a scratch copy of the repo replace components/pet_app/pet_app.c with this file, add
// radio_probe.c, esp_wifi/esp_event, the WiFi options of PET_TECH section 4 and
// CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192; flash to ota_4 only and erase it afterwards.
// PROBE firmware (ota_4 only): D1 = LVGL flush time of a 288x288 scene, R1 = passive WiFi
// scan with the UI up. Stays awake, writes no PMU registers, then returns to the launcher.
#include "pet_app.h"
#include <stdlib.h>
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pet_core.h"
#include "pet_render.h"
#include "watch_display.h"
#include "watch_launcher.h"
#include "watch_nvs.h"
#include "watch_power.h"

void pet_radio_probe_scan(uint32_t passive_ms);
static const char *TAG = "probe";
#define S 288

static uint16_t *s_buf[2];
static lv_image_dsc_t s_dsc;
static lv_obj_t *s_img;
static volatile bool s_run = true;

static void frame(int i, uint32_t t_ms, int *render_us)
{
    static uint8_t idx[PET_W * PET_H];
    uint16_t pal[PAL_COUNT];
    pet_view_t v = { .seed = 0x1234567, .stage = PET_ADULT, .shape = 1, .expr = EXPR_HAPPY };
    pet_pose(&v, (t_ms / 4000) & 1 ? ANIM_BOUNCE : ANIM_IDLE, t_ms % 4000);
    int64_t a = esp_timer_get_time();
    pet_render_creature(&v, idx, PET_W, PET_H);
    pet_palette_rgb565(v.seed, pal);
    pet_blit_scaled_rgb565(idx, PET_W, PET_H, 6, pal, s_buf[i & 1], S);
    *render_us = (int)(esp_timer_get_time() - a);
}

static void anim_task(void *arg)
{
    // D1: frames as fast as LVGL can flush them (lv_refr_now is synchronous).
    int n = 0;
    int64_t sum_flush = 0, max_flush = 0, sum_r = 0;
    int64_t t0 = esp_timer_get_time();
    while (s_run) {
        int r;
        frame(n, (uint32_t)((esp_timer_get_time() - t0) / 1000), &r);
        int64_t a = esp_timer_get_time();
        bsp_display_lock(0);
        s_dsc.data = (const uint8_t *)s_buf[n & 1];
        lv_image_set_src(s_img, &s_dsc);
        lv_obj_invalidate(s_img);
        lv_refr_now(NULL);
        bsp_display_unlock();
        int64_t f = esp_timer_get_time() - a;
        sum_flush += f; sum_r += r; if (f > max_flush) max_flush = f;
        n++;
        if (n % 50 == 0) {
            ESP_LOGI(TAG, "anim stack high-water free: %u B", (unsigned)uxTaskGetStackHighWaterMark(NULL));
            ESP_LOGI(TAG, "D1 frames=%d render=%lld us, refr+flush avg=%lld ms max=%lld ms", n,
                     sum_r / n, sum_flush / n / 1000, max_flush / 1000);
            sum_flush = sum_r = max_flush = 0; n = 0;
        }
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}

esp_err_t pet_app_start(void)
{
    watch_nvs_init();
    watch_power_quiet_peripherals();
    watch_display_config_t cfg = WATCH_DISPLAY_CONFIG_DEFAULT();
    cfg.buffer_lines = 20;
    cfg.brightness = 40;
    if (!watch_display_start(&cfg)) return ESP_FAIL;
    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_malloc(S * S * 2, MALLOC_CAP_SPIRAM);
        if (!s_buf[i]) { ESP_LOGE(TAG, "psram alloc"); return ESP_ERR_NO_MEM; }
    }
    s_dsc = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                          .w = S, .h = S, .stride = S * 2 },
                              .data_size = S * S * 2, .data = (const uint8_t *)s_buf[0] };
    int r;
    frame(0, 0, &r);
    bsp_display_lock(0);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    s_img = lv_image_create(lv_screen_active());
    lv_image_set_src(s_img, &s_dsc);
    lv_obj_center(s_img);
    bsp_display_unlock();
    ESP_LOGI(TAG, "ui up: internal free %u min %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    xTaskCreate(anim_task, "anim", 8192, NULL, 4, NULL);
    vTaskDelay(pdMS_TO_TICKS(8000));          // D1 alone
    ESP_LOGI(TAG, "=== R1 start (UI animating) ===");
    for (int k = 0; k < 3; k++) { pet_radio_probe_scan(120); vTaskDelay(pdMS_TO_TICKS(500)); }
    for (int k = 0; k < 3; k++) { pet_radio_probe_scan(360); vTaskDelay(pdMS_TO_TICKS(500)); }
    ESP_LOGI(TAG, "=== R1 done === main stack high-water free: %u B", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(3000));
    s_run = false;
    ESP_LOGI(TAG, "probe finished, returning to launcher");
    vTaskDelay(pdMS_TO_TICKS(300));
    if (watch_launcher_is_available()) watch_launcher_exit();
    return ESP_OK;
}
