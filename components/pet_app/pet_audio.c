// Speaker output for pet_sound. The codec (I2S + ES8311 + amplifier) is opened on the first
// sound and closed after IDLE_CLOSE_MS of silence, so the amplifier is off the rest of the
// time. Requests go through a queue: any task can ask for a sound without blocking.
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pet_priv.h"

#define Q_LEN 8
#define BLOCK 256            // samples per write (16 ms)
#define IDLE_CLOSE_MS 1500
#define OFF_WAIT_MS 300

static const char *TAG = "pet_audio";

// Codec output volume per level (0 = off). The Launcher beeps at 80; voice at 100 is quiet.
static const uint8_t CODEC_VOL[PET_VOL_MAX + 1] = {0, 62, 76, 90};

typedef struct {
    uint8_t id, semis;
} req_t;

static QueueHandle_t s_q;
static volatile uint8_t s_vol = PET_VOL_DEFAULT;
static volatile bool s_mute;
static volatile bool s_off;        // pet_sound_off(): stop now and close
static volatile bool s_open;
static volatile int s_stage = 1, s_branch;
static volatile uint32_t s_seed;
static volatile bool s_species_dirty = true;

static esp_codec_dev_handle_t s_spk;
static snd_t s_snd;
static int16_t s_buf[BLOCK];

static bool codec_open(void)
{
    if (s_spk == NULL) {
        s_spk = bsp_audio_codec_speaker_init();
        if (s_spk == NULL) {
            ESP_LOGW(TAG, "speaker not available: no sound");
            return false;
        }
    }
    esp_codec_dev_sample_info_t fs = {.sample_rate = SND_RATE, .channel = 1, .bits_per_sample = 16};
    if (esp_codec_dev_open(s_spk, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "speaker did not open");
        return false;
    }
    esp_codec_dev_set_out_vol(s_spk, CODEC_VOL[s_vol]);
    memset(s_buf, 0, sizeof(s_buf));
    esp_codec_dev_write(s_spk, s_buf, sizeof(s_buf));   // a little silence first: no pop
    s_open = true;
    return true;
}

static void codec_close(void)
{
    if (s_open) {
        esp_codec_dev_close(s_spk);
        s_open = false;
    }
}

static void take(const req_t *r)
{
    if (s_species_dirty) {
        s_species_dirty = false;
        snd_set_species(&s_snd, s_stage, s_branch, s_seed);
    }
    snd_play_pitch(&s_snd, (snd_id_t)r->id, r->semis);
}

static void audio_task(void *arg)
{
    uint8_t applied_vol = 0;
    for (;;) {
        req_t r;
        const TickType_t wait = s_open ? pdMS_TO_TICKS(IDLE_CLOSE_MS) : portMAX_DELAY;
        if (xQueueReceive(s_q, &r, wait) != pdTRUE) {
            codec_close();   // silent for a while: amplifier off
            continue;
        }
        if (s_off || s_mute || s_vol == 0) {
            if (s_off) {
                snd_stop(&s_snd);
                codec_close();
            }
            continue;
        }
        if (!s_open) {
            if (!codec_open()) {
                continue;
            }
            applied_vol = s_vol;
        }
        take(&r);
        while (snd_busy(&s_snd) && !s_off) {
            while (xQueueReceive(s_q, &r, 0) == pdTRUE) {
                if (!s_mute && s_vol) {
                    take(&r);
                }
            }
            if (applied_vol != s_vol && s_vol) {
                applied_vol = s_vol;
                esp_codec_dev_set_out_vol(s_spk, CODEC_VOL[applied_vol]);
            }
            snd_render(&s_snd, s_buf, BLOCK);
            esp_codec_dev_write(s_spk, s_buf, sizeof(s_buf));
        }
        if (s_off) {
            snd_stop(&s_snd);
            codec_close();
        } else {
            memset(s_buf, 0, sizeof(s_buf));   // flush the tail out of the DMA buffers
            esp_codec_dev_write(s_spk, s_buf, sizeof(s_buf));
        }
    }
}

void pet_audio_init(void)
{
    snd_init(&s_snd, (uint32_t)pet_mono_ms() | 1u);
    s_q = xQueueCreate(Q_LEN, sizeof(req_t));
    if (s_q == NULL || xTaskCreate(audio_task, "audio", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "audio task not created: no sound");
        s_q = NULL;
    }
}

void pet_sound_pitch(snd_id_t id, int semitones)
{
    if (s_q == NULL || s_mute || s_vol == 0 || id >= SND_COUNT) {
        return;
    }
    s_off = false;
    const req_t r = {(uint8_t)id, (uint8_t)(semitones < 0 ? 0 : semitones > 12 ? 12 : semitones)};
    xQueueSend(s_q, &r, 0);   // queue full: this one is dropped
}

void pet_sound(snd_id_t id) { pet_sound_pitch(id, 0); }

void pet_sound_volume(int level) { s_vol = (uint8_t)(level < 0 ? 0 : level > PET_VOL_MAX ? PET_VOL_MAX : level); }

void pet_sound_mute(bool mute) { s_mute = mute; }

void pet_sound_species(int stage, int branch, uint32_t seed)
{
    if (stage != s_stage || branch != s_branch || seed != s_seed) {
        s_stage = stage;
        s_branch = branch;
        s_seed = seed;
        s_species_dirty = true;
    }
}

void pet_sound_off(void)
{
    if (s_q == NULL) {
        return;
    }
    s_off = true;
    const req_t wake = {SND_TICK, 0};   // unblocks the task if it is waiting
    xQueueSend(s_q, &wake, 0);
    for (int i = 0; i < OFF_WAIT_MS / 10 && s_open; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
