#include "pet_room.h"
#include <string.h>

/* Paleta Sweetie-16 más unos tonos intermedios para madera y pared. */
#define INK    0x1a1c2c
#define WALL   0x29366f
#define WALL2  0x2f3c74
#define WALL3  0x333c78
#define SKIRT  0x232a58
#define SKIRT2 0x1d2350
#define TRIM   0x566c86
#define WOOD_D 0x3d2617
#define WOOD   0x5a3a1e
#define WOOD_M 0x7a4d2a
#define WOOD_L 0xa0663c
#define WOOD_H 0xd9a066

static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

typedef struct { pet_gfx_t *g; int ox, oy; } rv_t;   /* vista: búfer + desplazamiento */

static void f(const rv_t *v, int x, int y, int w, int h, uint32_t c)
{
    x -= v->ox;
    y -= v->oy;
    if (x >= v->g->w || y >= v->g->h || x + w <= 0 || y + h <= 0) return;
    pet_gfx_fill(v->g, x, y, w, h, pet_rgb565(c));
}

static void disc(const rv_t *v, int cx, int cy, int r, int s, uint32_t c)
{
    for (int y = -r; y <= r; y += s)
        for (int x = -r; x <= r; x += s)
            if (x * x + y * y <= r * r) f(v, cx + x - s / 2, cy + y - s / 2, s, s, c);
}

static void ellipse(const rv_t *v, int cx, int cy, int rx, int ry, int s, uint32_t c)
{
    for (int y = -ry; y <= ry; y += s)
        for (int x = -rx; x <= rx; x += s)
            if ((long)x * x * ry * ry + (long)y * y * rx * rx <= (long)rx * rx * ry * ry) f(v, cx + x - s / 2, cy + y - s / 2, s, s, c);
}

static void sprite(const rv_t *v, const char *const *rows, int n, const char *keys, const uint32_t *cols, int x, int y, int s)
{
    for (int j = 0; j < n; j++)
        for (int i = 0; rows[j][i]; i++) {
            const char *k = strchr(keys, rows[j][i]);
            if (rows[j][i] != '.' && k) f(v, x + i * s, y + j * s, s, s, cols[k - keys]);
        }
}

static void bands(const rv_t *v, int x, int y, int w, int h, const uint32_t *cols, int n, int s)
{
    for (int cy = 0; cy < h; cy += s) {
        const int t = cy * (n - 1) * 16 / (h > s ? h - s : 1);
        const int k = t / 16, fr = t % 16;
        for (int cx = 0; cx < w; cx += s) {
            const int pick = (k + 1 < n && fr > BAYER[(cy / s) & 3][(cx / s) & 3]) ? k + 1 : k;
            f(v, x + cx, y + cy, s, s, cols[pick]);
        }
    }
}

static const char *const PLANT[] = {
    "...gg..g....", "..gLg.gLg...", ".gLLggLLg.g.", "..gLgLLg.gLg", "g..gLLgggLg.", "gLg.gLLLLg..",
    ".gLggLgLg...", "..gggLgg....", "...ooooo....", "..oPPPPPo...", "..oPpPPPo...", "...oPPPo....", "...ooooo....",
};
static const char *const CLOUD[] = { "...wwww.....", ".wwwwwwww...", "wwwwwwwwwwww", ".llllllllll." };

static void window(const rv_t *v, pet_sky_t sky)
{
    const int wx = 228, wy = 150, ww = 130, wh = 110;
    f(v, wx - 10, wy - 10, ww + 20, wh + 20, WOOD);
    f(v, wx - 6, wy - 6, ww + 12, wh + 12, WOOD_L);
    static const uint32_t DAY[] = { 0x3b5dc9, 0x41a6f6, 0x73eff7 };
    static const uint32_t DUSK[] = { 0x5d275d, 0xb13e53, 0xef7d57, 0xffcd75 };
    static const uint32_t NIGHT[] = { 0x10142a, 0x1a1c2c, 0x29366f };
    if (sky == SKY_NIGHT) {
        bands(v, wx, wy, ww, wh, NIGHT, 3, 5);
        disc(v, wx + 96, wy + 30, 14, 5, 0xfff4c2);
        disc(v, wx + 102, wy + 26, 12, 5, 0x10142a);
        static const int ST[][2] = { { 14, 20 }, { 40, 66 }, { 70, 14 }, { 22, 86 }, { 112, 82 } };
        for (int i = 0; i < 5; i++) f(v, wx + ST[i][0], wy + ST[i][1], 5, 5, i % 2 ? 0x94b0c2 : 0xf4f4f4);
    } else if (sky == SKY_DUSK) {
        bands(v, wx, wy, ww, wh, DUSK, 4, 5);
        disc(v, wx + 92, wy + 92, 18, 5, 0xffe08a);
    } else {
        bands(v, wx, wy, ww, wh, DAY, 3, 5);
        disc(v, wx + 100, wy + 26, 15, 5, 0xffcd75);
        static const uint32_t CC[] = { 0xf4f4f4, 0xc5dbe6 };
        sprite(v, CLOUD, 4, "wl", CC, wx + 12, wy + 50, 5);
    }
    f(v, wx + ww / 2 - 3, wy, 6, wh, WOOD_L);
    f(v, wx, wy + wh / 2 - 3, ww, 6, WOOD_L);
    for (int i = 0; i < 4; i++) {   /* cortinas */
        const uint32_t c = i % 2 ? 0x8a2a3e : 0xb13e53;
        f(v, wx - 24 + i * 3, wy - 16 + i * 2, 20 - i * 3, wh + 30 - i * 6, c);
        f(v, wx + ww + 4, wy - 16 + i * 2, 20 - i * 3, wh + 30 - i * 6, c);
    }
    f(v, wx - 30, wy - 20, ww + 60, 8, WOOD);
}

void pet_room_draw(pet_gfx_t *g, int ox, int oy, pet_sky_t sky)
{
    const rv_t v = { g, ox, oy };
    const int fy = PET_ROOM_FLOOR;
    /* pared con papel pintado de rombos y zócalo */
    f(&v, 0, 0, PET_ROOM_W, fy, WALL);
    for (int y = 10; y < fy - 60; y += 30)
        for (int x = ((y / 30) % 2) * 20; x < PET_ROOM_W; x += 40) {
            f(&v, x + 4, y, 4, 4, WALL3);
            f(&v, x, y + 4, 12, 4, WALL2);
            f(&v, x + 4, y + 8, 4, 4, WALL3);
        }
    f(&v, 0, fy - 60, PET_ROOM_W, 60, SKIRT);
    f(&v, 0, fy - 64, PET_ROOM_W, 4, TRIM);
    for (int x = 0; x < PET_ROOM_W; x += 30) f(&v, x, fy - 56, 2, 52, SKIRT2);
    window(&v, sky);
    /* cuadro con un corazón */
    f(&v, 56, 160, 70, 60, WOOD);
    f(&v, 60, 164, 62, 52, 0xf4d29c);
    pet_gfx_icon(g, ICON_HEART, 67 - ox, 166 - oy, 4);
    /* suelo de tablas */
    f(&v, 0, fy, PET_ROOM_W, PET_ROOM_H - fy, WOOD);
    for (int y = fy; y < PET_ROOM_H; y += 20) {
        f(&v, 0, y, PET_ROOM_W, 3, WOOD_D);
        f(&v, 0, y + 3, PET_ROOM_W, 2, WOOD_M);
        const int off = ((y - fy) / 20 % 2) * 70;
        for (int x = off; x < PET_ROOM_W; x += 140) f(&v, x, y, 3, 20, WOOD_D);
    }
    /* planta y alfombra */
    static const uint32_t PC[] = { 0x257179, 0x38b764, 0x5d275d, 0xef7d57, 0xb13e53 };
    sprite(&v, PLANT, 13, "gLoPp", PC, 22, fy - 70, 6);
    ellipse(&v, 205, PET_ROOM_RUG_Y, 130, 26, 5, 0x8a2a3e);
    ellipse(&v, 205, PET_ROOM_RUG_Y - 2, 120, 21, 5, 0xb13e53);
    for (int i = -100; i <= 100; i += 25) f(&v, 205 + i - 2, PET_ROOM_RUG_Y - 4, 5, 5, 0xffcd75);
}

void pet_room_button(pet_gfx_t *g, int x, int y, int w, int h)
{
    pet_gfx_fill(g, x, y, w, h, pet_rgb565(INK));
    pet_gfx_fill(g, x + 3, y + 3, w - 6, h - 6, pet_rgb565(WOOD_L));
    pet_gfx_fill(g, x + 3, y + 3, w - 6, 4, pet_rgb565(WOOD_H));
    pet_gfx_fill(g, x + 3, y + h - 9, w - 6, 6, pet_rgb565(WOOD));
}

void pet_room_pips(pet_gfx_t *g, int x, int y, int value, uint32_t rgb)
{
    const uint32_t col = value < 25 ? 0xe43b44 : rgb;
    const uint32_t hi = ((col >> 1) & 0x7f7f7f) + 0x7f7f7f;   /* medio camino al blanco */
    for (int i = 0; i < 5; i++) {
        const bool on = value > i * 20 + 5 || (i == 0 && value > 0);
        pet_gfx_fill(g, x + i * 11, y, 9, 12, pet_rgb565(INK));
        pet_gfx_fill(g, x + i * 11 + 2, y + 2, 5, 8, pet_rgb565(on ? col : 0x333c57));
        if (on) pet_gfx_fill(g, x + i * 11 + 2, y + 2, 5, 2, pet_rgb565(hi));
    }
}

void pet_room_box(pet_gfx_t *g, int x, int y, int w, int h)
{
    pet_gfx_fill(g, x, y, w, h, pet_rgb565(INK));
    pet_gfx_fill(g, x + 3, y + 3, w - 6, h - 6, pet_rgb565(0xf4f4f4));
    pet_gfx_fill(g, x + 6, y + 6, w - 12, h - 12, pet_rgb565(INK));
    pet_gfx_fill(g, x + 9, y + 9, w - 18, h - 18, pet_rgb565(WALL));
}

void pet_room_dim(uint16_t *px, int n)
{
    for (int i = 0; i < n; i++) px[i] = (uint16_t)((px[i] >> 1) & 0x7BEF);
}

void pet_room_night(uint16_t *px, int n)
{
    for (int i = 0; i < n; i++) {
        const uint16_t c = px[i];
        const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
        /* 40 % de brillo con un poco de azul de luna */
        const int nr = r * 2 / 5, ng = g * 2 / 5, nb = b * 2 / 5 + 2;
        px[i] = (uint16_t)(nr << 11 | ng << 5 | (nb > 31 ? 31 : nb));
    }
}
