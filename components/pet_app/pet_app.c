#include "pet_app.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "watch_buttons.h"
#include "watch_display.h"
#include "watch_launcher.h"
#include "watch_nvs.h"
#include "watch_power.h"

#define LAUNCHER_NS "launcher" // read only: brightness and screen timeout set there
#define LOOP_MS 20
#define PWR_POLL_MS 50 // PWR comes over I2C: poll it, do not read it every loop
#define SYSTEM_TASK_STACK 4096

static const char *TAG = "pet_app";

static int s_timeout_s = 15;

// Brightness and screen timeout follow the launcher's settings, when it has saved any.
static void load_settings(int *brightness)
{
    nvs_handle_t h;
    uint8_t u8;
    uint16_t u16;
    if (nvs_open(LAUNCHER_NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    if (nvs_get_u8(h, "bright", &u8) == ESP_OK && u8 >= 10 && u8 <= 100) {
        *brightness = u8;
    }
    if (nvs_get_u16(h, "timeout", &u16) == ESP_OK && u16 >= 5 && u16 <= 600) {
        s_timeout_s = u16;
    }
    nvs_close(h);
}

// ---------- UI (LVGL lock held) ----------

static void create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1020), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP32Watch Pet");
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 56);

    // TODO: pet sprite, stats, interaction UI
    lv_obj_t *placeholder = lv_label_create(screen);
    lv_label_set_text(placeholder, LV_SYMBOL_IMAGE "\nPet goes here");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x67e8f9), LV_PART_MAIN);
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(placeholder);

    const char *help = watch_launcher_is_available() ? "BOOT: accion   PWR: launcher" : "BOOT: accion   PWR: dormir";
    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, help);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x94a3b8), LV_PART_MAIN);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -42);
}

// ---------- system task ----------

// Buttons follow core docs/ARCHITECTURE.md "Convencion De Botones": BOOT is the primary
// action, PWR at the app root goes back to the launcher (or sleeps when standalone).
// The screen turns off after the launcher's timeout; BOOT or PWR wake it.
static void system_task(void *arg)
{
    watch_boot_debouncer_t boot;
    watch_boot_debouncer_init(&boot, 0, 0);
    int64_t last_pwr = 0;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        const watch_boot_event_t ev = watch_boot_debouncer_poll(&boot);
        bool pwr = false;
        if (now - last_pwr >= PWR_POLL_MS * 1000) {
            last_pwr = now;
            watch_pwr_key_take_short_press(&pwr);
        }
        if (pwr && watch_launcher_is_available()) {
            watch_launcher_exit(); // save anything that must persist before this
        }

        bsp_display_lock(0);
        if (ev.down || pwr) {
            lv_display_trigger_activity(NULL);
        }
        if (ev.short_press) {
            // TODO: pet interaction (feed, play, etc.)
        }
        const bool sleep = pwr || lv_display_get_inactive_time(NULL) > (uint32_t)s_timeout_s * 1000;
        bsp_display_unlock();

        if (sleep) {
            // Must not hold the LVGL lock: watch_power stops LVGL and takes it itself.
            const watch_wake_t why = watch_power_sleep(0);
            ESP_LOGI(TAG, "Woke up by %s", why == WATCH_WAKE_BOOT ? "BOOT" : "PWR");
            watch_boot_debouncer_init(&boot, 0, 0); // the waking press is not an action
        }
        vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
    }
}

esp_err_t pet_app_start(void)
{
    esp_err_t err = watch_nvs_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS unavailable: %s", esp_err_to_name(err));
    }
    int brightness = 80;
    load_settings(&brightness);

    // The IMU and speaker amp may still be on from another app (esp_restart() does not
    // reset them).
    watch_power_quiet_peripherals();

    watch_display_config_t display = WATCH_DISPLAY_CONFIG_DEFAULT();
    display.brightness = brightness;
    if (watch_display_start(&display) == NULL) {
        return ESP_FAIL;
    }
    if ((err = watch_boot_button_init()) != ESP_OK) {
        ESP_LOGW(TAG, "BOOT button unavailable: %s", esp_err_to_name(err));
    }
    if ((err = watch_pwr_key_init()) != ESP_OK) {
        ESP_LOGW(TAG, "PWR key unavailable: %s", esp_err_to_name(err));
    }

    if (!bsp_display_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    create_ui();
    bsp_display_unlock();

    if (xTaskCreate(system_task, "system", SYSTEM_TASK_STACK, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Ready (screen timeout %d s)", s_timeout_s);
    return ESP_OK;
}
