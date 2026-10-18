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
#define CATCH_TARGET   44              /* puntuacion de rendimiento 100 */
#define CATCH_SPEED    9               /* px por 10 ms del personaje */
enum { DROP_FREE, DROP_FOOD, DROP_STAR, DROP_SPIKY };

/* ---- Memoria ---- */
#define MEM_START_LEN  3
#define MEM_ROUNDS     6
#define MEM_STEP_MS    650
#define MEM_ON_MS      450
#define MEM_WAIT_MS    5000
enum { MEM_READY, MEM_SHOW, MEM_INPUT, MEM_GOOD, MEM_FAIL, MEM_WIN };
static const int16_t PAD[4][4] = { {12, 10, 168, 116}, {220, 10, 168, 116}, {12, 154, 168, 116}, {220, 154, 168, 116} };
static const uint32_t PAD_ON[4]  = { 0xe43b44, 0x38b764, 0x41a6f6, 0xffcd75 };
static const uint32_t PAD_OFF[4] = { 0x4a1820, 0x153d24, 0x172f52, 0x4a3c1c };

/* ---- Pesas ---- */
#define LIFT_WEIGHTS   5
#define LIFT_TRY_MS    6000
#define LIFT_TAP       90
enum { LIFT_PULL, LIFT_GOOD, LIFT_FAIL, LIFT_WIN };

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
}

bool pet_game_over(const pet_game_t *g) { return g->over; }
int  pet_game_score(const pet_game_t *g) { return g->score; }

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
        return g->lift_t >= LIFT_TRY_MS ? 0 : (int)((LIFT_TRY_MS - g->lift_t + 999) / 1000);
    return -1;
}

const char *pet_game_hint(const pet_game_t *g)
{
    switch (g->id) {
    case GAME_CATCH: return "Arrastra: coge comida y estrellas, evita los bichos";
    case GAME_MEMORY:
        switch (g->phase) {
        case MEM_INPUT: return "Tu turno: repite la secuencia";
        case MEM_FAIL:  return "Fallo!";
        case MEM_WIN:   return "Perfecto! Todas las rondas";
        case MEM_GOOD:  return "Bien! Otra mas larga";
        default:        return "Mira la secuencia";
        }
    default: return "Toca muy rapido para levantar la pesa";
    }
}

/* ---------------- logica ---------------- */
static void catch_update(pet_game_t *g, uint32_t dt, const pet_input_t *in)
{
    const uint32_t t = g->t_ms;
    if (in->down && t >= g->stun_until) {
        int step = (int)(dt * CATCH_SPEED / 10);
        int d = in->x - g->px;
        g->px += d > step ? step : d < -step ? -step : d;
    }
    if (g->px < 40) g->px = 40;
    if (g->px > PET_GAME_W - 40) g->px = PET_GAME_W - 40;
    if (t >= g->next_spawn && t < CATCH_MS - 1200) {
        for (int i = 0; i < 16; i++) {
            if (g->drops[i].kind != DROP_FREE) continue;
            uint32_t r = rnd(g) % 100;
            g->drops[i].kind = r < 68 ? DROP_FOOD : r < 82 ? DROP_STAR : DROP_SPIKY;
            g->drops[i].icon = g->drops[i].kind == DROP_FOOD ? (uint8_t)(rnd(g) % 4)
                             : g->drops[i].kind == DROP_STAR ? ICON_STAR : ICON_SPIKY;
            g->drops[i].x = (int16_t)(30 + rnd(g) % (PET_GAME_W - 60));
            g->drops[i].y = -18;
            break;
        }
        g->next_spawn += 800 - 350 * t / CATCH_MS;
    }
    const int v = 120 + (int)(150 * t / CATCH_MS);   /* px/s */
    for (int i = 0; i < 16; i++) {
        pet_drop_t *d = &g->drops[i];
        if (d->kind == DROP_FREE) continue;
        int ny = d->y + (int)((v * (int)dt + 500) / 1000);
        bool hit = ny >= CATCH_BAND_Y0 && d->y <= CATCH_BAND_Y1 && (d->x - g->px) < CATCH_REACH && (g->px - d->x) < CATCH_REACH;
        d->y = (int16_t)ny;
        if (hit) {
            if (d->kind == DROP_SPIKY) {
                g->score = g->score > 2 ? g->score - 2 : 0;
                g->stun_until = t + 700;
                g->flash = -1;
            } else {
                g->score += d->kind == DROP_STAR ? 3 : 1;
                g->flash = 1;
            }
            g->flash_until = t + 350;
            d->kind = DROP_FREE;
        } else if (d->y > PET_GAME_H + 20) {
            d->kind = DROP_FREE;
        }
    }
    if (t >= CATCH_MS) g->over = true;
}

static int pad_at(int x, int y)
{
    for (int i = 0; i < 4; i++)
        if (x >= PAD[i][0] && x < PAD[i][0] + PAD[i][2] && y >= PAD[i][1] && y < PAD[i][1] + PAD[i][3]) return i;
    return -1;
}

static void mem_phase(pet_game_t *g, uint8_t p) { g->phase = p; g->phase_t = g->t_ms; }

static void memory_update(pet_game_t *g, const pet_input_t *in, bool tap)
{
    const uint32_t e = g->t_ms - g->phase_t;
    if (g->lit >= 0 && g->phase == MEM_INPUT && g->t_ms >= g->lit_until) g->lit = -1;
    switch (g->phase) {
    case MEM_READY:
        if (e >= 900) mem_phase(g, MEM_SHOW);
        break;
    case MEM_SHOW: {
        uint32_t step = e / MEM_STEP_MS;
        if (step >= g->len) { mem_phase(g, MEM_INPUT); g->pos = 0; g->lit = -1; break; }
        g->lit = (e % MEM_STEP_MS) < MEM_ON_MS ? (int8_t)g->seq[step] : -1;
        break;
    }
    case MEM_INPUT: {
        if (tap) {
            int p = pad_at(in->x, in->y);
            if (p < 0) break;
            g->lit = (int8_t)p;
            g->lit_until = g->t_ms + 220;
            g->phase_t = g->t_ms;                     /* reinicia la espera */
            if (p != g->seq[g->pos]) { mem_phase(g, MEM_FAIL); break; }
            if (++g->pos == g->len) {
                g->score++;
                if (g->score >= MEM_ROUNDS) mem_phase(g, MEM_WIN);
                else { g->len++; mem_phase(g, MEM_GOOD); }
            }
        } else if (e >= MEM_WAIT_MS) {
            mem_phase(g, MEM_FAIL);
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

static void lift_update(pet_game_t *g, uint32_t dt, bool tap)
{
    const uint32_t e = g->t_ms - g->phase_t;
    switch (g->phase) {
    case LIFT_PULL: {
        g->lift_t += dt;
        int decay = (250 + 120 * g->weight) * (int)dt / 1000;
        g->fill = g->fill > decay ? g->fill - decay : 0;
        if (tap) g->fill = g->fill + LIFT_TAP > 1000 ? 1000 : g->fill + LIFT_TAP;
        if (g->fill >= 1000) {
            g->score++;
            mem_phase(g, g->score >= LIFT_WEIGHTS ? LIFT_WIN : LIFT_GOOD);
        } else if (g->lift_t >= LIFT_TRY_MS) {
            mem_phase(g, LIFT_FAIL);
        }
        break;
    }
    case LIFT_GOOD:
        if (e >= 900) { g->weight++; g->fill = 0; g->lift_t = 0; mem_phase(g, LIFT_PULL); }
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
    default:
        out->down = !g->prev_down;
        out->x = PET_GAME_W / 2; out->y = PET_GAME_H / 2;
        return true;
    }
}

/* ---------------- dibujo ---------------- */
static void hud(pet_gfx_t *gf, const pet_game_t *g)
{
    char b[16];
    int n = 0, v = g->score;
    char tmp[8]; int k = 0;
    do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 7);
    while (k) b[n++] = tmp[--k];
    b[n] = 0;
    pet_gfx_icon(gf, ICON_STAR, 8, 8, 2);
    pet_gfx_text(gf, b, 40, 10, 4, pet_rgb565(0xf4f4f4));
    int s = pet_game_seconds_left(g);
    if (s >= 0 && g->id == GAME_CATCH) {
        int w = (int)((CATCH_MS - (g->t_ms < CATCH_MS ? g->t_ms : CATCH_MS)) * 200 / CATCH_MS);
        pet_gfx_fill(gf, 190, 16, 200, 8, pet_rgb565(0x1e293b));
        pet_gfx_fill(gf, 190, 16, w, 8, pet_rgb565(s <= 5 ? 0xef7d57 : 0x38b764));
    }
}

static void render_catch(pet_gfx_t *gf, const pet_game_t *g)
{
    pet_gfx_fill(gf, 0, PET_GAME_H - 3, PET_GAME_W, 3, pet_rgb565(0x1e293b));
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
    hud(gf, g);
}

static void render_memory(pet_gfx_t *gf, const pet_game_t *g)
{
    for (int i = 0; i < 4; i++) {
        bool on = g->lit == i;
        pet_gfx_rrect(gf, PAD[i][0], PAD[i][1], PAD[i][2], PAD[i][3], 22, pet_rgb565(on ? PAD_ON[i] : PAD_OFF[i]));
        if (on) pet_gfx_rrect(gf, PAD[i][0] + 10, PAD[i][1] + 10, PAD[i][2] - 20, PAD[i][3] - 20, 16, pet_rgb565(0xf4f4f4));
        if (on) pet_gfx_rrect(gf, PAD[i][0] + 14, PAD[i][1] + 14, PAD[i][2] - 28, PAD[i][3] - 28, 12, pet_rgb565(PAD_ON[i]));
    }
    pet_view_t v = g->pet;
    v.expr = g->phase == MEM_GOOD || g->phase == MEM_WIN ? EXPR_HAPPY : g->phase == MEM_FAIL ? EXPR_SAD : EXPR_NEUTRAL;
    if (g->phase == MEM_SHOW && g->lit >= 0) v.look_x = (g->lit & 1) ? 1 : -1;
    pet_gfx_rrect(gf, 148, 88, 104, 104, 30, 0);       /* hueco negro bajo la criatura */
    pet_gfx_creature(gf, &v, 152, 90, 2);
    /* rondas superadas: puntos bajo la criatura */
    for (int i = 0; i < MEM_ROUNDS; i++)
        pet_gfx_fill(gf, 200 - MEM_ROUNDS * 7 + i * 14 + 3, 180, 8, 8, pet_rgb565(i < g->score ? 0xffcd75 : 0x1e293b));
}

static void render_lift(pet_gfx_t *gf, const pet_game_t *g)
{
    const int pscale = 3, px = PET_GAME_W / 2 - PET_W * pscale / 2, py = PET_GAME_H - PET_W * pscale - 2;
    pet_gfx_fill(gf, 0, PET_GAME_H - 3, PET_GAME_W, 3, pet_rgb565(0x1e293b));
    int fill = g->phase == LIFT_PULL ? g->fill : (g->phase == LIFT_FAIL ? 0 : 1000);
    int bar_y = 150 - fill * 110 / 1000;
    const uint16_t arm = pet_rgb565(0x94b0c2), disc = pet_rgb565(0x566c86), disc_l = pet_rgb565(0xf4f4f4);
    pet_gfx_fill(gf, 150, bar_y + 6, 6, 190 - bar_y, arm);          /* brazos */
    pet_gfx_fill(gf, 244, bar_y + 6, 6, 190 - bar_y, arm);
    pet_view_t v = g->pet;
    v.expr = g->phase == LIFT_GOOD || g->phase == LIFT_WIN ? EXPR_HAPPY : g->phase == LIFT_FAIL ? EXPR_SAD
           : fill > 500 ? EXPR_ANGRY : EXPR_NEUTRAL;
    v.squash = g->phase == LIFT_PULL && (g->t_ms / 150) % 2;
    pet_gfx_creature(gf, &v, px, py, pscale);
    pet_gfx_fill(gf, 70, bar_y, 260, 8, arm);                       /* barra y discos */
    int dh = 26 + g->weight * 8;
    for (int k = 0; k < 2; k++) {
        int dx = k ? 300 : 76;
        pet_gfx_rrect(gf, dx, bar_y + 4 - dh / 2, 24, dh, 6, disc);
        pet_gfx_fill(gf, dx + 4, bar_y + 8 - dh / 2, 4, dh - 8, disc_l);
    }
    /* medidor */
    pet_gfx_rrect(gf, 360, 50, 30, 220, 10, pet_rgb565(0x1e293b));
    int h = fill * 208 / 1000;
    if (h > 0) pet_gfx_rrect(gf, 366, 264 - h, 18, h, 6, pet_rgb565(fill >= 1000 ? 0x38b764 : 0xffcd75));
    char b[12] = "PESA 0/5";
    b[5] = (char)('0' + (g->weight + 1 > LIFT_WEIGHTS ? LIFT_WEIGHTS : g->weight + 1));
    pet_gfx_text(gf, b, 400 - pet_gfx_text_width(b, 3) - 10, 12, 3, pet_rgb565(0x94b0c2));
    int s = pet_game_seconds_left(g);
    if (s >= 0) {                                                   /* segundos para esta pesa */
        char t[4] = { (char)('0' + (s > 9 ? 9 : s)), 'S', 0 };
        pet_gfx_text(gf, t, 200 - pet_gfx_text_width(t, 4) / 2, 10, 4, pet_rgb565(s <= 2 ? 0xef7d57 : 0xf4f4f4));
    }
    char sc[4] = { (char)('0' + g->score), 0 };
    pet_gfx_icon(gf, ICON_STAR, 8, 8, 2);
    pet_gfx_text(gf, sc, 40, 10, 4, pet_rgb565(0xf4f4f4));
}

void pet_game_render(const pet_game_t *g, uint16_t *dst)
{
    pet_gfx_t gf = { dst, PET_GAME_W, PET_GAME_H };
    memset(dst, 0, (size_t)PET_GAME_W * PET_GAME_H * 2);
    switch (g->id) {
    case GAME_CATCH:  render_catch(&gf, g); break;
    case GAME_MEMORY: render_memory(&gf, g); break;
    default:          render_lift(&gf, g); break;
    }
}
