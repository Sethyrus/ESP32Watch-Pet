#include "pet_scene.h"
#include <string.h>

#define POOP_SCALE 3
#define BUBBLE_W   76
#define BUBBLE_H   68

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

uint32_t pet_scene_fx_ms(pet_fx_t fx)
{
    switch (fx) {
    case FX_HEARTS:  return 1400;
    case FX_BUBBLES: return 1600;
    case FX_SPARKLE: return 500;
    case FX_COINS:   return 1200;
    default:         return 0;
    }
}

void pet_scene_poop_rect(int i, int *x, int *y, int *w, int *h)
{
    static const int16_t PX[4] = { 18, 346, 72, 292 };
    const int s = PET_ICON * POOP_SCALE;
    *x = PX[i & 3];
    *y = PET_SCENE_FLOOR - s + 2;
    *w = s;
    *h = s;
}

int pet_scene_poop_at(const pet_scene_t *sc, int x, int y)
{
    for (int i = sc->poops - 1; i >= 0; i--) {
        int px, py, pw, ph;
        pet_scene_poop_rect(i, &px, &py, &pw, &ph);
        if (x >= px - 14 && x < px + pw + 14 && y >= py - 18 && y < py + ph + 10) return i;   /* caja generosa */
    }
    return -1;
}

void pet_scene_bubble_rect(const pet_scene_t *sc, int *x, int *y, int *w, int *h)
{
    const int cx = sc->x + PET_W * PET_SCENE_SCALE / 2;
    *x = cx < PET_SCENE_W / 2 ? cx + 70 : cx - 70 - BUBBLE_W;   /* al lado libre de la cabeza */
    if (*x < 0) *x = 0;
    if (*x > PET_SCENE_W - BUBBLE_W) *x = PET_SCENE_W - BUBBLE_W;
    *y = 8;
    *w = BUBBLE_W;
    *h = BUBBLE_H;
}

bool pet_scene_bubble_at(const pet_scene_t *sc, int x, int y)
{
    if (sc->bubble < 0) return false;
    int bx, by, bw, bh;
    pet_scene_bubble_rect(sc, &bx, &by, &bw, &bh);
    return x >= bx - 10 && x < bx + bw + 10 && y >= by - 10 && y < by + bh + 20;
}

bool pet_scene_pet_at(const pet_scene_t *sc, int x, int y)
{
    const int s = PET_W * PET_SCENE_SCALE;
    return x >= sc->x + s / 8 && x < sc->x + s - s / 8 && y >= PET_SCENE_H - s / 2 - 120 && y < PET_SCENE_H;
}

static void stars(pet_gfx_t *g, uint32_t t_ms)
{
    const uint16_t dim = pet_rgb565(0x333c57), lit = pet_rgb565(0x94b0c2);
    for (int i = 0; i < 18; i++) {
        uint32_t h = mix(0x51a7u + (uint32_t)i * 977u);
        int x = (int)(h % PET_SCENE_W), y = (int)((h >> 9) % 150);
        bool tw = ((t_ms / 700 + h) % 7) == 0;
        pet_gfx_fill(g, x, y, 3, 3, tw ? lit : dim);
    }
}

static void bubble(pet_gfx_t *g, const pet_scene_t *sc)
{
    int bx, by, bw, bh;
    pet_scene_bubble_rect(sc, &bx, &by, &bw, &bh);
    by += (int)((sc->t_ms / 400) % 2) * 2;                 /* flota */
    const uint16_t edge = pet_rgb565(sc->bubble_wish ? 0xffcd75 : 0xef7d57), fill = pet_rgb565(0x10162a);
    pet_gfx_rrect(g, bx, by, bw, bh, 18, edge);
    pet_gfx_rrect(g, bx + 4, by + 4, bw - 8, bh - 8, 14, fill);
    /* rabito hacia la cabeza */
    const int left = bx > sc->x + PET_W * PET_SCENE_SCALE / 2;
    const int tx = left ? bx + 6 : bx + bw - 18;
    for (int k = 0; k < 3; k++) pet_gfx_fill(g, tx + (left ? -k * 6 : k * 6), by + bh + k * 6, 10 - k * 2, 6, edge);
    pet_gfx_icon(g, (pet_icon_t)sc->bubble, bx + (bw - PET_ICON * 4) / 2, by + (bh - PET_ICON * 4) / 2, 4);
}

static void effect(pet_gfx_t *g, const pet_scene_t *sc)
{
    const uint32_t dur = pet_scene_fx_ms(sc->fx);
    if (!dur || sc->fx_ms >= dur) return;
    const int t = (int)(sc->fx_ms * 100 / dur);            /* 0..99 */
    const int cx = sc->x + PET_W * PET_SCENE_SCALE / 2;
    switch (sc->fx) {
    case FX_HEARTS:
        for (int i = 0; i < 3; i++) {
            int lt = t - i * 15;
            if (lt < 0) continue;
            int x = cx - 60 + i * 50 + ((lt / 10) % 2) * 4, y = 120 - lt * 120 / 100;
            pet_gfx_icon(g, ICON_HEART, x, y, 3);
        }
        break;
    case FX_BUBBLES:
        for (int i = 0; i < 6; i++) {
            uint32_t h = mix((uint32_t)i * 131u + 7u);
            int x = cx - 110 + (int)(h % 200), y = 230 - (int)((t + (int)(h >> 8) % 40) * 200 / 140);
            pet_gfx_icon(g, ICON_SOAP, x, y, 2 + (int)(h >> 16) % 2);
        }
        break;
    case FX_SPARKLE: {
        int s = t < 50 ? 3 : 2;
        pet_gfx_icon(g, ICON_STAR, sc->fx_x - PET_ICON * s / 2, sc->fx_y - PET_ICON * s / 2 - t / 3, s);
        break;
    }
    case FX_COINS:
        for (int i = 0; i < 3; i++) {
            int lt = t - i * 12;
            if (lt < 0) continue;
            pet_gfx_icon(g, ICON_COIN, cx - 50 + i * 36, 110 - lt, 2);
        }
        break;
    default: break;
    }
}

static void dim(uint16_t *px, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint16_t c = px[i];
        px[i] = (uint16_t)(((c >> 2) & 0x39E7));            /* un cuarto de brillo */
    }
}

void pet_scene_draw(const pet_scene_t *sc, uint16_t *dst)
{
    pet_gfx_t g = { dst, PET_SCENE_W, PET_SCENE_H };
    memset(dst, 0, (size_t)PET_SCENE_W * PET_SCENE_H * 2);
    if (sc->night) stars(&g, sc->t_ms);
    /* suelo: una linea y unas briznas que no se queman (se desplazan con la escena) */
    const uint16_t floor_c = pet_rgb565(0x1e293b), tuft = pet_rgb565(0x253247);
    pet_gfx_fill(&g, 0, PET_SCENE_FLOOR, PET_SCENE_W, 3, floor_c);
    for (int i = 0; i < 9; i++) pet_gfx_fill(&g, 20 + i * 44 + (i % 2) * 9, PET_SCENE_FLOOR + 7 + (i % 3) * 3, 14, 2, tuft);

    for (int i = 0; i < sc->poops && i < 4; i++) {
        int x, y, w, h;
        pet_scene_poop_rect(i, &x, &y, &w, &h);
        pet_gfx_icon(&g, ICON_POOP, x, y, POOP_SCALE);
        if (((sc->t_ms / 300) + (uint32_t)i) % 4 == 0)          /* moscas/olor */
            pet_gfx_fill(&g, x + w / 2 + (i % 2 ? 6 : -8), y - 10, 3, 3, pet_rgb565(0x94b0c2));
    }
    const int top = PET_SCENE_FLOOR + 2 - PET_H * PET_SCENE_SCALE - sc->hop;
    pet_gfx_creature(&g, &sc->view, sc->x, top, PET_SCENE_SCALE);
    if (sc->dark) dim(dst, (size_t)PET_SCENE_W * PET_SCENE_H);
    if (sc->bubble >= 0) bubble(&g, sc);
    effect(&g, sc);
}
