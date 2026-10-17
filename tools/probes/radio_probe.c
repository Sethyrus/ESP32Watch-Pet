// Measurement probe (not part of the app): one passive WiFi scan with the radio on only
// during the scan; logs init/scan/deinit times, AP counts and internal heap.
// Use (docs/PET_TECH.md, section 8): add this file to pet_app's SRCS, add esp_wifi and
// esp_event to PRIV_REQUIRES and the WiFi options of section 4 to sdkconfig.defaults, call
// pet_radio_probe_scan(120) and pet_radio_probe_scan(360) after the UI is up, and flash to
// the ota_4 test slot. Never leave it on the watch without saying so.
#include <string.h>
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "radio_probe";

static void log_heap(const char *when)
{
    ESP_LOGI(TAG, "%s: internal free %u (min %u, largest %u), psram free %u", when,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void pet_radio_probe_scan(uint32_t passive_ms)
{
    static bool netif_ready;
    if (!netif_ready) {
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        netif_ready = true;
    }
    log_heap("before wifi");
    int64_t t0 = esp_timer_get_time();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_country_code("ES", false));
    ESP_ERROR_CHECK(esp_wifi_start());
    log_heap("wifi started");
    wifi_scan_config_t sc = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.passive = passive_ms,
    };
    int64_t t1 = esp_timer_get_time();
    ESP_ERROR_CHECK(esp_wifi_scan_start(&sc, true));
    int64_t t2 = esp_timer_get_time();
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    wifi_ap_record_t recs[32];
    uint16_t got = 32;
    esp_wifi_scan_get_ap_records(&got, recs);
    int open = 0, hidden = 0;
    for (int i = 0; i < got; i++) {
        open += recs[i].authmode == WIFI_AUTH_OPEN;
        hidden += recs[i].ssid[0] == 0;
    }
    log_heap("after scan");
    esp_wifi_stop();
    esp_wifi_deinit();
    int64_t t3 = esp_timer_get_time();
    log_heap("after deinit");
    ESP_LOGI(TAG, "aps=%u open=%d hidden=%d | init %lld ms, scan %lld ms, deinit %lld ms", n, open, hidden,
             (t1 - t0) / 1000, (t2 - t1) / 1000, (t3 - t2) / 1000);
}
