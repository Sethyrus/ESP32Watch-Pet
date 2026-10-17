/* Mundo simulado para los tests de ciclo de vida: un entorno WiFi con casa,
 * trabajo y calle, y perfiles de jugador. Determinista (semilla propia). */
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
    int      visits;              /* hasta 3: a las 08:30, 13:00 y 20:00 */
    int      visit_min;           /* minutos activos por visita */
    int      bites;               /* bocados maximos por visita */
    int      exotic_pct;          /* % de elecciones exoticas cuando hay */
} profile_t;

static const profile_t ACTIVE  = { 3, 6, 6, 70 };
static const profile_t CASUAL  = { 1, 6, 3, 50 };

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

static void visit_run(pet_state_t *s, world_t *w, const profile_t *p, int64_t t)
{
    pet_touch(s, t, (uint32_t)(t * 1000));
    int fed = 0;
    for (int m = 0; m < p->visit_min; m++) {
        pet_step(s, t + m * 60);
        while (fed < p->bites && s->satiety < 85 * PET_ONE && s->pantry_n > 0) {
            bool want_exo = (int)(wr(w) % 100) < p->exotic_pct;
            pet_food_t order[3] = { want_exo ? FOOD_EXOTIC : FOOD_DOMESTIC,
                                    want_exo ? FOOD_URBAN : FOOD_URBAN,
                                    want_exo ? FOOD_DOMESTIC : FOOD_EXOTIC };
            if (p->exotic_pct == 0) { order[1] = FOOD_DOMESTIC; order[2] = FOOD_DOMESTIC; }
            bool ok = false;
            for (int k = 0; k < 3 && !ok; k++) ok = pet_feed(s, order[k], t + m * 60);
            if (!ok) break;
            fed++;
        }
    }
}

typedef struct {
    int day_juv, day_adult;       /* dia desde la instalacion (-1 si no llego) */
    int branch, variant;
    int care_errors, juv_errors, gp, exo_eaten, dom_eaten;
} outcome_t;

/* Simula `days` dias. Escanea cada 15 min de 08 a 23 (modo muneca). */
static outcome_t run_days(pet_state_t *s, world_t *w, const profile_t *p, int days)
{
    outcome_t o = { -1, -1, 0, 0, 0, 0, 0, 0, 0 };
    static const int vh[3] = { 8 * 3600 + 1800, 13 * 3600, 20 * 3600 };
    static const int order[3][3] = { {2, -1, -1}, {0, 2, -1}, {0, 1, 2} };
    for (int d = 0; d < days; d++) {
        int64_t base = T0 + (int64_t)d * DAY;
        if (w->outing_pct > 0) w->stay_home = (int)(wr(w) % 100) >= w->outing_pct;
        int vi = 0;
        const int *vs = order[p->visits - 1];
        for (int64_t off = 8 * 3600; off < 23 * 3600; off += 900) {
            int64_t t = base + off;
            while (vi < 3 && vs[vi] >= 0 && base + vh[vs[vi]] <= t) {
                visit_run(s, w, p, base + vh[vs[vi]]);
                vi++;
            }
            pet_ap_t aps[64];
            size_t n = scan_at(w, t, aps);
            pet_ingest_scan(s, aps, n, t);
        }
        while (vi < 3 && vs[vi] >= 0) { visit_run(s, w, p, base + vh[vs[vi]]); vi++; }
        pet_step(s, base + 23 * 3600);
        if (o.day_juv < 0 && s->stage >= PET_JUVENILE) o.day_juv = d + 1;
        if (o.day_adult < 0 && s->stage == PET_ADULT) o.day_adult = d + 1;
    }
    o.branch = s->branch; o.variant = s->variant;
    o.exo_eaten = s->exo_eaten; o.dom_eaten = s->dom_eaten;
    o.care_errors = s->care_errors; o.juv_errors = s->juv_errors; o.gp = s->gp;
    return o;
}

static void make_key(uint8_t key[PET_KEY_LEN], uint32_t seed)
{
    uint64_t x = 0x1234567ULL * (seed + 1);
    for (int i = 0; i < PET_KEY_LEN; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; key[i] = (uint8_t)(x >> 24); }
}
