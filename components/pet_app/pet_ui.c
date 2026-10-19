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
#include "fonts/pet_fonts.h"
#include "lvgl.h"
#include "pet_game.h"
#include "pet_gfx.h"
#include "pet_priv.h"
#include "pet_room.h"
#include "pet_scene.h"

#define SCENE_X 5
#define SCENE_Y 136        // the scene floor (266) lands on the room's rug (PET_ROOM_RUG_Y)
#define LOCK_MS 100
#define DECIDE_MS 8000
#define TOAST_MS 3500
// "Pocket toy" look (DECISIONS R26): a pixel-art room, wooden buttons and RPG menu windows.
#define INK 0x1a1c2c       // outlines and text shadows
#define BOX_BG 0x29366f    // inside of the menu windows
#define WOOD 0xa0663c      // buttons
#define WOOD_DOWN 0x7a4d2a
#define PICK 0x38b764      // selected option
#define BOX_X 24           // the menu window (inside the screen's rounded corners)
#define BOX_Y 38
#define BOX_W 362
#define BOX_H 374
#define HUD_Y 70           // strip with the XP and need pips, redrawn when they change
#define HUD_H (SCENE_Y - HUD_Y)
#define TRAY_Y 416
#define ACT_MS 4500        // an activity on its own (ball, weights, book, doze)
#define LOOK_MS 1500       // it looks at the finger this long after a touch
#define WALK_PX 2          // per 40 ms tick
#define PREVIEW_W 96
#define PREVIEW_H 60
#define ALBUM_W 340
#define ALBUM_H 292

static const char *TAG = "pet_ui";

typedef enum { P_NONE, P_FOOD, P_PLAY, P_GAME, P_MORE, P_STATUS, P_SHOP, P_ALBUM, P_SETTINGS } panel_t;

// ---------- names (ASCII) ----------
static const char *const STAGE[] = {"Huevo", "Cría", "Juvenil", "Adulto"};
static const char *const BRANCH[] = {"", "Hogareño", "Saltarín", "Sabio", "Forzudo"};
static const char *const VARIANT[] = {"Normal", "Pulido", "Torpe", "Raro"};
static const char *const MOOD[] = {"Durmiendo", "Malito", "Con hambre", "Sucio", "Cansado", "Aburrido", "Contento",
                                   "Tranquilo"};
static const char *const ITEM_NAME[ITEM_COUNT] = {"Pan", "Fruta", "Guiso", "Pastel", "Chuche de señal", "Medicina"};
static const char *const ACC_NAME[ACC_COUNT] = {"Nada", "Lazo", "Flor", "Gorro", "Gafas", "Chistera", "Corona",
                                                "Radar", "Salacot"};
static const char *const TRAIN_NAME[GAME_COUNT] = {"Agilidad", "Ingenio", "Fuerza"};
static const pet_icon_t GAME_ICON[GAME_COUNT] = {ICON_STAR, ICON_GRID, ICON_DUMBBELL};
static const uint32_t NEED_COLOR[NEED_COUNT] = {0xef7d57, 0xf28ab6, 0x41a6f6, 0xffcd75};
static const pet_icon_t NEED_ICON[NEED_COUNT] = {ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_BOLT};

// ---------- buffers and widgets ----------
static uint16_t *s_buf[2];
static int s_back;                 // buffer being drawn; the other is on screen
static lv_image_dsc_t s_dsc;
static lv_obj_t *s_img;            // home scene
static lv_obj_t *s_time, *s_time_sh, *s_batt, *s_level, *s_level_sh, *s_coins, *s_coins_sh, *s_gain, *s_toast;
static uint16_t *s_room, *s_room_dim;   // the whole room (fixed background) and a dimmed copy for menus
static lv_image_dsc_t s_room_dsc, s_room_dim_dsc;
static lv_obj_t *s_room_img;
static uint16_t *s_scene_bg;            // the room seen through the scene window, for pet_scene
static pet_sky_t s_sky = (pet_sky_t)-1;
static uint16_t *s_hud[2];
static int s_hud_back;
static lv_image_dsc_t s_hud_dsc;
static lv_obj_t *s_hud_img;
static uint32_t s_hud_sig = 0xffffffffu;
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
// effects: asked for from LVGL callbacks and ui_task, emitted by ui_task, which owns the
// particles. The queue is guarded by pet_lock (the LVGL lock may already be held).
typedef struct { uint8_t fx; int16_t x, y; } fx_req_t;   // x < 0: at the creature's head
static fx_req_t s_fxq[8];
static uint8_t s_fxq_n;
static pet_parts_t s_parts;

// minigame
static pet_game_t s_game;
static volatile int s_finger_x;
static volatile uint32_t s_finger_ms;
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
    lv_obj_set_style_bg_color(b, lv_color_hex(WOOD), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(WOOD_DOWN), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(INK), 0);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_radius(b, 0, 0);
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
    lv_obj_t *l = make_label(b, &pet_font_m, 0xf4f4f4);
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
        lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 6);
    } else {
        lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -94);
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
    pet_lock();
    if (s_fxq_n < sizeof(s_fxq) / sizeof(s_fxq[0])) {
        s_fxq[s_fxq_n++] = (fx_req_t){(uint8_t)fx, (int16_t)x, (int16_t)y};
    }
    pet_unlock();
}

static void fx_at_head(pet_fx_t fx) { start_fx(fx, -1, -1); }

static void need_bar_style(lv_obj_t *bar, int i)
{
    lv_obj_set_style_bg_color(bar, lv_color_hex(INK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(NEED_COLOR[i]), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(bar, 2, LV_PART_MAIN);
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
        lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 6);
    }
}

static void on_back(lv_event_t *e);

static lv_obj_t *open_panel(panel_t kind, const char *title)
{
    close_panel();
    s_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_panel, 410, 502);
    lv_obj_set_pos(s_panel, 0, 0);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(INK), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, 0, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    if (s_room_dim) {   // the room behind, dimmed
        lv_obj_t *bg = lv_image_create(s_panel);
        lv_image_set_src(bg, &s_room_dim_dsc);
        lv_obj_set_pos(bg, 0, 0);
    }
    if (kind != P_GAME) {   // RPG window: dark edge, white line, navy inside
        lv_obj_t *box = lv_obj_create(s_panel);
        lv_obj_set_size(box, BOX_W - 6, BOX_H - 6);
        lv_obj_set_pos(box, BOX_X + 3, BOX_Y + 3);
        lv_obj_set_style_bg_color(box, lv_color_hex(BOX_BG), 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0xf4f4f4), 0);
        lv_obj_set_style_border_width(box, 3, 0);
        lv_obj_set_style_outline_color(box, lv_color_hex(INK), 0);
        lv_obj_set_style_outline_width(box, 3, 0);
        lv_obj_set_style_radius(box, 0, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *t = make_label(s_panel, &pet_font_l, 0xffcd75);
    lv_label_set_text(t, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, kind == P_GAME ? 22 : BOX_Y + 8);
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
    lv_obj_t *b = make_button(parent, 334, h, cb, (void *)(intptr_t)idx);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(BOX_BG), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x3b5dc9), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 0, 0);
    int x = 14;
    if (img) {
        lv_obj_t *im = lv_image_create(b);
        lv_image_set_src(im, img);
        lv_obj_align(im, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
        x = 20 + (int)img->header.w;
    }
    lv_obj_t *l = make_label(b, &pet_font_s, 0xf4f4f4);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(l, 334 - x - 64);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_t *r = make_label(b, &pet_font_m, 0xffcd75);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, -10, 0);
    if (idx >= 0 && idx < 10) {
        s_rows[idx] = b;
        s_row_lbl[idx] = l;
        s_row_right[idx] = r;
    }
    return b;
}

static void coins_in_title(void)
{
    lv_obj_t *ic = add_icon(s_panel, ICON_COIN, 2, BOX_BG);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 300, BOX_Y + 14);
    s_title_coins = make_label(s_panel, &pet_font_m, 0xffcd75);
    lv_obj_set_style_text_align(s_title_coins, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_title_coins, LV_ALIGN_TOP_LEFT, 328, BOX_Y + 12);
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
        if (it == ITEM_MEDICINE) {
            pet_sound(SND_CURE);
            fx_at_head(FX_STARS);
            toast("Ya está mejor");
        } else {
            start_override(ANIM_EAT);
            pet_sound(SND_EAT);
            if (fav && it < PET_MENU_COUNT) {
                pet_sound(SND_V_LOVE);
                fx_at_head(FX_HEARTS);
                toast("¡Su plato favorito!");
            }
        }
        pet_mark_dirty();
        break;
    case FEED_FULL: pet_sound(SND_DENY); toast("Está lleno"); break;
    case FEED_NO_COINS: pet_sound(SND_DENY); toast("Faltan monedas: juega para ganar"); break;
    case FEED_ASLEEP: toast("Está durmiendo"); break;
    case FEED_NOT_SICK: pet_sound(SND_DENY); toast("No está malito"); break;
    case FEED_EGG: toast("Aún es un huevo"); break;
    default: pet_sound(SND_DENY); toast("No te queda: huele redes para encontrar"); break;
    }
}

static void open_food(void)
{
    open_panel(P_FOOD, "Comer");
    coins_in_title();
    for (int i = 0; i < ITEM_COUNT; i++) {
        list_row(s_panel, i, BOX_Y + 48 + i * 52, 48, icon_img((pet_icon_t)i, 3, BOX_BG), on_food);
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
            snprintf(b, sizeof(b), "%s\n#94b0c2 cura el malito#", ITEM_NAME[i]);
        } else if (fun) {
            snprintf(b, sizeof(b), "%s%s\n#94b0c2 +%d comida +%d diversión#", ITEM_NAME[i], known_fav ? " (fav)" : "",
                     food, fun);
        } else {
            snprintf(b, sizeof(b), "%s%s\n#94b0c2 +%d comida#", ITEM_NAME[i], known_fav ? " (fav)" : "", food);
        }
        lv_label_set_recolor(s_row_lbl[i], true);
        lv_label_set_text(s_row_lbl[i], b);
        const int price = pet_item_price((pet_item_t)i);
        const bool free_bread = i == ITEM_BREAD && pet_bread_free(s);
        if (s->inv[i]) {
            snprintf(b, sizeof(b), "x%d", s->inv[i]);
        } else if (free_bread) {
            snprintf(b, sizeof(b), "gratis");
        } else if (price) {
            snprintf(b, sizeof(b), "%d", price);
        } else {
            snprintf(b, sizeof(b), "x0");
        }
        lv_label_set_text(s_row_right[i], b);
        const bool can = s->inv[i] || free_bread || (price && s->coins >= (uint32_t)price);
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
    case PLAY_TIRED: pet_sound(SND_DENY); toast("Está cansado: déjalo dormir"); break;
    case PLAY_SICK: pet_sound(SND_V_SAD); toast("Está malito: dale medicina"); break;
    case PLAY_ASLEEP: toast("Está durmiendo"); break;
    case PLAY_EGG: toast("Aún es un huevo"); break;
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
        list_row(s_panel, i, BOX_Y + 50 + i * 92, 84, icon_img(GAME_ICON[i], 4, BOX_BG), on_play);
    }
    s_info = make_label(s_panel, &pet_font_s, 0x94b0c2);
    lv_obj_set_width(s_info, 330);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, BOX_Y + 330);
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
            snprintf(b, sizeof(b), "%s\n#94b0c2 nivel %d#", pet_game_name((pet_game_id_t)i), pet_game_level((pet_game_id_t)i));
        } else {
            snprintf(b, sizeof(b), "%s\n#94b0c2 %s · récord %d#", pet_game_name((pet_game_id_t)i), TRAIN_NAME[i], s->best[i]);
        }
        lv_label_set_recolor(s_row_lbl[i], true);
        lv_label_set_text(s_row_lbl[i], b);
        lv_label_set_text(s_row_right[i], r == PLAY_LOCKED ? LV_SYMBOL_CLOSE : LV_SYMBOL_PLAY);
        lv_obj_set_style_opa(s_rows[i], r == PLAY_OK ? LV_OPA_COVER : LV_OPA_50, 0);
    }
    if (s_info) {
        snprintf(b, sizeof(b), "Energía %d. Cada partida gasta 8.", pet_need(s, NEED_ENERGY));
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
        pet_sound(SND_DENY);
        toast(r == PLAY_TIRED ? "Está cansado: déjalo dormir" : "No puede jugar ahora");
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
    const int32_t n = (int32_t)lv_obj_get_child_count(s_panel);
    s_game_title = lv_obj_get_child(s_panel, n - 2);
    lv_obj_delete(lv_obj_get_child(s_panel, n - 1));   // the generic "Volver"
    lv_obj_t *x = text_button(s_panel, "Salir", 150, 46, on_game_exit, NULL);
    lv_obj_align(x, LV_ALIGN_BOTTOM_MID, 0, -30);
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
    s_game_hint = make_label(s_panel, &pet_font_s, 0xc5dbe6);
    lv_obj_set_width(s_game_hint, 370);
    lv_label_set_long_mode(s_game_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_game_hint, LV_ALIGN_TOP_MID, 0, 360);
}

static void show_result(const pet_reward_t *r, int score, int perf)
{
    s_result = lv_obj_create(s_panel);
    lv_obj_set_size(s_result, 360, 300);
    lv_obj_align(s_result, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(s_result, lv_color_hex(BOX_BG), 0);
    lv_obj_set_style_border_color(s_result, lv_color_hex(0xf4f4f4), 0);
    lv_obj_set_style_border_width(s_result, 3, 0);
    lv_obj_set_style_outline_color(s_result, lv_color_hex(INK), 0);
    lv_obj_set_style_outline_width(s_result, 3, 0);
    lv_obj_set_style_radius(s_result, 0, 0);
    lv_obj_remove_flag(s_result, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *t = make_label(s_result, &pet_font_l, 0xffcd75);
    lv_label_set_text(t, perf >= 80 ? "¡Genial!" : perf >= 40 ? "Bien hecho" : "Otra vez será");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_t *d = make_label(s_result, &pet_font_m, 0xf4f4f4);
    char b[160];
    int n = snprintf(b, sizeof(b), "Puntos %d%s\n+%d monedas  +%d XP\n+%d diversión", score, r->record ? " (¡récord!)" : "",
                     r->coins, r->xp, r->fun);
    if (r->train) {
        snprintf(b + n, sizeof(b) - n, "\n%s +%d", TRAIN_NAME[s_game.id], r->train);
    }
    lv_label_set_text(d, b);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_t *a = text_button(s_result, "Otra vez", 150, 50, on_again, NULL);
    lv_obj_set_style_bg_color(a, lv_color_hex(PICK), 0);
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
        const uint8_t level = g_pet.level;
        pet_unlock();
        v.expr = EXPR_NEUTRAL;
        pet_game_start(&s_game, (pet_game_id_t)req, mono ^ 0x5bd1e995u, &v);
        s_game.level = level;   // harder as the creature levels up
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
    const uint16_t gev = pet_game_take_events(&s_game);
    if (gev & GEV_ZAP) pet_sound(SND_ZAP);
    if (gev & GEV_STAR) pet_sound(SND_STAR);
    if (gev & GEV_CATCH) pet_sound_pitch(SND_CATCH, s_game.combo > 12 ? 12 : s_game.combo);
    if (gev & GEV_PAD) pet_sound((snd_id_t)(SND_PAD0 + (s_game.ev_pad & 3)));
    if (gev & GEV_GOOD) pet_sound(SND_GOOD);
    if (gev & GEV_FAIL) pet_sound(SND_FAIL);
    if (gev & GEV_TICK) pet_sound_pitch(SND_TICK, s_game.fill * 12 / 1000);
    if (gev & GEV_LIFTED) pet_sound(SND_LIFTED);
    if (gev & GEV_DROP) pet_sound(SND_DROP);
    if (gev & GEV_WIN) pet_sound(SND_LIFTED);
    if (gev & GEV_PERFECT) pet_sound(SND_STAR);
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
        pet_sound(r.record ? SND_RECORD : perf >= 40 ? SND_END : SND_V_SAD);
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
    open_panel(P_MORE, "Más");
    static const char *const names[] = {"Estado", "Armario", "Álbum", "Ajustes"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = text_button(s_panel, names[i], 290, 52, on_more, (void *)(intptr_t)i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, BOX_Y + 52 + i * 62);
    }
    pet_lock();
    const bool gen = pet_can_new_generation(&g_pet, pet_now_s());
    pet_unlock();
    if (gen) {
        lv_obj_t *b = text_button(s_panel, "Nueva generación", 290, 52, on_more, (void *)(intptr_t)4);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xb13e53), 0);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, BOX_Y + 52 + 4 * 62);
    }
}

// ---------- Estado ----------

static void open_status(void)
{
    open_panel(P_STATUS, "Estado");
    static const char *const names[NEED_COUNT] = {"Comida", "Diversión", "Higiene", "Energía"};
    for (int i = 0; i < NEED_COUNT; i++) {
        const int y = BOX_Y + 52 + i * 34;
        lv_obj_t *ic = add_icon(s_panel, NEED_ICON[i], 2, BOX_BG);
        lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 44, y);
        lv_obj_t *l = make_label(s_panel, &pet_font_s, 0xc5dbe6);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 76, y + 2);
        s_bars[i] = lv_bar_create(s_panel);
        lv_obj_set_size(s_bars[i], 130, 16);
        need_bar_style(s_bars[i], i);
        lv_obj_align(s_bars[i], LV_ALIGN_TOP_LEFT, 184, y + 4);
        s_bar_lbl[i] = make_label(s_panel, &pet_font_s, 0xf4f4f4);
        lv_obj_align(s_bar_lbl[i], LV_ALIGN_TOP_LEFT, 326, y + 2);
    }
    s_info = make_label(s_panel, &pet_font_s, 0xf4f4f4);
    lv_obj_set_width(s_info, 330);
    lv_label_set_recolor(s_info, true);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, BOX_Y + 194);
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
        n += snprintf(b + n, sizeof(b) - n, "#ffcd75 Evoluciona: nivel %d y %dh de edad#\n", lvl, (int)(need_age / 3600));
    }
    if (s->stage == PET_BABY || s->stage == PET_JUVENILE) {
        n += snprintf(b + n, sizeof(b) - n, "Agilidad %d  Ingenio %d  Fuerza %d\n#73eff7 Va para: %s#\n", s->train[0],
                      s->train[1], s->train[2], BRANCH[pet_branch_lean(s)]);
    }
    const int next = pet_next_milestone(s);
    if (next > 0) {
        snprintf(b + n, sizeof(b) - n, "Señales %u (regalo a las %d)", (unsigned)s->signals, next);
    } else {
        snprintf(b + n, sizeof(b) - n, "Señales %u", (unsigned)s->signals);
    }
    lv_label_set_text(s_info, b);
}

// ---------- Armario (shop + wardrobe) ----------

// The pet wearing `acc`, cropped to its head, on the button colour.
// Drawing a creature needs ~5.5 KB of stack (more than the LVGL task has), so the panels only
// allocate and clear their pictures; ui_task draws them (panel_fill) holding the LVGL lock.
static volatile int s_fill_req;   // panel_t to fill, or 0

static void prepare_preview(int acc)
{
    s_preview[acc] = heap_caps_malloc(PREVIEW_W * PREVIEW_H * 2, MALLOC_CAP_SPIRAM);
    if (!s_preview[acc]) {
        return;
    }
    const uint16_t bg = pet_rgb565(BOX_BG);
    for (int i = 0; i < PREVIEW_W * PREVIEW_H; i++) {
        s_preview[acc][i] = bg;
    }
    s_preview_dsc[acc] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                     .w = PREVIEW_W, .h = PREVIEW_H, .stride = PREVIEW_W * 2},
                                          .data_size = PREVIEW_W * PREVIEW_H * 2, .data = (const uint8_t *)s_preview[acc]};
}

static void fill_preview(int acc)
{
    uint16_t *full = heap_caps_malloc(96 * 96 * 2, MALLOC_CAP_SPIRAM);
    if (!s_preview[acc] || !full) {
        free(full);
        return;
    }
    const uint16_t bg = pet_rgb565(BOX_BG);
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
}

static void on_acc(lv_event_t *e)
{
    const pet_acc_t a = (pet_acc_t)(intptr_t)lv_event_get_user_data(e);
    char b[48];
    pet_lock();
    pet_buy_t r = BUY_OK;
    bool bought = false;
    if (g_pet.stage == PET_EGG) {
        r = BUY_NOT_SOLD;
    } else if (pet_acc_owned(&g_pet, a)) {
        pet_wear(&g_pet, g_pet.acc_worn == a ? ACC_NONE : a);
    } else {
        r = pet_buy_acc(&g_pet, a);
        bought = r == BUY_OK;
    }
    const int lvl = pet_acc_level(a);
    pet_unlock();
    switch (r) {
    case BUY_OK: pet_sound(bought ? SND_BUY : SND_CLEAN); pet_mark_dirty(); break;
    case BUY_LOCKED: pet_sound(SND_DENY); snprintf(b, sizeof(b), "Se desbloquea en el nivel %d", lvl); toast(b); break;
    case BUY_NO_COINS: pet_sound(SND_DENY); toast("Faltan monedas"); break;
    default: toast(a >= ACC_RADAR ? "Regalo por oler señales" : "Aún es un huevo"); break;
    }
}

static void open_shop(void)
{
    open_panel(P_SHOP, "Armario");
    coins_in_title();
    lv_obj_t *list = lv_obj_create(s_panel);
    lv_obj_set_size(list, 344, BOX_H - 64);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, BOX_Y + 50);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    for (int a = ACC_NONE + 1; a < ACC_COUNT; a++) {
        prepare_preview(a);
        list_row(list, a, (a - 1) * 74, 68, s_preview[a] ? &s_preview_dsc[a] : NULL, on_acc);
        lv_obj_remove_flag(s_rows[a], LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
    s_fill_req = P_SHOP;
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
            snprintf(b, sizeof(b), "%s\n#94b0c2 %s#", ACC_NAME[a], worn ? "puesto" : "tuyo");
            lv_label_set_text(s_row_right[a], worn ? LV_SYMBOL_OK : "");
        } else if (!price) {
            const int need = a == ACC_RADAR ? pet_milestone_signals(1) : pet_milestone_signals(3);
            snprintf(b, sizeof(b), "%s\n#94b0c2 regalo a las %d señales#", ACC_NAME[a], need);
            lv_label_set_text(s_row_right[a], "");
        } else if (s->level < lvl) {
            snprintf(b, sizeof(b), "%s\n#94b0c2 nivel %d · %d monedas#", ACC_NAME[a], lvl, price);
            lv_label_set_text(s_row_right[a], LV_SYMBOL_CLOSE);
        } else {
            snprintf(b, sizeof(b), "%s\n#94b0c2 comprar#", ACC_NAME[a]);
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
    open_panel(P_ALBUM, "Álbum");
    s_album_px = heap_caps_malloc(ALBUM_W * ALBUM_H * 2, MALLOC_CAP_SPIRAM);
    if (s_album_px) {
        const uint16_t bg = pet_rgb565(BOX_BG);
        for (int i = 0; i < ALBUM_W * ALBUM_H; i++) {
            s_album_px[i] = bg;
        }
        s_album_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                  .w = ALBUM_W, .h = ALBUM_H, .stride = ALBUM_W * 2},
                                       .data_size = ALBUM_W * ALBUM_H * 2, .data = (const uint8_t *)s_album_px};
        lv_obj_t *im = lv_image_create(s_panel);
        lv_image_set_src(im, &s_album_dsc);
        lv_obj_align(im, LV_ALIGN_TOP_MID, 0, BOX_Y + 40);
        s_fill_req = P_ALBUM;
    }
    s_info = make_label(s_panel, &pet_font_s, 0xc5dbe6);
    lv_obj_set_width(s_info, 330);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, BOX_Y + 326);
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
    snprintf(b, sizeof(b), "Formas %d/16  Platos %d/4  Gen. %d\nRécords: %d / %d / %d", forms, foods, s->generation + 1,
             s->best[0], s->best[1], s->best[2]);
    lv_label_set_text(s_info, b);
}

static void fill_album(void)
{
    uint16_t *tmp = heap_caps_malloc(96 * 96 * 2, MALLOC_CAP_SPIRAM);
    if (!s_album_px || !tmp) {
        free(tmp);
        return;
    }
    pet_lock();
    const uint16_t album = g_pet.album;
    const uint32_t seed = g_pet.view_seed;
    pet_unlock();
    const uint16_t sil = pet_rgb565(0x1e293b);
    for (int b = 0; b < 4; b++) {
        for (int v = 0; v < 4; v++) {
            const bool have = album >> (b * 4 + v) & 1;
            pet_view_t pv = {.seed = seed, .stage = PET_ADULT, .shape = (uint8_t)b, .variant = (uint8_t)v,
                             .expr = EXPR_HAPPY};
            memset(tmp, 0, 96 * 96 * 2);
            pet_gfx_t t = {tmp, 96, 96};
            pet_gfx_creature(&t, &pv, 0, 0, 2);
            const int cx = v * 84 - 6, cy = b * 72 - 14;
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
    free(tmp);
}

// ui_task: draws the pictures of the panel that was just opened. Holds the LVGL lock for the
// whole drawing, so the panel cannot be closed (and its buffers freed) under it.
static void panel_fill(void)
{
    const int req = s_fill_req;
    if (!req) {
        return;
    }
    s_fill_req = 0;
    if (!bsp_display_lock(1000)) {
        s_fill_req = req;   // try again next tick
        return;
    }
    if ((int)s_kind == req) {
        if (req == P_SHOP) {
            for (int a = ACC_NONE + 1; a < ACC_COUNT; a++) {
                fill_preview(a);
            }
        } else if (req == P_ALBUM) {
            fill_album();
        }
        lv_obj_invalidate(s_panel);
    }
    bsp_display_unlock();
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

static lv_obj_t *s_vol_btn[PET_VOL_MAX + 1];

static void on_volume(lv_event_t *e)
{
    const int v = (int)(intptr_t)lv_event_get_user_data(e);
    pet_lock();
    g_pet.volume = (uint8_t)v;
    pet_unlock();
    pet_sound_volume(v);
    pet_sound(SND_COIN);   // a sample at the new volume
    pet_mark_dirty();
}

static void on_reset(lv_event_t *e)
{
    const uint32_t now = pet_mono_ms();
    if (!s_reset_armed || now - s_reset_ms > 5000) {
        s_reset_armed = true;
        s_reset_ms = now;
        lv_label_set_text(s_reset_lbl, "¿Seguro? Toca otra vez");
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
        const int y = BOX_Y + 44 + i * 66;
        lv_obj_t *l = make_label(s_panel, &pet_font_s, 0xc5dbe6);
        lv_label_set_text(l, names[i]);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_t *m = text_button(s_panel, "-", 60, 38, on_night, (void *)(intptr_t) - (i + 1));
        lv_obj_align(m, LV_ALIGN_TOP_MID, -96, y + 22);
        lv_obj_t *p = text_button(s_panel, "+", 60, 38, on_night, (void *)(intptr_t)(i + 1));
        lv_obj_align(p, LV_ALIGN_TOP_MID, 96, y + 22);
        s_night_lbl[i] = make_label(s_panel, &pet_font_l, 0xf4f4f4);
        lv_obj_align(s_night_lbl[i], LV_ALIGN_TOP_MID, 0, y + 24);
    }
    lv_obj_t *l = make_label(s_panel, &pet_font_s, 0xc5dbe6);
    lv_label_set_text(l, "Sonido (de noche, en silencio)");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, BOX_Y + 178);
    static const char *const vol[PET_VOL_MAX + 1] = {"No", "Bajo", "Medio", "Alto"};
    for (int i = 0; i <= PET_VOL_MAX; i++) {
        s_vol_btn[i] = text_button(s_panel, vol[i], 76, 36, on_volume, (void *)(intptr_t)i);
        lv_obj_align(s_vol_btn[i], LV_ALIGN_TOP_MID, -120 + i * 80, BOX_Y + 200);
    }
    l = make_label(s_panel, &pet_font_s, 0xc5dbe6);
    lv_label_set_text(l, "Velocidad del tiempo (prueba)");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, BOX_Y + 244);
    static const char *const sp[4] = {"×1", "×10", "×60", "+1 h"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = text_button(s_panel, sp[i], 76, 36, on_speed, (void *)(intptr_t)i);
        lv_obj_align(b, LV_ALIGN_TOP_MID, -120 + i * 80, BOX_Y + 266);
        if (i < 3) {
            s_speed_btn[i] = b;
        }
    }
    lv_obj_t *r = text_button(s_panel, "Reiniciar mascota", 260, 40, on_reset, NULL);
    lv_obj_set_style_bg_color(r, lv_color_hex(0xb13e53), 0);
    lv_obj_align(r, LV_ALIGN_TOP_MID, 0, BOX_Y + 316);
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
        lv_obj_set_style_bg_color(s_speed_btn[i], lv_color_hex(pet_time_speed() == SPEEDS[i] ? PICK : WOOD), 0);
    }
    for (int i = 0; i <= PET_VOL_MAX; i++) {
        lv_obj_set_style_bg_color(s_vol_btn[i], lv_color_hex(s->volume == i ? PICK : WOOD), 0);
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
        fx_at_head(FX_BUBBLES);
        pet_sound(SND_WASH);
        toast("¡Limpito!");
        pet_mark_dirty();
    } else {
        toast(egg ? "Aún es un huevo" : "Está durmiendo");
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
        pet_sound(SND_V_HELLO);
        start_override(ANIM_BOUNCE);
        toast("Despierto");
    } else if (ok) {
        pet_sound(SND_V_YAWN);
        toast("Siesta de 30 min");
        pet_mark_dirty();
    } else {
        toast(egg ? "Aún es un huevo" : "Ya duerme: es de noche");
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
        case WISH_PET: toast("Quiere mimos: acarícialo"); break;
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
            pet_sound(SND_CLEAN);
            pet_mark_dirty();
        }
        return;
    }
    if (pet_scene_pet_at(&s_last_sc, x, y)) {
        pet_lock();
        const pet_reaction_t r = pet_touch(&g_pet, pet_now_s(), pet_mono_ms());
        const bool egg = g_pet.stage == PET_EGG;
        pet_unlock();
        int hx, hy;
        pet_scene_head(&s_last_sc, &hx, &hy);
        const bool belly = y > hy + 40;   // tummy: tickles; head: a pat
        if (r == REACT_PET) {
            start_override(ANIM_BOUNCE);
            if (egg) {
                pet_sound(SND_CRACK);
            } else if (belly) {
                pet_sound(SND_V_LAUGH);
                start_fx(FX_SPARKLE, x, y);
            } else {
                pet_sound(SND_V_HAPPY);
                fx_at_head(FX_HEARTS);
            }
        } else if (r == REACT_DIZZY) {
            pet_sound(SND_V_DIZZY);
            fx_at_head(FX_DIZZY);
        } else if (r == REACT_SHAKE) {
            pet_sound(SND_V_YAWN);
            toast("Lo has despertado");
        }
    }
}

// While a finger is on the scene, the creature turns to look at it.
static void on_scene_press(lv_event_t *e)
{
    lv_point_t p;
    lv_area_t a;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_obj_get_coords(s_img, &a);
    s_finger_x = p.x - a.x1;
    s_finger_ms = pet_mono_ms() | 1u;
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

static const pet_icon_t TRAY_ICON[5] = {ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_MOON, ICON_MENU};

// The fixed background: the room (lights off at night), the dimmed copy menus sit on (without the
// tray, so "Volver" does not stand on top of other buttons), then the tray painted on the room.
static void room_build(pet_sky_t sky)
{
    pet_gfx_t g = {s_room, PET_ROOM_W, PET_ROOM_H};
    pet_room_draw(&g, 0, 0, sky);
    if (sky == SKY_NIGHT) {
        pet_room_night(s_room, PET_ROOM_W * PET_ROOM_H);
    }
    memcpy(s_room_dim, s_room, PET_ROOM_W * PET_ROOM_H * 2);
    pet_room_dim(s_room_dim, PET_ROOM_W * PET_ROOM_H);
    for (int i = 0; i < 5; i++) {
        pet_room_button(&g, 30 + i * 72, TRAY_Y, 62, 56);
        pet_gfx_icon(&g, TRAY_ICON[i], 30 + i * 72 + 13, TRAY_Y + 7, 3);
    }
}

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

    // The room: drawn once as the fixed background, with the tray's wooden buttons painted on it.
    s_room = heap_caps_malloc(PET_ROOM_W * PET_ROOM_H * 2, MALLOC_CAP_SPIRAM);
    s_room_dim = heap_caps_malloc(PET_ROOM_W * PET_ROOM_H * 2, MALLOC_CAP_SPIRAM);
    s_scene_bg = heap_caps_malloc(PET_SCENE_W * PET_SCENE_H * 2, MALLOC_CAP_SPIRAM);
    for (int i = 0; i < 2; i++) {
        s_hud[i] = heap_caps_malloc(PET_ROOM_W * HUD_H * 2, MALLOC_CAP_SPIRAM);
    }
    if (s_room && s_room_dim) {
        room_build(SKY_DAY);
        s_room_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                 .w = PET_ROOM_W, .h = PET_ROOM_H, .stride = PET_ROOM_W * 2},
                                      .data_size = PET_ROOM_W * PET_ROOM_H * 2, .data = (const uint8_t *)s_room};
        s_room_dim_dsc = s_room_dsc;
        s_room_dim_dsc.data = (const uint8_t *)s_room_dim;
        s_room_img = lv_image_create(scr);
        lv_image_set_src(s_room_img, &s_room_dsc);
        lv_obj_set_pos(s_room_img, 0, 0);
    }
    if (s_hud[0] && s_hud[1]) {
        s_hud_dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                                                .w = PET_ROOM_W, .h = HUD_H, .stride = PET_ROOM_W * 2},
                                     .data_size = PET_ROOM_W * HUD_H * 2, .data = (const uint8_t *)s_hud[0]};
        pet_gfx_t g = {s_hud[0], PET_ROOM_W, HUD_H};
        pet_room_draw(&g, 0, HUD_Y, SKY_DAY);
        s_hud_img = lv_image_create(scr);
        lv_image_set_src(s_hud_img, &s_hud_dsc);
        lv_obj_set_pos(s_hud_img, 0, HUD_Y);
        s_hud_back = 1;
    }

    // Texts over the room, each with a hard pixel shadow (a dark copy 2-4 px lower).
    s_time_sh = make_label(scr, &pet_font_clock, INK);
    lv_obj_align(s_time_sh, LV_ALIGN_TOP_MID, 0, 8);
    s_time = make_label(scr, &pet_font_clock, 0xf4f4f4);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 4);
    s_batt = make_label(scr, &pet_font_s, 0x94b0c2);
    lv_obj_set_style_text_align(s_batt, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_batt, LV_ALIGN_TOP_LEFT, 56, 32);
    s_level_sh = make_label(scr, &pet_font_m, INK);
    lv_obj_set_style_text_align(s_level_sh, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_level_sh, LV_ALIGN_TOP_LEFT, 52, HUD_Y + 6);
    s_level = make_label(scr, &pet_font_m, 0x73eff7);
    lv_obj_set_style_text_align(s_level, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_level, LV_ALIGN_TOP_LEFT, 52, HUD_Y + 4);
    s_coins_sh = make_label(scr, &pet_font_m, INK);
    lv_obj_set_style_text_align(s_coins_sh, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_coins_sh, LV_ALIGN_TOP_LEFT, 302, HUD_Y + 6);
    s_coins = make_label(scr, &pet_font_m, 0xffcd75);
    lv_obj_set_style_text_align(s_coins, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_coins, LV_ALIGN_TOP_LEFT, 302, HUD_Y + 4);
    s_gain = make_label(scr, &pet_font_m, 0xa7f070);
    lv_obj_align(s_gain, LV_ALIGN_TOP_LEFT, 346, HUD_Y + 4);
    lv_obj_add_flag(s_gain, LV_OBJ_FLAG_HIDDEN);

    s_img = lv_image_create(scr);
    lv_image_set_src(s_img, &s_dsc);
    lv_obj_set_pos(s_img, SCENE_X, SCENE_Y);
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_img, on_scene, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_img, on_scene_press, LV_EVENT_PRESSING, NULL);

    // Tray: invisible hit areas over the painted buttons; a press darkens them.
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = make_button(scr, 62, 56, on_tray, (void *)(intptr_t)i);
        lv_obj_set_pos(b, 30 + i * 72, TRAY_Y);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(b, lv_color_black(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(b, 0, 0);
    }

    s_toast = make_label(scr, &pet_font_m, 0xf4f4f4);   // a small RPG message window
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(BOX_BG), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_toast, lv_color_hex(0xf4f4f4), 0);
    lv_obj_set_style_border_width(s_toast, 3, 0);
    lv_obj_set_style_outline_color(s_toast, lv_color_hex(INK), 0);
    lv_obj_set_style_outline_width(s_toast, 3, 0);
    lv_obj_set_style_pad_hor(s_toast, 14, 0);
    lv_obj_set_style_pad_ver(s_toast, 4, 0);
    lv_obj_set_style_radius(s_toast, 0, 0);
    lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, SCENE_Y + 6);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
}

// ---------- tick (ui_task) ----------

// The sky the room's window shows: night, the hour around dawn and dusk, or day.
static pet_sky_t sky_for(const pet_state_t *s, int64_t now)
{
    if (pet_is_night(s, now)) {
        return SKY_NIGHT;
    }
    const int h = (int)((now % 86400) / 3600);
    return (h == s->night_end_h || (h + 1) % 24 == s->night_start_h) ? SKY_DUSK : SKY_DAY;
}

// Redraws the HUD strip (XP segments, coin icon, need pips) into the back buffer when something
// it shows changed. Returns true when the LVGL image must switch buffers.
static bool hud_draw(const pet_state_t *s)
{
    if (!s_hud[0] || !s_hud[1]) {
        return false;
    }
    int have, need;
    pet_level_progress(s, &have, &need);
    const int xp6 = need > 0 ? have * 6 / need : 0;
    int pip[NEED_COUNT];
    uint32_t sig = (uint32_t)xp6;
    for (int i = 0; i < NEED_COUNT; i++) {
        const int v = s->stage == PET_EGG ? 0 : pet_need(s, (pet_need_t)i);
        pip[i] = v < 25 ? -1 - v / 6 : v / 20;   // low values: redraw as they drop (red pips)
        sig = sig * 31u + (uint32_t)(pip[i] + 8);
    }
    sig = sig * 31u + (uint32_t)s_sky;
    if (sig == s_hud_sig) {
        return false;
    }
    s_hud_sig = sig;
    pet_gfx_t g = {s_hud[s_hud_back], PET_ROOM_W, HUD_H};
    pet_room_draw(&g, 0, HUD_Y, s_sky);
    if (s_sky == SKY_NIGHT) {
        pet_room_night(g.px, PET_ROOM_W * HUD_H);
    }
    for (int i = 0; i < 6; i++) {   // XP: six segments after "Nv N"
        pet_gfx_fill(&g, 128 + i * 16, 14, 12, 8, pet_rgb565(INK));
        if (i < xp6) {
            pet_gfx_fill(&g, 130 + i * 16, 16, 8, 4, pet_rgb565(0x73eff7));
        }
    }
    pet_gfx_icon(&g, ICON_COIN, 272, 6, 2);
    for (int i = 0; i < NEED_COUNT; i++) {
        const int x = 26 + i * 92;
        pet_gfx_icon(&g, NEED_ICON[i], x, 36, 2);
        pet_room_pips(&g, x + 28, 42, s->stage == PET_EGG ? 0 : pet_need(s, (pet_need_t)i), NEED_COLOR[i]);
    }
    return true;
}

static void events_to_toasts(uint32_t ev, const pet_state_t *s)
{
    char b[48];
    if (ev & EVT_HATCHED) toast("¡Ha nacido!");
    else if (ev & EVT_EVOLVED) {
        snprintf(b, sizeof(b), "Ha evolucionado: %s%s%s", STAGE[s->stage], s->stage == PET_ADULT ? " " : "",
                 s->stage == PET_ADULT ? BRANCH[s->branch] : "");
        toast(b);
    } else if (ev & EVT_LEVEL) {
        snprintf(b, sizeof(b), "¡Nivel %d!", s->level);
        toast(b);
    } else if (ev & EVT_MILESTONE) {
        static const char *const M[] = {"monedas", "un Radar", "chuches", "un Salacot", "monedas"};
        snprintf(b, sizeof(b), "%d señales: regalo de %s", pet_milestone_signals(s->last_milestone),
                 M[s->last_milestone % 5]);
        toast(b);
    } else if (ev & EVT_WISH_DONE) toast("¡Deseo cumplido!");
    else if (ev & EVT_SICK) toast("Se ha puesto malito");
    else if (ev & EVT_CURED) toast("Ya está curado");
    else if (ev & EVT_WISH_NEW) toast("Quiere algo: toca la burbuja");
    else if (ev & EVT_DAILY) toast("Visita del día");
    else if (ev & EVT_TREAT) toast("Ha encontrado una chuche");
    else if (ev & EVT_POOP) toast("Ups… caca");
    else if (ev & EVT_NEW_SIGNAL) toast("Señal nueva olfateada");
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

// Sound and particles for what the engine just did; like the toasts, one at a time.
static void events_to_fx(uint32_t ev)
{
    if (ev & EVT_HATCHED) {
        pet_sound(SND_CRACK);
        pet_sound(SND_V_HELLO);
        fx_at_head(FX_STARS);
    } else if (ev & EVT_EVOLVED) {
        pet_sound(SND_EVOLVE);
        fx_at_head(FX_STARS);
    } else if (ev & EVT_LEVEL) {
        pet_sound(SND_LEVEL);
        fx_at_head(FX_STARS);
    } else if (ev & EVT_MILESTONE) {
        pet_sound(SND_RECORD);
    } else if (ev & EVT_WISH_DONE) {
        pet_sound(SND_WISH_DONE);
    } else if (ev & EVT_SICK) {
        pet_sound(SND_V_SAD);
    } else if (ev & EVT_WISH_NEW) {
        pet_sound(SND_V_ASK);
    } else if (ev & EVT_DAILY) {
        pet_sound(SND_COIN);
        fx_at_head(FX_COINS);
    } else if (ev & EVT_TREAT) {
        pet_sound(SND_STAR);
    } else if (ev & EVT_POOP) {
        pet_sound(SND_POOP);
    } else if (ev & EVT_NEW_SIGNAL) {
        pet_sound(SND_CLEAN);
    }
}

static bool scene_eq(const pet_scene_t *a, const pet_scene_t *b)
{
    return memcmp(&a->view, &b->view, sizeof(a->view)) == 0 && a->x == b->x && a->hop == b->hop &&
           a->poops == b->poops && a->bubble == b->bubble && a->bubble_wish == b->bubble_wish && a->night == b->night &&
           a->act == b->act && a->act_ms / 100 == b->act_ms / 100 && a->t_ms / 400 == b->t_ms / 400;
}

void pet_ui_wake(void)
{
    const int64_t now = pet_now_s();
    pet_lock();
    const bool greet = g_pet.stage != PET_EGG && !pet_is_asleep(&g_pet, now);
    pet_unlock();
    if (greet && s_kind == P_NONE) {
        pet_sound(SND_V_HELLO);
        start_override(ANIM_BOUNCE);
    }
}

void pet_ui_tick(void)
{
    static pet_scene_t last;
    static bool have_last;
    static int64_t last_sec = -1;
    static uint32_t last_decide, last_mono, last_zzz, last_crumb;
    static int last_hop;
    static bool parts_ready;
    static pet_state_t snap;

    if (!s_buf[0] || !s_buf[1]) {
        return;
    }
    const uint32_t mono = pet_mono_ms();
    if (game_tick(mono)) {
        have_last = false;
        return;
    }
    panel_fill();
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
    fx_req_t fxq[sizeof(s_fxq) / sizeof(s_fxq[0])];
    const int nfx = s_fxq_n;
    memcpy(fxq, s_fxq, sizeof(fxq));
    s_fxq_n = 0;
    pet_unlock();
    if (!parts_ready) {
        parts_ready = true;
        pet_parts_init(&s_parts, mono | 1u);
        last_mono = mono;
    }

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
    // Activities: every half minute or so, if it needs nothing, it does what it trains most.
    static uint32_t act_start, act_next = 20000;
    static uint8_t act;
    const bool free_to_act = !asleep && snap.stage != PET_EGG && want == WANT_NONE && !s_ov_active;
    if (act && (mono - act_start >= ACT_MS || !free_to_act)) {
        if (mono - act_start >= ACT_MS && act != ACT_DOZE) {
            pet_sound(SND_V_HAPPY);
        }
        act = ACT_NONE;
        act_next = mono + 25000 + (mono * 2654435761u >> 18) % 20000;
    } else if (!act && free_to_act && s_px == s_target && (int32_t)(mono - act_next) >= 0) {
        switch (pet_branch_lean(&snap)) {
        case BRANCH_AGILE: act = ACT_BALL; break;
        case BRANCH_STRONG: act = ACT_LIFT; break;
        case BRANCH_WISE: act = ACT_READ; break;
        default: act = ACT_DOZE; break;
        }
        act_start = mono;
        ESP_LOGI(TAG, "activity %d", act);
    }
    if (act) {
        s_target = s_px;   // stays put while busy
    }
    const uint32_t act_t = mono - act_start;

    const int8_t look = sc.view.look_x;
    pet_pose(&sc.view, anim, t);
    if (hop) {
        sc.view.look_x = look;
    }
    switch (act) {
    case ACT_BALL:   // hops with the ball, eyes on it
        hop = (int)(act_t % 700) < 350 ? (int)(act_t % 350) * (350 - (int)(act_t % 350)) / 2500 : 0;
        sc.view.look_x = s_px + PET_W * PET_SCENE_SCALE / 2 < PET_SCENE_W / 2 ? 1 : -1;
        sc.view.expr = EXPR_HAPPY;
        break;
    case ACT_LIFT:   // a squat with each rep
        sc.view.squash = (act_t / 500) % 2 ? 0 : 1;
        sc.view.expr = (act_t / 500) % 2 ? EXPR_HAPPY : EXPR_ANGRY;
        break;
    case ACT_READ:
        sc.view.look_x = 0;
        sc.view.expr = EXPR_NEUTRAL;
        break;
    case ACT_DOZE:
        sc.view.expr = EXPR_SLEEP;
        break;
    default: break;
    }
    if (s_finger_ms && mono - s_finger_ms < LOOK_MS && !asleep) {
        sc.view.look_x = s_finger_x < s_px + PET_W * PET_SCENE_SCALE / 2 ? -1 : 1;
    }
    sc.act = act;
    sc.act_ms = act_t;
    sc.x = s_px;
    sc.hop = hop;
    sc.poops = snap.poops;
    sc.bubble = bubble_icon(want, &snap);
    sc.bubble_wish = want == WANT_WISH;
    sc.night = pet_is_night(&snap, now);
    sc.t_ms = mono;
    const pet_sky_t sky = sky_for(&snap, now);
    if (s_scene_bg && sky != s_sky) {   // the window changes with the hour; at night the light is off
        const bool lights = (s_sky == SKY_NIGHT) != (sky == SKY_NIGHT) || s_sky == (pet_sky_t)-1;
        s_sky = sky;
        pet_gfx_t g = {s_scene_bg, PET_SCENE_W, PET_SCENE_H};
        pet_room_draw(&g, SCENE_X, SCENE_Y, sky);
        if (sky == SKY_NIGHT) {
            pet_room_night(s_scene_bg, PET_SCENE_W * PET_SCENE_H);
        }
        if (lights && s_room && s_room_img && bsp_display_lock(1000)) {   // twice a day
            room_build(sky);
            lv_obj_invalidate(s_room_img);
            bsp_display_unlock();
        }
        have_last = false;
    }
    sc.bg = s_scene_bg;

    // sound: the creature's voice follows its form; quiet at night and while it sleeps
    pet_sound_species(snap.stage, snap.branch, snap.view_seed);
    pet_sound_mute(sc.night || anim == ANIM_SLEEP);
    if (ev) {
        events_to_fx(ev);
    }

    // particles: queued bursts, plus a few that come from what the creature is doing
    int hx, hy;
    pet_scene_head(&sc, &hx, &hy);
    for (int i = 0; i < nfx; i++) {
        pet_parts_emit(&s_parts, (pet_fx_t)fxq[i].fx, fxq[i].x < 0 ? hx : fxq[i].x, fxq[i].x < 0 ? hy : fxq[i].y);
    }
    if ((anim == ANIM_SLEEP || sc.act == ACT_DOZE) && snap.stage != PET_EGG && mono - last_zzz >= 1500) {
        last_zzz = mono;
        pet_parts_emit(&s_parts, FX_ZZZ, hx, hy);
    }
    if (anim == ANIM_EAT && mono - last_crumb >= 260) {
        last_crumb = mono;
        pet_parts_emit(&s_parts, FX_CRUMBS, hx, hy + 30);
    }
    if (last_hop && !hop && ((mono >> 4) % 3) == 0) {   // landed after a step: a puff now and then
        pet_parts_emit(&s_parts, FX_DUST, hx, PET_SCENE_FLOOR);
    }
    last_hop = hop;
    pet_parts_step(&s_parts, mono - last_mono);
    last_mono = mono;
    const bool parts_on = pet_parts_active(&s_parts);
    sc.parts = &s_parts;

    if (ev & (EVT_HATCHED | EVT_EVOLVED | EVT_LEVEL | EVT_MILESTONE | EVT_WISH_DONE)) {
        pet_mark_dirty();
        if (ev & (EVT_WISH_DONE | EVT_LEVEL)) {
            fx_at_head(FX_COINS);
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
    static bool parts_were_on;
    const bool redraw = scene_visible && (!have_last || parts_on || parts_were_on || !scene_eq(&sc, &last));
    parts_were_on = parts_on;   // one more frame after the last particle, to clear it
    if (redraw) {
        pet_scene_draw(&sc, s_buf[s_back]);
        last = sc;
        have_last = true;
    }
    const bool hud_new = scene_visible && hud_draw(&snap);
    const bool toast_expired = s_toast_until && (int32_t)(mono - s_toast_until) >= 0;
    const bool gain_expired = s_gain_until && (int32_t)(mono - s_gain_until) >= 0;
    if (!redraw && !hud_new && !new_sec && !ev && !gain_c && !toast_expired && !gain_expired) {
        return;
    }
    if (!bsp_display_lock(LOCK_MS)) {
        have_last = false; // the swap did not happen: draw again next tick
        return;
    }
    if (hud_new) {
        s_hud_dsc.data = (const uint8_t *)s_hud[s_hud_back];
        lv_image_set_src(s_hud_img, &s_hud_dsc);
        lv_obj_invalidate(s_hud_img);
        s_hud_back ^= 1;
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
        lv_label_set_text(s_time_sh, b);
        const int pct = g_batt_pct;
        if (pct < 0) {
            lv_label_set_text(s_batt, g_usb ? LV_SYMBOL_CHARGE : "");
        } else {
            snprintf(b, sizeof(b), "%s%d%%", g_usb ? LV_SYMBOL_CHARGE : "", pct);
            lv_label_set_text(s_batt, b);
        }
        snprintf(b, sizeof(b), "Nv %d%s", snap.level, pet_time_speed() > 1 ? " >>" : "");
        lv_label_set_text(s_level, b);
        lv_label_set_text(s_level_sh, b);
        snprintf(b, sizeof(b), "%u", (unsigned)snap.coins);
        lv_label_set_text(s_coins, b);
        lv_label_set_text(s_coins_sh, b);
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
