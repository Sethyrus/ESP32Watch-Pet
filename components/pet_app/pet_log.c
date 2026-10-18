#include "pet_log.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#define NS "pet"
#define KEY "tlm"

static const char *TAG = "pet_log";
static pet_log_entry_t s_e[PET_LOG_MAX];
static size_t s_n;

void pet_log_load(void)
{
    nvs_handle_t h;
    s_n = 0;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(s_e);
    if (nvs_get_blob(h, KEY, s_e, &len) == ESP_OK && len % sizeof(s_e[0]) == 0) {
        s_n = len / sizeof(s_e[0]);
    }
    nvs_close(h);
}

void pet_log_add(int64_t t, int mv, int pct, uint8_t flags, uint32_t scans)
{
    if (s_n == PET_LOG_MAX) {
        memmove(&s_e[0], &s_e[1], (PET_LOG_MAX - 1) * sizeof(s_e[0]));
        s_n--;
    }
    s_e[s_n++] = (pet_log_entry_t){.t = (uint32_t)t, .mv = (uint16_t)mv, .scans = (uint16_t)scans,
                                   .pct = (int8_t)pct, .flags = flags};
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, KEY, s_e, s_n * sizeof(s_e[0]));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    ESP_LOGI(TAG, "hourly: %d mV, %d %%, flags %u, scans %u%s", mv, pct, flags, (unsigned)scans,
             err == ESP_OK ? "" : " (not saved)");
}
