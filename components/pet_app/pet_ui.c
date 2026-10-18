// Screens (diseno v2, docs/PET_DESIGN.md section 10): home (HUD, needs, scene, tray, XP),
// Comer, Jugar + the minigame screen, and Mas (Estado, Armario, Album, Ajustes).
// The scene and the games are drawn by pet_scene / pet_game into a 400x280 RGB565 buffer in
// PSRAM shown through an lv_image (double buffered). Text is plain ASCII: the Montserrat
// fonts have no accents. Runs unchanged in tools/uisim.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "pet_game.h"
#include "pet_gfx.h"
#include "pet_priv.h"
#include "pet_scene.h"

#define SCENE_X 5
#define SCENE_Y 106
#define LOCK_MS 100
#define DECIDE_MS 8000
#define TOAST_MS 3500
#define SHIFT_MS 60000
#define BTN_BG 0x1e293b
#define PANEL_BG 0x0b1020
#define WALK_PX 2          // per 40 ms tick
#define PREVIEW_W 96
#define PREVIEW_H 60
#define ALBUM_W 368
#define ALBUM_H 320

static const char *TAG = "pet_ui";

typedef enum { P_NONE, P_FOOD, P_PLAY, P_GAME, P_MORE, P_STATUS, P_SHOP, P_ALBUM, P_SETTINGS } panel_t;

// ---------- names (ASCII) ----------
static const char *const STAGE[] = {"Huevo", "Cria", "Juvenil", "Adulto"};
static const char *const BRANCH[] = {"", "Hogareno", "Saltarin", "Sabio", "Forzudo"};
static const char *const VARIANT[] = {"Normal", "Pulido", "Torpe", "Raro"};
static const char *const MOOD[] = {"Durmiendo", "Malito", "Con hambre", "Sucio", "Cansado", "Aburrido", "Contento",
                                   "Tranquilo"};
static const char *const ITEM_NAME[ITEM_COUNT] = {"Pan", "Fruta", "Guiso", "Pastel", "Chuche de senal", "Medicina"};
static const char *const ACC_NAME[ACC_COUNT] = {"Nada", "Lazo", "Flor", "Gorro", "Gafas", "Chistera", "Corona",
                                                "Radar", "Salacot"};
static const char *const TRAIN_NAME[GAME_COUNT] = {"Agilidad", "Ingenio", "Fuerza"};
static const pet_icon_t GAME_ICON[GAME_COUNT] = {ICON_STAR, ICON_GRID, ICON_DUMBBELL};
static const uint32_t NEED_COLOR[NEED_COUNT] = {0xf97316, 0xf472b6, 0x22d3ee, 0xfacc15};
static const pet_icon_t NEED_ICON[NEED_COUNT] = {ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_BOLT};

// ---------- buffers and widgets ----------
static uint16_t *s_buf[2];
static int s_back;                 // buffer being drawn; the other is on screen
static lv_image_dsc_t s_dsc;
static lv_obj_t *s_img;            // home scene
static lv_obj_t *s_time, *s_batt, *s_level, *s_coins, *s_gain, *s_toast, *s_xp_bar, *s_xp_lbl;
static lv_obj_t *s_need_bar[NEED_COUNT];
static lv_obj_t *s_panel;
static panel_t s_kind;
static volatile panel_t s_shown = P_NONE; // read by ui_task without the LVGL lock

// panel widgets refreshed every second
static lv_obj_t *s_rows[10], *s_row_lbl[10], *s_row_right[10];
static lv_obj_t *s_bars[NEED_COUNT], *s_bar_lbl[NEED_COUNT];
static lv_obj_t *s_info, *s_title_coins;
static lv_obj_t *s_night_lbl[2], *s_speed_btn[3], *s_reset_lbl;
static bool s_reset_armed;
static uint32_t s_reset_ms;
static uint16_t *s_preview[ACC_COUNT];
static lv_image_dsc_t s_preview_dsc[ACC_COUNT];
static uint16_t *s_album_px;
static lv_image_dsc_t s_album_dsc;

// toasts and gains
static char s_toast_text[48];
static uint32_t s_toast_until, s_gain_until;

// scene animation (ui_task; s_last_sc is copied under the LVGL lock for the tap handler)
static pet_scene_t s_last_sc;
static int s_px = PET_SCENE_XMAX / 2, s_target = PET_SCENE_XMAX / 2;
static pet_anim_t s_ov_anim;
static uint32_t s_ov_start;
static bool s_ov_active;
static volatile pet_fx_t s_fx;
static volatile uint32_t s_fx_start;
static volatile int s_fx_x, s_fx_y;

// minigame
static pet_game_t s_game;
static volatile int s_game_req = -1;  // set by the LVGL callback, started by ui_task
static volatile bool s_game_on, s_game_done;
static volatile pet_input_t s_in;
static uint32_t s_game_last;
static lv_image_dsc_t s_game_dsc;
static lv_obj_t *s_game_img, *s_game_hint, *s_game_title, *s_result;

// ---------- small helpers ----------

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static lv_obj_t *make_button(lv_obj_t *parent, int w, int h, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(BTN_BG), 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    return b;
}

static lv_obj_t *text_button(lv_obj_t *parent, const char *text, int w, int h, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = make_button(parent, w, h, cb, user);
    lv_obj_t *l = make_label(b, &lv_font_montserrat_16, 0xf8fafc);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

// Pixel-art icons as LVGL images, rendered once per (icon, scale, background) and cached.
typedef struct { uint8_t icon, scale; uint32_t bg; uint16_t *px; lv_image_dsc_t dsc; } icon_cache_t;
static icon_cache_t s_icons[40];

static const lv_image_dsc_t *icon_img(pet_icon_t icon, int scale, uint32_t bg)
{
    for (size_t i = 0; i < sizeof(s_icons) / sizeof(s_icons[0]); i++) {
        icon_cache_t *c = &s_icons[i];
        if (c->px && c->icon == icon && c->scale == scale && c->bg == bg) {
            return &c->dsc;
        }
        if (!c->px) {
            const int n = PET_ICON * scale;
            c->px = heap_caps_malloc((size_t)n * n * 2, MALLOC_CAP_SPIRAM);
            if (!c->px) {
                return NULL;
            }
            c->icon = (uint8_t)icon;
            c->scale = (uint8_t)scale;
            c->bg = bg;
            pet_icon_rgb565(icon, scale, pet_rgb565(bg), c->px);
            c->dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                 .w = (uint32_t)n, .h = (uint32_t)n, .stride = (uint32_t)n * 2},
                                      .data_size = (uint32_t)n * n * 2, .data = (const uint8_t *)c->px};
            return &c->dsc;
        }
    }
    return NULL;
}

static lv_obj_t *add_icon(lv_obj_t *parent, pet_icon_t icon, int scale, uint32_t bg)
{
    lv_obj_t *im = lv_image_create(parent);
    const lv_image_dsc_t *d = icon_img(icon, scale, bg);
    if (d) {
        lv_image_set_src(im, d);
    }
    lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
    return im;
}

// On the home screen the toast sits on top of the scene; over a panel, above its bottom
// button. Never over a game (the canvas and the result card are the feedback there).
static void toast(const char *text)
{
    if (!s_toast || (s_kind == P_GAME && !s_result)) {
        return;
    }
    snprintf(s_toast_text, sizeof(s_toast_text), "%s", text);
    s_toast_until = pet_mono_ms() + TOAST_MS;
    lv_label_set_text(s_toast, s_toast_text);
    if (s_kind == P_NONE) {
        lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 4);
    } else {
        lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -88);
    }
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
}

static void start_override(pet_anim_t a)
{
    s_ov_anim = a;
    s_ov_start = pet_mono_ms();
    s_ov_active = true;
}

static void start_fx(pet_fx_t fx, int x, int y)
{
    s_fx_x = x;
    s_fx_y = y;
    s_fx_start = pet_mono_ms();
    s_fx = fx;
}

static void need_bar_style(lv_obj_t *bar, int i)
{
    lv_obj_set_style_bg_color(bar, lv_color_hex(BTN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(NEED_COLOR[i]), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
}

static void set_need(lv_obj_t *bar, int i, int v)
{
    lv_bar_set_value(bar, v, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(v < 25 ? 0xef4444 : NEED_COLOR[i]), LV_PART_INDICATOR);
}

// ---------- panels: common ----------

static void free_panel_buffers(void)
{
    for (int i = 0; i < ACC_COUNT; i++) {
        free(s_preview[i]);
        s_preview[i] = NULL;
    }
    free(s_album_px);
    s_album_px = NULL;
}

static void close_panel(void)
{
    if (s_panel) {
        lv_obj_delete(s_panel);
        s_panel = NULL;
    }
    free_panel_buffers();
    s_kind = P_NONE;
    s_shown = P_NONE;
    s_reset_armed = false;
    s_result = NULL;
    s_game_img = NULL;
    s_info = NULL;
    memset(s_rows, 0, sizeof(s_rows));
    if (s_toast) {
        lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 4);
    }
}

static void on_back(lv_event_t *e);

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
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_t *b = text_button(s_panel, "Volver", 150, 46, on_back, NULL);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -30);
    s_title_coins = NULL;
    s_kind = kind;
    s_shown = kind;
    if (s_toast) {                        // a toast from the previous screen does not follow
        s_toast_until = 0;
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
    return s_panel;
}

// Row button: icon (optional), title + detail on the left, a short text on the right.
static lv_obj_t *list_row(lv_obj_t *parent, int idx, int y, int h, const lv_image_dsc_t *img, lv_event_cb_t cb)
{
    lv_obj_t *b = make_button(parent, 350, h, cb, (void *)(intptr_t)idx);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    int x = 14;
    if (img) {
        lv_obj_t *im = lv_image_create(b);
        lv_image_set_src(im, img);
        lv_obj_align(im, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
        x = 20 + (int)img->header.w;
    }
    lv_obj_t *l = make_label(b, &lv_font_montserrat_16, 0xf8fafc);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(l, 350 - x - 70);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_t *r = make_label(b, &lv_font_montserrat_16, 0xfde68a);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, -14, 0);
    if (idx >= 0 && idx < 10) {
        s_rows[idx] = b;
        s_row_lbl[idx] = l;
        s_row_right[idx] = r;
    }
    return b;
}

static void coins_in_title(void)
{
    lv_obj_t *ic = add_icon(s_panel, ICON_COIN, 2, PANEL_BG);
    lv_obj_align(ic, LV_ALIGN_TOP_RIGHT, -96, 28);
    s_title_coins = make_label(s_panel, &lv_font_montserrat_20, 0xfde68a);
    lv_obj_set_style_text_align(s_title_coins, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_title_coins, LV_ALIGN_TOP_LEFT, 410 - 90, 30);
}

// ---------- Comer ----------

static void on_food(lv_event_t *e)
{
    const pet_item_t it = (pet_item_t)(intptr_t)lv_event_get_user_data(e);
    pet_lock();
    const pet_feed_t r = pet_feed(&g_pet, it, pet_now_s());
    const bool fav = it == pet_favorite(&g_pet);
    pet_unlock();
    switch (r) {
    case FEED_OK:
        close_panel();
        start_override(ANIM_EAT);
        if (fav && it < PET_MENU_COUNT) {
            start_fx(FX_HEARTS, 0, 0);
            toast("Su plato favorito!");
        } else if (it == ITEM_MEDICINE) {
            toast("Ya esta mejor");
        }
        pet_mark_dirty();
        break;
    case FEED_FULL: toast("Esta lleno"); break;
    case FEED_NO_COINS: toast("Faltan monedas: juega para ganar"); break;
    case FEED_ASLEEP: toast("Esta durmiendo"); break;
    case FEED_NOT_SICK: toast("No esta malito"); break;
    case FEED_EGG: toast("Aun es un huevo"); break;
    default: toast("No te queda: huele redes para encontrar"); break;
    }
}

static void open_food(void)
{
    open_panel(P_FOOD, "Comer");
    coins_in_title();
    for (int i = 0; i < ITEM_COUNT; i++) {
        list_row(s_panel, i, 70 + i * 56, 50, icon_img((pet_icon_t)i, 3, BTN_BG), on_food);
    }
}

static void refresh_food(const pet_state_t *s)
{
    char b[64];
    const pet_item_t fav = pet_favorite(s);
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!s_rows[i]) {
            continue;
        }
        int food, fun, xp;
        pet_item_effect((pet_item_t)i, &food, &fun, &xp);
        const bool known_fav = i == (int)fav && (s->tasted >> i & 1);
        if (i == ITEM_MEDICINE) {
            snprintf(b, sizeof(b), "%s\n#94a3b8 cura el malito#", ITEM_NAME[i]);
        } else if (fun) {
            snprintf(b, sizeof(b), "%s%s\n#94a3b8 +%d comida +%d diversion#", ITEM_NAME[i], known_fav ? " (fav)" : "",
                     food, fun);
        } else {
            snprintf(b, sizeof(b), "%s%s\n#94a3b8 +%d comida#", ITEM_NAME[i], known_fav ? " (fav)" : "", food);
        }
        lv_label_set_recolor(s_row_lbl[i], true);
        lv_label_set_text(s_row_lbl[i], b);
        const int price = pet_item_price((pet_item_t)i);
        if (s->inv[i]) {
            snprintf(b, sizeof(b), "x%d", s->inv[i]);
        } else if (price) {
            snprintf(b, sizeof(b), "%d", price);
        } else {
            snprintf(b, sizeof(b), "x0");
        }
        lv_label_set_text(s_row_right[i], b);
        const bool can = s->inv[i] || (price && s->coins >= (uint32_t)price);
        lv_obj_set_style_opa(s_rows[i], can ? LV_OPA_COVER : LV_OPA_50, 0);
    }
}

// ---------- Jugar ----------

static void request_game(int id)
{
    s_game_done = false;
    s_game_req = id;
}

static void on_play(lv_event_t *e)
{
    const pet_game_id_t g = (pet_game_id_t)(intptr_t)lv_event_get_user_data(e);
    pet_lock();
    const pet_play_t r = pet_can_play(&g_pet, g, pet_now_s());
    pet_unlock();
    char b[48];
    switch (r) {
    case PLAY_OK: request_game(g); break;
    case PLAY_TIRED: toast("Esta cansado: dejalo dormir"); break;
    case PLAY_SICK: toast("Esta malito: dale medicina"); break;
    case PLAY_ASLEEP: toast("Esta durmiendo"); break;
    case PLAY_EGG: toast("Aun es un huevo"); break;
    default:
        snprintf(b, sizeof(b), "Se desbloquea en el nivel %d", pet_game_level(g));
        toast(b);
        break;
    }
}

static void open_play(void)
{
    open_panel(P_PLAY, "Jugar");
    coins_in_title();
    for (int i = 0; i < GAME_COUNT; i++) {
        list_row(s_panel, i, 74 + i * 96, 88, icon_img(GAME_ICON[i], 4, BTN_BG), on_play);
    }
    s_info = make_label(s_panel, &lv_font_montserrat_16, 0x94a3b8);
    lv_obj_set_width(s_info, 340);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 370);
}

static void refresh_play(const pet_state_t *s, int64_t now)
{
    char b[96];
    for (int i = 0; i < GAME_COUNT; i++) {
        if (!s_rows[i]) {
            continue;
        }
        const pet_play_t r = pet_can_play(s, (pet_game_id_t)i, now);
        if (r == PLAY_LOCKED) {
            snprintf(b, sizeof(b), "%s\n#94a3b8 nivel %d#", pet_game_name((pet_game_id_t)i), pet_game_level((pet_game_id_t)i));
        } else {
            snprintf(b, sizeof(b), "%s\n#94a3b8 %s - record %d#", pet_game_name((pet_game_id_t)i), TRAIN_NAME[i], s->best[i]);
        }
        lv_label_set_recolor(s_row_lbl[i], true);
        lv_label_set_text(s_row_lbl[i], b);
        lv_label_set_text(s_row_right[i], r == PLAY_LOCKED ? LV_SYMBOL_CLOSE : LV_SYMBOL_PLAY);
        lv_obj_set_style_opa(s_rows[i], r == PLAY_OK ? LV_OPA_COVER : LV_OPA_50, 0);
    }
    if (s_info) {
        snprintf(b, sizeof(b), "Energia %d. Cada partida gasta 8.", pet_need(s, NEED_ENERGY));
        lv_label_set_text(s_info, b);
    }
}

// ---------- minigame screen ----------

static void on_game_touch(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    pet_input_t in = {0};
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_area_t a;
        lv_indev_get_point(lv_indev_active(), &p);
        lv_obj_get_coords(s_game_img, &a);
        in.down = true;
        in.x = (int16_t)(p.x - a.x1);
        in.y = (int16_t)(p.y - a.y1);
    }
    s_in = in;
}

static void on_again(lv_event_t *e)
{
    const int id = s_game.id;
    pet_lock();
    const pet_play_t r = pet_can_play(&g_pet, (pet_game_id_t)id, pet_now_s());
    pet_unlock();
    if (r == PLAY_OK) {
        request_game(id);
    } else {
        toast(r == PLAY_TIRED ? "Esta cansado: dejalo dormir" : "No puede jugar ahora");
    }
}

static void on_game_exit(lv_event_t *e)
{
    s_game_on = false;
    s_game_req = -1;
    open_play();
}

// Built by ui_task (under the LVGL lock) when a requested game starts.
static void open_game_screen(void)
{
    open_panel(P_GAME, pet_game_name((pet_game_id_t)s_game.id));
    // Replace the generic "Volver" with an exit that also stops the game.
    lv_obj_delete(lv_obj_get_child(s_panel, 1));
    lv_obj_t *x = text_button(s_panel, "Salir", 150, 46, on_game_exit, NULL);
    lv_obj_align(x, LV_ALIGN_BOTTOM_MID, 0, -30);
    s_game_title = lv_obj_get_child(s_panel, 0);
    s_game_dsc = s_dsc;
    s_game_img = lv_image_create(s_panel);
    lv_image_set_src(s_game_img, &s_game_dsc);
    lv_obj_set_pos(s_game_img, SCENE_X, 70);
    lv_obj_add_flag(s_game_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_game_img, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s_game_img, on_game_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_game_img, on_game_touch, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_game_img, on_game_touch, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_game_img, on_game_touch, LV_EVENT_PRESS_LOST, NULL);
    s_game_hint = make_label(s_panel, &lv_font_montserrat_16, 0x94a3b8);
    lv_obj_set_width(s_game_hint, 370);
    lv_label_set_long_mode(s_game_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_game_hint, LV_ALIGN_TOP_MID, 0, 360);
}

static void show_result(const pet_reward_t *r, int score, int perf)
{
    s_result = lv_obj_create(s_panel);
    lv_obj_set_size(s_result, 360, 300);
    lv_obj_align(s_result, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(s_result, lv_color_hex(0x111a33), 0);
    lv_obj_set_style_border_color(s_result, lv_color_hex(0xffcd75), 0);
    lv_obj_set_style_border_width(s_result, 3, 0);
    lv_obj_set_style_radius(s_result, 24, 0);
    lv_obj_remove_flag(s_result, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *t = make_label(s_result, &lv_font_montserrat_28, 0xfde68a);
    lv_label_set_text(t, perf >= 80 ? "Genial!" : perf >= 40 ? "Bien hecho" : "Otra vez sera");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_t *d = make_label(s_result, &lv_font_montserrat_20, 0xf8fafc);
    char b[160];
    int n = snprintf(b, sizeof(b), "Puntos %d%s\n+%d monedas  +%d XP\n+%d diversion", score, r->record ? " (record!)" : "",
                     r->coins, r->xp, r->fun);
    if (r->train) {
        snprintf(b + n, sizeof(b) - n, "\n%s +%d", TRAIN_NAME[s_game.id], r->train);
    }
    lv_label_set_text(d, b);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_t *a = text_button(s_result, "Otra vez", 150, 50, on_again, NULL);
    lv_obj_set_style_bg_color(a, lv_color_hex(0x15803d), 0);
    lv_obj_align(a, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ui_task: start a requested game, run one step, render and swap. Returns false when no
// game screen is active.
static bool game_tick(uint32_t mono)
{
    const int req = s_game_req;
    if (req >= 0) {
        s_game_req = -1;
        pet_view_t v;
        pet_lock();
        pet_view(&g_pet, pet_now_s(), mono, &v);
        pet_unlock();
        v.expr = EXPR_NEUTRAL;
        pet_game_start(&s_game, (pet_game_id_t)req, mono ^ 0x5bd1e995u, &v);
        s_in = (pet_input_t){0};
        s_game_last = mono;
        s_game_done = false;
        if (!bsp_display_lock(1000)) {
            return true;
        }
        open_game_screen();
        bsp_display_unlock();
        s_game_on = true;
    }
    if (!s_game_on) {
        return false;
    }
    if (s_game_done) {
        return true;
    }
    uint32_t dt = mono - s_game_last;
    s_game_last = mono;
    const pet_input_t in = s_in;
    pet_game_update(&s_game, dt, &in);
    pet_game_render(&s_game, s_buf[s_back]);
    const bool over = pet_game_over(&s_game);
    pet_reward_t r = {0};
    const int score = pet_game_score(&s_game), perf = pet_game_perf(&s_game);
    if (over) {
        pet_lock();
        r = pet_game_finish(&g_pet, (pet_game_id_t)s_game.id, perf, score, pet_now_s());
        int c, x;
        pet_take_gains(&g_pet, &c, &x);       // shown on the result card instead
        pet_unlock();
        pet_mark_dirty();
        ESP_LOGI(TAG, "game %d over: score %d perf %d coins %d", s_game.id, score, perf, r.coins);
    }
    if (!bsp_display_lock(LOCK_MS)) {
        return true;
    }
    if (s_kind == P_GAME && s_game_img) {
        s_game_dsc.data = (const uint8_t *)s_buf[s_back];
        lv_image_set_src(s_game_img, &s_game_dsc);
        lv_obj_invalidate(s_game_img);
        s_back ^= 1;
        const char *hint = pet_game_hint(&s_game);
        if (strcmp(lv_label_get_text(s_game_hint), hint) != 0) {
            lv_label_set_text(s_game_hint, hint);
        }
        lv_display_trigger_activity(NULL); // a game in progress never times the screen out
        if (over) {
            s_game_done = true;
            show_result(&r, score, perf);
        }
    } else {
        s_game_on = false;                   // the panel was closed (PWR)
    }
    bsp_display_unlock();
    return true;
}

// ---------- Mas ----------

static void open_status(void);
static void open_and_refresh(void (*open)(void));
static void open_shop(void);
static void open_album(void);
static void open_settings(void);

static void on_newgen(lv_event_t *e)
{
    pet_lock();
    const bool ok = pet_new_generation(&g_pet, pet_now_s());
    pet_unlock();
    close_panel();
    if (ok) {
        pet_mark_dirty();
        toast("Se va de viaje y deja un huevo");
    }
}

static void on_more(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: open_and_refresh(open_status); break;
    case 1: open_and_refresh(open_shop); break;
    case 2: open_and_refresh(open_album); break;
    case 3: open_and_refresh(open_settings); break;
    default: on_newgen(e); return;
    }
}

static void open_more(void)
{
    open_panel(P_MORE, "Mas");
    static const char *const names[] = {"Estado", "Armario", "Album", "Ajustes"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = text_button(s_panel, names[i], 300, 54, on_more, (void *)(intptr_t)i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 76 + i * 64);
    }
    pet_lock();
    const bool gen = pet_can_new_generation(&g_pet, pet_now_s());
    pet_unlock();
    if (gen) {
        lv_obj_t *b = text_button(s_panel, "Nueva generacion", 300, 54, on_more, (void *)(intptr_t)4);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x854d0e), 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 76 + 4 * 64);
    }
}

// ---------- Estado ----------

static void open_status(void)
{
    open_panel(P_STATUS, "Estado");
    static const char *const names[NEED_COUNT] = {"Comida", "Diversion", "Higiene", "Energia"};
    for (int i = 0; i < NEED_COUNT; i++) {
        lv_obj_t *ic = add_icon(s_panel, NEED_ICON[i], 2, PANEL_BG);
        lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 40, 72 + i * 38);
        lv_obj_t *l = make_label(s_panel, &lv_font_montserrat_16, 0xcbd5e1);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 74, 75 + i * 38);
        s_bars[i] = lv_bar_create(s_panel);
        lv_obj_set_size(s_bars[i], 140, 14);
        need_bar_style(s_bars[i], i);
        lv_obj_align(s_bars[i], LV_ALIGN_TOP_LEFT, 180, 78 + i * 38);
        s_bar_lbl[i] = make_label(s_panel, &lv_font_montserrat_16, 0xf8fafc);
        lv_obj_align(s_bar_lbl[i], LV_ALIGN_TOP_LEFT, 330, 75 + i * 38);
    }
    s_info = make_label(s_panel, &lv_font_montserrat_16, 0xf8fafc);
    lv_obj_set_width(s_info, 350);
    lv_label_set_recolor(s_info, true);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 230);
}

static void refresh_status(const pet_state_t *s, int64_t now)
{
    char b[400];
    for (int i = 0; i < NEED_COUNT; i++) {
        const int v = pet_need(s, (pet_need_t)i);
        set_need(s_bars[i], i, v);
        snprintf(b, sizeof(b), "%d", v);
        lv_label_set_text(s_bar_lbl[i], b);
    }
    const int64_t age = pet_age_s(s, now);
    int n = snprintf(b, sizeof(b), "%s", STAGE[s->stage]);
    if (s->stage == PET_ADULT) {
        n += snprintf(b + n, sizeof(b) - n, " %s %s", BRANCH[s->branch], VARIANT[s->variant]);
    }
    n += snprintf(b + n, sizeof(b) - n, " - %s\nEdad %dd %dh   Nivel %d   Racha %d\n", MOOD[pet_mood(s, now)],
                  (int)(age / 86400), (int)(age % 86400 / 3600), s->level, s->streak);
    int lvl;
    int64_t need_age;
    if (pet_evo_target(s, &lvl, &need_age) && s->stage != PET_EGG) {
        n += snprintf(b + n, sizeof(b) - n, "#fde68a Evoluciona: nivel %d y %dh de edad#\n", lvl, (int)(need_age / 3600));
    }
    if (s->stage == PET_BABY || s->stage == PET_JUVENILE) {
        n += snprintf(b + n, sizeof(b) - n, "Agilidad %d  Ingenio %d  Fuerza %d\n#67e8f9 Va para: %s#\n", s->train[0],
                      s->train[1], s->train[2], BRANCH[pet_branch_lean(s)]);
    }
    const int next = pet_next_milestone(s);
    if (next > 0) {
        snprintf(b + n, sizeof(b) - n, "Senales %u (regalo a las %d)", (unsigned)s->signals, next);
    } else {
        snprintf(b + n, sizeof(b) - n, "Senales %u", (unsigned)s->signals);
    }
    lv_label_set_text(s_info, b);
}

// ---------- Armario (shop + wardrobe) ----------

// The pet wearing `acc`, cropped to its head, on the button colour.
static void render_preview(int acc)
{
    s_preview[acc] = heap_caps_malloc(PREVIEW_W * PREVIEW_H * 2, MALLOC_CAP_SPIRAM);
    uint16_t *full = heap_caps_malloc(96 * 96 * 2, MALLOC_CAP_SPIRAM);
    if (!s_preview[acc] || !full) {
        free(full);
        return;
    }
    const uint16_t bg = pet_rgb565(BTN_BG);
    for (int i = 0; i < 96 * 96; i++) {
        full[i] = bg;
    }
    pet_view_t v;
    pet_lock();
    pet_view(&g_pet, pet_now_s(), 0, &v);
    pet_unlock();
    v.expr = EXPR_HAPPY;
    v.acc = (uint8_t)acc;
    if (v.stage == PET_EGG) {
        v.stage = PET_BABY;
        v.shape = 0;
    }
    pet_gfx_t g = {full, 96, 96};
    pet_gfx_creature(&g, &v, 0, 0, 2);
    int top = 0;
    while (top < 96 - PREVIEW_H) {
        bool any = false;
        for (int x = 0; x < 96 && !any; x++) {
            any = full[top * 96 + x] != bg;
        }
        if (any) {
            break;
        }
        top++;
    }
    top = top > 2 ? top - 2 : 0;
    memcpy(s_preview[acc], full + top * 96, PREVIEW_W * PREVIEW_H * 2);
    free(full);
    s_preview_dsc[acc] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                     .w = PREVIEW_W, .h = PREVIEW_H, .stride = PREVIEW_W * 2},
                                          .data_size = PREVIEW_W * PREVIEW_H * 2, .data = (const uint8_t *)s_preview[acc]};
}

static void on_acc(lv_event_t *e)
{
    const pet_acc_t a = (pet_acc_t)(intptr_t)lv_event_get_user_data(e);
    char b[48];
    pet_lock();
    pet_buy_t r = BUY_OK;
    if (g_pet.stage == PET_EGG) {
        r = BUY_NOT_SOLD;
    } else if (pet_acc_owned(&g_pet, a)) {
        pet_wear(&g_pet, g_pet.acc_worn == a ? ACC_NONE : a);
    } else {
        r = pet_buy_acc(&g_pet, a);
    }
    const int lvl = pet_acc_level(a);
    pet_unlock();
    switch (r) {
    case BUY_OK: pet_mark_dirty(); break;
    case BUY_LOCKED: snprintf(b, sizeof(b), "Se desbloquea en el nivel %d", lvl); toast(b); break;
    case BUY_NO_COINS: toast("Faltan monedas"); break;
    default: toast(a >= ACC_RADAR ? "Regalo por oler senales" : "Aun es un huevo"); break;
    }
}

static void open_shop(void)
{
    open_panel(P_SHOP, "Armario");
    coins_in_title();
    lv_obj_t *list = lv_obj_create(s_panel);
    lv_obj_set_size(list, 380, 330);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    for (int a = ACC_NONE + 1; a < ACC_COUNT; a++) {
        render_preview(a);
        list_row(list, a, (a - 1) * 74, 68, s_preview[a] ? &s_preview_dsc[a] : NULL, on_acc);
        lv_obj_remove_flag(s_rows[a], LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
}

static void refresh_shop(const pet_state_t *s)
{
    char b[64];
    for (int a = ACC_NONE + 1; a < ACC_COUNT; a++) {
        if (!s_rows[a]) {
            continue;
        }
        const bool own = pet_acc_owned(s, (pet_acc_t)a), worn = s->acc_worn == a;
        const int price = pet_acc_price((pet_acc_t)a), lvl = pet_acc_level((pet_acc_t)a);
        lv_label_set_recolor(s_row_lbl[a], true);
        if (own) {
            snprintf(b, sizeof(b), "%s\n#94a3b8 %s#", ACC_NAME[a], worn ? "puesto" : "tuyo");
            lv_label_set_text(s_row_right[a], worn ? LV_SYMBOL_OK : "");
        } else if (!price) {
            const int need = a == ACC_RADAR ? pet_milestone_signals(1) : pet_milestone_signals(3);
            snprintf(b, sizeof(b), "%s\n#94a3b8 regalo a las %d senales#", ACC_NAME[a], need);
            lv_label_set_text(s_row_right[a], "");
        } else if (s->level < lvl) {
            snprintf(b, sizeof(b), "%s\n#94a3b8 nivel %d - %d monedas#", ACC_NAME[a], lvl, price);
            lv_label_set_text(s_row_right[a], LV_SYMBOL_CLOSE);
        } else {
            snprintf(b, sizeof(b), "%s\n#94a3b8 comprar#", ACC_NAME[a]);
            char p[12];
            snprintf(p, sizeof(p), "%d", price);
            lv_label_set_text(s_row_right[a], p);
        }
        lv_label_set_text(s_row_lbl[a], b);
        const bool dim = !own && (!price || s->level < lvl || s->coins < (uint32_t)price);
        lv_obj_set_style_opa(s_rows[a], dim ? LV_OPA_60 : LV_OPA_COVER, 0);
    }
}

// ---------- Album ----------

static void open_album(void)
{
    open_panel(P_ALBUM, "Album");
    s_album_px = heap_caps_malloc(ALBUM_W * ALBUM_H * 2, MALLOC_CAP_SPIRAM);
    if (s_album_px) {
        const uint16_t bg = pet_rgb565(PANEL_BG);
        for (int i = 0; i < ALBUM_W * ALBUM_H; i++) {
            s_album_px[i] = bg;
        }
        pet_lock();
        const uint16_t album = g_pet.album;
        const uint32_t seed = g_pet.view_seed;
        pet_unlock();
        const uint16_t sil = pet_rgb565(0x1e293b);
        uint16_t tmp[96 * 96];
        for (int b = 0; b < 4; b++) {
            for (int v = 0; v < 4; v++) {
                const bool have = album >> (b * 4 + v) & 1;
                pet_view_t pv = {.seed = seed, .stage = PET_ADULT, .shape = (uint8_t)b, .variant = (uint8_t)v,
                                 .expr = EXPR_HAPPY};
                memset(tmp, 0, sizeof(tmp));
                pet_gfx_t t = {tmp, 96, 96};
                pet_gfx_creature(&t, &pv, 0, 0, 2);
                const int cx = v * 92 - 2, cy = b * 80 - 14;
                for (int y = 8; y < 96; y++) {
                    const int yy = cy + y;
                    if (yy < 0 || yy >= ALBUM_H) {
                        continue;
                    }
                    for (int x = 0; x < 96; x++) {
                        const int xx = cx + x;
                        if (xx >= 0 && xx < ALBUM_W && tmp[y * 96 + x]) {
                            s_album_px[yy * ALBUM_W + xx] = have ? tmp[y * 96 + x] : sil;
                        }
                    }
                }
            }
        }
        s_album_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                  .w = ALBUM_W, .h = ALBUM_H, .stride = ALBUM_W * 2},
                                       .data_size = ALBUM_W * ALBUM_H * 2, .data = (const uint8_t *)s_album_px};
        lv_obj_t *im = lv_image_create(s_panel);
        lv_image_set_src(im, &s_album_dsc);
        lv_obj_align(im, LV_ALIGN_TOP_MID, 0, 58);
    }
    s_info = make_label(s_panel, &lv_font_montserrat_14, 0xcbd5e1);
    lv_obj_set_width(s_info, 330);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 382);
}

static void refresh_album(const pet_state_t *s)
{
    char b[128];
    int forms = 0, foods = 0;
    for (int i = 0; i < 16; i++) {
        forms += s->album >> i & 1;
    }
    for (int i = 0; i < PET_MENU_COUNT; i++) {
        foods += s->tasted >> i & 1;
    }
    snprintf(b, sizeof(b), "Formas %d/16  Platos %d/4  Gen %d\nRecords: %d / %d / %d", forms, foods, s->generation + 1,
             s->best[0], s->best[1], s->best[2]);
    lv_label_set_text(s_info, b);
}

// ---------- Ajustes ----------

static void on_night(lv_event_t *e)
{
    const int code = (int)(intptr_t)lv_event_get_user_data(e); // +-1 start, +-2 end
    pet_lock();
    uint8_t *h = (code == 1 || code == -1) ? &g_pet.night_start_h : &g_pet.night_end_h;
    *h = (uint8_t)((*h + 24 + (code > 0 ? 1 : -1)) % 24);
    pet_unlock();
    pet_mark_dirty();
}

static void on_speed(lv_event_t *e)
{
    static const int SPEEDS[3] = {1, 10, 60};
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 3) {
        pet_skip_time(3600);
        toast("+1 hora");
        return;
    }
    pet_set_time_speed(SPEEDS[i]);
}

static void on_reset(lv_event_t *e)
{
    const uint32_t now = pet_mono_ms();
    if (!s_reset_armed || now - s_reset_ms > 5000) {
        s_reset_armed = true;
        s_reset_ms = now;
        lv_label_set_text(s_reset_lbl, "Seguro? Toca otra vez");
        return;
    }
    pet_reset_game();
    close_panel();
    toast("Huevo nuevo");
}

static void open_settings(void)
{
    open_panel(P_SETTINGS, "Ajustes");
    static const char *const names[2] = {"Duerme desde", "Despierta a las"};
    for (int i = 0; i < 2; i++) {
        const int y = 66 + i * 76;
        lv_obj_t *l = make_label(s_panel, &lv_font_montserrat_16, 0xcbd5e1);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_t *m = text_button(s_panel, "-", 64, 42, on_night, (void *)(intptr_t) - (i + 1));
        lv_obj_align(m, LV_ALIGN_TOP_MID, -100, y + 24);
        lv_obj_t *p = text_button(s_panel, "+", 64, 42, on_night, (void *)(intptr_t)(i + 1));
        lv_obj_align(p, LV_ALIGN_TOP_MID, 100, y + 24);
        s_night_lbl[i] = make_label(s_panel, &lv_font_montserrat_28, 0xf8fafc);
        lv_obj_align(s_night_lbl[i], LV_ALIGN_TOP_MID, 0, y + 28);
    }
    lv_obj_t *l = make_label(s_panel, &lv_font_montserrat_16, 0xcbd5e1);
    lv_label_set_text(l, "Velocidad del tiempo (prueba)");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 222);
    static const char *const sp[4] = {"x1", "x10", "x60", "+1h"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = text_button(s_panel, sp[i], 72, 42, on_speed, (void *)(intptr_t)i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, -117 + i * 78, 248);
        if (i < 3) {
            s_speed_btn[i] = b;
        }
    }
    lv_obj_t *r = text_button(s_panel, "Reiniciar mascota", 280, 48, on_reset, NULL);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x7f1d1d), 0);
    lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 326);
    s_reset_lbl = lv_obj_get_child(r, 0);
}

static void refresh_settings(const pet_state_t *s)
{
    char b[16];
    snprintf(b, sizeof(b), "%02d:00", s->night_start_h);
    lv_label_set_text(s_night_lbl[0], b);
    snprintf(b, sizeof(b), "%02d:00", s->night_end_h);
    lv_label_set_text(s_night_lbl[1], b);
    static const int SPEEDS[3] = {1, 10, 60};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(s_speed_btn[i], lv_color_hex(pet_time_speed() == SPEEDS[i] ? 0x0e7490 : BTN_BG), 0);
    }
    if (s_reset_armed && pet_mono_ms() - s_reset_ms > 5000) {
        s_reset_armed = false;
        lv_label_set_text(s_reset_lbl, "Reiniciar mascota");
    }
}

static void panel_refresh(int64_t now)
{
    if (s_kind == P_NONE || s_kind == P_GAME || s_kind == P_MORE) {
        return;
    }
    static pet_state_t snap; // copy under the lock, format without it
    pet_lock();
    snap = g_pet;
    pet_unlock();
    if (s_title_coins) {
        char b[16];
        snprintf(b, sizeof(b), "%u", (unsigned)snap.coins);
        lv_label_set_text(s_title_coins, b);
    }
    switch (s_kind) {
    case P_FOOD: refresh_food(&snap); break;
    case P_PLAY: refresh_play(&snap, now); break;
    case P_STATUS: refresh_status(&snap, now); break;
    case P_SHOP: refresh_shop(&snap); break;
    case P_ALBUM: refresh_album(&snap); break;
    case P_SETTINGS: refresh_settings(&snap); break;
    default: break;
    }
}

static void open_and_refresh(void (*open)(void))
{
    open();
    panel_refresh(pet_now_s());
}

// ---------- home actions ----------

static void do_wash(void)
{
    pet_lock();
    const bool ok = pet_wash(&g_pet, pet_now_s());
    const bool egg = g_pet.stage == PET_EGG;
    pet_unlock();
    if (ok) {
        start_fx(FX_BUBBLES, 0, 0);
        toast("Limpito!");
        pet_mark_dirty();
    } else {
        toast(egg ? "Aun es un huevo" : "Esta durmiendo");
    }
}

static void do_sleep(void)
{
    const int64_t now = pet_now_s();
    pet_lock();
    const bool napping = pet_is_napping(&g_pet, now), night = pet_is_night(&g_pet, now), egg = g_pet.stage == PET_EGG;
    bool ok = false;
    if (napping) {
        pet_wake(&g_pet, now);
    } else if (!night && !egg) {
        ok = pet_nap(&g_pet, now);
    }
    pet_unlock();
    if (napping) {
        toast("Despierto");
    } else if (ok) {
        toast("Siesta de 30 min");
        pet_mark_dirty();
    } else {
        toast(egg ? "Aun es un huevo" : "Ya duerme: es de noche");
    }
}

static void on_tray(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: open_and_refresh(open_food); break;
    case 1: open_and_refresh(open_play); break;
    case 2: do_wash(); break;
    case 3: do_sleep(); break;
    default: open_more(); break;
    }
}

static void on_bubble(void)
{
    pet_lock();
    const pet_want_t w = pet_want(&g_pet, pet_now_s());
    const uint8_t wish = g_pet.wish_kind, arg = g_pet.wish_arg;
    pet_unlock();
    switch (w) {
    case WANT_FOOD:
    case WANT_MEDICINE: open_and_refresh(open_food); break;
    case WANT_CLEAN: do_wash(); break;
    case WANT_SLEEP: do_sleep(); break;
    case WANT_PLAY: open_and_refresh(open_play); break;
    case WANT_WISH:
        switch (wish) {
        case WISH_FOOD: open_and_refresh(open_food); break;
        case WISH_GAME: {
            pet_lock();
            const pet_play_t r = pet_can_play(&g_pet, (pet_game_id_t)arg, pet_now_s());
            pet_unlock();
            if (r == PLAY_OK) {
                request_game(arg);
            } else {
                open_and_refresh(open_play);
            }
            break;
        }
        case WISH_PET: toast("Quiere mimos: acaricialo"); break;
        case WISH_CLEAN: do_wash(); break;
        case WISH_SIGNAL: toast("Quiere oler una red nueva: sal de paseo"); break;
        default: break;
        }
        break;
    default: break;
    }
}

static void on_scene(lv_event_t *e)
{
    if (s_kind != P_NONE || lv_indev_get_gesture_dir(lv_indev_active()) != LV_DIR_NONE) {
        return;
    }
    lv_point_t p;
    lv_area_t a;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_obj_get_coords(s_img, &a);
    const int x = p.x - a.x1, y = p.y - a.y1;
    if (pet_scene_bubble_at(&s_last_sc, x, y)) {
        on_bubble();
        return;
    }
    const int poop = pet_scene_poop_at(&s_last_sc, x, y);
    if (poop >= 0) {
        pet_lock();
        const bool ok = pet_clean_poop(&g_pet, pet_now_s());
        pet_unlock();
        if (ok) {
            int px, py, pw, ph;
            pet_scene_poop_rect(poop, &px, &py, &pw, &ph);
            start_fx(FX_SPARKLE, px + pw / 2, py + ph / 2);
            pet_mark_dirty();
        }
        return;
    }
    if (pet_scene_pet_at(&s_last_sc, x, y)) {
        pet_lock();
        const pet_reaction_t r = pet_touch(&g_pet, pet_now_s(), pet_mono_ms());
        const bool egg = g_pet.stage == PET_EGG;
        pet_unlock();
        if (r == REACT_PET) {
            start_override(ANIM_BOUNCE);
            if (!egg) {
                start_fx(FX_HEARTS, 0, 0);
            }
        } else if (r == REACT_SHAKE) {
            toast("Lo has despertado");
        }
    }
}

static void on_gesture(lv_event_t *e)
{
    if (s_kind != P_NONE) {
        return;
    }
    const lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_active());
    if (d == LV_DIR_TOP) {
        open_and_refresh(open_food);
    } else if (d == LV_DIR_BOTTOM) {
        open_and_refresh(open_status);
    }
}

static void on_back(lv_event_t *e) { pet_ui_back(); }

bool pet_ui_back(void)
{
    switch (s_kind) {
    case P_NONE: return false;
    case P_GAME:
        s_game_on = false;
        s_game_req = -1;
        open_and_refresh(open_play);
        return true;
    case P_STATUS:
    case P_SHOP:
    case P_ALBUM:
    case P_SETTINGS: open_more(); return true;
    default: close_panel(); return true;
    }
}

void pet_ui_boot_action(void)
{
    if (s_kind == P_NONE) {
        open_and_refresh(open_food);
    } else if (s_kind == P_GAME && s_result) {
        on_again(NULL);
    }
}

// ---------- init ----------

void pet_ui_init(void)
{
    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_calloc(PET_SCENE_W * PET_SCENE_H, 2, MALLOC_CAP_SPIRAM);
        if (!s_buf[i]) {
            ESP_LOGE(TAG, "no PSRAM for the scene buffer");
            return;
        }
    }
    s_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                        .w = PET_SCENE_W, .h = PET_SCENE_H, .stride = PET_SCENE_W * 2},
                             .data_size = PET_SCENE_W * PET_SCENE_H * 2, .data = (const uint8_t *)s_buf[0]};
    s_back = 1;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);

    s_time = make_label(scr, &lv_font_montserrat_28, 0xf8fafc);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 10);
    s_batt = make_label(scr, &lv_font_montserrat_16, 0x94a3b8);
    lv_obj_align(s_batt, LV_ALIGN_TOP_MID, -130, 48);
    s_level = make_label(scr, &lv_font_montserrat_16, 0x67e8f9);
    lv_obj_align(s_level, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_t *ci = add_icon(scr, ICON_COIN, 2, 0x000000);
    lv_obj_align(ci, LV_ALIGN_TOP_MID, 100, 44);
    s_coins = make_label(scr, &lv_font_montserrat_16, 0xfde68a);
    lv_obj_set_style_text_align(s_coins, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_coins, LV_ALIGN_TOP_LEFT, 205 + 120, 48);
    s_gain = make_label(scr, &lv_font_montserrat_16, 0x4ade80);
    lv_obj_align(s_gain, LV_ALIGN_TOP_LEFT, 205 + 162, 48);
    lv_obj_add_flag(s_gain, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < NEED_COUNT; i++) {
        const int gx = 22 + i * 94;
        lv_obj_t *ic = add_icon(scr, NEED_ICON[i], 2, 0x000000);
        lv_obj_set_pos(ic, gx, 74);
        s_need_bar[i] = lv_bar_create(scr);
        lv_obj_set_size(s_need_bar[i], 56, 10);
        need_bar_style(s_need_bar[i], i);
        lv_obj_set_pos(s_need_bar[i], gx + 30, 81);
    }

    s_img = lv_image_create(scr);
    lv_image_set_src(s_img, &s_dsc);
    lv_obj_set_pos(s_img, SCENE_X, SCENE_Y);
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_img, on_scene, LV_EVENT_CLICKED, NULL);

    static const char *const tray[5] = {"Comer", "Jugar", "Asear", "Dormir", "Mas"};
    static const pet_icon_t ticon[5] = {ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_MOON, ICON_MENU};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = make_button(scr, 74, 60, on_tray, (void *)(intptr_t)i);
        lv_obj_set_pos(b, 12 + i * 78, 392);
        lv_obj_t *ic = add_icon(b, ticon[i], 2, BTN_BG);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 6);
        lv_obj_t *l = make_label(b, &lv_font_montserrat_12, 0xcbd5e1);
        lv_label_set_text(l, tray[i]);
        lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -5);
    }
    s_xp_bar = lv_bar_create(scr);
    lv_obj_set_size(s_xp_bar, 200, 8);
    lv_obj_set_style_bg_color(s_xp_bar, lv_color_hex(BTN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_xp_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_xp_bar, lv_color_hex(0x67e8f9), LV_PART_INDICATOR);
    lv_obj_align(s_xp_bar, LV_ALIGN_TOP_MID, 0, 462);
    s_xp_lbl = make_label(scr, &lv_font_montserrat_12, 0x64748b);
    lv_obj_align(s_xp_lbl, LV_ALIGN_TOP_MID, 0, 472);

    s_toast = make_label(scr, &lv_font_montserrat_16, 0xfde68a);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x111a33), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_90, 0);
    lv_obj_set_style_pad_hor(s_toast, 14, 0);
    lv_obj_set_style_pad_ver(s_toast, 6, 0);
    lv_obj_set_style_radius(s_toast, 14, 0);
    lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 4);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
}

// ---------- tick (ui_task) ----------

static void events_to_toasts(uint32_t ev, const pet_state_t *s)
{
    char b[48];
    if (ev & EVT_HATCHED) toast("Ha nacido!");
    else if (ev & EVT_EVOLVED) {
        snprintf(b, sizeof(b), "Ha evolucionado: %s%s%s", STAGE[s->stage], s->stage == PET_ADULT ? " " : "",
                 s->stage == PET_ADULT ? BRANCH[s->branch] : "");
        toast(b);
    } else if (ev & EVT_LEVEL) {
        snprintf(b, sizeof(b), "Nivel %d!", s->level);
        toast(b);
    } else if (ev & EVT_MILESTONE) {
        static const char *const M[] = {"monedas", "un Radar", "chuches", "un Salacot", "monedas"};
        snprintf(b, sizeof(b), "%d senales: regalo de %s", pet_milestone_signals(s->last_milestone),
                 M[s->last_milestone % 5]);
        toast(b);
    } else if (ev & EVT_WISH_DONE) toast("Deseo cumplido!");
    else if (ev & EVT_SICK) toast("Se ha puesto malito");
    else if (ev & EVT_CURED) toast("Ya esta curado");
    else if (ev & EVT_WISH_NEW) toast("Quiere algo: toca la burbuja");
    else if (ev & EVT_DAILY) toast("Visita del dia");
    else if (ev & EVT_TREAT) toast("Ha encontrado una chuche");
    else if (ev & EVT_POOP) toast("Ups... caca");
    else if (ev & EVT_NEW_SIGNAL) toast("Senal nueva olfateada");
}

static int bubble_icon(pet_want_t w, const pet_state_t *s)
{
    switch (w) {
    case WANT_MEDICINE: return ICON_MEDICINE;
    case WANT_FOOD: return ICON_FOOD;
    case WANT_CLEAN: return ICON_SOAP;
    case WANT_SLEEP: return ICON_MOON;
    case WANT_PLAY: return ICON_BALL;
    case WANT_WISH:
        switch (s->wish_kind) {
        case WISH_FOOD: return s->wish_arg < ITEM_COUNT ? s->wish_arg : ICON_FOOD;
        case WISH_GAME: return GAME_ICON[s->wish_arg % GAME_COUNT];
        case WISH_PET: return ICON_HEART;
        case WISH_CLEAN: return ICON_SOAP;
        default: return ICON_SIGNAL;
        }
    default: return -1;
    }
}

static bool scene_eq(const pet_scene_t *a, const pet_scene_t *b)
{
    return memcmp(&a->view, &b->view, sizeof(a->view)) == 0 && a->x == b->x && a->hop == b->hop &&
           a->poops == b->poops && a->bubble == b->bubble && a->bubble_wish == b->bubble_wish && a->night == b->night &&
           a->fx == b->fx && a->fx_ms / 40 == b->fx_ms / 40 && a->t_ms / 400 == b->t_ms / 400;
}

void pet_ui_tick(void)
{
    static pet_scene_t last;
    static bool have_last;
    static int64_t last_sec = -1;
    static uint32_t last_decide, last_shift;
    static int shift_n;
    static pet_state_t snap;

    if (!s_buf[0] || !s_buf[1]) {
        return;
    }
    const uint32_t mono = pet_mono_ms();
    if (game_tick(mono)) {
        have_last = false;
        return;
    }
    const int64_t now = pet_now_s();
    const bool new_sec = now != last_sec;
    last_sec = now;

    pet_scene_t sc;
    memset(&sc, 0, sizeof(sc));
    pet_anim_t anim;
    uint32_t ev;
    int gain_c = 0, gain_x = 0;
    pet_lock();
    if (new_sec) {
        pet_step(&g_pet, now);
    }
    bool decided = false;
    if (mono - last_decide >= DECIDE_MS) {
        last_decide = mono;
        pet_decide(&g_pet, now);
        decided = true;
    }
    ev = pet_take_events(&g_pet);
    pet_take_gains(&g_pet, &gain_c, &gain_x);
    anim = pet_view(&g_pet, now, mono, &sc.view);
    if (g_pet.react_kind == REACT_LOVE && g_pet.react_until_ms == 0) {   // armed by pet_core without a clock
        g_pet.react_until_ms = mono + 2500;
    }
    const pet_want_t want = pet_want(&g_pet, now);
    snap = g_pet;
    pet_unlock();

    // walking
    if (decided) {
        if (snap.behavior == BEH_WALK) {
            s_target = (int)((mono * 2654435761u) >> 8) % (PET_SCENE_XMAX + 1);
        } else if (snap.behavior == BEH_ATTENTION) {
            s_target = PET_SCENE_XMAX / 2;
        }
    }
    const bool asleep = anim == ANIM_SLEEP || snap.stage == PET_EGG;
    int hop = 0;
    if (!asleep && s_px != s_target) {
        const int d = s_target - s_px;
        s_px += d > WALK_PX ? WALK_PX : d < -WALK_PX ? -WALK_PX : d;
        hop = (mono / 140) % 2 ? 4 : 0;
        sc.view.look_x = d > 0 ? 1 : -1;
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
    const int8_t look = sc.view.look_x;
    pet_pose(&sc.view, anim, t);
    if (hop) {
        sc.view.look_x = look;
    }
    sc.x = s_px;
    sc.hop = hop;
    sc.poops = snap.poops;
    sc.bubble = bubble_icon(want, &snap);
    sc.bubble_wish = want == WANT_WISH;
    sc.night = pet_is_night(&snap, now);
    sc.t_ms = mono;
    sc.fx = s_fx;
    sc.fx_ms = mono - s_fx_start;
    sc.fx_x = s_fx_x;
    sc.fx_y = s_fx_y;
    if (sc.fx != FX_NONE && sc.fx_ms >= pet_scene_fx_ms(sc.fx)) {
        s_fx = FX_NONE;
        sc.fx = FX_NONE;
    }
    if (ev & (EVT_HATCHED | EVT_EVOLVED | EVT_LEVEL | EVT_MILESTONE | EVT_WISH_DONE)) {
        pet_mark_dirty();
        if (ev & (EVT_WISH_DONE | EVT_LEVEL)) {
            start_fx(FX_COINS, 0, 0);
        }
    }
    if (ev & (EVT_HATCHED | EVT_EVOLVED)) {
        ESP_LOGI(TAG, "event: %s (stage %d)", (ev & EVT_HATCHED) ? "hatched" : "evolved", snap.stage);
    }
    if (new_sec && now % 60 == 0) {
        ESP_LOGI(TAG, "stage %d lvl %d food %d fun %d hyg %d en %d coins %u poops %d wish %d", snap.stage, snap.level,
                 pet_need(&snap, NEED_FOOD), pet_need(&snap, NEED_FUN), pet_need(&snap, NEED_HYGIENE),
                 pet_need(&snap, NEED_ENERGY), (unsigned)snap.coins, snap.poops, snap.wish_kind);
    }

    const bool scene_visible = s_shown == P_NONE;
    const bool redraw = scene_visible && (!have_last || !scene_eq(&sc, &last));
    if (redraw) {
        pet_scene_draw(&sc, s_buf[s_back]);
        last = sc;
        have_last = true;
    }
    const bool toast_expired = s_toast_until && (int32_t)(mono - s_toast_until) >= 0;
    const bool gain_expired = s_gain_until && (int32_t)(mono - s_gain_until) >= 0;
    if (!redraw && !new_sec && !ev && !gain_c && !toast_expired && !gain_expired) {
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
        s_last_sc = sc;
    }
    char b[32];
    if (new_sec || ev || gain_c) {
        struct tm tm;
        time_t tt = (time_t)now;
        gmtime_r(&tt, &tm);
        snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(s_time, b);
        const int pct = g_batt_pct;
        if (pct < 0) {
            lv_label_set_text(s_batt, g_usb ? LV_SYMBOL_CHARGE : "");
        } else {
            snprintf(b, sizeof(b), "%s%d%%", g_usb ? LV_SYMBOL_CHARGE : "", pct);
            lv_label_set_text(s_batt, b);
        }
        snprintf(b, sizeof(b), "Nv %d%s", snap.level, pet_time_speed() > 1 ? " >>" : "");
        lv_label_set_text(s_level, b);
        snprintf(b, sizeof(b), "%u", (unsigned)snap.coins);
        lv_label_set_text(s_coins, b);
        for (int i = 0; i < NEED_COUNT; i++) {
            set_need(s_need_bar[i], i, snap.stage == PET_EGG ? 0 : pet_need(&snap, (pet_need_t)i));
        }
        int have, need;
        pet_level_progress(&snap, &have, &need);
        lv_bar_set_range(s_xp_bar, 0, need > 0 ? need : 1);
        lv_bar_set_value(s_xp_bar, have, LV_ANIM_OFF);
        snprintf(b, sizeof(b), "XP %d / %d", have, need);
        lv_label_set_text(s_xp_lbl, b);
        if (new_sec && now % 60 == 0 && mono - last_shift >= SHIFT_MS / 2) { // anti burn-in
            last_shift = mono;
            shift_n = (shift_n + 1) % 5;
            lv_obj_set_pos(s_img, SCENE_X + (shift_n % 3 - 1) * 2, SCENE_Y + (shift_n / 2 - 1) * 2);
        }
        panel_refresh(now);
    }
    if (gain_c > 0) {
        snprintf(b, sizeof(b), "+%d", gain_c);
        lv_label_set_text(s_gain, b);
        lv_obj_remove_flag(s_gain, LV_OBJ_FLAG_HIDDEN);
        s_gain_until = mono + 1800;
    } else if (gain_expired) {
        s_gain_until = 0;
        lv_obj_add_flag(s_gain, LV_OBJ_FLAG_HIDDEN);
    }
    if (ev) {
        events_to_toasts(ev, &snap);
    }
    if (toast_expired) {
        s_toast_until = 0;
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
}
