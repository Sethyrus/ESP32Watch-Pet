/* Minijuegos: Atrapa (agilidad), Memoria (ingenio) y Pesas (fuerza). */
#include "pet_game.h"
#include "pet_gfx.h"
#include <string.h>

/* ---- Atrapa ---- */
#define CATCH_MS       30000
#define CATCH_SCALE    2
#define CATCH_PET_Y    (PET_GAME_H - PET_W * CATCH_SCALE - 2)
#define CATCH_BAND_Y0  196
#define CATCH_BAND_Y1  240
#define CATCH_REACH    36
#define CATCH_TARGET   72              /* puntuacion de rendimiento 100 (con la racha x2) */
#define CATCH_SPEED    9               /* px por 10 ms del personaje */
enum { DROP_FREE, DROP_FOOD, DROP_STAR, DROP_SPIKY };

/* ---- Memoria ---- */
#define MEM_START_LEN  3
#define MEM_ROUNDS     6
#define MEM_STEP_MS    650              /* primera ronda; cada ronda va más rápida */
#define MEM_STEP_MIN   380
#define MEM_WAIT_MS    5000
enum { MEM_READY, MEM_SHOW, MEM_INPUT, MEM_GOOD, MEM_FAIL, MEM_WIN };
static const int16_t PAD[4][4] = { {38, 30, 118, 92}, {244, 30, 118, 92}, {38, 150, 118, 92}, {244, 150, 118, 92} };
static const uint32_t PAD_ON[4]  = { 0xe43b44, 0x38b764, 0x41a6f6, 0xffcd75 };
static const uint32_t PAD_OFF[4] = { 0x4a1820, 0x153d24, 0x172f52, 0x4a3c1c };

/* ---- Pesas: precisión ---- */
#define LIFT_WEIGHTS   5
#define LIFT_TRY_MS    8000            /* sin tocar en este tiempo cuenta como fallo */
#define LIFT_LIVES     3
#define LIFT_ZONE_W    120             /* media anchura de la zona en la primera pesa (de 1000) */
#define LIFT_ZONE_MIN  50
enum { LIFT_PULL, LIFT_GOOD, LIFT_FAIL, LIFT_WIN, LIFT_MISS };

static int diff(const pet_game_t *g) { return g->level > 1 ? (g->level - 1 > 9 ? 9 : g->level - 1) : 0; }

static uint32_t rnd(pet_game_t *g)
{
    uint32_t x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x ? x : 1;
    return g->rng;
}

const char *pet_game_name(pet_game_id_t id)
{
    static const char *const N[GAME_COUNT] = { "Atrapa", "Memoria", "Pesas" };
    return id < GAME_COUNT ? N[id] : "?";
}

void pet_game_start(pet_game_t *g, pet_game_id_t id, uint32_t seed, const pet_view_t *pet)
{
    memset(g, 0, sizeof *g);
    g->id = (uint8_t)id;
    g->rng = seed ? seed : 0x9e3779b9u;
    g->pet = *pet;
    g->px = PET_GAME_W / 2;
    g->next_spawn = 400;
    g->lit = -1;
    for (int i = 0; i < 12; i++) g->seq[i] = (uint8_t)(rnd(g) % 4);
    g->len = MEM_START_LEN;
    g->lives = LIFT_LIVES;
    g->dir = 1;
    g->level = 1;
    pet_parts_init(&g->parts, g->rng ^ 0x51ed27u);
}

static void lift_new_weight(pet_game_t *g)
{
    int w = LIFT_ZONE_W - 16 * g->weight - 4 * diff(g);
    g->zone_w = (int16_t)(w < LIFT_ZONE_MIN ? LIFT_ZONE_MIN : w);
    g->zone = (int16_t)(g->zone_w + (int)(rnd(g) % (uint32_t)(1000 - 2 * g->zone_w)));
    g->lift_t = 0;
}

static int lift_speed(const pet_game_t *g)   /* unidades del medidor por segundo */
{
    const int v = 650 + 150 * g->weight + 30 * diff(g);
    return v > 1300 ? 1300 : v;
}

bool pet_game_over(const pet_game_t *g) { return g->over; }
int  pet_game_score(const pet_game_t *g) { return g->score; }

uint16_t pet_game_take_events(pet_game_t *g)
{
    const uint16_t e = g->events;
    g->events = 0;
    return e;
}

int pet_game_perf(const pet_game_t *g)
{
    int p;
    switch (g->id) {
    case GAME_CATCH:  p = g->score * 100 / CATCH_TARGET; break;
    case GAME_MEMORY: p = g->score * 100 / MEM_ROUNDS; break;
    default:          p = g->score * 100 / LIFT_WEIGHTS; break;
    }
    return p < 0 ? 0 : p > 100 ? 100 : p;
}

int pet_game_seconds_left(const pet_game_t *g)
{
    if (g->id == GAME_CATCH) return g->t_ms >= CATCH_MS ? 0 : (int)((CATCH_MS - g->t_ms + 999) / 1000);
    if (g->id == GAME_LIFT && g->phase == LIFT_PULL)
        return g->lift_t >= LIFT_TRY_MS ? 0 : (int)((LIFT_TRY_MS - g->lift_t + 999) / 1000);   /* (no se dibuja) */
    return -1;
}

const char *pet_game_hint(const pet_game_t *g)
{
    switch (g->id) {
    case GAME_CATCH: return "Arrastra: coge comida y estrellas, evita los rayos";
    case GAME_MEMORY:
        switch (g->phase) {
        case MEM_INPUT: return "Tu turno: repite la secuencia";
        case MEM_FAIL:  return "¡Fallo!";
        case MEM_WIN:   return "¡Perfecto! Todas las rondas";
        case MEM_GOOD:  return "¡Bien! Otra más larga";
        default:        return "Mira la secuencia";
        }
    default:
        switch (g->phase) {
        case LIFT_GOOD: return g->last_perfect ? "¡Perfecto!" : "¡Arriba!";
        case LIFT_MISS: return "¡Uy! Se le ha escapado";
        case LIFT_FAIL: return "Sin fuerzas por hoy";
        case LIFT_WIN:  return "¡Todas las pesas!";
        default:        return "Toca cuando la flecha esté en la zona verde";
        }
    }
}

/* ---------------- logica ---------------- */
static void catch_update(pet_game_t *g, uint32_t dt, const pet_input_t *in)
{
    const uint32_t t = g->t_ms;
    const int d = diff(g);
    if (in->down && t >= g->stun_until) {
        int step = (int)(dt * CATCH_SPEED / 10);
        int dx = in->x - g->px;
        g->px += dx > step ? step : dx < -step ? -step : dx;
    }
    if (g->px < 40) g->px = 40;
    if (g->px > PET_GAME_W - 40) g->px = PET_GAME_W - 40;
    if (t >= g->next_spawn && t < CATCH_MS - 1200) {
        for (int i = 0; i < 16; i++) {
            if (g->drops[i].kind != DROP_FREE) continue;
            uint32_t r = rnd(g) % 1000;
            const uint32_t spiky = 180 + 15 * (uint32_t)d;   /* más rayos con el nivel */
            g->drops[i].kind = r < 1000 - spiky - 140 ? DROP_FOOD : r < 1000 - spiky ? DROP_STAR : DROP_SPIKY;
            g->drops[i].icon = g->drops[i].kind == DROP_FOOD ? (uint8_t)(rnd(g) % 4)
                             : g->drops[i].kind == DROP_STAR ? ICON_STAR : ICON_SPIKY;
            g->drops[i].x = (int16_t)(30 + rnd(g) % (PET_GAME_W - 60));
            g->drops[i].y = -18;
            break;
        }
        g->next_spawn += (uint32_t)(800 - 350 * (int)(t / 100) / (CATCH_MS / 100) - 12 * d);
    }
    const int v = 120 + (int)(150 * t / CATCH_MS) + 8 * d;   /* px/s */
    for (int i = 0; i < 16; i++) {
        pet_drop_t *dr = &g->drops[i];
        if (dr->kind == DROP_FREE) continue;
        int ny = dr->y + (int)((v * (int)dt + 500) / 1000);
        bool hit = ny >= CATCH_BAND_Y0 && dr->y <= CATCH_BAND_Y1 && (dr->x - g->px) < CATCH_REACH && (g->px - dr->x) < CATCH_REACH;
        dr->y = (int16_t)ny;
        if (hit) {
            if (dr->kind == DROP_SPIKY) {
                g->score = g->score > 2 ? g->score - 2 : 0;
                g->stun_until = t + 700;
                g->flash = -1;
                g->combo = 0;
                g->events |= GEV_ZAP;
                pet_parts_emit(&g->parts, FX_DIZZY, g->px, CATCH_PET_Y + 40);
            } else {
                /* racha de 5 o más: la comida vale doble */
                g->score += dr->kind == DROP_STAR ? 3 : (g->combo >= 5 ? 2 : 1);
                g->flash = 1;
                if (g->combo < 255) g->combo++;
                g->events |= dr->kind == DROP_STAR ? GEV_STAR : GEV_CATCH;
                pet_parts_emit(&g->parts, dr->kind == DROP_STAR ? FX_STARS : FX_SPARKLE, dr->x, dr->y);
            }
            g->flash_until = t + 350;
            dr->kind = DROP_FREE;
        } else if (dr->y > PET_GAME_H + 20) {
            if (dr->kind != DROP_SPIKY) g->combo = 0;   /* se le escapó */
            dr->kind = DROP_FREE;
        }
    }
    if (t >= CATCH_MS) g->over = true;
}

static int pad_at(int x, int y)
{
    for (int i = 0; i < 4; i++)
        if (x >= PAD[i][0] - 8 && x < PAD[i][0] + PAD[i][2] + 8 && y >= PAD[i][1] - 8 && y < PAD[i][1] + PAD[i][3] + 8) return i;
    return -1;
}

static void mem_phase(pet_game_t *g, uint8_t p) { g->phase = p; g->phase_t = g->t_ms; }

/* Cada ronda la secuencia se enseña más deprisa (y algo más con el nivel). */
static uint32_t mem_step(const pet_game_t *g)
{
    const int v = MEM_STEP_MS - 45 * (g->len - MEM_START_LEN) - 10 * diff(g);
    return (uint32_t)(v < MEM_STEP_MIN ? MEM_STEP_MIN : v);
}

static void memory_update(pet_game_t *g, const pet_input_t *in, bool tap)
{
    const uint32_t e = g->t_ms - g->phase_t;
    if (g->lit >= 0 && g->phase == MEM_INPUT && g->t_ms >= g->lit_until) g->lit = -1;
    switch (g->phase) {
    case MEM_READY:
        if (e >= 900) mem_phase(g, MEM_SHOW);
        break;
    case MEM_SHOW: {
        const uint32_t sm = mem_step(g);
        uint32_t step = e / sm;
        if (step >= g->len) { mem_phase(g, MEM_INPUT); g->pos = 0; g->lit = -1; break; }
        const int8_t lit = (e % sm) < sm * 7 / 10 ? (int8_t)g->seq[step] : -1;
        if (lit >= 0 && g->lit < 0) {   /* entre tecla y tecla hay un hueco apagado */
            g->events |= GEV_PAD;
            g->ev_pad = lit;
        }
        g->lit = lit;
        break;
    }
    case MEM_INPUT: {
        if (tap) {
            int p = pad_at(in->x, in->y);
            if (p < 0) break;
            g->lit = (int8_t)p;
            g->lit_until = g->t_ms + 220;
            g->phase_t = g->t_ms;                     /* reinicia la espera */
            if (p != g->seq[g->pos]) { mem_phase(g, MEM_FAIL); g->events |= GEV_FAIL; break; }
            g->events |= GEV_PAD;
            g->ev_pad = (int8_t)p;
            if (++g->pos == g->len) {
                g->score++;
                pet_parts_emit(&g->parts, FX_HEARTS, 200, 150);
                if (g->score >= MEM_ROUNDS) { mem_phase(g, MEM_WIN); g->events |= GEV_WIN; pet_parts_emit(&g->parts, FX_STARS, 200, 140); }
                else { g->len++; mem_phase(g, MEM_GOOD); g->events |= GEV_GOOD; }
            }
        } else if (e >= MEM_WAIT_MS) {
            mem_phase(g, MEM_FAIL);
            g->events |= GEV_FAIL;
        }
        break;
    }
    case MEM_GOOD:
        if (e >= 220) g->lit = -1;
        if (e >= 700) mem_phase(g, MEM_SHOW);
        break;
    default:                                          /* fallo o victoria */
        if (e >= 220) g->lit = -1;
        if (e >= 1200) g->over = true;
        break;
    }
}

static void lift_miss(pet_game_t *g)
{
    g->events |= GEV_DROP;
    if (--g->lives == 0) {
        mem_phase(g, LIFT_FAIL);
    } else {
        mem_phase(g, LIFT_MISS);
    }
}

static void lift_update(pet_game_t *g, uint32_t dt, bool tap)
{
    const uint32_t e = g->t_ms - g->phase_t;
    switch (g->phase) {
    case LIFT_PULL: {
        if (g->zone_w == 0) lift_new_weight(g);
        g->lift_t += dt;
        g->fill += g->dir * lift_speed(g) * (int)dt / 1000;   /* la flecha rebota en los extremos */
        if (g->fill >= 1000) { g->fill = 2000 - g->fill; g->dir = -1; }
        if (g->fill <= 0) { g->fill = -g->fill; g->dir = 1; }
        if (tap) {
            const int off = g->fill > g->zone ? g->fill - g->zone : g->zone - g->fill;
            g->events |= GEV_TICK;
            if (off <= g->zone_w) {
                g->score++;
                g->last_perfect = off <= g->zone_w / 3;
                if (g->last_perfect) { g->perfects++; g->events |= GEV_PERFECT; }
                mem_phase(g, g->score >= LIFT_WEIGHTS ? LIFT_WIN : LIFT_GOOD);
                g->events |= g->score >= LIFT_WEIGHTS ? GEV_WIN : GEV_LIFTED;
                pet_parts_emit(&g->parts, g->last_perfect ? FX_STARS : FX_SPARKLE, 200, 110);
            } else {
                lift_miss(g);
            }
        } else if (g->lift_t >= LIFT_TRY_MS) {
            lift_miss(g);
        }
        break;
    }
    case LIFT_GOOD:
        if (e >= 900) { g->weight++; lift_new_weight(g); mem_phase(g, LIFT_PULL); }
        break;
    case LIFT_MISS:
        if (e >= 700) { lift_new_weight(g); mem_phase(g, LIFT_PULL); }
        break;
    default:
        if (e >= 1200) g->over = true;
        break;
    }
}

void pet_game_update(pet_game_t *g, uint32_t dt_ms, const pet_input_t *in)
{
    if (g->over) return;
    if (dt_ms > 100) dt_ms = 100;                     /* un tiron del sistema no salta medio juego */
    g->t_ms += dt_ms;
    const bool tap = in->down && !g->prev_down;
    switch (g->id) {
    case GAME_CATCH:  catch_update(g, dt_ms, in); break;
    case GAME_MEMORY: memory_update(g, in, tap); break;
    default:          lift_update(g, dt_ms, tap); break;
    }
    pet_parts_step(&g->parts, dt_ms);
    g->prev_down = in->down;
    if (g->t_ms >= PET_GAME_MAX_MS) g->over = true;
}

bool pet_game_bot_input(const pet_game_t *g, pet_input_t *out)
{
    out->down = false; out->x = 0; out->y = 0;
    switch (g->id) {
    case GAME_CATCH: {
        int best = -1, by = -1000;
        for (int i = 0; i < 16; i++) {
            const pet_drop_t *d = &g->drops[i];
            if (d->kind == DROP_FREE || d->kind == DROP_SPIKY || d->y > CATCH_BAND_Y1) continue;
            if (d->y > by) { by = d->y; best = i; }
        }
        int tx = best >= 0 ? g->drops[best].x : g->px;
        for (int i = 0; i < 16; i++) {                /* aparta de los bichos que estan cayendo encima */
            const pet_drop_t *d = &g->drops[i];
            if (d->kind == DROP_SPIKY && d->y > CATCH_BAND_Y0 - 60 && d->y < CATCH_BAND_Y1 &&
                (tx - d->x) < CATCH_REACH + 10 && (d->x - tx) < CATCH_REACH + 10)
                tx = d->x + (d->x < PET_GAME_W / 2 ? 1 : -1) * (CATCH_REACH + 20);
        }
        out->down = true; out->x = (int16_t)tx; out->y = 230;
        return true;
    }
    case GAME_MEMORY:
        if (g->phase != MEM_INPUT || g->prev_down) return true;
        out->down = true;
        out->x = (int16_t)(PAD[g->seq[g->pos]][0] + PAD[g->seq[g->pos]][2] / 2);
        out->y = (int16_t)(PAD[g->seq[g->pos]][1] + PAD[g->seq[g->pos]][3] / 2);
        return true;
    default: {
        /* toca con la flecha cerca del centro de la zona */
        const int off = g->fill > g->zone ? g->fill - g->zone : g->zone - g->fill;
        out->down = g->phase == LIFT_PULL && g->zone_w && !g->prev_down && off <= g->zone_w * 2 / 3;
        out->x = PET_GAME_W / 2; out->y = PET_GAME_H / 2;
        return true;
    }
    }
}

/* ---------------- dibujo ---------------- */
static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static void fill(pet_gfx_t *g, int x, int y, int w, int h, uint32_t c) { pet_gfx_fill(g, x, y, w, h, pet_rgb565(c)); }

static void bands(pet_gfx_t *g, int x, int y, int w, int h, const uint32_t *cols, int n, int s)
{
    for (int cy = 0; cy < h; cy += s) {
        const int t = cy * (n - 1) * 16 / (h > s ? h - s : 1);
        const int k = t / 16, fr = t % 16;
        for (int cx = 0; cx < w; cx += s)
            fill(g, x + cx, y + cy, s, s, cols[(k + 1 < n && fr > BAYER[(cy / s) & 3][(cx / s) & 3]) ? k + 1 : k]);
    }
}

static uint32_t mixc(uint32_t a, uint32_t b, int t, int n)
{
    const int r = (int)(a >> 16 & 255) + ((int)(b >> 16 & 255) - (int)(a >> 16 & 255)) * t / n;
    const int gg = (int)(a >> 8 & 255) + ((int)(b >> 8 & 255) - (int)(a >> 8 & 255)) * t / n;
    const int bb = (int)(a & 255) + ((int)(b & 255) - (int)(a & 255)) * t / n;
    return (uint32_t)(r << 16 | gg << 8 | bb);
}

static void number(pet_gfx_t *gf, int v, int x, int y, int sc, uint32_t c)
{
    char b[8], tmp[8];
    int n = 0, k = 0;
    do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 7);
    while (k) b[n++] = tmp[--k];
    b[n] = 0;
    pet_gfx_text(gf, b, x + 2, y + 2, sc, pet_rgb565(0x1a1c2c));   /* sombra dura */
    pet_gfx_text(gf, b, x, y, sc, pet_rgb565(c));
}

static void hud(pet_gfx_t *gf, const pet_game_t *g)
{
    pet_gfx_icon(gf, ICON_STAR, 8, 8, 2);
    number(gf, g->score, 40, 10, 4, 0xf4f4f4);
    int s = pet_game_seconds_left(g);
    if (g->id == GAME_CATCH && g->combo >= 5) {
        pet_gfx_text(gf, "X2", 122, 12, 4, pet_rgb565(0x1a1c2c));
        pet_gfx_text(gf, "X2", 120, 10, 4, pet_rgb565(0xffcd75));
    }
    if (s >= 0 && g->id == GAME_CATCH) {
        int w = (int)((CATCH_MS - (g->t_ms < CATCH_MS ? g->t_ms : CATCH_MS)) * 196 / CATCH_MS);
        fill(gf, 188, 14, 202, 12, 0x1a1c2c);
        fill(gf, 191, 17, w, 6, s <= 5 ? 0xef7d57 : 0xa7f070);
    }
}

static const char *const CLOUD[] = { "...wwww.....", ".wwwwwwww...", "wwwwwwwwwwww", ".llllllllll." };

static void render_catch(pet_gfx_t *gf, const pet_game_t *g)
{
    /* prado; el cielo se vuelve atardecer según se acaba el tiempo */
    const int t = (int)(g->t_ms < CATCH_MS ? g->t_ms : CATCH_MS);
    const uint32_t top = mixc(0x3b5dc9, 0x5d275d, t, CATCH_MS), mid = mixc(0x41a6f6, 0xb13e53, t, CATCH_MS),
                   low = mixc(0x73eff7, 0xffcd75, t, CATCH_MS);
    const uint32_t sky[3] = { top, mid, low };
    bands(gf, 0, 0, PET_GAME_W, 240, sky, 3, 5);
    const int cx = (int)(g->t_ms / 120) % (PET_GAME_W + 80) - 70;   /* una nube que pasa */
    for (int j = 0; j < 4; j++)
        for (int i = 0; CLOUD[j][i]; i++)
            if (CLOUD[j][i] != '.') fill(gf, cx + i * 5, 60 + j * 5, 5, 5, CLOUD[j][i] == 'w' ? 0xf4f4f4 : 0xc5dbe6);
    for (int x = 0; x < PET_GAME_W; x += 5) {   /* colinas */
        const int hh = 206 + ((x / 5 * 7) % 13) - ((x / 40) % 3) * 6;
        fill(gf, x, hh, 5, 240 - hh, 0x257179);
    }
    fill(gf, 0, 240, PET_GAME_W, 40, 0x38b764);
    fill(gf, 0, 240, PET_GAME_W, 5, 0xa7f070);
    for (int x = 8; x < PET_GAME_W; x += 37) fill(gf, x, 252 + (x % 3) * 6, 5, 5, 0x257179);
    pet_view_t v = g->pet;
    bool flash = g->t_ms < g->flash_until;
    v.expr = flash ? (g->flash > 0 ? EXPR_HAPPY : EXPR_SURPRISE) : EXPR_NEUTRAL;
    v.mouth_open = flash && g->flash > 0;
    int sx = g->t_ms < g->stun_until ? ((g->t_ms / 60) % 2 ? 4 : -4) : 0;
    pet_gfx_creature(gf, &v, g->px - PET_W * CATCH_SCALE / 2 + sx, CATCH_PET_Y, CATCH_SCALE);
    for (int i = 0; i < 16; i++) {
        const pet_drop_t *d = &g->drops[i];
        if (d->kind == DROP_FREE) continue;
        pet_gfx_icon(gf, (pet_icon_t)d->icon, d->x - 18, d->y - 18, 3);
    }
    pet_parts_draw(&g->parts, gf);
    hud(gf, g);
}

static void render_memory(pet_gfx_t *gf, const pet_game_t *g)
{
    /* escenario: fondo oscuro, telón a los lados, tablas abajo */
    fill(gf, 0, 0, PET_GAME_W, PET_GAME_H, 0x1a1c2c);
    for (int x = 0; x < PET_GAME_W; x += 20) fill(gf, x, 0, 10, 250, 0x211f36);
    for (int k = 0; k < 4; k++) {
        const uint32_t c = k % 2 ? 0x8a2a3e : 0xb13e53;
        fill(gf, k * 8, 0, 8, 250, c);
        fill(gf, PET_GAME_W - 8 - k * 8, 0, 8, 250, c);
    }
    fill(gf, 0, 0, PET_GAME_W, 14, 0xb13e53);
    for (int x = 0; x < PET_GAME_W; x += 20) fill(gf, x, 14, 10, 6, 0x8a2a3e);
    fill(gf, 0, 250, PET_GAME_W, 30, 0x7a4d2a);
    for (int x = 0; x < PET_GAME_W; x += 50) fill(gf, x, 250, 3, 30, 0x3d2617);
    fill(gf, 0, 250, PET_GAME_W, 3, 0xa0663c);
    /* teclas: botones de pixel art con bisel; la encendida brilla */
    for (int i = 0; i < 4; i++) {
        const bool on = g->lit == i;
        const int x = PAD[i][0], y = PAD[i][1], w = PAD[i][2], h = PAD[i][3];
        fill(gf, x - 3, y - 3, w + 6, h + 6, on ? 0xf4f4f4 : 0x1a1c2c);
        fill(gf, x, y, w, h, on ? PAD_ON[i] : PAD_OFF[i]);
        fill(gf, x, y, w, 6, on ? 0xf4f4f4 : mixc(PAD_OFF[i], 0xffffff, 1, 4));
        fill(gf, x, y + h - 8, w, 8, mixc(on ? PAD_ON[i] : PAD_OFF[i], 0x000000, 1, 3));
    }
    /* la criatura baila: salta con cada nota y mira hacia la tecla */
    pet_view_t v = g->pet;
    v.expr = g->phase == MEM_GOOD || g->phase == MEM_WIN ? EXPR_HAPPY : g->phase == MEM_FAIL ? EXPR_SAD : EXPR_NEUTRAL;
    int hop = 0;
    if (g->lit >= 0) { v.look_x = (g->lit & 1) ? 1 : -1; hop = 10; v.mouth_open = g->phase == MEM_SHOW; }
    pet_gfx_creature(gf, &v, 152, 154 - hop, 2);
    for (int i = 0; i < MEM_ROUNDS; i++) {   /* rondas superadas */
        fill(gf, 200 - MEM_ROUNDS * 9 + i * 18, 262, 12, 12, 0x1a1c2c);
        fill(gf, 202 - MEM_ROUNDS * 9 + i * 18, 264, 8, 8, i < g->score ? 0xffcd75 : 0x3d2617);
    }
    pet_parts_draw(&g->parts, gf);
}

static void render_lift(pet_gfx_t *gf, const pet_game_t *g)
{
    /* gimnasio: azulejos, espejo, suelo de goma */
    fill(gf, 0, 0, PET_GAME_W, 230, 0x566c86);
    for (int y = 0; y < 230; y += 20)
        for (int x = (y / 20 % 2) * 20; x < PET_GAME_W; x += 40) fill(gf, x, y, 20, 20, 0x5f7690);
    fill(gf, 0, 120, PET_GAME_W, 4, 0x333c57);
    fill(gf, 0, 230, PET_GAME_W, 50, 0x333c57);
    fill(gf, 60, 236, 280, 40, 0x29366f);
    fill(gf, 60, 236, 280, 4, 0x3b5dc9);
    /* medidor de arriba: zona verde, centro dorado y la flecha */
    const int mx = 30, mw = 340, my = 48;
    fill(gf, mx - 3, my - 3, mw + 6, 22, 0x1a1c2c);
    fill(gf, mx, my, mw, 16, 0x333c57);
    if (g->zone_w) {
        const int z0 = mx + (g->zone - g->zone_w) * mw / 1000, z1 = mx + (g->zone + g->zone_w) * mw / 1000;
        fill(gf, z0, my, z1 - z0, 16, 0x38b764);
        const int p0 = mx + (g->zone - g->zone_w / 3) * mw / 1000, p1 = mx + (g->zone + g->zone_w / 3) * mw / 1000;
        fill(gf, p0, my, p1 - p0 < 2 ? 2 : p1 - p0, 16, 0xa7f070);
    }
    const int ax = mx + g->fill * mw / 1000;
    if (g->phase == LIFT_PULL) {
        fill(gf, ax - 2, my - 8, 4, 32, 0xf4f4f4);
        fill(gf, ax - 6, my - 10, 12, 4, 0xf4f4f4);
        fill(gf, ax - 6, my + 22, 12, 4, 0xf4f4f4);
    }
    /* criatura con la barra: arriba al acertar, en el suelo al fallar */
    const int pscale = 3, px = PET_GAME_W / 2 - PET_W * pscale / 2, py = PET_GAME_H - 30 - PET_W * pscale + 6;
    const bool up = g->phase == LIFT_GOOD || g->phase == LIFT_WIN;
    const bool down = g->phase == LIFT_MISS || g->phase == LIFT_FAIL;
    const int bar_y = up ? 96 : down ? 238 : 150 + ((g->t_ms / 200) % 2) * 3;
    pet_view_t v = g->pet;
    v.expr = up ? EXPR_HAPPY : down ? EXPR_SAD : EXPR_ANGRY;
    v.squash = g->phase == LIFT_PULL && (g->t_ms / 200) % 2;
    pet_gfx_creature(gf, &v, px, py, pscale);
    const uint32_t steel = 0x94b0c2;
    fill(gf, 64, bar_y, 272, 8, 0x1a1c2c);
    fill(gf, 66, bar_y + 2, 268, 4, steel);
    const int dh = 34 + g->weight * 8;
    for (int k = 0; k < 2; k++) {
        const int dx = k ? 296 : 80;
        fill(gf, dx - 2, bar_y + 4 - dh / 2 - 2, 28, dh + 4, 0x1a1c2c);
        fill(gf, dx, bar_y + 4 - dh / 2, 24, dh, 0xb13e53);
        fill(gf, dx + 4, bar_y + 8 - dh / 2, 4, dh - 8, 0xef7d57);
    }
    /* pesa N/5 y vidas */
    char b[8] = "0/5";
    b[0] = (char)('0' + (g->weight + 1 > LIFT_WEIGHTS ? LIFT_WEIGHTS : g->weight + 1));
    pet_gfx_text(gf, b, 400 - pet_gfx_text_width(b, 4) - 10, 12, 4, pet_rgb565(0x1a1c2c));
    pet_gfx_text(gf, b, 400 - pet_gfx_text_width(b, 4) - 12, 10, 4, pet_rgb565(0xf4f4f4));
    for (int i = 0; i < LIFT_LIVES; i++)
        if (i < g->lives) pet_gfx_icon(gf, ICON_HEART, 150 + i * 30, 8, 2);
    number(gf, g->score, 40, 10, 4, 0xf4f4f4);
    pet_gfx_icon(gf, ICON_STAR, 8, 8, 2);
    pet_parts_draw(&g->parts, gf);
}

void pet_game_render(const pet_game_t *g, uint16_t *dst)
{
    pet_gfx_t gf = { dst, PET_GAME_W, PET_GAME_H };
    switch (g->id) {
    case GAME_CATCH:  render_catch(&gf, g); break;
    case GAME_MEMORY: render_memory(&gf, g); break;
    default:          render_lift(&gf, g); break;
    }
}
