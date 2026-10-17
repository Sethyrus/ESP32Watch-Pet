// ESP32Watch Pet: board init, tasks and power. Game rules live in pet_core, drawing in
// pet_render, screens in pet_ui.c, radio in pet_radio.c and saves in pet_store.c.
//
// Tasks (docs/PET_TECH.md, section 6):
//   ui_task     step + render + LVGL swap while the screen is on
//   radio_task  one passive scan per notification (radio on only during the scan)
//   system_task buttons, inactivity, scan schedule, saves, screen off (watch_power_screen_off)
#include "pet_app.h"

#include <string.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "pet_priv.h"
#include "pet_radio.h"
#include "pet_store.h"
#include "watch_battery.h"
#include "watch_buttons.h"
#include "watch_display.h"
#include "watch_launcher.h"
#include "watch_nvs.h"
#include "watch_power.h"
#include "watch_rtc.h"

#define LAUNCHER_NS "launcher" // read only: brightness and screen timeout set there
#define LOOP_MS 20
#define PWR_POLL_MS 50 // PWR comes over I2C: poll it, do not read it every loop
#define UI_MS 40
#define BATT_ON_S 30
#define BATT_OFF_S 120
#define SAVE_MIN_S 30
#define SAVE_BEAT_S 600
#define SCAN_RETRY_S 120
#define MAX_APS 40
#define SCAN_POLL_MS 5000

static const char *TAG = "pet_app";

pet_state_t g_pet;
volatile int g_batt_pct = -1;
volatile bool g_usb;

static SemaphoreHandle_t s_mu;
static SemaphoreHandle_t s_save_mu; // system_task and radio_task may both save
static TaskHandle_t s_radio;
static int s_timeout_s = 30;
static volatile bool s_screen_on = true;
static volatile bool s_scanning;
static volatile bool s_dirty;
static volatile bool s_force_save;
static int64_t s_last_save_s;
static int64_t s_retry_s;
static int64_t s_last_batt_s;

void pet_lock(void) { xSemaphoreTake(s_mu, portMAX_DELAY); }
void pet_unlock(void) { xSemaphoreGive(s_mu); }
int64_t pet_now_s(void) { return (int64_t)time(NULL); }
uint32_t pet_mono_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
void pet_mark_dirty(void) { s_dirty = true; }

// ---------- game setup and saves ----------

static void new_game(void)
{
    uint8_t key[PET_KEY_LEN];
    esp_fill_random(key, sizeof(key));
    // Without the RF subsystem the hardware RNG is weak: mix in the MAC and the boot timer.
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    uint64_t t = (uint64_t)esp_timer_get_time();
    for (size_t i = 0; i < sizeof(key); i++) {
        key[i] ^= mac[i % 6] ^ (uint8_t)(t >> (8 * (i % 8)));
    }
    pet_init(&g_pet, key, pet_now_s());
}

void pet_reset_game(void)
{
    pet_lock();
    new_game();
    pet_unlock();
    s_force_save = true;
}

static void save_now(void)
{
    static pet_state_t snap; // keep the 1.2 KB copy off the stack
    xSemaphoreTake(s_save_mu, portMAX_DELAY);
    pet_lock();
    snap = g_pet;
    pet_unlock();
    if (pet_store_save(&snap) == ESP_OK) {
        ESP_LOGI(TAG, "saved");
        s_dirty = false;
        s_last_save_s = pet_now_s();
    }
    xSemaphoreGive(s_save_mu);
}

// Event-driven saves at most every SAVE_MIN_S, plus a heartbeat every SAVE_BEAT_S.
static void maybe_save(void)
{
    const int64_t now = pet_now_s();
    const int64_t since = now - s_last_save_s;
    if (s_force_save || (s_dirty && since >= SAVE_MIN_S) || since >= SAVE_BEAT_S || since < 0) {
        s_force_save = false;
        save_now();
    }
}

// ---------- battery and scan schedule ----------

static void read_battery(int64_t now)
{
    s_last_batt_s = now;
    watch_battery_t b;
    if (watch_battery_read(&b) == ESP_OK) {
        g_batt_pct = b.present ? b.percent : -1;
        g_usb = b.usb;
    }
}

static bool scan_due(void)
{
    const int64_t now = pet_now_s();
    pet_lock();
    int64_t next = g_pet.stage == PET_EGG ? PET_NEVER : pet_next_scan_s(&g_pet, now, g_usb, g_batt_pct);
    pet_unlock();
    if (next == PET_NEVER) {
        return false;
    }
    return now >= next && now >= s_retry_s;
}

// Claims the radio for one scan; false when one is already running or queued.
static bool claim_scan(void) { return !__atomic_exchange_n(&s_scanning, true, __ATOMIC_ACQ_REL); }

static void request_scan(void)
{
    if (claim_scan()) {
        ESP_LOGI(TAG, "scan requested (screen %s)", s_screen_on ? "on" : "off");
        xTaskNotifyGive(s_radio);
    }
}

static void radio_task(void *arg)
{
    static pet_ap_t aps[MAX_APS];
    for (;;) {
        // Woken by request_scan(). With the screen off on USB, watch_power_screen_off() never
        // calls stay_awake() (the chip does not sleep), so this task also checks the schedule
        // itself; on battery the check happens in stay_awake() because ticks stop in sleep.
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SCAN_POLL_MS))) {
            if (s_screen_on || !scan_due() || !claim_scan()) {
                continue;
            }
            ESP_LOGI(TAG, "scan requested (screen off, radio task)");
        }
        const int n = pet_radio_scan(aps, MAX_APS);
        if (n < 0) {
            s_retry_s = pet_now_s() + SCAN_RETRY_S;
        } else {
            pet_lock();
            const int added = pet_ingest_scan(&g_pet, aps, (size_t)n, pet_now_s());
            const uint32_t scans = g_pet.total_scans;
            pet_unlock();
            ESP_LOGI(TAG, "scan #%u: %d networks, %d bites added", (unsigned)scans, n, added);
            s_dirty = true;
            if (!s_screen_on) {
                save_now(); // nobody is looking: persist before the chip sleeps again
            }
        }
        memset(aps, 0, sizeof(aps));
        s_scanning = false;
    }
}

// Called by watch_power_screen_off() on the system task about every 50 ms while it returns
// true and on each ~200 ms light-sleep wake after that. It starts due scans and saves the
// result, and keeps the chip awake until both are done.
static bool stay_awake(void)
{
    if (s_scanning) {
        return true;
    }
    const int64_t now = pet_now_s();
    if (now - s_last_batt_s >= BATT_OFF_S) {
        read_battery(now);
    }
    if (scan_due()) {
        request_scan();
        return true;
    }
    if (s_force_save) {
        maybe_save();
    }
    return false;
}

// ---------- system task ----------

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

static void ui_task(void *arg)
{
    for (;;) {
        if (s_screen_on) {
            pet_ui_tick();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_MS));
    }
}

// Buttons follow core docs/ARCHITECTURE.md "Convencion De Botones": BOOT is the primary
// action (opens the pantry), PWR goes back; at the root it returns to the launcher, or
// turns the screen off when standalone.
static void system_task(void *arg)
{
    watch_boot_debouncer_t boot;
    watch_boot_debouncer_init(&boot, 0, 0);
    int64_t last_pwr = 0;
    for (;;) {
        const int64_t now_us = esp_timer_get_time();
        const watch_boot_event_t ev = watch_boot_debouncer_poll(&boot);
        bool pwr = false;
        if (now_us - last_pwr >= PWR_POLL_MS * 1000) {
            last_pwr = now_us;
            watch_pwr_key_take_short_press(&pwr);
        }

        bool consumed = false;
        bool sleep = false;
        bsp_display_lock(0);
        if (ev.down || pwr) {
            lv_display_trigger_activity(NULL);
        }
        if (pwr) {
            consumed = pet_ui_back();
        }
        if (ev.short_press) {
            pet_ui_boot_action();
        }
        sleep = (pwr && !consumed && !watch_launcher_is_available()) ||
                lv_display_get_inactive_time(NULL) > (uint32_t)s_timeout_s * 1000;
        bsp_display_unlock();

        if (pwr && !consumed && watch_launcher_is_available()) {
            s_force_save = true;
            maybe_save();
            watch_launcher_exit();
        }

        const int64_t now = pet_now_s();
        if (now - s_last_batt_s >= BATT_ON_S) {
            read_battery(now);
        }
        if (scan_due()) {
            request_scan();
        }
        maybe_save();

        if (sleep) {
            s_force_save = true;
            maybe_save();
            s_screen_on = false;
            ESP_LOGI(TAG, "screen off");
            // Must not hold the LVGL lock: watch_power stops LVGL and takes it itself.
            const watch_wake_t why = watch_power_screen_off(0, stay_awake);
            ESP_LOGI(TAG, "Woke up by %s", why == WATCH_WAKE_BOOT ? "BOOT" : "PWR");
            s_screen_on = true;
            watch_boot_debouncer_init(&boot, 0, 0); // the waking press is not an action
            if (bsp_display_lock(1000)) {
                lv_display_trigger_activity(NULL);
                bsp_display_unlock();
            }
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

    s_mu = xSemaphoreCreateMutex();
    s_save_mu = xSemaphoreCreateMutex();
    if (s_mu == NULL || s_save_mu == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (watch_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "battery unavailable");
    }
    if ((err = watch_rtc_init(false)) != ESP_OK) {
        ESP_LOGW(TAG, "RTC unavailable: %s", esp_err_to_name(err));
    }

    // Game: load the save, or start with a new egg. A clock that lost its time re-anchors
    // the pet instead of simulating a bogus gap.
    if (!pet_store_load(&g_pet)) {
        new_game();
        s_force_save = true;
    } else if (watch_rtc_time_was_lost()) {
        pet_clock_lost(&g_pet, pet_now_s());
        ESP_LOGW(TAG, "RTC lost its time: pet re-anchored");
    }
    read_battery(pet_now_s());
    s_last_save_s = pet_now_s(); // the save just loaded (or forced for a new game) is the last one

    watch_display_config_t display = WATCH_DISPLAY_CONFIG_DEFAULT();
    display.brightness = brightness;
    display.buffer_lines = 20; // leaves RAM for WiFi (PET_TECH section 5)
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
    pet_ui_init();
    bsp_display_unlock();

    if (xTaskCreate(radio_task, "radio", 8192, NULL, 3, &s_radio) != pdPASS ||
        xTaskCreate(ui_task, "ui", 9216, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(system_task, "system", 6144, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Ready (%s, screen timeout %d s)", watch_launcher_is_available() ? "launcher app" : "standalone",
             s_timeout_s);
    return ESP_OK;
}
