/* mock: maquetas de las direcciones estéticas candidatas, a tamaño real (410x502).
 * Usa lo mismo que el firmware: LVGL 9 para textos y widgets, pet_gfx/pet_render para la criatura
 * y el pixel art, y tipografías convertidas con lv_font_conv (mock/fonts). Lo que se ve aquí se puede
 * construir tal cual en el reloj. `make -C tools/uisim mock` -> tools/uisim/out/mock/*.png */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "pet_gfx.h"
#include "png.h"
#include "fonts/pet_fonts.h"

#define W 410
#define H 502

LV_FONT_DECLARE(mk_pix16)
LV_FONT_DECLARE(mk_pix20)
LV_FONT_DECLARE(mk_pix28)
LV_FONT_DECLARE(mk_jersey64)
LV_FONT_DECLARE(mk_silk16)
LV_FONT_DECLARE(mk_j15_20)
LV_FONT_DECLARE(mk_j15_24)
LV_FONT_DECLARE(mk_j15_30)
LV_FONT_DECLARE(mk_cond72)
LV_FONT_DECLARE(mk_fre16)
LV_FONT_DECLARE(mk_fre20)
LV_FONT_DECLARE(mk_fre28)
LV_FONT_DECLARE(mk_fre56)

/* ---------- LVGL sin pantalla ---------- */
static uint16_t s_fb[W * H];
static lv_display_t *s_disp;
static uint32_t s_tick;
static uint32_t tick_cb(void) { return s_tick; }
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    const uint16_t *p = (const uint16_t *)px;
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) s_fb[y * W + x] = *p++;
    lv_display_flush_ready(d);
}

static int shot(const char *dir, const char *name)
{
    lv_refr_now(s_disp);
    static uint32_t rgb[W * H];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            /* fuera del rectángulo redondeado de la pantalla: gris */
            const int R = 100;
            int cx = x < R ? R : x > W - R ? W - R : x, cy = y < R ? R : y > H - R ? H - R : y;
            const bool out = (x - cx) * (x - cx) + (y - cy) * (y - cy) > R * R;
            const uint16_t p = s_fb[y * W + x];
            const uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
            rgb[y * W + x] = out ? 0x303030u : ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
        }
    char p[1024];
    snprintf(p, sizeof p, "%s/%s.png", dir, name);
    printf("%s\n", p);
    /* copia para la hoja comparativa: columna = dirección (a/b/c), fila = segunda cifra (1 inicio, 2 comer) */
    const int col = name[0] - 'a', row = name[1] - '1';
    if (col >= 0 && col < 3 && row >= 0 && row < 2) {
        extern uint32_t g_sheet[];
        for (int y = 0; y < H; y++)
            memcpy(&g_sheet[(row * (H + 20) + y) * (3 * W + 40) + col * (W + 20)], &rgb[y * W], W * 4);
    }
    return png_write_rgb(p, rgb, W, H);
}
uint32_t g_sheet[(2 * H + 20) * (3 * W + 40)];

/* ---------- lienzo de fondo (pixel art) ---------- */
static uint16_t s_bg[W * H];
static pet_gfx_t G = { s_bg, W, H };
static lv_image_dsc_t s_bg_dsc;

static void canvas_show(lv_obj_t *scr)
{
    s_bg_dsc = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = W, .h = H,
                                             .stride = W * 2 },
                                 .data_size = W * H * 2, .data = (const uint8_t *)s_bg };
    lv_obj_t *im = lv_image_create(scr);
    lv_image_set_src(im, &s_bg_dsc);
    lv_obj_set_pos(im, 0, 0);
}

static uint16_t C(uint32_t rgb) { return pet_rgb565(rgb); }
static void fill(int x, int y, int w, int h, uint32_t c) { pet_gfx_fill(&G, x, y, w, h, C(c)); }

static uint32_t lerp(uint32_t a, uint32_t b, int t, int n)
{
    int r = (int)(a >> 16 & 255) + ((int)(b >> 16 & 255) - (int)(a >> 16 & 255)) * t / n;
    int g = (int)(a >> 8 & 255) + ((int)(b >> 8 & 255) - (int)(a >> 8 & 255)) * t / n;
    int bl = (int)(a & 255) + ((int)(b & 255) - (int)(a & 255)) * t / n;
    return (uint32_t)(r << 16 | g << 8 | bl);
}

static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

/* Degradado vertical en bandas de colores con transición tramada, en celdas de s px. */
static void grad_bands(int x, int y, int w, int h, const uint32_t *cols, int n, int s)
{
    for (int cy = 0; cy < h; cy += s) {
        const int t = cy * (n - 1) * 16 / (h > s ? h - s : 1);   /* 0..(n-1)*16 */
        const int k = t / 16, f = t % 16;
        for (int cx = 0; cx < w; cx += s) {
            const int pick = (k + 1 < n && f > BAYER[(cy / s) & 3][(cx / s) & 3]) ? k + 1 : k;
            fill(x + cx, y + cy, s, s, cols[pick]);
        }
    }
}

/* Círculo relleno en celdas de s px (bordes pixelados). */
static void pdisc(int cx, int cy, int r, int s, uint32_t c)
{
    for (int y = -r; y <= r; y += s)
        for (int x = -r; x <= r; x += s)
            if (x * x + y * y <= r * r) fill(cx + x - s / 2, cy + y - s / 2, s, s, c);
}

static void pellipse(int cx, int cy, int rx, int ry, int s, uint32_t c)
{
    for (int y = -ry; y <= ry; y += s)
        for (int x = -rx; x <= rx; x += s)
            if ((long)x * x * ry * ry + (long)y * y * rx * rx <= (long)rx * rx * ry * ry) fill(cx + x - s / 2, cy + y - s / 2, s, s, c);
}

/* Mapa de caracteres -> colores, dibujado a escala s. '.' es transparente. */
static void sprite(const char *const *rows, int nrows, const char *keys, const uint32_t *cols, int x, int y, int s)
{
    for (int j = 0; j < nrows; j++)
        for (int i = 0; rows[j][i]; i++) {
            const char *k = strchr(keys, rows[j][i]);
            if (rows[j][i] != '.' && k) fill(x + i * s, y + j * s, s, s, cols[k - keys]);
        }
}

static void creature(const pet_view_t *v, int x, int y, int scale) { pet_gfx_creature(&G, v, x, y, scale); }
static void icon(pet_icon_t i, int x, int y, int s) { pet_gfx_icon(&G, i, x, y, s); }

/* ---------- LVGL ---------- */
static lv_obj_t *label(lv_obj_t *p, const lv_font_t *f, uint32_t color, const char *txt, int x, int y, lv_align_t al)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    lv_obj_align(l, al, x, y);
    return l;
}

/* texto con sombra dura de pixel art */
static void label_sh(lv_obj_t *p, const lv_font_t *f, uint32_t color, uint32_t shadow, int d, const char *txt, int x, int y,
                     lv_align_t al)
{
    label(p, f, shadow, txt, x, y + d, al);
    label(p, f, color, txt, x, y, al);
}

static lv_obj_t *ring(lv_obj_t *p, int cx, int cy, int r, int w, int val, uint32_t col, uint32_t bg)
{
    lv_obj_t *a = lv_arc_create(p);
    lv_obj_set_size(a, r * 2, r * 2);
    lv_obj_set_pos(a, cx - r, cy - r);
    lv_arc_set_rotation(a, 270);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, val);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}

static lv_obj_t *new_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(scr);
    memset(s_bg, 0, sizeof s_bg);
    return scr;
}

static const uint32_t NEED_C[4] = { 0xef7d57, 0xf28ab6, 0x41a6f6, 0xffcd75 };
static const pet_icon_t NEED_I[4] = { ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_BOLT };
static const int NEED_V[4] = { 72, 38, 90, 55 };

/* ======================================================================================
 * A. Juguete de bolsillo: una habitación en pixel art y menús de RPG.
 * ====================================================================================== */
static const char *const PLANT[] = {
    "...gg..g....", "..gLg.gLg...", ".gLLggLLg.g.", "..gLgLLg.gLg", "g..gLLgggLg.", "gLg.gLLLLg..",
    ".gLggLgLg...", "..gggLgg....", "...ooooo....", "..oPPPPPo...", "..oPpPPPo...", "...oPPPo....", "...ooooo....",
};
static const char *const CLOUD[] = { "...wwww.....", ".wwwwwwww...", "wwwwwwwwwwww", ".llllllllll." };

static void room(int wall_h, bool night)
{
    /* pared con papel pintado de rombos */
    fill(0, 0, W, wall_h, 0x29366f);
    for (int y = 10; y < wall_h - 60; y += 30)
        for (int x = ((y / 30) % 2) * 20; x < W; x += 40) { fill(x + 4, y, 4, 4, 0x333c78); fill(x, y + 4, 12, 4, 0x2f3c74); fill(x + 4, y + 8, 4, 4, 0x333c78); }
    /* zócalo */
    fill(0, wall_h - 60, W, 60, 0x232a58);
    fill(0, wall_h - 64, W, 4, 0x566c86);
    for (int x = 0; x < W; x += 30) fill(x, wall_h - 56, 2, 52, 0x1d2350);
    /* ventana con el cielo de la hora */
    const int wx = 228, wy = 150, ww = 130, wh = 110;
    fill(wx - 10, wy - 10, ww + 20, wh + 20, 0x5a3a1e);
    fill(wx - 6, wy - 6, ww + 12, wh + 12, 0xa0663c);
    static const uint32_t DAY[] = { 0x3b5dc9, 0x41a6f6, 0x73eff7 };
    static const uint32_t NIGHT[] = { 0x10142a, 0x1a1c2c, 0x29366f };
    grad_bands(wx, wy, ww, wh, night ? NIGHT : DAY, 3, 5);
    if (night) {
        pdisc(wx + 96, wy + 30, 14, 5, 0xfff4c2);
        pdisc(wx + 102, wy + 26, 12, 5, 0x1a1c2c);
        static const int ST[][2] = { { 14, 20 }, { 40, 62 }, { 70, 14 }, { 22, 84 }, { 110, 80 } };
        for (int i = 0; i < 5; i++) fill(wx + ST[i][0], wy + ST[i][1], 5, 5, 0xf4f4f4);
    } else {
        pdisc(wx + 100, wy + 26, 15, 5, 0xffcd75);
        static const uint32_t CC[] = { 0xf4f4f4, 0xc5dbe6 };
        sprite(CLOUD, 4, "wl", CC, wx + 12, wy + 50, 5);
    }
    fill(wx + ww / 2 - 3, wy, 6, wh, 0xa0663c);
    fill(wx, wy + wh / 2 - 3, ww, 6, 0xa0663c);
    /* cortinas */
    for (int i = 0; i < 4; i++) {
        fill(wx - 24 + i * 3, wy - 16 + i * 2, 20 - i * 3, wh + 30 - i * 6, i % 2 ? 0x8a2a3e : 0xb13e53);
        fill(wx + ww + 4 + i * 0, wy - 16 + i * 2, 20 - i * 3, wh + 30 - i * 6, i % 2 ? 0x8a2a3e : 0xb13e53);
    }
    fill(wx - 30, wy - 20, ww + 60, 8, 0x5a3a1e);
    /* cuadro con un corazón */
    fill(56, 160, 70, 60, 0x5a3a1e);
    fill(60, 164, 62, 52, 0xf4d29c);
    icon(ICON_HEART, 67, 166, 4);
    /* suelo de tablas */
    fill(0, wall_h, W, H - wall_h, 0x5a3a1e);
    for (int y = wall_h; y < H; y += 20) {
        fill(0, y, W, 3, 0x3d2617);
        fill(0, y + 3, W, 2, 0x7a4d2a);
        const int off = ((y - wall_h) / 20 % 2) * 70;
        for (int x = off; x < W; x += 140) fill(x, y, 3, 20, 0x3d2617);
    }
    /* planta */
    static const uint32_t PC[] = { 0x257179, 0x38b764, 0x5d275d, 0xef7d57, 0xb13e53 };
    sprite(PLANT, 13, "gLoPp", PC, 22, wall_h - 70, 6);
}

static void rpg_box(int x, int y, int w, int h)
{
    fill(x, y, w, h, 0x1a1c2c);
    fill(x + 3, y + 3, w - 6, h - 6, 0xf4f4f4);
    fill(x + 6, y + 6, w - 12, h - 12, 0x1a1c2c);
    fill(x + 9, y + 9, w - 18, h - 18, 0x29366f);
}

static void pips(int x, int y, int val, uint32_t col)
{
    for (int i = 0; i < 5; i++) {
        const bool on = val > i * 20 + 5;
        fill(x + i * 11, y, 9, 12, 0x1a1c2c);
        fill(x + i * 11 + 2, y + 2, 5, 8, on ? col : 0x333c57);
        if (on) fill(x + i * 11 + 2, y + 2, 5, 2, lerp(col, 0xffffff, 1, 2));
    }
}

static void dir_a_home(const char *dir, bool night)
{
    lv_obj_t *scr = new_screen();
    const int wall_h = 330;
    room(wall_h, night);
    /* alfombra y criatura */
    pellipse(205, 388, 130, 26, 5, 0x8a2a3e);
    pellipse(205, 386, 120, 21, 5, 0xb13e53);
    for (int i = -100; i <= 100; i += 25) fill(205 + i - 2, 384, 5, 5, 0xffcd75);
    pet_view_t v = { .seed = 0x2b5a, .stage = PET_JUVENILE, .expr = night ? EXPR_SLEEP : EXPR_HAPPY };
    creature(&v, 205 - 120, 398 - 240, 5);
    /* bocadillo de RPG: quiere fruta */
    if (!night) {
        fill(286, 196, 70, 62, 0x1a1c2c);
        fill(290, 200, 62, 54, 0xf4f4f4);
        fill(300, 258, 10, 5, 0x1a1c2c); fill(302, 258, 6, 3, 0xf4f4f4);
        fill(296, 262, 6, 5, 0x1a1c2c);
        icon(ICON_FRUIT, 297, 203, 4);
    }
    /* bandeja: botones de madera biselados, solo iconos */
    static const pet_icon_t T[5] = { ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_MOON, ICON_MENU };
    for (int i = 0; i < 5; i++) {
        const int x = 30 + i * 72, y = 418;
        fill(x, y, 62, 58, 0x1a1c2c);
        fill(x + 3, y + 3, 56, 52, 0xa0663c);
        fill(x + 3, y + 3, 56, 4, 0xd9a066);
        fill(x + 3, y + 49, 56, 6, 0x5a3a1e);
        icon(T[i], x + 13, y + 10, 3);
    }
    /* HUD: iconos y pips en el lienzo, textos en LVGL */
    icon(ICON_COIN, 296, 78, 2);
    for (int i = 0; i < 6; i++) fill(132 + i * 16, 84, 12, 8, i < 4 ? 0x73eff7 : 0x1a1c2c);   /* XP */
    for (int i = 0; i < 4; i++) {
        const int x = 26 + i * 92, y = 110;
        icon(NEED_I[i], x, y, 2);
        pips(x + 28, y + 6, NEED_V[i], NEED_C[i]);
    }
    canvas_show(scr);
    label_sh(scr, &mk_jersey64, night ? 0xc5dbe6 : 0xf4f4f4, 0x1a1c2c, 4, night ? "23:41" : "10:02", 0, 8, LV_ALIGN_TOP_MID);
    label_sh(scr, &mk_j15_24, 0x73eff7, 0x1a1c2c, 2, "Nv 3", 74, 74, LV_ALIGN_TOP_LEFT);
    label_sh(scr, &mk_j15_24, 0xffcd75, 0x1a1c2c, 2, "125", 324, 74, LV_ALIGN_TOP_LEFT);
    shot(dir, night ? "a3_juguete_noche" : "a1_juguete");
}

static void dir_a_food(const char *dir)
{
    lv_obj_t *scr = new_screen();
    room(330, false);
    pellipse(205, 388, 130, 26, 5, 0x8a2a3e);
    pellipse(205, 386, 120, 21, 5, 0xb13e53);
    pet_view_t v = { .seed = 0x2b5a, .stage = PET_JUVENILE, .expr = EXPR_HUNGRY };
    creature(&v, 205 - 120, 398 - 240, 5);
    /* atenúa el fondo */
    for (int i = 0; i < W * H; i++) s_bg[i] = (uint16_t)((s_bg[i] >> 1) & 0x7BEF);
    rpg_box(28, 40, 354, 360);
    static const pet_icon_t I[5] = { ICON_BREAD, ICON_FRUIT, ICON_STEW, ICON_CAKE, ICON_MEDICINE };
    for (int i = 0; i < 5; i++) {
        icon(I[i], 74, 98 + i * 56, 3);
        icon(ICON_COIN, 296, 106 + i * 56, 2);
    }
    /* cursor */
    static const char *const CUR[] = { "k.....", "kk....", "kwk...", "kwwk..", "kwk...", "kk....", "k....." };
    static const uint32_t CC[] = { 0x1a1c2c, 0xf4f4f4 };
    sprite(CUR, 7, "kw", CC, 50, 158, 3);
    canvas_show(scr);
    label(scr, &mk_j15_30, 0xffcd75, "COMER", 0, 50, LV_ALIGN_TOP_MID);
    static const char *const N[5] = { "Pan", "Fruta", "Guiso", "Pastel", "Medicina" };
    static const char *const D[5] = { "+25 comida", "+15 comida +5 diversión", "+45 comida", "+25 diversión", "cura el malito" };
    static const char *const P[5] = { "3", "5", "8", "10", "20" };
    for (int i = 0; i < 5; i++) {
        label(scr, &mk_j15_24, i == 1 ? 0xffffff : 0xc5dbe6, N[i], 120, 92 + i * 56, LV_ALIGN_TOP_LEFT);
        label(scr, &mk_j15_20, 0x94b0c2, D[i], 120, 116 + i * 56, LV_ALIGN_TOP_LEFT);
        label(scr, &mk_j15_24, 0xffcd75, P[i], 324, 101 + i * 56, LV_ALIGN_TOP_LEFT);
    }
    label_sh(scr, &mk_j15_24, 0xf4f4f4, 0x1a1c2c, 2, "Tienes 60 monedas", 0, 410, LV_ALIGN_TOP_MID);
    shot(dir, "a2_juguete_comer");
}

/* ======================================================================================
 * B. Jardín de noche: negro AMOLED, una isla flotante y complicaciones de reloj.
 * ====================================================================================== */
static const char *const ISLAND[] = {
    "....ggggggggggggggggggggggggggg....",
    "..gLLggLLLggggLLLggggLLggLLLggLLg..",
    ".gggggggggggggggggggggggggggggggggg",
    ".dDDdDDDdDDDDdDDDDdDDDdDDDDdDDDDdd.",
    "..dDDDDdDDDDDDDDdDDDDDDDdDDDDDDdd..",
    "...ddDDDDDDdDDDDDDDDDdDDDDDDDdd....",
    ".....ddDDDDDDDDdDDDDDDDDDDDdd......",
    ".......dddDDDDDDDDDDDDDDddd........",
    "..........dddDDDDDDDDddd...........",
    ".............dddDDdd...............",
    "................dd.................",
};

static void dir_b_home(const char *dir)
{
    lv_obj_t *scr = new_screen();
    /* halo tramado detrás de la criatura */
    static const uint32_t HALO[] = { 0x000000, 0x03100f, 0x061e20, 0x0a2a2e };
    for (int y = 120; y < 420; y += 2)
        for (int x = 40; x < 370; x += 2) {
            const int dx = x - 205, dy = (y - 300) * 4 / 3;
            const int d = 150 - (int)sqrt((double)(dx * dx + dy * dy));
            if (d <= 0) continue;
            const int t = d * 3 * 16 / 150, k = t / 16, f = t % 16;
            const int pick = (k < 3 && f > BAYER[(y / 2) & 3][(x / 2) & 3]) ? k + 1 : k;
            if (pick) fill(x, y, 2, 2, HALO[pick]);
        }
    /* estrellas */
    static const int ST[][2] = { { 60, 150 }, { 340, 140 }, { 300, 210 }, { 96, 240 }, { 150, 130 }, { 360, 300 }, { 44, 330 } };
    for (int i = 0; i < 7; i++) fill(ST[i][0], ST[i][1], i % 3 ? 3 : 5, i % 3 ? 3 : 5, i % 2 ? 0x566c86 : 0x94b0c2);
    /* isla */
    static const uint32_t IC[] = { 0x38b764, 0xa7f070, 0x333c57, 0x29366f };
    sprite(ISLAND, 11, "gLdD", IC, 205 - 35 * 5 / 2, 360, 5);
    fill(150, 356, 5, 5, 0xffcd75); fill(255, 354, 5, 5, 0xf28ab6); fill(275, 357, 5, 5, 0xf4f4f4);
    pet_view_t v = { .seed = 0x2b5a, .stage = PET_JUVENILE, .expr = EXPR_HAPPY };
    creature(&v, 205 - 120, 364 - 240, 5);
    /* botones redondos */
    static const pet_icon_t T[5] = { ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_MOON, ICON_MENU };
    for (int i = 0; i < 5; i++) {
        const int cx = 72 + i * 66, cy = 448;
        pdisc(cx, cy, 28, 1, 0x161b2e);
        icon(T[i], cx - 18, cy - 18, 3);
    }
    canvas_show(scr);
    label(scr, &mk_cond72, 0xf4f4f4, "10:02", 0, 2, LV_ALIGN_TOP_MID);
    label(scr, &pet_font_16, 0x94b0c2, "sáb 18  ·  Nivel 3  ·  125 monedas", 0, 84, LV_ALIGN_TOP_MID);
    /* complicaciones: cuatro anillos con su icono */
    static const int RP[4][2] = { { 56, 176 }, { 56, 262 }, { 354, 176 }, { 354, 262 } };
    for (int i = 0; i < 4; i++) {
        ring(scr, RP[i][0], RP[i][1], 30, 6, NEED_V[i], NEED_C[i], 0x1c2236);
    }
    lv_obj_t *ov = lv_obj_create(scr);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, W, H);
    static uint16_t ib[4][24 * 24];
    static lv_image_dsc_t id[4];
    for (int i = 0; i < 4; i++) {
        pet_icon_rgb565(NEED_I[i], 2, 0, ib[i]);
        id[i] = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = 24, .h = 24, .stride = 48 },
                                  .data_size = sizeof ib[i], .data = (const uint8_t *)ib[i] };
        lv_obj_t *im = lv_image_create(ov);
        lv_image_set_src(im, &id[i]);
        lv_obj_set_pos(im, RP[i][0] - 12, RP[i][1] - 12);
    }
    /* deseo: un aviso fino sobre la cabeza */
    lv_obj_t *pill = lv_obj_create(ov);
    lv_obj_set_size(pill, 150, 34);
    lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 116);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0x161b2e), 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_radius(pill, 17, 0);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    label(pill, &pet_font_16, 0xffcd75, "Quiere fruta", 0, 0, LV_ALIGN_CENTER);
    shot(dir, "b1_jardin_noche");
}

static void dir_b_food(const char *dir)
{
    lv_obj_t *scr = new_screen();
    static const pet_icon_t I[5] = { ICON_BREAD, ICON_FRUIT, ICON_STEW, ICON_CAKE, ICON_MEDICINE };
    for (int i = 0; i < 5; i++) {
        const int y = 92 + i * 62;
        pdisc(70, y + 24, 24, 1, 0x161b2e);
        icon(I[i], 52, y + 6, 3);
        if (i < 4) fill(110, y + 56, 250, 1, 0x1c2236);
    }
    canvas_show(scr);
    label(scr, &pet_font_28, 0xf4f4f4, "Comer", 0, 30, LV_ALIGN_TOP_MID);
    static const char *const N[5] = { "Pan", "Fruta", "Guiso", "Pastel", "Medicina" };
    static const char *const D[5] = { "+25 comida", "+15 comida  +5 diversión", "+45 comida", "+25 diversión", "cura el malito" };
    static const char *const P[5] = { "3", "5", "8", "10", "20" };
    for (int i = 0; i < 5; i++) {
        label(scr, &pet_font_20, 0xf4f4f4, N[i], 110, 96 + i * 62, LV_ALIGN_TOP_LEFT);
        label(scr, &pet_font_14, 0x94a3b8, D[i], 110, 122 + i * 62, LV_ALIGN_TOP_LEFT);
        label(scr, &pet_font_20, 0xffcd75, P[i], -54, 104 + i * 62, LV_ALIGN_TOP_RIGHT);
    }
    label(scr, &pet_font_16, 0x64748b, "60 monedas", 0, 420, LV_ALIGN_TOP_MID);
    shot(dir, "b2_jardin_comer");
}

/* ======================================================================================
 * C. Cuento: prado cálido en pixel art suave y tarjetas redondeadas color crema.
 * ====================================================================================== */
static const char *const TREE[] = {
    "....gggg....", "..ggLLggg...", ".gLLggggLg..", "gLggggggggg.", "gggggggLggg.", ".ggggggggggg",
    "..gggggggg..", "....tttg....", ".....tt.....", ".....tt.....", "....tttt....",
};

static void meadow(void)
{
    static const uint32_t SKY[] = { 0x5fa8d3, 0x8cc8de, 0xc6e3d8, 0xf6e7c1 };
    grad_bands(0, 0, W, 300, SKY, 4, 6);
    pdisc(66, 206, 30, 6, 0xfff4c2);
    pdisc(66, 206, 22, 6, 0xffe08a);
    /* nubes redondas */
    pdisc(160, 196, 18, 6, 0xffffff); pdisc(184, 186, 24, 6, 0xffffff); pdisc(210, 198, 16, 6, 0xffffff);
    fill(150, 198, 70, 14, 0xffffff);
    /* colinas */
    for (int x = 0; x < W; x += 6) {
        const int h1 = 250 + (int)(18 * sin(x / 60.0) + 10 * sin(x / 23.0));
        fill(x, h1 - h1 % 6, 6, 300 - h1 + h1 % 6 + 40, 0x8fc78a);
    }
    for (int x = 0; x < W; x += 6) {
        const int h2 = 290 + (int)(14 * sin(x / 45.0 + 2) + 6 * sin(x / 17.0));
        fill(x, h2 - h2 % 6, 6, H, 0x5aa469);
    }
    static const uint32_t TC[] = { 0x38b764, 0xa7f070, 0x8b5a2b };
    sprite(TREE, 11, "gLt", TC, 300, 196, 8);
    sprite(TREE, 11, "gLt", TC, 20, 236, 5);
    /* prado delantero con flores */
    fill(0, 360, W, H - 360, 0x3f8f5a);
    for (int y = 366; y < H; y += 18)
        for (int x = (y / 18 % 2) * 22; x < W; x += 44) fill(x, y, 6, 6, 0x4fa86a);
    static const int FL[][2] = { { 40, 380 }, { 96, 420 }, { 330, 390 }, { 370, 430 }, { 300, 470 }, { 60, 470 } };
    static const uint32_t FC[] = { 0xf28ab6, 0xffcd75, 0xf4f4f4 };
    for (int i = 0; i < 6; i++) { fill(FL[i][0], FL[i][1], 6, 6, FC[i % 3]); fill(FL[i][0] - 6, FL[i][1] + 6, 6, 6, FC[i % 3]); fill(FL[i][0] + 6, FL[i][1] + 6, 6, 6, FC[i % 3]); fill(FL[i][0], FL[i][1] + 12, 6, 6, FC[i % 3]); fill(FL[i][0], FL[i][1] + 6, 6, 6, 0xef7d57); }
}

static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h, int r, uint32_t bg)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_size(c, w, h);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, r, 0);
    lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static void dir_c_home(const char *dir)
{
    lv_obj_t *scr = new_screen();
    meadow();
    pellipse(205, 404, 90, 12, 6, 0x2f7348);   /* sombra */
    pet_view_t v = { .seed = 0x2b5a, .stage = PET_JUVENILE, .expr = EXPR_HAPPY, .acc = ACC_FLOWER };
    creature(&v, 205 - 120, 408 - 240, 5);
    canvas_show(scr);
    lv_obj_t *top = card(scr, 115, 14, 180, 64, 32, 0xfff4e0);
    label(top, &mk_fre56, 0x6b4226, "10:02", 0, -2, LV_ALIGN_CENTER);
    /* chips de necesidades */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = card(scr, 24 + i * 92, 88, 86, 34, 17, 0xfff4e0);
        static uint16_t ib[4][24 * 24];
        static lv_image_dsc_t id[4];
        pet_icon_rgb565(NEED_I[i], 2, pet_rgb565(0xfff4e0), ib[i]);
        id[i] = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = 24, .h = 24, .stride = 48 },
                                  .data_size = sizeof ib[i], .data = (const uint8_t *)ib[i] };
        lv_obj_t *im = lv_image_create(c);
        lv_image_set_src(im, &id[i]);
        lv_obj_align(im, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_t *b = lv_bar_create(c);
        lv_obj_set_size(b, 44, 10);
        lv_obj_align(b, LV_ALIGN_LEFT_MID, 34, 0);
        lv_bar_set_value(b, NEED_V[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xead7b7), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(NEED_C[i]), LV_PART_INDICATOR);
    }
    lv_obj_t *lv = card(scr, 116, 130, 84, 30, 15, 0x6b4226);
    label(lv, &mk_fre16, 0xfff4e0, "Nivel 3", 0, 0, LV_ALIGN_CENTER);
    lv_obj_t *co = card(scr, 210, 130, 84, 30, 15, 0x6b4226);
    label(co, &mk_fre16, 0xffcd75, "125 mon.", 0, 0, LV_ALIGN_CENTER);
    /* bocadillo redondo */
    lv_obj_t *bub = card(scr, 276, 178, 76, 60, 30, 0xffffff);
    static uint16_t fb[36 * 36];
    static lv_image_dsc_t fd;
    pet_icon_rgb565(ICON_FRUIT, 3, pet_rgb565(0xffffff), fb);
    fd = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = 36, .h = 36, .stride = 72 },
                           .data_size = sizeof fb, .data = (const uint8_t *)fb };
    lv_obj_t *fi = lv_image_create(bub);
    lv_image_set_src(fi, &fd);
    lv_obj_center(fi);
    card(scr, 282, 236, 14, 14, 7, 0xffffff);
    card(scr, 272, 252, 8, 8, 4, 0xffffff);
    /* dock */
    lv_obj_t *dock = card(scr, 26, 418, 358, 66, 33, 0xfff4e0);
    static const pet_icon_t T[5] = { ICON_FOOD, ICON_BALL, ICON_SOAP, ICON_MOON, ICON_MENU };
    static uint16_t tb[5][24 * 24];
    static lv_image_dsc_t td[5];
    static const char *const TL[5] = { "Comer", "Jugar", "Asear", "Dormir", "Más" };
    for (int i = 0; i < 5; i++) {
        pet_icon_rgb565(T[i], 2, pet_rgb565(0xfff4e0), tb[i]);
        td[i] = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = 24, .h = 24, .stride = 48 },
                                  .data_size = sizeof tb[i], .data = (const uint8_t *)tb[i] };
        lv_obj_t *im = lv_image_create(dock);
        lv_image_set_src(im, &td[i]);
        lv_obj_set_pos(im, 30 + i * 66 - 12 + 6, 8);
        label(dock, &mk_fre16, 0x6b4226, TL[i], 30 + i * 66 + 6 - 179, 36, LV_ALIGN_TOP_MID);
    }
    shot(dir, "c1_cuento");
}

static void dir_c_food(const char *dir)
{
    lv_obj_t *scr = new_screen();
    meadow();
    pet_view_t v = { .seed = 0x2b5a, .stage = PET_JUVENILE, .expr = EXPR_HUNGRY, .acc = ACC_FLOWER };
    creature(&v, 205 - 120, 300 - 240, 4);
    canvas_show(scr);
    lv_obj_t *sheet = card(scr, 10, 150, 390, 380, 32, 0xfff4e0);
    label(sheet, &mk_fre28, 0x6b4226, "¿Qué le das?", 0, 14, LV_ALIGN_TOP_MID);
    static const pet_icon_t I[5] = { ICON_BREAD, ICON_FRUIT, ICON_STEW, ICON_CAKE, ICON_MEDICINE };
    static const char *const N[5] = { "Pan", "Fruta", "Guiso", "Pastel", "Medicina" };
    static const char *const P[5] = { "3", "5", "8", "10", "20" };
    static uint16_t ib[5][36 * 36];
    static lv_image_dsc_t id[5];
    for (int i = 0; i < 4; i++) {
        lv_obj_t *row = card(sheet, 30 + (i % 2) * 168, 60 + (i / 2) * 120, 160, 110, 24, 0xffffff);
        pet_icon_rgb565(I[i], 3, pet_rgb565(0xffffff), ib[i]);
        id[i] = (lv_image_dsc_t){ .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = 36, .h = 36, .stride = 72 },
                                  .data_size = sizeof ib[i], .data = (const uint8_t *)ib[i] };
        lv_obj_t *im = lv_image_create(row);
        lv_image_set_src(im, &id[i]);
        lv_obj_align(im, LV_ALIGN_TOP_MID, 0, 10);
        label(row, &mk_fre20, 0x6b4226, N[i], 0, 50, LV_ALIGN_TOP_MID);
        lv_obj_t *pr = card(row, 46, 78, 68, 24, 12, 0xffcd75);
        char b[12];
        snprintf(b, sizeof b, "%s mon.", P[i]);
        label(pr, &mk_fre16, 0x6b4226, b, 0, 0, LV_ALIGN_CENTER);
    }
    (void)id[4];
    shot(dir, "c2_cuento_comer");
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "out/mock";
    lv_init();
    lv_tick_set_cb(tick_cb);
    s_disp = lv_display_create(W, H);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    static uint8_t buf[W * H * 2];
    lv_display_set_buffers(s_disp, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);
    dir_a_home(dir, false);
    dir_a_food(dir);
    dir_a_home(dir, true);
    dir_b_home(dir);
    dir_b_food(dir);
    dir_c_home(dir);
    dir_c_food(dir);
    char p[1024];
    snprintf(p, sizeof p, "%s/comparativa.png", dir);
    printf("%s\n", p);
    return png_write_rgb(p, g_sheet, 3 * W + 40, 2 * H + 20);
}
