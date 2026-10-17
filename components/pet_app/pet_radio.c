#include "pet_radio.h"

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_wifi.h"

#define PASSIVE_MS 150 // per channel; provisional (DECISIONS A2): re-check in a dense area
#define MAX_RECORDS 40
#define MIN_FREE_INTERNAL (30 * 1024) // below this the scan is skipped (PET_TECH section 5)

static const char *TAG = "pet_radio";

int pet_radio_scan(pet_ap_t *aps, size_t max)
{
    static bool loop_ready;
    if (!loop_ready) {
        esp_err_t e = esp_event_loop_create_default();
        if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "event loop: %s", esp_err_to_name(e));
            return -1;
        }
        loop_ready = true;
    }
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < MIN_FREE_INTERNAL + 70 * 1024) {
        ESP_LOGW(TAG, "not enough internal RAM, scan skipped");
        return -1;
    }

    int n = -1;
    bool started = false;
    // 40 records are ~5 KB: keep them off the task stack and out of internal RAM.
    wifi_ap_record_t *recs = heap_caps_malloc(MAX_RECORDS * sizeof(*recs), MALLOC_CAP_SPIRAM);
    if (recs == NULL) {
        return -1;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    if (esp_wifi_init(&cfg) != ESP_OK) {
        free(recs);
        return -1;
    }
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_set_country_code("ES", false) != ESP_OK || esp_wifi_start() != ESP_OK) {
        goto out;
    }
    started = true;

    wifi_scan_config_t sc = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.passive = PASSIVE_MS,
    };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        goto out;
    }
    uint16_t got = MAX_RECORDS;
    if (esp_wifi_scan_get_ap_records(&got, recs) != ESP_OK) {
        goto out;
    }
    n = 0;
    for (uint16_t i = 0; i < got && (size_t)n < max; i++) {
        memcpy(aps[n].bssid, recs[i].bssid, 6);
        aps[n].rssi = recs[i].rssi;
        aps[n].open = recs[i].authmode == WIFI_AUTH_OPEN;
        aps[n].hidden = recs[i].ssid[0] == 0;
        n++;
    }
    ESP_LOGI(TAG, "scan: %d networks, internal min free %u B", n,
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
out:
    if (started) {
        esp_wifi_stop();
    }
    esp_wifi_deinit();
    memset(recs, 0, MAX_RECORDS * sizeof(*recs));
    free(recs);
    return n;
}
