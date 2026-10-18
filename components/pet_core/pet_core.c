/* pet_core: reglas del juego. Valores *iniciales* de PET_DESIGN.md calibrados en
 * los tests de tests/ (R4). Todo en enteros; el azar sale de s->rng. */
#include "pet_core.h"
#include <string.h>

/* ---------------- parametros (PET_DESIGN.md, secciones 4-8) ---------------- */
#define SAT_DECAY_H        8        /* puntos/h despierta */
#define SAT_NIGHT_PCT      25
#define ENERGY_DECAY_H     5
#define ENERGY_REST_H      12
#define MOOD_DECAY_H       2
#define HUNGRY_PTS         25
#define TIRED_PTS          20
#define SAD_PTS            40
#define HAPPY_PTS          70
#define NEED_LOW_PTS       5
#define ERROR_WINDOW_S     900
#define PECK_BELOW_PTS     30
#define PECK_IDLE_S        300
#define PECK_COOLDOWN_S    300
#define PECK_PCT           60
#define ABSENCE_PECK_PCT   50
#define ABSENCE_PECK_BELOW 50
#define ABSENCE_SAT_PCT    35
#define ABSENCE_SAT_FLOOR  15
#define ABSENCE_MOOD_DECAY 1
#define ABSENCE_MOOD_FLOOR 35
#define ABSENCE_CAP_S      (7 * 86400)
#define ABSENCE_ERR_BLOCK  (12 * 3600)
#define ABSENCE_ERR_MAX    2
#define STEP_MAX_GAP_S     300
#define EGG_HATCH_S        PET_EGG_HATCH_S
#define JUV_AGE_S          (36 * 3600)
#define JUV_GP             40
#define JUV_FORCE_S        (7 * 86400)
#define ADULT_AGE_S        (6 * 86400)
#define ADULT_GP           200
#define ADULT_FORCE_S      (14 * 86400)
#define GP_FOOD_DAY_CAP    50
#define GP_VISIT           10
#define EXPLORER_PERMILLE  300
#define NOVELTY_DAY_CAP    12
#define FAM_EPOCH_S        1200
#define FAM_SEEN_4         4
#define FAM_PROMOTE_4      20
#define FAM_CAP_4          48
#define URBAN_NETS         15
#define URBAN_OPEN         3
#define URBAN_COOLDOWN_S   1800
#define BORED_S            (2 * 3600)
#define NOVEL_VIEW_S       120
#define TOUCH_BUDGET_H     10
#define SCAN_DESK_S        60
#define SCAN_WRIST_S       900
#define SCAN_MIN_BATTERY   15

static const int32_t TTL_S[FOOD_COUNT] = { 24 * 3600, 48 * 3600, 36 * 3600 };
/* saciedad, animo, PC por tipo */
static const int8_t FOOD_FX[FOOD_COUNT][3] = { {12, 1, 1}, {8, 8, 4}, {10, 4, 3} };

/* (v/100)^0.7 en Q10, v = 0..100 */
static const uint16_t DAMP[101] = {
    0, 41, 66, 88, 108, 126, 143, 159, 175, 190, 204, 218, 232, 246, 259, 271, 284, 296, 308, 320,
    332, 343, 355, 366, 377, 388, 399, 410, 420, 431, 441, 451, 461, 471, 481, 491, 501, 511, 520,
    530, 539, 549, 558, 567, 576, 586, 595, 604, 613, 621, 630, 639, 648, 657, 665, 674, 682, 691,
    699, 708, 716, 724, 733, 741, 749, 757, 766, 774, 782, 790, 798, 806, 814, 822, 829, 837, 845,
    853, 861, 868, 876, 884, 891, 899, 906, 914, 921, 929, 936, 944, 951, 959, 966, 973, 981, 988,
    995, 1002, 1010, 1017, 1024 };

/* ---------------- azar ---------------- */
static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint32_t rnd(pet_state_t *s)
{
    uint64_t x = s->rng;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    s->rng = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

static uint32_t rnd_below(pet_state_t *s, uint32_t n) { return n ? rnd(s) % n : 0; }

/* ---------------- tiempo ---------------- */
static int hour_of(int64_t t) { return (int)((((t % 86400) + 86400) % 86400) / 3600); }
static uint32_t day_of(int64_t t) { return (uint32_t)(t / 86400); }

bool pet_is_night(const pet_state_t *s, int64_t now_s)
{
    int h = hour_of(now_s), a = s->night_start_h, b = s->night_end_h;
    if (a == b) return false;
    return a > b ? (h >= a || h < b) : (h >= a && h < b);
}

/* ---------------- estadisticas ---------------- */
static int32_t clampi(int64_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : (int32_t)v; }

/* Ganancia amortiguada: x ((100-v)/100)^0.7 y hasta x3 por debajo de 25. */
static void gain(int32_t *stat, int32_t amount)
{
    int v = *stat / PET_ONE;
    int64_t d = (int64_t)amount * DAMP[100 - v] / 1024;
    if (v < 25) d = d * (256 + 512 * (25 - v) / 25) / 256;
    *stat = clampi(*stat + d, 0, PET_MAX);
}

/* Perdida de un evento: x (v/100)^0.7. */
static void lose_damped(int32_t *stat, int32_t amount)
{
    int v = *stat / PET_ONE;
    *stat = clampi(*stat - (int64_t)amount * DAMP[v] / 1024, 0, PET_MAX);
}

/* Perdida pasiva lineal con suelo (el suelo solo frena, nunca sube el valor). */
static void lose_floor(int32_t *stat, int64_t amount, int32_t floor_pts)
{
    int32_t fl = floor_pts * PET_ONE;
    if (*stat <= fl) return;
    *stat = clampi(*stat - amount, fl, PET_MAX);
}

static int64_t rate(int pts_per_h, int64_t secs) { return (int64_t)pts_per_h * PET_ONE * secs / 3600; }

/* ---------------- despensa ---------------- */
static void pantry_remove(pet_state_t *s, int i)
{
    memmove(&s->pantry[i], &s->pantry[i + 1], (size_t)(s->pantry_n - i - 1) * sizeof(pet_bite_t));
    s->pantry_n--;
}

static void pantry_expire(pet_state_t *s, int64_t now)
{
    for (int i = 0; i < s->pantry_n;) {
        if (now - s->pantry[i].born_s >= TTL_S[s->pantry[i].type]) pantry_remove(s, i);
        else i++;
    }
}

static void pantry_add(pet_state_t *s, pet_food_t type, int64_t now)
{
    if (s->pantry_n >= PET_PANTRY_MAX) {
        int victim = -1;
        for (int i = 0; i < s->pantry_n; i++)
            if (s->pantry[i].type == FOOD_DOMESTIC) { victim = i; break; }
        if (victim < 0) {
            if (type == FOOD_DOMESTIC) return;
            victim = 0;
        }
        pantry_remove(s, victim);
    }
    s->pantry[s->pantry_n].type = (uint8_t)type;
    s->pantry[s->pantry_n].born_s = now;
    s->pantry_n++;
}

int pet_pantry_count(const pet_state_t *s, pet_food_t type)
{
    int n = 0;
    for (int i = 0; i < s->pantry_n; i++) n += s->pantry[i].type == type;
    return n;
}

int64_t pet_pantry_expires_in(const pet_state_t *s, pet_food_t type, int64_t now_s)
{
    int64_t best = -1;
    for (int i = 0; i < s->pantry_n; i++) {
        if (s->pantry[i].type != type) continue;
        int64_t left = s->pantry[i].born_s + TTL_S[type] - now_s;
        if (left < 0) left = 0;
        if (best < 0 || left < best) best = left;
    }
    return best;
}

/* ---------------- evolucion ---------------- */
int pet_diet_axis_permille(const pet_state_t *s)
{
    uint32_t tot = (uint32_t)s->exo_eaten + s->dom_eaten;
    return tot ? (int)((uint32_t)s->exo_eaten * 1000 / tot) : 0;
}

int64_t pet_age_s(const pet_state_t *s, int64_t now_s)
{
    return s->stage >= PET_BABY && now_s > s->born_s ? now_s - s->born_s : 0;   /* desde la instalacion */
}

static bool evolve_once(pet_state_t *s, int64_t now)
{
    int64_t age = pet_age_s(s, now);
    switch (s->stage) {
    case PET_EGG:
        if (s->egg_active_s < EGG_HATCH_S) return false;
        s->stage = PET_BABY;
        s->hatch_s = now;
        s->events |= EVT_HATCHED;
        break;
    case PET_BABY:
        if (!((age >= JUV_AGE_S && s->gp >= JUV_GP) || age >= JUV_FORCE_S)) return false;
        s->stage = PET_JUVENILE;
        s->mood_sum = s->mood_n = 0;
        s->events |= EVT_EVOLVED;
        break;
    case PET_JUVENILE: {
        if (!((age >= ADULT_AGE_S && s->gp >= ADULT_GP) || age >= ADULT_FORCE_S)) return false;
        s->juv_errors = s->care_errors;
        s->branch = pet_diet_axis_permille(s) >= EXPLORER_PERMILLE ? BRANCH_EXPLORER : BRANCH_HOMEBODY;
        uint32_t mean = s->mood_n ? s->mood_sum / s->mood_n : 50;
        s->variant = s->juv_errors >= 4 ? VAR_CLUMSY
                   : mean < 40 ? VAR_ODD
                   : (s->juv_errors <= 2 && mean >= 60) ? VAR_POLISHED : VAR_NORMAL;
        s->stage = PET_ADULT;
        s->events |= EVT_EVOLVED;
        break;
    }
    default: return false;
    }
    s->care_errors = 0;
    s->need_low_s = 0;
    return true;
}

static void evolve_check(pet_state_t *s, int64_t now)
{
    for (int i = 0; i < 3 && evolve_once(s, now); i++) {}
}

/* ---------------- comer ---------------- */
static void visit(pet_state_t *s, int64_t now)
{
    uint32_t d = day_of(now);
    if (s->visit_day == d) return;
    s->visit_day = d;
    s->gp = (uint16_t)(s->gp + GP_VISIT > 65535 ? 65535 : s->gp + GP_VISIT);
}

static void eat(pet_state_t *s, int idx, int pct, bool manual, int64_t now)
{
    pet_food_t t = (pet_food_t)s->pantry[idx].type;
    pantry_remove(s, idx);
    gain(&s->satiety, FOOD_FX[t][0] * PET_ONE * pct / 100);
    gain(&s->mood, FOOD_FX[t][1] * PET_ONE * pct / 100);
    uint32_t d = day_of(now);
    if (s->gp_day != d) { s->gp_day = d; s->gp_food_today = 0; }
    int pc = (FOOD_FX[t][2] * pct + 50) / 100;
    if (pc > GP_FOOD_DAY_CAP - s->gp_food_today) pc = GP_FOOD_DAY_CAP - s->gp_food_today;
    if (pc < 0) pc = 0;
    s->gp_food_today = (uint16_t)(s->gp_food_today + pc);
    s->gp = (uint16_t)(s->gp + pc > 65535 ? 65535 : s->gp + pc);
    if (s->stage < PET_ADULT) {
        if (t == FOOD_DOMESTIC) { if (s->dom_eaten < 65535) s->dom_eaten++; }
        else if (s->exo_eaten < 65535) s->exo_eaten++;
    }
    if (s->total_eaten < 65535) s->total_eaten++;
    if (manual && s->manual_eaten < 65535) s->manual_eaten++;
}

bool pet_feed(pet_state_t *s, pet_food_t type, int64_t now_s)
{
    if (s->stage == PET_EGG || type >= FOOD_COUNT) return false;
    pet_step(s, now_s);
    if (pet_is_night(s, now_s)) return false;
    int idx = -1;
    for (int i = 0; i < s->pantry_n; i++)
        if (s->pantry[i].type == type) { idx = i; break; }
    if (idx < 0) return false;
    eat(s, idx, 100, true, now_s);
    s->last_action_s = now_s;
    s->last_novel_s = now_s;       /* comer tambien entretiene: reinicia el aburrimiento */
    s->need_low_s = 0;
    visit(s, now_s);
    evolve_check(s, now_s);
    return true;
}

/* ---------------- escaneo ---------------- */
static uint32_t ap_hash(const pet_state_t *s, const uint8_t bssid[6])
{
    uint64_t h = pet_siphash24(s->key, bssid, 6);
    return (uint32_t)(h ^ (h >> 32));
}

static int fam_find(const pet_fam_t *t, int n, uint32_t h)
{
    for (int i = 0; i < n; i++) if (t[i].hash == h) return i;
    return -1;
}

static void fam_remove(pet_fam_t *t, uint8_t *n, int i)
{
    memmove(&t[i], &t[i + 1], (size_t)(*n - i - 1) * sizeof(pet_fam_t));
    (*n)--;
}

static int fam_lowest(const pet_fam_t *t, int n)
{
    int lo = 0;
    for (int i = 1; i < n; i++) if (t[i].score4 < t[lo].score4) lo = i;
    return lo;
}

static void fam_insert(pet_fam_t *t, uint8_t *n, uint32_t h, uint8_t score4)
{
    if (*n >= PET_FAM_MAX) fam_remove(t, n, fam_lowest(t, *n));
    t[*n].hash = h;
    t[*n].score4 = score4;
    (*n)++;
}

int pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s)
{
    if (pet_is_night(s, now_s) || s->stage == PET_EGG) return 0;
    if (n > 64) n = 64;
    pantry_expire(s, now_s);
    uint32_t d = day_of(now_s);
    if (s->exo_day != d) { s->exo_day = d; s->exo_today = 0; }

    bool epoch = now_s - s->last_fam_epoch_s >= FAM_EPOCH_S;
    uint8_t seen_f[PET_FAM_MAX] = {0}, seen_r[PET_FAM_MAX] = {0};
    uint32_t newh[64];
    int new_n = 0, fam_vis = 0, open_n = 0;
    uint32_t seen_h[64];
    int seen_n = 0;

    for (size_t i = 0; i < n; i++) {
        uint32_t h = ap_hash(s, aps[i].bssid);
        bool dup = false;
        for (int k = 0; k < seen_n; k++) if (seen_h[k] == h) { dup = true; break; }
        if (dup) continue;
        seen_h[seen_n++] = h;
        if (aps[i].open) open_n++;
        int f = fam_find(s->familiar, s->familiar_n, h);
        if (f >= 0) { seen_f[f] = 1; fam_vis++; continue; }
        int r = fam_find(s->recent, s->recent_n, h);
        if (r >= 0) { seen_r[r] = 1; continue; }
        newh[new_n++] = h;
    }

    if (epoch) {
        s->last_fam_epoch_s = now_s;
        for (int i = s->familiar_n - 1; i >= 0; i--) {
            if (seen_f[i]) s->familiar[i].score4 = (uint8_t)(s->familiar[i].score4 + FAM_SEEN_4 > FAM_CAP_4 ? FAM_CAP_4 : s->familiar[i].score4 + FAM_SEEN_4);
            else if (--s->familiar[i].score4 == 0) fam_remove(s->familiar, &s->familiar_n, i);
        }
        uint32_t promote[PET_FAM_MAX];
        int pn = 0;
        for (int i = s->recent_n - 1; i >= 0; i--) {
            if (seen_r[i]) {
                s->recent[i].score4 = (uint8_t)(s->recent[i].score4 + FAM_SEEN_4);
                if (s->recent[i].score4 >= FAM_PROMOTE_4) {
                    promote[pn++] = s->recent[i].hash;
                    fam_remove(s->recent, &s->recent_n, i);
                }
            } else if (--s->recent[i].score4 == 0) fam_remove(s->recent, &s->recent_n, i);
        }
        for (int i = 0; i < pn; i++) fam_insert(s->familiar, &s->familiar_n, promote[i], FAM_PROMOTE_4);
    }
    for (int i = 0; i < new_n; i++) fam_insert(s->recent, &s->recent_n, newh[i], FAM_SEEN_4);

    int added = 0;
    int dom = epoch ? (fam_vis < 2 ? fam_vis : 2) : 0;
    int exo = new_n < 3 ? new_n : 3;
    if (exo > NOVELTY_DAY_CAP - s->exo_today) exo = NOVELTY_DAY_CAP - s->exo_today;
    if (exo < 0) exo = 0;
    for (int i = 0; i < dom; i++) { pantry_add(s, FOOD_DOMESTIC, now_s); added++; }
    for (int i = 0; i < exo; i++) { pantry_add(s, FOOD_EXOTIC, now_s); added++; }
    s->exo_today = (uint8_t)(s->exo_today + exo);
    if ((seen_n >= URBAN_NETS || open_n >= URBAN_OPEN) && now_s - s->last_urban_s >= URBAN_COOLDOWN_S) {
        pantry_add(s, FOOD_URBAN, now_s);
        s->last_urban_s = now_s;
        added++;
    }
    if (new_n > 0) {
        s->last_novel_s = now_s;
        s->novel_until_s = now_s + NOVEL_VIEW_S;
        s->events |= EVT_NEW_SIGNAL;
    }
    s->last_scan_s = now_s;
    s->total_scans++;
    if (added > 0) visit(s, now_s);
    return added;
}

/* ---------------- paso activo ---------------- */
static void active_minute(pet_state_t *s, int64_t now)
{
    bool awake = !pet_is_night(s, now);
    int64_t m = 60;

    if (awake) {
        int rs = SAT_DECAY_H;
        lose_floor(&s->satiety, rate(rs, m), 0);
        lose_floor(&s->energy, rate(ENERGY_DECAY_H, m), 0);
        int md = MOOD_DECAY_H * (s->satiety < HUNGRY_PTS * PET_ONE ? 2 : 1);
        lose_floor(&s->mood, rate(md, m), 0);
    } else {
        lose_floor(&s->satiety, rate(SAT_DECAY_H * SAT_NIGHT_PCT / 100, m), 0);
        s->energy = clampi((int64_t)s->energy + rate(ENERGY_REST_H, m), 0, PET_MAX);
    }

    if (s->stage == PET_EGG) {
        if (s->egg_active_s < 65535 - 60) s->egg_active_s += 60;
    } else {
        if (awake) { s->mood_sum += (uint32_t)(s->mood / PET_ONE); s->mood_n++; }
        bool low = awake && (s->satiety <= NEED_LOW_PTS * PET_ONE || s->mood <= NEED_LOW_PTS * PET_ONE);
        if (low && now - s->last_action_s > 60) {
            s->need_low_s = (uint16_t)(s->need_low_s + 60);
            if (s->need_low_s >= ERROR_WINDOW_S) {
                if (s->care_errors < 255) s->care_errors++;
                s->need_low_s = 0;
                s->events |= EVT_ERROR;
            }
        } else {
            s->need_low_s = 0;
        }
        if (awake && s->satiety < PECK_BELOW_PTS * PET_ONE && s->pantry_n > 0 &&
            now - s->last_action_s >= PECK_IDLE_S && now - s->last_peck_s >= PECK_COOLDOWN_S) {
            eat(s, 0, PECK_PCT, false, now);
            s->last_peck_s = now;
            s->events |= EVT_PECKED;
        }
    }
    evolve_check(s, now);
}

void pet_step(pet_state_t *s, int64_t now_s)
{
    if (!s->started) { s->last_s = now_s; s->started = true; return; }
    int64_t dt = now_s - s->last_s;
    if (dt < 0) { s->last_s = now_s; return; }
    if (dt > STEP_MAX_GAP_S) { pet_catch_up(s, now_s); return; }
    pantry_expire(s, now_s);
    int64_t total = s->step_rem_s + dt;
    int64_t mins = total / 60;
    s->step_rem_s = total % 60;
    s->last_s = now_s;
    for (int64_t i = 0; i < mins; i++) active_minute(s, now_s);
}

void pet_clock_lost(pet_state_t *s, int64_t now_s)
{
    s->last_s = now_s;
    s->started = true;
    s->step_rem_s = 0;
}

void pet_catch_up(pet_state_t *s, int64_t now_s)
{
    if (!s->started) { pet_clock_lost(s, now_s); return; }
    int64_t dt = now_s - s->last_s;
    if (dt <= 0) { if (dt < 0) s->last_s = now_s; return; }
    int64_t t = dt > ABSENCE_CAP_S ? now_s - ABSENCE_CAP_S : s->last_s;
    int64_t hungry_run = 0;
    int blocks = 0;

    while (t < now_s) {
        int64_t chunk = now_s - t < 3600 ? now_s - t : 3600;
        int64_t mid = t + chunk / 2;
        bool awake = !pet_is_night(s, mid);
        pantry_expire(s, t);
        if (awake && s->stage != PET_EGG && s->pantry_n > 0 && s->satiety < ABSENCE_PECK_BELOW * PET_ONE && chunk >= 1800)
            eat(s, 0, ABSENCE_PECK_PCT, false, t);
        if (awake) {
            lose_floor(&s->satiety, rate(SAT_DECAY_H, chunk) * ABSENCE_SAT_PCT / 100, ABSENCE_SAT_FLOOR);
            lose_floor(&s->energy, rate(ENERGY_DECAY_H, chunk), 0);
            lose_floor(&s->mood, rate(ABSENCE_MOOD_DECAY, chunk), ABSENCE_MOOD_FLOOR);
            if (s->stage >= PET_BABY) { s->mood_sum += (uint32_t)(s->mood / PET_ONE) * (uint32_t)(chunk / 60); s->mood_n += (uint32_t)(chunk / 60); }
            if (s->satiety < HUNGRY_PTS * PET_ONE) {
                hungry_run += chunk;
                if (hungry_run >= ABSENCE_ERR_BLOCK) { blocks++; hungry_run = 0; }
            }
        } else {
            lose_floor(&s->satiety, rate(SAT_DECAY_H * SAT_NIGHT_PCT / 100, chunk) * ABSENCE_SAT_PCT / 100, ABSENCE_SAT_FLOOR);
            s->energy = clampi((int64_t)s->energy + rate(ENERGY_REST_H, chunk), 0, PET_MAX);
        }
        t += chunk;
    }
    if (s->stage >= PET_BABY) {
        int add = blocks < ABSENCE_ERR_MAX ? blocks : ABSENCE_ERR_MAX;
        s->care_errors = (uint8_t)(s->care_errors + add > 255 ? 255 : s->care_errors + add);
    }
    s->need_low_s = 0;
    s->step_rem_s = 0;
    s->last_s = now_s;
    evolve_check(s, now_s);
}

/* ---------------- interaccion ---------------- */
static bool react_active(const pet_state_t *s, uint32_t mono_ms, uint8_t kind)
{
    return s->react_kind == kind && (int32_t)(s->react_until_ms - mono_ms) > 0;
}

static void react(pet_state_t *s, uint8_t kind, uint32_t mono_ms, uint32_t dur_ms)
{
    s->react_kind = kind;
    s->react_until_ms = mono_ms + dur_ms;
}

static void ring_push(uint32_t *ring, uint8_t *cnt, uint32_t v)
{
    memmove(ring, ring + 1, 3 * sizeof(uint32_t));
    ring[3] = v;
    if (*cnt < 4) (*cnt)++;
}

pet_reaction_t pet_touch(pet_state_t *s, int64_t now_s, uint32_t mono_ms)
{
    pet_step(s, now_s);
    s->last_action_s = now_s;
    visit(s, now_s);
    ring_push(s->touch_ms, &s->touch_cnt, mono_ms);
    if (react_active(s, mono_ms, REACT_DIZZY)) return REACT_DIZZY;
    if (s->touch_cnt == 4 && (int32_t)(mono_ms - s->touch_ms[0]) <= 1500) {
        react(s, REACT_DIZZY, mono_ms, 3000);
        return REACT_DIZZY;
    }
    if (s->stage != PET_EGG && !pet_is_night(s, now_s)) {
        uint32_t hr = (uint32_t)(now_s / 3600);
        if (s->touch_hour != hr) { s->touch_hour = hr; s->touch_gain = 0; }
        if (s->touch_gain < TOUCH_BUDGET_H * PET_ONE) {
            int32_t before = s->mood;
            gain(&s->mood, 2 * PET_ONE);
            s->touch_gain += (uint32_t)(s->mood - before);
        }
    }
    if (s->stage != PET_EGG && !pet_is_night(s, now_s)) s->last_novel_s = now_s;   /* una caricia entretiene */
    react(s, REACT_PET, mono_ms, 2500);
    evolve_check(s, now_s);
    return REACT_PET;
}

pet_reaction_t pet_shake(pet_state_t *s, int64_t now_s, uint32_t mono_ms)
{
    (void)now_s;
    ring_push(s->shake_ms, &s->shake_cnt, mono_ms);
    if (s->shake_cnt == 4 && (int32_t)(mono_ms - s->shake_ms[0]) <= 60000 && s->stage != PET_EGG)
        lose_damped(&s->mood, 1 * PET_ONE);
    react(s, REACT_SHAKE, mono_ms, 2000);
    return REACT_SHAKE;
}

pet_reaction_t pet_usb_changed(pet_state_t *s, bool plugged, uint32_t mono_ms)
{
    pet_reaction_t r = plugged ? REACT_USB_IN : REACT_USB_OUT;
    react(s, (uint8_t)r, mono_ms, 3000);
    return r;
}

uint32_t pet_take_events(pet_state_t *s)
{
    uint32_t e = s->events;
    s->events = 0;
    return e;
}

/* ---------------- animo y decisiones ---------------- */
pet_mood_t pet_mood(const pet_state_t *s, int64_t now_s)
{
    bool awake = !pet_is_night(s, now_s);
    if (!awake) return MOOD_ASLEEP;
    if (s->satiety < HUNGRY_PTS * PET_ONE) return MOOD_HUNGRY;
    if (s->energy < TIRED_PTS * PET_ONE) return MOOD_TIRED;
    if (s->mood < SAD_PTS * PET_ONE) return MOOD_SAD;
    if (now_s - s->last_novel_s > BORED_S) return MOOD_BORED;
    if (now_s < s->novel_until_s) return MOOD_CURIOUS;
    if (s->mood >= HAPPY_PTS * PET_ONE) return MOOD_HAPPY;
    return MOOD_CALM;
}

pet_behavior_t pet_decide(pet_state_t *s, int64_t now_s)
{
    int sat = s->satiety / PET_ONE, en = s->energy / PET_ONE, mo = s->mood / PET_ONE;
    int desire[BEH_COUNT];
    bool gate[BEH_COUNT];
    bool asleep = pet_is_night(s, now_s);

    for (int i = 0; i < BEH_COUNT; i++) { gate[i] = !asleep; desire[i] = 0; }
    gate[BEH_SLEEP] = asleep;
    desire[BEH_SLEEP] = 100;
    desire[BEH_REST] = 20 + (100 - en) / 2 + s->trait_calm * 2;
    desire[BEH_CURIOUS] = 25 + s->trait_curiosity * 4 + (now_s < s->novel_until_s ? 30 : 0);
    desire[BEH_LOOK] = 22 + s->trait_curiosity * 2;
    gate[BEH_PLAY] = !asleep && en >= 30 && mo >= 30;
    desire[BEH_PLAY] = 15 + mo / 3 + en / 4 + s->trait_glutton;
    gate[BEH_ATTENTION] = !asleep && (sat < 40 || mo < 50);
    desire[BEH_ATTENTION] = 30 + (100 - sat) / 2 + (100 - mo) / 3;

    int score[BEH_COUNT], best = -1000;
    for (int i = 0; i < BEH_COUNT; i++) {
        if (!gate[i]) { score[i] = -1000; continue; }
        int pen = 0;
        for (int k = 0; k < 5; k++)
            if (s->hist[k] == i + 1) { pen = 50 - 10 * k; break; }   /* hist[0] = el mas reciente */
        score[i] = desire[i] + (int)rnd_below(s, 10) - pen;
        if (score[i] > best) best = score[i];
    }
    int cand[BEH_COUNT], nc = 0;
    for (int i = 0; i < BEH_COUNT; i++) if (gate[i] && score[i] >= best - 10) cand[nc++] = i;
    int pick = cand[rnd_below(s, (uint32_t)nc)];
    memmove(&s->hist[1], &s->hist[0], 4);
    s->hist[0] = (uint8_t)(pick + 1);
    s->behavior = (uint8_t)pick;
    return (pet_behavior_t)pick;
}

/* ---------------- planificador ---------------- */
int64_t pet_next_scan_s(const pet_state_t *s, int64_t now_s, bool desk, int battery_pct)
{
    if (battery_pct >= 0 && battery_pct < SCAN_MIN_BATTERY) return PET_NEVER;
    int64_t next = s->last_scan_s + (desk ? SCAN_DESK_S : SCAN_WRIST_S);
    if (next < now_s) next = now_s;
    if (pet_is_night(s, next)) {
        int64_t t = (next / 86400) * 86400 + (int64_t)s->night_end_h * 3600;
        if (t <= next) t += 86400;
        next = t;
    }
    return next;
}

/* ---------------- init ---------------- */
void pet_init(pet_state_t *s, const uint8_t key[PET_KEY_LEN], int64_t now_s)
{
    memset(s, 0, sizeof(*s));
    memcpy(s->key, key, PET_KEY_LEN);
    uint64_t x = 0;
    for (int i = 0; i < PET_KEY_LEN; i++) x = (x << 5 | x >> 59) ^ key[i];
    x ^= 0x70657421ULL;
    s->rng = splitmix(&x);
    if (!s->rng) s->rng = 1;
    s->view_seed = (uint32_t)splitmix(&x);
    s->trait_curiosity = (uint8_t)rnd_below(s, 10);
    s->trait_calm = (uint8_t)rnd_below(s, 10);
    s->trait_glutton = (uint8_t)rnd_below(s, 10);
    s->stage = PET_EGG;
    s->satiety = 80 * PET_ONE;
    s->energy = 100 * PET_ONE;
    s->mood = 70 * PET_ONE;
    s->born_s = s->last_s = s->last_novel_s = now_s;
    s->last_fam_epoch_s = now_s - FAM_EPOCH_S;
    s->visit_day = UINT32_MAX;
    s->gp_day = s->exo_day = day_of(now_s);
    s->night_start_h = 23;
    s->night_end_h = 8;
    s->sound_on = 1;
    s->started = true;
}

/* ---------------- SipHash-2-4 (dominio publico, Aumasson y Bernstein) ---------------- */
#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
    v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; v0 = ROTL(v0, 32); \
    v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; v2 = ROTL(v2, 32); } while (0)

static uint64_t le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

uint64_t pet_siphash24(const uint8_t key[16], const uint8_t *in, size_t len)
{
    uint64_t k0 = le64(key), k1 = le64(key + 8);
    uint64_t v0 = k0 ^ 0x736f6d6570736575ULL, v1 = k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = k0 ^ 0x6c7967656e657261ULL, v3 = k1 ^ 0x7465646279746573ULL;
    const uint8_t *end = in + (len - len % 8);
    for (; in != end; in += 8) {
        uint64_t m = le64(in);
        v3 ^= m; SIPROUND; SIPROUND; v0 ^= m;
    }
    uint64_t b = (uint64_t)len << 56;
    for (size_t i = 0; i < len % 8; i++) b |= (uint64_t)in[i] << (8 * i);
    v3 ^= b; SIPROUND; SIPROUND; v0 ^= b;
    v2 ^= 0xff; SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}
