#include "pet_store.h"

#include "esp_log.h"
#include "nvs.h"

#define NS "pet"

static const char *TAG = "pet_store";
static uint32_t s_seq;
static int s_next; // copy to write next: 0 = A, 1 = B

static const char *key_of(int i) { return i ? "save_b" : "save_a"; }

bool pet_store_load(pet_state_t *s)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    uint8_t buf[2][PET_SAVE_MAX];
    size_t len[2] = {sizeof(buf[0]), sizeof(buf[1])};
    for (int i = 0; i < 2; i++) {
        if (nvs_get_blob(h, key_of(i), buf[i], &len[i]) != ESP_OK) {
            len[i] = 0;
        }
    }
    nvs_close(h);
    int idx = pet_load_ab(s, buf[0], len[0], buf[1], len[1]);
    if (idx < 0) {
        ESP_LOGI(TAG, "no valid save, new game");
        return false;
    }
    pet_load(s, buf[idx], len[idx], &s_seq);
    s_next = !idx;
    ESP_LOGI(TAG, "loaded copy %c, seq %u", idx ? 'B' : 'A', (unsigned)s_seq);
    return true;
}

esp_err_t pet_store_save(const pet_state_t *s)
{
    uint8_t buf[PET_SAVE_MAX];
    size_t n = pet_save(s, s_seq + 1, buf, sizeof(buf));
    if (n == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, key_of(s_next), buf, n);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        s_seq++;
        s_next = !s_next;
    } else {
        ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}
