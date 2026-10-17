// Scene (pet_render -> 288x288 RGB565 in PSRAM -> lv_image), HUD, tray and the pantry,
// status and settings panels. Text is plain ASCII: the Montserrat fonts have no accents.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "pet_priv.h"
#include "pet_render.h"

#define SCALE 6
#define SCENE (PET_W * SCALE)
#define SCENE_Y 100
#define LOCK_MS 100
#define DECIDE_MS 10000
#define TOAST_MS 4000
#define SHIFT_MS 60000
#define PANEL_BG 0x0b1020

static const char *TAG = "pet_ui";

typedef enum { PANEL_NONE, PANEL_PANTRY, PANEL_STATUS, PANEL_SETTINGS } panel_t;

static uint16_t *s_buf[2];
static int s_back; // buffer the renderer draws into; the other one is on screen
static lv_image_dsc_t s_dsc;
static lv_obj_t *s_img, *s_time, *s_batt, *s_pantry, *s_toast;
static lv_obj_t *s_panel;
static panel_t s_kind;
static lv_obj_t *s_rows[3];     // pantry rows
static lv_obj_t *s_bars[3];     // status bars
static lv_obj_t *s_info;        // status text
static lv_obj_t *s_night_lbl[2];
static lv_obj_t *s_reset_lbl;
static bool s_reset_armed;
static uint32_t s_reset_armed_ms;

static volatile panel_t s_panel_shown = PANEL_NONE; // read by ui_task without the LVGL lock
static pet_anim_t s_ov_anim;
static uint32_t s_ov_start;
static bool s_ov_active;
static uint32_t s_toast_until;
static char s_toast_text[32];

static const char *STAGE[] = {"Huevo", "Cria", "Juvenil", "Adulto"};
static const char *BRANCH[] = {"", "Casero", "Explorador"};
static const char *VARIANT[] = {"", "Pulido", "Torpe", "Raro"};
static const char *MOOD[] = {"Durmiendo", "Con hambre", "Con sueno", "Triste",
                             "Aburrido", "Curioso", "Contento", "Tranquilo"};
static const char *FOOD_NAME[] = {"Casero", "Lejano", "Ciudad"};

// ---------- small helpers ----------

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int w, int h, lv_event_cb_t cb, void *user,
                             lv_obj_t **label_out)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *l = make_label(b, &lv_font_montserrat_16, 0xf8fafc);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (label_out) {
        *label_out = l;
    }
    return b;
}

static void toast(const char *text)
{
    snprintf(s_toast_text, sizeof(s_toast_text), "%s", text);
    s_toast_until = pet_mono_ms() + TOAST_MS;
    if (s_toast) {
        lv_label_set_text(s_toast, s_toast_text);
        lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

static void start_override(pet_anim_t a)
{
    s_ov_anim = a;
    s_ov_start = pet_mono_ms();
    s_ov_active = true;
}

static void fmt_ttl(char *out, size_t n, int64_t secs)
{
    if (secs < 0) {
        snprintf(out, n, "vacio");
    } else if (secs >= 3600) {
        snprintf(out, n, "caduca en %d h", (int)((secs + 1800) / 3600));
    } else {
        snprintf(out, n, "caduca en %d min", (int)(secs / 60) + 1);
    }
}

// ---------- panels ----------

static void close_panel(void)
{
    if (s_panel) {
        lv_obj_delete(s_panel);
        s_panel = NULL;
    }
    s_kind = PANEL_NONE;
    s_panel_shown = PANEL_NONE;
    s_reset_armed = false;
}

static void on_back(lv_event_t *e) { close_panel(); }

static void on_feed(lv_event_t *e)
{
    pet_food_t type = (pet_food_t)(intptr_t)lv_event_get_user_data(e);
    pet_lock();
    bool ok = pet_feed(&g_pet, type, pet_now_s());
    pet_unlock();
    close_panel();
    if (ok) {
        start_override(ANIM_EAT);
        pet_mark_dirty();
    } else {
        toast("No quiere comer");
    }
}

static lv_obj_t *open_panel(panel_t kind, const char *title)
{
    close_panel();
    s_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_panel, 410, 502);
    lv_obj_set_pos(s_panel, 0, 0);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(PANEL_BG), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, 0, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *t = make_label(s_panel, &lv_font_montserrat_24, 0x67e8f9);
    lv_label_set_text(t, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
    make_button(s_panel, "Volver", 160, 54, on_back, NULL, NULL);
    lv_obj_align(lv_obj_get_child(s_panel, -1), LV_ALIGN_BOTTOM_MID, 0, -30);
    s_kind = kind;
    s_panel_shown = kind;
    return s_panel;
}

static void open_pantry(void)
{
    open_panel(PANEL_PANTRY, "Despensa");
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = make_button(s_panel, "", 330, 68, on_feed, (void *)(intptr_t)i, &s_rows[i]);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 96 + i * 82);
    }
}

static void open_status(void)
{
    open_panel(PANEL_STATUS, "Estado");
    static const char *names[3] = {"Saciedad", "Energia", "Animo"};
    static const uint32_t colors[3] = {0xf97316, 0x38bdf8, 0xf472b6};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *l = make_label(s_panel, &lv_font_montserrat_16, 0xcbd5e1);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 60, 100 + i * 50);
        s_bars[i] = lv_bar_create(s_panel);
        lv_obj_set_size(s_bars[i], 170, 16);
        lv_bar_set_range(s_bars[i], 0, 100);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(colors[i]), LV_PART_INDICATOR);
        lv_obj_align(s_bars[i], LV_ALIGN_TOP_LEFT, 180, 104 + i * 50);
    }
    s_info = make_label(s_panel, &lv_font_montserrat_16, 0xf8fafc);
    lv_obj_set_width(s_info, 330);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 262);
}

static void on_night(lv_event_t *e)
{
    int code = (int)(intptr_t)lv_event_get_user_data(e); // +-1 start, +-2 end
    pet_lock();
    uint8_t *h = (code == 1 || code == -1) ? &g_pet.night_start_h : &g_pet.night_end_h;
    *h = (uint8_t)((*h + 24 + (code > 0 ? 1 : -1)) % 24);
    pet_unlock();
    pet_mark_dirty();
}

static void on_reset(lv_event_t *e)
{
    uint32_t now = pet_mono_ms();
    if (!s_reset_armed || now - s_reset_armed_ms > 5000) {
        s_reset_armed = true;
        s_reset_armed_ms = now;
        lv_label_set_text(s_reset_lbl, "Seguro? Toca otra vez");
        return;
    }
    pet_reset_game();
    close_panel();
    toast("Huevo nuevo");
}

static void open_settings(void)
{
    open_panel(PANEL_SETTINGS, "Ajustes");
    static const char *names[2] = {"Dormir desde", "Despierta a las"};
    for (int i = 0; i < 2; i++) {
        int y = 110 + i * 80;
        lv_obj_t *l = make_label(s_panel, &lv_font_montserrat_16, 0xcbd5e1);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
        int code = i + 1;
        lv_obj_t *m = make_button(s_panel, "-", 64, 44, on_night, (void *)(intptr_t)-code, NULL);
        lv_obj_align(m, LV_ALIGN_TOP_MID, -100, y + 26);
        lv_obj_t *p = make_button(s_panel, "+", 64, 44, on_night, (void *)(intptr_t)code, NULL);
        lv_obj_align(p, LV_ALIGN_TOP_MID, 100, y + 26);
        s_night_lbl[i] = make_label(s_panel, &lv_font_montserrat_28, 0xf8fafc);
        lv_obj_align(s_night_lbl[i], LV_ALIGN_TOP_MID, 0, y + 28);
    }
    lv_obj_t *r = make_button(s_panel, "Reiniciar mascota", 280, 50, on_reset, NULL, &s_reset_lbl);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x7f1d1d), 0);
    lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 290);
}

static void panel_refresh(int64_t now)
{
    char buf[96], ttl[32];
    pet_lock();
    pet_state_t *s = &g_pet;
    switch (s_kind) {
    case PANEL_PANTRY:
        for (int i = 0; i < 3; i++) {
            int n = pet_pantry_count(s, (pet_food_t)i);
            fmt_ttl(ttl, sizeof(ttl), pet_pantry_expires_in(s, (pet_food_t)i, now));
            snprintf(buf, sizeof(buf), "%s  x%d\n%s", FOOD_NAME[i], n, ttl);
            lv_label_set_text(s_rows[i], buf);
            lv_obj_set_style_opa(lv_obj_get_parent(s_rows[i]), n ? LV_OPA_COVER : LV_OPA_40, 0);
        }
        break;
    case PANEL_STATUS: {
        lv_bar_set_value(s_bars[0], s->satiety / PET_ONE, LV_ANIM_OFF);
        lv_bar_set_value(s_bars[1], s->energy / PET_ONE, LV_ANIM_OFF);
        lv_bar_set_value(s_bars[2], s->mood / PET_ONE, LV_ANIM_OFF);
        int64_t age = pet_age_s(s, now);
        int n = snprintf(buf, sizeof(buf), "%s", STAGE[s->stage]);
        if (s->stage == PET_ADULT) {
            n += snprintf(buf + n, sizeof(buf) - n, " %s", BRANCH[s->branch]);
            if (s->variant) {
                n += snprintf(buf + n, sizeof(buf) - n, " %s", VARIANT[s->variant]);
            }
        }
        n += snprintf(buf + n, sizeof(buf) - n, "\n%s\nEdad %dd %dh   Redes %u", MOOD[pet_mood(s, now)],
                      (int)(age / 86400), (int)(age % 86400 / 3600), (unsigned)s->total_scans);
        lv_label_set_text(s_info, buf);
        break;
    }
    case PANEL_SETTINGS:
        snprintf(buf, sizeof(buf), "%02d:00", s->night_start_h);
        lv_label_set_text(s_night_lbl[0], buf);
        snprintf(buf, sizeof(buf), "%02d:00", s->night_end_h);
        lv_label_set_text(s_night_lbl[1], buf);
        break;
    default:
        break;
    }
    pet_unlock();
    if (s_kind == PANEL_SETTINGS && s_reset_armed && pet_mono_ms() - s_reset_armed_ms > 5000) {
        s_reset_armed = false;
        lv_label_set_text(s_reset_lbl, "Reiniciar mascota");
    }
}

// ---------- main screen events ----------

static void on_tray(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: open_pantry(); break;
    case 1: open_status(); break;
    default: open_settings(); break;
    }
    panel_refresh(pet_now_s());
}

static void on_creature(lv_event_t *e)
{
    if (s_kind != PANEL_NONE || lv_indev_get_gesture_dir(lv_indev_active()) != LV_DIR_NONE) {
        return;
    }
    pet_lock();
    pet_reaction_t r = pet_touch(&g_pet, pet_now_s(), pet_mono_ms());
    pet_unlock();
    if (r == REACT_PET || r == REACT_SHAKE) {
        start_override(ANIM_BOUNCE);
    }
}

static void on_gesture(lv_event_t *e)
{
    if (s_kind != PANEL_NONE) {
        return;
    }
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_active());
    if (d == LV_DIR_TOP) {
        open_pantry();
    } else if (d == LV_DIR_BOTTOM) {
        open_status();
    } else {
        return;
    }
    panel_refresh(pet_now_s());
}

bool pet_ui_back(void)
{
    if (s_kind == PANEL_NONE) {
        return false;
    }
    close_panel();
    return true;
}

void pet_ui_boot_action(void)
{
    if (s_kind == PANEL_NONE) {
        open_pantry();
        panel_refresh(pet_now_s());
    }
}

// ---------- init ----------

void pet_ui_init(void)
{
    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_calloc(SCENE * SCENE, 2, MALLOC_CAP_SPIRAM);
        if (!s_buf[i]) {
            ESP_LOGE(TAG, "no PSRAM for the scene buffer");
            return;
        }
    }
    s_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                        .w = SCENE, .h = SCENE, .stride = SCENE * 2},
                             .data_size = SCENE * SCENE * 2, .data = (const uint8_t *)s_buf[0]};
    s_back = 1;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);

    s_time = make_label(scr, &lv_font_montserrat_28, 0xf8fafc);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 28);
    s_batt = make_label(scr, &lv_font_montserrat_16, 0x94a3b8);
    lv_obj_align(s_batt, LV_ALIGN_TOP_MID, -140, 70);
    s_pantry = make_label(scr, &lv_font_montserrat_16, 0x94a3b8);
    lv_obj_align(s_pantry, LV_ALIGN_TOP_MID, 140, 70);
    s_toast = make_label(scr, &lv_font_montserrat_16, 0xfde68a);
    lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

    s_img = lv_image_create(scr);
    lv_image_set_src(s_img, &s_dsc);
    lv_obj_align(s_img, LV_ALIGN_TOP_MID, 0, SCENE_Y);
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_img, on_creature, LV_EVENT_CLICKED, NULL);

    static const char *tray[3] = {"Comer", "Estado", "Ajustes"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = make_button(scr, tray[i], 100, 56, on_tray, (void *)(intptr_t)i, NULL);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i - 1) * 108, 410);
    }
}

// ---------- tick (ui_task) ----------

static bool view_eq(const pet_view_t *a, const pet_view_t *b)
{
    return a->seed == b->seed && a->stage == b->stage && a->shape == b->shape && a->expr == b->expr &&
           a->squash == b->squash && a->dy == b->dy && a->look_x == b->look_x && a->variant == b->variant &&
           a->mouth_open == b->mouth_open && a->food == b->food && a->zstep == b->zstep && a->blink == b->blink;
}

void pet_ui_tick(void)
{
    static uint8_t idx[PET_W * PET_H];
    static pet_view_t last;
    static bool have_last;
    static int64_t last_sec = -1;
    static uint32_t last_decide;
    static uint32_t last_shift;
    static int shift_n;

    if (!s_buf[0] || !s_buf[1]) {
        return;
    }
    const uint32_t mono = pet_mono_ms();
    const int64_t now = pet_now_s();
    const bool new_sec = now != last_sec;
    last_sec = now;

    pet_view_t v;
    pet_anim_t anim;
    uint32_t events;
    int pantry = 0;
    pet_lock();
    if (new_sec) {
        pet_step(&g_pet, now);
    }
    if (mono - last_decide >= DECIDE_MS) {
        last_decide = mono;
        pet_decide(&g_pet, now);
    }
    events = pet_take_events(&g_pet);
    anim = pet_view(&g_pet, now, mono, &v);
    for (int i = 0; i < 3; i++) {
        pantry += pet_pantry_count(&g_pet, (pet_food_t)i);
    }
    pet_unlock();

    if (events & (EVT_HATCHED | EVT_EVOLVED)) {
        ESP_LOGI(TAG, "event: %s", (events & EVT_HATCHED) ? "hatched" : "evolved");
    }
    if (new_sec && now % 60 == 0) {
        pet_lock();
        ESP_LOGI(TAG, "stage %d sat %d en %d mood %d pantry %d age %lld s", g_pet.stage, (int)(g_pet.satiety / PET_ONE),
                 (int)(g_pet.energy / PET_ONE), (int)(g_pet.mood / PET_ONE), pantry, (long long)pet_age_s(&g_pet, now));
        pet_unlock();
    }
    if (events & EVT_PECKED) {
        start_override(ANIM_EAT);
    }
    uint32_t t = mono;
    if (s_ov_active) {
        const uint32_t dur = pet_anim_duration_ms(s_ov_anim);
        if (dur && mono - s_ov_start >= dur) {
            s_ov_active = false;
        } else {
            anim = s_ov_anim;
            t = mono - s_ov_start;
        }
    }
    pet_pose(&v, anim, t);
    if (events & (EVT_HATCHED | EVT_EVOLVED)) {
        pet_mark_dirty();
    }

    const bool scene_visible = s_panel_shown == PANEL_NONE;
    const bool redraw = scene_visible && (!have_last || !view_eq(&v, &last));
    if (redraw) {
        uint16_t pal[PAL_COUNT];
        pet_render_creature(&v, idx, PET_W, PET_H);
        pet_palette_rgb565(v.seed, pal);
        pet_blit_scaled_rgb565(idx, PET_W, PET_H, SCALE, pal, s_buf[s_back], SCENE);
        last = v;
        have_last = true;
    }

    if (!redraw && !new_sec && !(s_toast_until && (int32_t)(mono - s_toast_until) >= 0)) {
        return;
    }
    if (!bsp_display_lock(LOCK_MS)) {
        have_last = false; // the swap did not happen: draw again next tick
        return;
    }
    if (redraw) {
        s_dsc.data = (const uint8_t *)s_buf[s_back];
        lv_image_set_src(s_img, &s_dsc);
        lv_obj_invalidate(s_img);
        s_back ^= 1;
    }
    if (new_sec) {
        char b[24];
        struct tm tm;
        time_t tt = (time_t)now;
        gmtime_r(&tt, &tm);
        snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(s_time, b);
        int pct = g_batt_pct;
        if (pct < 0) {
            lv_label_set_text(s_batt, g_usb ? LV_SYMBOL_CHARGE : "");
        } else {
            snprintf(b, sizeof(b), "%s%d%%", g_usb ? LV_SYMBOL_CHARGE : "", pct);
            lv_label_set_text(s_batt, b);
        }
        snprintf(b, sizeof(b), "Despensa %d", pantry);
        lv_label_set_text(s_pantry, b);
        if (new_sec && now % 60 == 0 && mono - last_shift >= SHIFT_MS / 2) { // anti burn-in
            last_shift = mono;
            shift_n = (shift_n + 1) % 5;
            lv_obj_align(s_img, LV_ALIGN_TOP_MID, (shift_n % 3 - 1) * 2, SCENE_Y + (shift_n / 2 - 1) * 2);
        }
        if (s_kind != PANEL_NONE) {
            panel_refresh(now);
        }
    }
    if (events & EVT_HATCHED) toast("Ha nacido!");
    else if (events & EVT_EVOLVED) toast("Ha evolucionado!");
    else if (events & EVT_NEW_SIGNAL) toast("Senal nueva!");
    if (s_toast_until && (int32_t)(mono - s_toast_until) >= 0) {
        s_toast_until = 0;
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
}
