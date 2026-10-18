#include "pet_scene.h"
#include <math.h>
#include <string.h>

#define POOP_SCALE 3
#define BUBBLE_W   76
#define BUBBLE_H   68

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
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

/* Bocadillo de RPG: caja blanca con borde oscuro y un rabito escalonado hacia la cabeza. */
static void bubble(pet_gfx_t *g, const pet_scene_t *sc)
{
    int bx, by, bw, bh;
    pet_scene_bubble_rect(sc, &bx, &by, &bw, &bh);
    by += (int)((sc->t_ms / 400) % 2) * 3;                 /* flota */
    const uint16_t ink = pet_rgb565(0x1a1c2c);
    const uint16_t paper = pet_rgb565(sc->bubble_wish ? 0xfff4c2 : 0xf4f4f4);
    const uint16_t edge = sc->bubble_wish ? pet_rgb565(0xffcd75) : ink;
    pet_gfx_fill(g, bx, by, bw, bh, ink);
    if (sc->bubble_wish) pet_gfx_fill(g, bx + 3, by + 3, bw - 6, bh - 6, edge);
    pet_gfx_fill(g, bx + (sc->bubble_wish ? 6 : 4), by + (sc->bubble_wish ? 6 : 4), bw - (sc->bubble_wish ? 12 : 8),
                 bh - (sc->bubble_wish ? 12 : 8), paper);
    const int left = bx > sc->x + PET_W * PET_SCENE_SCALE / 2;   /* la cabeza queda a la izquierda */
    const int tx = left ? bx + 10 : bx + bw - 20;
    for (int k = 0; k < 3; k++) {
        const int x = tx + (left ? -k * 5 : k * 5);
        pet_gfx_fill(g, x, by + bh + k * 5, 10 - k * 2, 5, ink);
        if (k < 2) pet_gfx_fill(g, x + 2, by + bh + k * 5 - (k ? 0 : 4), 6 - k * 2, 4, paper);
    }
    pet_gfx_icon(g, (pet_icon_t)sc->bubble, bx + (bw - PET_ICON * 4) / 2, by + (bh - PET_ICON * 4) / 2, 4);
}

/* ---------------- partículas ---------------- */
enum { P_HEART, P_BUBBLE, P_POP, P_SPARK, P_STAR, P_COIN, P_DUST, P_CRUMB, P_Z, P_ORBIT };

static uint32_t prnd(pet_parts_t *ps)
{
    uint32_t x = ps->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return ps->rng = x ? x : 0x2545f491u;
}
static int pr(pet_parts_t *ps, int lo, int hi) { return lo + (int)(prnd(ps) % (uint32_t)(hi - lo + 1)); }

void pet_parts_init(pet_parts_t *ps, uint32_t seed)
{
    memset(ps, 0, sizeof(*ps));
    ps->rng = seed ? seed : 1;
}

static pet_part_t *spawn(pet_parts_t *ps, int kind, float x, float y, int life, int delay)
{
    pet_part_t *p = NULL;
    for (int i = 0; i < PET_PARTS_MAX; i++) {
        if (ps->p[i].life == 0) { p = &ps->p[i]; break; }
    }
    if (!p) {   /* lleno: sustituye a la más vieja */
        p = &ps->p[0];
        for (int i = 1; i < PET_PARTS_MAX; i++)
            if (ps->p[i].age * 1000 / ps->p[i].life > p->age * 1000 / p->life) p = &ps->p[i];
    }
    memset(p, 0, sizeof(*p));
    p->kind = (uint8_t)kind;
    p->x = x; p->y = y;
    p->life = (int16_t)life;
    p->age = (int16_t)-delay;
    p->ph = (uint8_t)pr(ps, 0, 255);
    return p;
}

/* salida en abanico: ángulo en grados (0 = derecha, 90 = arriba) y velocidad en px/s */
static void aim(pet_part_t *p, int deg, int speed)
{
    const float a = (float)deg * 3.14159265f / 180.0f;
    p->vx = cosf(a) * (float)speed;
    p->vy = -sinf(a) * (float)speed;
}

void pet_parts_emit(pet_parts_t *ps, pet_fx_t fx, int x, int y)
{
    static const uint32_t SPARK_C[4] = { 0xffffff, 0xffcd75, 0x73eff7, 0xa7f070 };
    pet_part_t *p;
    switch (fx) {
    case FX_HEARTS:
        for (int i = 0, n = pr(ps, 4, 6); i < n; i++) {
            p = spawn(ps, P_HEART, (float)(x + pr(ps, -45, 45)), (float)(y + pr(ps, -20, 10)), pr(ps, 900, 1500), pr(ps, 0, 380));
            p->vx = (float)pr(ps, -15, 15); p->vy = (float)-pr(ps, 45, 90); p->ay = -12;
            p->size = (uint8_t)pr(ps, 2, 3); p->wob = (uint8_t)pr(ps, 3, 8);
        }
        break;
    case FX_BUBBLES:
        for (int i = 0, n = pr(ps, 11, 15); i < n; i++) {
            p = spawn(ps, P_BUBBLE, (float)(x + pr(ps, -80, 80)), (float)pr(ps, y - 20, PET_SCENE_FLOOR - 12), pr(ps, 700, 1800),
                      pr(ps, 0, 800));
            p->vx = (float)pr(ps, -12, 12); p->vy = (float)-pr(ps, 20, 60); p->ay = -18;
            p->size = (uint8_t)(pr(ps, 4, 11) * 2); p->wob = (uint8_t)pr(ps, 2, 6);
        }
        break;
    case FX_SPARKLE:
        p = spawn(ps, P_STAR, (float)x, (float)y, 380, 0);
        p->vy = -40; p->size = 3;
        for (int i = 0, n = pr(ps, 7, 10); i < n; i++) {
            p = spawn(ps, P_SPARK, (float)x, (float)y, pr(ps, 250, 550), pr(ps, 0, 60));
            aim(p, pr(ps, 0, 359), pr(ps, 70, 170));
            p->ay = 220; p->size = (uint8_t)pr(ps, 3, 5);
            p->color = pet_rgb565(SPARK_C[pr(ps, 0, 3)]);
        }
        break;
    case FX_COINS:
        for (int i = 0, n = pr(ps, 3, 5); i < n; i++) {
            p = spawn(ps, P_COIN, (float)(x + pr(ps, -20, 20)), (float)(y - 10), pr(ps, 1000, 1400), pr(ps, 0, 260));
            p->vx = (float)pr(ps, -70, 70); p->vy = (float)-pr(ps, 200, 280); p->ay = 620; p->size = 2;
        }
        break;
    case FX_DUST:
        for (int i = 0, n = pr(ps, 4, 6); i < n; i++) {
            const int side = i & 1 ? 1 : -1;
            p = spawn(ps, P_DUST, (float)(x + side * pr(ps, 10, 40)), (float)(PET_SCENE_FLOOR - pr(ps, 3, 8)), pr(ps, 280, 480), 0);
            p->vx = (float)(side * pr(ps, 30, 75)); p->vy = (float)-pr(ps, 8, 30);
            p->size = (uint8_t)pr(ps, 4, 7); p->color = pet_rgb565(0x566c86);
        }
        break;
    case FX_CRUMBS:
        for (int i = 0, n = pr(ps, 3, 5); i < n; i++) {
            p = spawn(ps, P_CRUMB, (float)(x + pr(ps, -12, 12)), (float)(y + pr(ps, -4, 4)), pr(ps, 600, 950), pr(ps, 0, 80));
            p->vx = (float)pr(ps, -55, 55); p->vy = (float)-pr(ps, 60, 120); p->ay = 520;
            p->size = (uint8_t)pr(ps, 4, 7); p->color = pet_rgb565(pr(ps, 0, 1) ? 0xd6a35c : 0xf4d48a);
        }
        break;
    case FX_ZZZ:
        p = spawn(ps, P_Z, (float)(x + pr(ps, 20, 40)), (float)(y - pr(ps, 10, 30)), pr(ps, 1800, 2400), 0);
        p->vx = (float)pr(ps, 10, 25); p->vy = (float)-pr(ps, 18, 28); p->size = (uint8_t)pr(ps, 2, 3); p->wob = 4;
        break;
    case FX_STARS:
        for (int i = 0, n = pr(ps, 12, 16); i < n; i++) {
            const bool big = i % 3 == 0;
            p = spawn(ps, big ? P_STAR : P_SPARK, (float)x, (float)y, pr(ps, 600, 1100), pr(ps, 0, 150));
            aim(p, pr(ps, 0, 359), pr(ps, 80, 220));
            p->ay = 120; p->size = (uint8_t)(big ? pr(ps, 2, 3) : pr(ps, 3, 6));
            p->color = pet_rgb565(SPARK_C[pr(ps, 0, 3)]);
        }
        break;
    case FX_DIZZY:
        for (int i = 0; i < 3; i++) {
            p = spawn(ps, P_ORBIT, (float)x, (float)(y - 40), 1600, 0);
            p->ph = (uint8_t)(i * 85); p->size = 2;
        }
        break;
    default: break;
    }
}

void pet_parts_step(pet_parts_t *ps, uint32_t dt_ms)
{
    if (dt_ms > 100) dt_ms = 100;   /* tras una pausa no salen disparadas */
    const float dt = (float)dt_ms / 1000.0f;
    for (int i = 0; i < PET_PARTS_MAX; i++) {
        pet_part_t *p = &ps->p[i];
        if (p->life == 0) continue;
        p->age = (int16_t)(p->age + (int)dt_ms);
        if (p->age < 0) continue;
        if (p->age >= p->life) {
            if (p->kind == P_BUBBLE) {   /* la pompa explota */
                p->kind = P_POP; p->age = 0; p->life = 140;
                continue;
            }
            p->life = 0;
            continue;
        }
        if (p->kind == P_ORBIT) continue;
        p->vy += p->ay * dt;
        if (p->kind == P_DUST) { p->vx *= 0.9f; p->vy *= 0.9f; }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        if ((p->kind == P_COIN || p->kind == P_CRUMB) && p->y > PET_SCENE_FLOOR - 6 && p->vy > 0) {
            p->y = PET_SCENE_FLOOR - 6;   /* rebote con pérdida */
            p->vy = -p->vy * 0.45f;
            p->vx *= 0.6f;
        }
    }
}

bool pet_parts_active(const pet_parts_t *ps)
{
    for (int i = 0; i < PET_PARTS_MAX; i++) if (ps->p[i].life) return true;
    return false;
}

/* onda triangular -1..1 */
static float tri(float t)
{
    t -= floorf(t);
    return t < 0.5f ? t * 4.0f - 1.0f : 3.0f - t * 4.0f;
}

static void ring(pet_gfx_t *g, int x, int y, int d, uint16_t c, uint16_t hi)
{
    const int k = d > 10 ? 3 : 2;   /* esquina recortada */
    pet_gfx_fill(g, x + k, y, d - 2 * k, 2, c);
    pet_gfx_fill(g, x + k, y + d - 2, d - 2 * k, 2, c);
    pet_gfx_fill(g, x, y + k, 2, d - 2 * k, c);
    pet_gfx_fill(g, x + d - 2, y + k, 2, d - 2 * k, c);
    if (k == 3) {
        pet_gfx_fill(g, x + 2, y + 2, 1, 1, c); pet_gfx_fill(g, x + d - 3, y + 2, 1, 1, c);
        pet_gfx_fill(g, x + 2, y + d - 3, 1, 1, c); pet_gfx_fill(g, x + d - 3, y + d - 3, 1, 1, c);
    }
    pet_gfx_fill(g, x + k + 1, y + k, 2, 2, hi);   /* brillo */
}

static void diamond(pet_gfx_t *g, int x, int y, int s, uint16_t c)
{
    pet_gfx_fill(g, x - s, y - 1, 2 * s + 1, 3, c);
    pet_gfx_fill(g, x - 1, y - s, 3, 2 * s + 1, c);
}

void pet_parts_draw(const pet_parts_t *ps, pet_gfx_t *g)
{
    for (int i = 0; i < PET_PARTS_MAX; i++) {
        const pet_part_t *p = &ps->p[i];
        if (p->life == 0 || p->age < 0) continue;
        const int left = p->life - p->age;
        const int pct = p->age * 100 / p->life;
        if (p->kind != P_POP && p->kind != P_DUST && left < p->life / 6 && (p->age / 60) % 2) continue;   /* parpadea al final */
        const float sway = p->wob ? tri((float)p->age / 900.0f + (float)p->ph / 256.0f) * (float)p->wob : 0.0f;
        int x = (int)(p->x + sway), y = (int)p->y;
        const int sc = p->size > 1 && pct > 75 ? p->size - 1 : p->size;   /* encoge al final */
        switch (p->kind) {
        case P_HEART: pet_gfx_icon(g, ICON_HEART, x - PET_ICON * sc / 2, y - PET_ICON * sc / 2, sc); break;
        case P_STAR:  pet_gfx_icon(g, ICON_STAR, x - PET_ICON * sc / 2, y - PET_ICON * sc / 2, sc); break;
        case P_BUBBLE: {
            const int d = p->size;
            ring(g, x - d / 2, y - d / 2, d, pet_rgb565(0x9fe8ff), pet_rgb565(0xffffff));
            break;
        }
        case P_POP: {
            const int r = 4 + p->age / 20;
            const uint16_t c = pet_rgb565(0xd8f6ff);
            pet_gfx_fill(g, x - r, y, 3, 2, c); pet_gfx_fill(g, x + r - 2, y, 3, 2, c);
            pet_gfx_fill(g, x, y - r, 2, 3, c); pet_gfx_fill(g, x, y + r - 2, 2, 3, c);
            break;
        }
        case P_SPARK: diamond(g, x, y, sc, p->color); break;
        case P_COIN: {
            /* gira: moneda entera, de canto, entera... */
            const int f = (p->age / 70 + p->ph) % 4;
            if (f == 1 || f == 3) pet_gfx_fill(g, x - 3, y - PET_ICON, 6, PET_ICON * 2, pet_rgb565(f == 1 ? 0xffcd75 : 0xd69e3e));
            else pet_gfx_icon(g, ICON_COIN, x - PET_ICON, y - PET_ICON, 2);
            break;
        }
        case P_DUST: {
            const int s = p->size * left / p->life + 1;
            pet_gfx_fill(g, x - s / 2, y - s / 2, s, s, p->color);
            break;
        }
        case P_CRUMB: pet_gfx_fill(g, x, y, p->size, p->size, p->color); break;
        case P_Z: pet_gfx_text(g, "Z", x, y, sc + 1, pet_rgb565(0x94b0c2)); break;
        case P_ORBIT: {
            const float a = (float)p->age / 400.0f * 6.2832f + (float)p->ph / 256.0f * 6.2832f;
            const int ox = (int)(cosf(a) * 46.0f), oy = (int)(sinf(a) * 12.0f);
            pet_gfx_icon(g, ICON_STAR, x + ox - PET_ICON, y + oy - PET_ICON, sinf(a) > 0 ? 2 : 1);
            break;
        }
        default: break;
        }
    }
}

void pet_scene_head(const pet_scene_t *sc, int *x, int *y)
{
    static const int16_t HEAD_Y[PET_STAGE_COUNT] = { 170, 180, 150, 125 };
    *x = sc->x + PET_W * PET_SCENE_SCALE / 2;
    *y = HEAD_Y[sc->view.stage < PET_STAGE_COUNT ? sc->view.stage : 1] - sc->hop;
}

/* Objeto de la actividad: la pelota bota al lado, la pesa sube y baja sobre la cabeza, el libro
 * se sostiene delante (y pasa página). */
static void prop(pet_gfx_t *g, const pet_scene_t *sc)
{
    int hx, hy;
    pet_scene_head(sc, &hx, &hy);
    const uint32_t t = sc->act_ms;
    switch (sc->act) {
    case ACT_BALL: {
        const int ph = (int)(t % 700);                       /* parábola de 0,7 s */
        const int up = ph * (700 - ph) / 1300;               /* 0..94 px */
        const int bx = hx + (hx < PET_SCENE_W / 2 ? 80 : -80) - PET_ICON * 2;
        pet_gfx_icon(g, ICON_BALL, bx, PET_SCENE_FLOOR - PET_ICON * 4 - up, 4);
        break;
    }
    case ACT_LIFT: {
        const int lift = (t / 500) % 2 ? 0 : 22;
        pet_gfx_icon(g, ICON_DUMBBELL, hx - PET_ICON * 2, hy - 80 - lift, 4);
        break;
    }
    case ACT_READ:
        pet_gfx_icon(g, ICON_BOOK, hx - PET_ICON * 2, hy + 34 + (int)((t / 900) % 2) * 2, 4);
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
    if (sc->bg) {
        memcpy(dst, sc->bg, (size_t)PET_SCENE_W * PET_SCENE_H * 2);
    } else {
        memset(dst, 0, (size_t)PET_SCENE_W * PET_SCENE_H * 2);
        if (sc->night) stars(&g, sc->t_ms);
        /* suelo: una linea y unas briznas que no se queman (se desplazan con la escena) */
        const uint16_t floor_c = pet_rgb565(0x1e293b), tuft = pet_rgb565(0x253247);
        pet_gfx_fill(&g, 0, PET_SCENE_FLOOR, PET_SCENE_W, 3, floor_c);
        for (int i = 0; i < 9; i++) pet_gfx_fill(&g, 20 + i * 44 + (i % 2) * 9, PET_SCENE_FLOOR + 7 + (i % 3) * 3, 14, 2, tuft);
    }

    for (int i = 0; i < sc->poops && i < 4; i++) {
        int x, y, w, h;
        pet_scene_poop_rect(i, &x, &y, &w, &h);
        pet_gfx_icon(&g, ICON_POOP, x, y, POOP_SCALE);
        if (((sc->t_ms / 300) + (uint32_t)i) % 4 == 0)          /* moscas/olor */
            pet_gfx_fill(&g, x + w / 2 + (i % 2 ? 6 : -8), y - 10, 3, 3, pet_rgb565(0x94b0c2));
    }
    const int top = PET_SCENE_FLOOR + 2 - PET_H * PET_SCENE_SCALE - sc->hop;
    pet_gfx_creature(&g, &sc->view, sc->x, top, PET_SCENE_SCALE);
    if (sc->act) prop(&g, sc);
    if (sc->dark) dim(dst, (size_t)PET_SCENE_W * PET_SCENE_H);
    if (sc->bubble >= 0) bubble(&g, sc);
    if (sc->parts) pet_parts_draw(sc->parts, &g);
}
