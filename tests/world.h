/* Mundo simulado para los tests de ciclo de vida: un entorno WiFi con casa,
 * trabajo y calle, y bots de jugador que cuidan y juegan. Determinista. */
#pragma once
#include "pet_core.h"
#include <string.h>

#define DAY 86400LL
#define T0  (20000LL * DAY)       /* medianoche de un dia cualquiera */

typedef struct {
    uint64_t rng;
    uint32_t fresh;               /* contador de APs nunca vistos */
    bool     stay_home;           /* sin salidas: solo AP de casa */
    int      outing_pct;          /* si > 0: % de dias con salidas (sobrescribe stay_home por dia) */
} world_t;

typedef struct {
    int visits;                   /* 1..3: a las 20:00; 13:00 y 20:00; 08:30, 13:00 y 20:00 */
    int every_days;               /* visita cada N dias (1 = a diario) */
    int games;                    /* partidas por visita */
    int perf;                     /* rendimiento medio 0..100 */
    int fav_game;                 /* -1: rota; si no, juega sobre todo a este (80 %) */
    bool wishes;                  /* atiende los deseos */
} profile_t;

static const profile_t ACTIVE  = { 3, 1, 2, 60, -1, true };
static const profile_t CASUAL  = { 1, 1, 1, 50, -1, true };
static const profile_t NEGLECT = { 1, 3, 1, 40, -1, false };
static const profile_t COUCH   = { 3, 1, 0, 0, -1, true };   /* cuida pero no juega nunca */

static uint32_t wr(world_t *w)
{
    uint64_t x = w->rng;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    w->rng = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

static void mk_ap(pet_ap_t *a, uint32_t id, bool open)
{
    memset(a, 0, sizeof(*a));
    a->bssid[0] = 0x02; a->bssid[1] = (uint8_t)(id >> 24); a->bssid[2] = (uint8_t)(id >> 16);
    a->bssid[3] = (uint8_t)(id >> 8); a->bssid[4] = (uint8_t)id; a->bssid[5] = 0x5A;
    a->rssi = -60; a->open = open;
}

/* Escaneo segun la hora local. Devuelve n de APs. */
static size_t scan_at(world_t *w, int64_t t, pet_ap_t *out)
{
    int h = (int)((t % DAY) / 3600), n = 0;
    bool commute = !w->stay_home && (h == 9 || h == 18);
    bool work = !w->stay_home && h >= 10 && h < 18;
    if (commute) {
        for (int i = 0; i < 14; i++) mk_ap(&out[n++], 0x40000000u + w->fresh++, i < 4);
    } else if (work) {
        for (int i = 0; i < 10; i++) if (wr(w) % 10 < 8) mk_ap(&out[n++], 0x20000000u + (uint32_t)i, false);
    } else {
        for (int i = 0; i < 8; i++) if (wr(w) % 10 < 9) mk_ap(&out[n++], 0x10000000u + (uint32_t)i, false);
    }
    return (size_t)n;
}

static int pick_game(world_t *w, const profile_t *p, const pet_state_t *s)
{
    if (p->fav_game >= 0 && wr(w) % 100 < 80) return p->fav_game;
    int g;
    do g = (int)(wr(w) % GAME_COUNT); while (s->level < pet_game_level((pet_game_id_t)g));
    return g;
}

static void play(pet_state_t *s, world_t *w, const profile_t *p, int g, int64_t t)
{
    if (pet_can_play(s, (pet_game_id_t)g, t) != PLAY_OK) return;
    int perf = p->perf + (int)(wr(w) % 31) - 15;
    pet_game_finish(s, (pet_game_id_t)g, perf, perf, t + 40);
}

/* Una visita de unos minutos: cuidar, jugar y atender el deseo. */
static void visit_run(pet_state_t *s, world_t *w, const profile_t *p, int64_t t)
{
    pet_step(s, t);
    if (s->stage == PET_EGG) {                     /* el huevo eclosiona con la pantalla encendida */
        for (int m = 1; m <= 3; m++) pet_step(s, t + m * 60);
        t += 180;
    }
    for (int i = 0; i < 3; i++) pet_touch(s, t, (uint32_t)(t * 1000 + i * 700));
    t += 20;
    if (s->sick) pet_feed(s, ITEM_MEDICINE, t);
    if (s->poops > 0 || s->hygiene < 60 * PET_ONE) pet_wash(s, t);
    for (int k = 0; k < 4 && s->satiety < 70 * PET_ONE; k++) {
        if (s->inv[ITEM_TREAT] && pet_feed(s, ITEM_TREAT, t) == FEED_OK) continue;
        pet_item_t it = s->coins >= 30 ? ITEM_STEW : ITEM_BREAD;
        if (pet_feed(s, it, t) != FEED_OK) break;
    }
    t += 30;
    if (p->wishes) {
        switch (s->wish_kind) {
        case WISH_FOOD:  pet_feed(s, (pet_item_t)s->wish_arg, t); break;
        case WISH_GAME:  if (p->games) play(s, w, p, s->wish_arg, t); t += 60; break;
        case WISH_PET:   for (int i = 0; i < 3; i++) pet_touch(s, t + 5 + i * 3, (uint32_t)((t + 5 + i * 3) * 1000)); break;
        case WISH_CLEAN: pet_wash(s, t); break;
        default: break;
        }
    }
    for (int k = 0; k < p->games; k++) { play(s, w, p, pick_game(w, p, s), t); t += 60; }
    if (s->energy < 25 * PET_ONE) pet_nap(s, t);
    pet_step(s, t + 60);
}

typedef struct {
    int day_juv, day_adult;       /* dia desde la instalacion (-1 si no llego) */
    int branch, variant;
    int min_coins_after_day3;     /* para la economia */
    int coins_day3;
    int mood_adult;               /* animo medio de juvenil al hacerse adulto */
    int coins_end, spent_food;
} outcome_t;

/* Simula `days` dias. Escanea cada 15 min de 08 a 23 (modo muneca). */
static outcome_t run_days(pet_state_t *s, world_t *w, const profile_t *p, int days)
{
    outcome_t o = { -1, -1, 0, 0, 1 << 30, 0, -1, 0, 0 };
    static const int vh[3] = { 8 * 3600 + 1800, 13 * 3600, 20 * 3600 };
    static const int order[3][3] = { {2, -1, -1}, {1, 2, -1}, {0, 1, 2} };
    for (int d = 0; d < days; d++) {
        int64_t base = T0 + (int64_t)d * DAY;
        if (w->outing_pct > 0) w->stay_home = (int)(wr(w) % 100) >= w->outing_pct;
        bool today = d % p->every_days == 0;
        int vi = 0;
        const int *vs = order[p->visits - 1];
        for (int64_t off = 8 * 3600; off < 23 * 3600; off += 900) {
            int64_t t = base + off;
            while (today && vi < 3 && vs[vi] >= 0 && base + vh[vs[vi]] <= t) {
                visit_run(s, w, p, base + vh[vs[vi]]);
                vi++;
            }
            /* la app solo escanea si se abrio alguna vez; con la pantalla apagada sigue activa */
            pet_ap_t aps[64];
            size_t n = scan_at(w, t, aps);
            pet_step(s, t);
            pet_ingest_scan(s, aps, n, t);
        }
        while (today && vi < 3 && vs[vi] >= 0) { visit_run(s, w, p, base + vh[vs[vi]]); vi++; }
        pet_step(s, base + 23 * 3600);
        if (o.day_juv < 0 && s->stage >= PET_JUVENILE) o.day_juv = d + 1;
        if (o.day_adult < 0 && s->stage == PET_ADULT) { o.day_adult = d + 1; o.mood_adult = s->mood_n ? (int)(s->mood_sum / s->mood_n) : -1; }
        if (d == 2) o.coins_day3 = (int)s->coins;
        if (d >= 2 && (int)s->coins < o.min_coins_after_day3) o.min_coins_after_day3 = (int)s->coins;
    }
    o.branch = s->branch; o.variant = s->variant;
    o.coins_end = (int)s->coins;
    return o;
}

static void make_key(uint8_t key[PET_KEY_LEN], uint32_t seed)
{
    uint64_t x = 0x1234567ULL * (seed + 1);
    for (int i = 0; i < PET_KEY_LEN; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; key[i] = (uint8_t)(x >> 24); }
}
