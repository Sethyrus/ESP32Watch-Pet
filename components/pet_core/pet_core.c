/* pet_core: reglas del juego (diseno v2, docs/PET_DESIGN.md). Valores iniciales
 * calibrados con los bots de tests/. Todo en enteros; el azar sale de s->rng. */
#include "pet_core.h"
#include <string.h>

/* ---------------- parametros (PET_DESIGN.md, secciones 3-8) ---------------- */
#define SAT_DECAY_H        9        /* puntos/h despierta */
#define SAT_NIGHT_PCT      25
#define FUN_DECAY_H        8
#define HYG_DECAY_H        3
#define HYG_POOP_H         6        /* extra por caca en el suelo */
#define HYG_NIGHT_H        1
#define ENERGY_DECAY_H     4
#define ENERGY_REST_H      15       /* de noche */
#define ENERGY_NAP_H       80
#define LOW_PTS            25       /* burbuja y cara */
#define TIRED_PTS          20
#define HAPPY_PTS          70
#define NEED_LOW_PTS       5
#define ERROR_WINDOW_S     1800
#define GUT_POOP           60
#define POOP_GAP_S         5400
#define DIRTY_PTS          10
#define DIRTY_POOPS        3
#define SICK_AFTER_S       3600
#define SICK_CURE_S        (12 * 3600)
#define ABSENCE_SAT_PCT    60
#define ABSENCE_SAT_FLOOR  15
#define ABSENCE_FUN_PCT    50
#define ABSENCE_FUN_FLOOR  20
#define ABSENCE_HYG_PCT    50
#define ABSENCE_HYG_FLOOR  10
#define ABSENCE_POOP_MAX   3
#define ABSENCE_POOP_GAP_S (4 * 3600)
#define ABSENCE_CAP_S      (7 * 86400)
#define ABSENCE_ERR_BLOCK  (12 * 3600)
#define ABSENCE_ERR_MAX    2
#define STEP_MAX_GAP_S     300
#define JUV_LEVEL          3
#define JUV_AGE_S          (12 * 3600)
#define JUV_FORCE_S        (3 * 86400)
#define ADULT_LEVEL        6
#define ADULT_AGE_S        (3 * 86400)
#define ADULT_FORCE_S      (10 * 86400)
#define NEWGEN_AGE_S       (3 * 86400)
#define HOMEBODY_TRAIN     25
#define POLISHED_MOOD      52       /* animo medio de juvenil (ver bots) */
#define ODD_MOOD           30
#define LEVEL_COINS        10
#define TOUCH_FUN_H        15
#define TOUCH_XP_H         5
#define WISH_FIRST_S       600
#define WISH_LIFE_S        7200
#define WISH_GAP_MIN_S     3600
#define WISH_GAP_MAX_S     9000
#define WISH_PETS          3
#define SIGNAL_DAY_CAP     15
#define TREAT_ONE_IN       6
#define INV_MAX            99
#define COINS_MAX          99999
#define FAM_EPOCH_S        1200
#define FAM_SEEN_4         4
#define FAM_PROMOTE_4      20
#define FAM_CAP_4          48
#define URBAN_NETS         15
#define URBAN_OPEN         3
#define URBAN_COOLDOWN_S   1800
#define NOVEL_VIEW_S       120
#define SCAN_DESK_S        60
#define SCAN_WRIST_S       900
#define SCAN_MIN_BATTERY   15

/* precio, comida, diversion, xp, tripa */
static const int16_t ITEM_FX[ITEM_COUNT][5] = {
    [ITEM_BREAD]    = {  3, 25,  0,  2, 30 },
    [ITEM_FRUIT]    = {  5, 15,  5,  3, 15 },
    [ITEM_STEW]     = {  8, 45,  5,  4, 45 },
    [ITEM_CAKE]     = { 10, 10, 25,  4, 25 },
    [ITEM_TREAT]    = {  0, 20, 15, 15, 20 },
    [ITEM_MEDICINE] = { 20,  0,  0,  2,  0 },
};
/* precio, nivel */
static const int16_t ACC_SHOP[ACC_COUNT][2] = {
    [ACC_NONE] = {0, 0}, [ACC_BOW] = {60, 2}, [ACC_FLOWER] = {80, 3}, [ACC_BEANIE] = {110, 4},
    [ACC_GLASSES] = {150, 5}, [ACC_TOPHAT] = {220, 7}, [ACC_CROWN] = {400, 10},
    [ACC_RADAR] = {0, 0}, [ACC_SAFARI] = {0, 0},
};
static const uint8_t GAME_LEVEL[GAME_COUNT] = { 1, 1, 2 };
static const uint16_t MILESTONES[] = { 10, 30, 60, 100, 200 };
#define MILESTONE_N ((int)(sizeof MILESTONES / sizeof MILESTONES[0]))

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

bool pet_is_napping(const pet_state_t *s, int64_t now_s) { return s->nap_until_s > now_s; }

bool pet_is_asleep(const pet_state_t *s, int64_t now_s)
{
    return s->stage != PET_EGG && (pet_is_night(s, now_s) || pet_is_napping(s, now_s));
}

/* ---------------- estadisticas ---------------- */
static int32_t clampi(int64_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : (int32_t)v; }

static void add_pts(int32_t *stat, int pts) { *stat = clampi((int64_t)*stat + (int64_t)pts * PET_ONE, 0, PET_MAX); }

/* Perdida pasiva lineal con suelo (el suelo solo frena, nunca sube el valor). */
static void lose_floor(int32_t *stat, int64_t amount, int32_t floor_pts)
{
    int32_t fl = floor_pts * PET_ONE;
    if (*stat <= fl) return;
    *stat = clampi(*stat - amount, fl, PET_MAX);
}

static int64_t rate(int pts_per_h, int64_t secs) { return (int64_t)pts_per_h * PET_ONE * secs / 3600; }

int pet_need(const pet_state_t *s, pet_need_t n)
{
    switch (n) {
    case NEED_FOOD:    return s->satiety / PET_ONE;
    case NEED_FUN:     return s->fun / PET_ONE;
    case NEED_HYGIENE: return s->hygiene / PET_ONE;
    case NEED_ENERGY:  return s->energy / PET_ONE;
    default:           return 0;
    }
}

int pet_happiness(const pet_state_t *s)
{
    int sum = 0, lo = 100;
    for (int i = 0; i < NEED_COUNT; i++) {
        int v = pet_need(s, (pet_need_t)i);
        sum += v;
        if (v < lo) lo = v;
    }
    int h = (sum / NEED_COUNT + lo) / 2;
    return s->sick ? h / 2 : h;
}

/* ---------------- monedas, xp y nivel ---------------- */
static uint32_t level_floor(int level) { return 20u * (uint32_t)level * (uint32_t)(level - 1); }

static void add_coins(pet_state_t *s, int n)
{
    if (n <= 0) return;
    s->coins = s->coins + (uint32_t)n > COINS_MAX ? COINS_MAX : s->coins + (uint32_t)n;
    s->gain_coins += n;
}

static void add_xp(pet_state_t *s, int n)
{
    if (n <= 0 || s->stage == PET_EGG) return;
    s->xp += (uint32_t)n;
    s->gain_xp += n;
    while (s->level < 99 && s->xp >= level_floor(s->level + 1)) {
        s->level++;
        add_coins(s, LEVEL_COINS);
        s->events |= EVT_LEVEL;
    }
}

void pet_level_progress(const pet_state_t *s, int *have, int *need)
{
    uint32_t lo = level_floor(s->level), hi = level_floor(s->level + 1);
    if (have) *have = s->xp > lo ? (int)(s->xp - lo) : 0;
    if (need) *need = (int)(hi - lo);
}

/* ---------------- evolucion ---------------- */
int64_t pet_age_s(const pet_state_t *s, int64_t now_s)
{
    return s->stage >= PET_BABY && now_s > s->born_s ? now_s - s->born_s : 0;
}

bool pet_evo_target(const pet_state_t *s, int *level, int64_t *age_s)
{
    switch (s->stage) {
    case PET_EGG:      if (level) *level = 0; if (age_s) *age_s = 0; return true;
    case PET_BABY:     if (level) *level = JUV_LEVEL; if (age_s) *age_s = JUV_AGE_S; return true;
    case PET_JUVENILE: if (level) *level = ADULT_LEVEL; if (age_s) *age_s = ADULT_AGE_S; return true;
    default:           return false;
    }
}

pet_branch_t pet_branch_lean(const pet_state_t *s)
{
    int total = s->train[0] + s->train[1] + s->train[2];
    if (total < HOMEBODY_TRAIN) return BRANCH_HOMEBODY;
    int best = 0;
    for (int i = 1; i < GAME_COUNT; i++) if (s->train[i] > s->train[best]) best = i;
    static const uint8_t MAP[GAME_COUNT] = { BRANCH_AGILE, BRANCH_WISE, BRANCH_STRONG };
    return (pet_branch_t)MAP[best];
}

static void schedule_wish(pet_state_t *s, int64_t now)
{
    s->next_wish_s = now + WISH_GAP_MIN_S + rnd_below(s, WISH_GAP_MAX_S - WISH_GAP_MIN_S);
}

static bool evolve_once(pet_state_t *s, int64_t now)
{
    int64_t age = pet_age_s(s, now);
    switch (s->stage) {
    case PET_EGG:
        if (s->egg_active_s < PET_EGG_HATCH_S) return false;
        s->stage = PET_BABY;
        s->hatch_s = now;
        s->next_wish_s = now + WISH_FIRST_S;
        s->events |= EVT_HATCHED;
        break;
    case PET_BABY:
        if (!((age >= JUV_AGE_S && s->level >= JUV_LEVEL) || age >= JUV_FORCE_S)) return false;
        s->stage = PET_JUVENILE;
        s->mood_sum = s->mood_n = 0;
        s->events |= EVT_EVOLVED;
        break;
    case PET_JUVENILE: {
        if (!((age >= ADULT_AGE_S && s->level >= ADULT_LEVEL) || age >= ADULT_FORCE_S)) return false;
        s->juv_errors = s->care_errors;
        s->branch = (uint8_t)pet_branch_lean(s);
        uint32_t mean = s->mood_n ? s->mood_sum / s->mood_n : 50;
        s->variant = s->juv_errors >= 4 ? VAR_CLUMSY
                   : mean < ODD_MOOD ? VAR_ODD
                   : (s->juv_errors <= 1 && mean >= POLISHED_MOOD) ? VAR_POLISHED : VAR_NORMAL;
        s->album |= (uint16_t)(1u << ((s->branch - 1) * 4 + s->variant));
        s->stage = PET_ADULT;
        s->adult_s = now;
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

/* ---------------- visitas y deseos ---------------- */
static void visit(pet_state_t *s, int64_t now)
{
    if (s->stage == PET_EGG) return;
    uint32_t d = day_of(now);
    if (s->visit_day == d) return;
    s->streak = (s->visit_day != UINT32_MAX && s->visit_day + 1 == d) ? (uint16_t)(s->streak + 1) : 1;
    s->visit_day = d;
    int extra = s->streak > 1 ? 2 * (s->streak - 1) : 0;
    add_coins(s, 5 + (extra > 10 ? 10 : extra));
    add_xp(s, 10);
    s->events |= EVT_DAILY;
}

static void react(pet_state_t *s, uint8_t kind, uint32_t mono_ms, uint32_t dur_ms);

static void wish_done(pet_state_t *s, int64_t now)
{
    int c = 5 + s->level;
    add_coins(s, c > 20 ? 20 : c);
    add_xp(s, 15);
    add_pts(&s->fun, 10);
    if (s->wishes_done < 65535) s->wishes_done++;
    s->wish_kind = WISH_NONE;
    s->wish_prog = 0;
    schedule_wish(s, now);
    s->events |= EVT_WISH_DONE;
    s->react_kind = REACT_LOVE;
    s->react_until_ms = 0;            /* lo arma la UI con su reloj (ver pet_view) */
}

static void wish_check(pet_state_t *s, pet_wish_t kind, int arg, int64_t now)
{
    if (s->wish_kind != kind) return;
    if ((kind == WISH_FOOD || kind == WISH_GAME) && s->wish_arg != arg) return;
    if (kind == WISH_PET && ++s->wish_prog < WISH_PETS) return;
    wish_done(s, now);
}

static void wish_tick(pet_state_t *s, int64_t now)
{
    if (s->stage == PET_EGG) return;
    if (s->wish_kind != WISH_NONE) {
        if (now >= s->wish_until_s) { s->wish_kind = WISH_NONE; s->wish_prog = 0; schedule_wish(s, now); }
        return;
    }
    if (s->next_wish_s == 0) { s->next_wish_s = now + WISH_FIRST_S; return; }
    if (now < s->next_wish_s || pet_is_asleep(s, now)) return;
    uint8_t cand[WISH_COUNT], nc = 0;
    cand[nc++] = WISH_FOOD; cand[nc++] = WISH_GAME; cand[nc++] = WISH_PET;
    if (s->poops > 0 || s->hygiene < 60 * PET_ONE) cand[nc++] = WISH_CLEAN;
    if (s->last_novel_s > s->born_s && now - s->last_novel_s < 3 * 86400) cand[nc++] = WISH_SIGNAL;
    s->wish_kind = cand[rnd_below(s, nc)];
    s->wish_prog = 0;
    s->wish_arg = 0;
    if (s->wish_kind == WISH_FOOD) {
        s->wish_arg = (uint8_t)rnd_below(s, PET_MENU_COUNT);
    } else if (s->wish_kind == WISH_GAME) {
        uint8_t g[GAME_COUNT], ng = 0;
        for (int i = 0; i < GAME_COUNT; i++) if (s->level >= GAME_LEVEL[i]) g[ng++] = (uint8_t)i;
        s->wish_arg = g[rnd_below(s, ng)];
    }
    s->wish_until_s = now + WISH_LIFE_S;
    s->events |= EVT_WISH_NEW;
}

/* ---------------- comer ---------------- */
int pet_item_price(pet_item_t item) { return item < ITEM_COUNT ? ITEM_FX[item][0] : 0; }

void pet_item_effect(pet_item_t item, int *food, int *fun, int *xp)
{
    if (item >= ITEM_COUNT) item = ITEM_BREAD;
    if (food) *food = ITEM_FX[item][1];
    if (fun) *fun = ITEM_FX[item][2];
    if (xp) *xp = ITEM_FX[item][3];
}

pet_item_t pet_favorite(const pet_state_t *s) { return (pet_item_t)((s->view_seed >> 12) % PET_MENU_COUNT); }

/* Sin monedas para el plato más barato el pan es gratis: nunca se queda sin poder comer. */
bool pet_bread_free(const pet_state_t *s) { return s->inv[ITEM_BREAD] == 0 && s->coins < (uint32_t)ITEM_FX[ITEM_BREAD][0]; }

pet_feed_t pet_feed(pet_state_t *s, pet_item_t item, int64_t now_s)
{
    if (item >= ITEM_COUNT) return FEED_NONE;
    if (s->stage == PET_EGG) return FEED_EGG;
    pet_step(s, now_s);
    if (pet_is_asleep(s, now_s)) return FEED_ASLEEP;
    if (item == ITEM_MEDICINE && !s->sick) return FEED_NOT_SICK;
    if (item != ITEM_MEDICINE && s->satiety >= 95 * PET_ONE) return FEED_FULL;
    if (s->inv[item] > 0) {
        s->inv[item]--;
    } else if (item == ITEM_BREAD && pet_bread_free(s)) {
        /* gratis */
    } else {
        int price = ITEM_FX[item][0];
        if (price <= 0) return FEED_NONE;
        if (s->coins < (uint32_t)price) return FEED_NO_COINS;
        s->coins -= (uint32_t)price;
    }
    int food = ITEM_FX[item][1], fun = ITEM_FX[item][2], xp = ITEM_FX[item][3];
    if (item == ITEM_CAKE) {
        uint32_t d = day_of(now_s);
        if (s->cake_day != d) { s->cake_day = d; s->cake_today = 0; }
        if (s->cake_today++ > 0) fun /= 2;
    }
    if (item == ITEM_MEDICINE) {
        s->sick = 0;
        s->dirty_s = 0;
        s->events |= EVT_CURED;
    }
    if (item < PET_MENU_COUNT && item == pet_favorite(s)) {
        fun += 10;
        xp += 2;
        s->react_kind = REACT_LOVE;
        s->react_until_ms = 0;
    }
    if (s->sick) { food /= 2; fun /= 2; }
    add_pts(&s->satiety, food);
    add_pts(&s->fun, fun);
    s->gut = (int16_t)(s->gut + ITEM_FX[item][4] > 120 ? 120 : s->gut + ITEM_FX[item][4]);
    s->tasted |= (uint8_t)(1u << item);
    s->last_action_s = now_s;
    s->need_low_s = 0;
    visit(s, now_s);
    add_xp(s, xp);
    wish_check(s, WISH_FOOD, item, now_s);
    evolve_check(s, now_s);
    return FEED_OK;
}

/* ---------------- aseo y siesta ---------------- */
bool pet_clean_poop(pet_state_t *s, int64_t now_s)
{
    if (s->poops == 0) return false;
    pet_step(s, now_s);
    s->poops--;
    add_pts(&s->hygiene, 2);
    add_coins(s, 1);
    visit(s, now_s);
    add_xp(s, 2);
    s->last_action_s = now_s;
    if (s->poops == 0) wish_check(s, WISH_CLEAN, 0, now_s);
    return true;
}

bool pet_wash(pet_state_t *s, int64_t now_s)
{
    if (s->stage == PET_EGG) return false;
    pet_step(s, now_s);
    if (pet_is_night(s, now_s)) return false;
    bool useful = s->poops > 0 || s->hygiene < 90 * PET_ONE;
    s->poops = 0;
    add_pts(&s->hygiene, 60);
    s->dirty_s = 0;
    s->last_action_s = now_s;
    s->need_low_s = 0;
    visit(s, now_s);
    if (useful) add_xp(s, 3);
    wish_check(s, WISH_CLEAN, 0, now_s);
    return true;
}

bool pet_nap(pet_state_t *s, int64_t now_s)
{
    if (s->stage == PET_EGG) return false;
    pet_step(s, now_s);
    if (pet_is_asleep(s, now_s)) return false;
    s->nap_until_s = now_s + PET_NAP_S;
    s->last_action_s = now_s;
    visit(s, now_s);
    return true;
}

void pet_wake(pet_state_t *s, int64_t now_s)
{
    if (!pet_is_napping(s, now_s)) return;
    pet_step(s, now_s);
    s->nap_until_s = 0;
    s->events |= EVT_WOKE;
}

/* ---------------- minijuegos ---------------- */
int pet_game_level(pet_game_id_t g) { return g < GAME_COUNT ? GAME_LEVEL[g] : 99; }

pet_play_t pet_can_play(const pet_state_t *s, pet_game_id_t g, int64_t now_s)
{
    if (g >= GAME_COUNT) return PLAY_LOCKED;
    if (s->stage == PET_EGG) return PLAY_EGG;
    if (pet_is_asleep(s, now_s)) return PLAY_ASLEEP;
    if (s->sick) return PLAY_SICK;
    if (s->level < GAME_LEVEL[g]) return PLAY_LOCKED;
    if (s->energy < 10 * PET_ONE) return PLAY_TIRED;
    return PLAY_OK;
}

pet_reward_t pet_game_finish(pet_state_t *s, pet_game_id_t g, int perf, int score, int64_t now_s)
{
    pet_reward_t r = {0, 0, 0, 0, false};
    if (g >= GAME_COUNT || s->stage == PET_EGG) return r;
    pet_step(s, now_s);
    if (perf < 0) perf = 0;
    if (perf > 100) perf = 100;
    r.fun = 15 + 25 * perf / 100;
    r.xp = 10 + perf / 10;
    r.coins = 2 + 13 * perf / 100;
    r.train = s->stage < PET_ADULT ? 2 + perf / 10 : 0;
    add_pts(&s->fun, r.fun);
    lose_floor(&s->energy, 8 * PET_ONE, 0);
    s->train[g] = (uint16_t)(s->train[g] + r.train > 65535 ? 65535 : s->train[g] + r.train);
    if (s->games[g] < 65535) s->games[g]++;
    if (score > s->best[g]) { s->best[g] = (uint16_t)(score > 65535 ? 65535 : score); r.record = true; }
    s->last_action_s = now_s;
    s->need_low_s = 0;
    visit(s, now_s);
    add_coins(s, r.coins);
    add_xp(s, r.xp);
    wish_check(s, WISH_GAME, g, now_s);
    evolve_check(s, now_s);
    return r;
}

/* ---------------- tienda ---------------- */
int pet_acc_price(pet_acc_t a) { return a < ACC_COUNT ? ACC_SHOP[a][0] : 0; }
int pet_acc_level(pet_acc_t a) { return a < ACC_COUNT ? ACC_SHOP[a][1] : 0; }
bool pet_acc_owned(const pet_state_t *s, pet_acc_t a) { return a > ACC_NONE && a < ACC_COUNT && (s->acc_owned >> a & 1); }

pet_buy_t pet_buy_acc(pet_state_t *s, pet_acc_t a)
{
    if (a <= ACC_NONE || a >= ACC_COUNT || ACC_SHOP[a][0] <= 0) return BUY_NOT_SOLD;
    if (pet_acc_owned(s, a)) return BUY_OWNED;
    if (s->level < ACC_SHOP[a][1]) return BUY_LOCKED;
    if (s->coins < (uint32_t)ACC_SHOP[a][0]) return BUY_NO_COINS;
    s->coins -= (uint32_t)ACC_SHOP[a][0];
    s->acc_owned |= (uint16_t)(1u << a);
    s->acc_worn = (uint8_t)a;
    return BUY_OK;
}

bool pet_wear(pet_state_t *s, pet_acc_t a)
{
    if (a != ACC_NONE && !pet_acc_owned(s, a)) return false;
    s->acc_worn = (uint8_t)a;
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

static void add_inv(pet_state_t *s, pet_item_t it, int n)
{
    s->inv[it] = (uint8_t)(s->inv[it] + n > INV_MAX ? INV_MAX : s->inv[it] + n);
}

int pet_milestone_count(void) { return MILESTONE_N; }
int pet_milestone_signals(int i) { return i >= 0 && i < MILESTONE_N ? MILESTONES[i] : -1; }

int pet_next_milestone(const pet_state_t *s)
{
    for (int i = 0; i < MILESTONE_N; i++) if (!(s->sig_milestones >> i & 1)) return MILESTONES[i];
    return -1;
}

static void milestones(pet_state_t *s)
{
    for (int i = 0; i < MILESTONE_N; i++) {
        if ((s->sig_milestones >> i & 1) || s->signals < MILESTONES[i]) continue;
        s->sig_milestones |= (uint8_t)(1u << i);
        s->last_milestone = (uint8_t)i;
        s->events |= EVT_MILESTONE;
        switch (i) {
        case 0: add_coins(s, 25); break;
        case 1: s->acc_owned |= 1u << ACC_RADAR; break;
        case 2: add_inv(s, ITEM_TREAT, 3); break;
        case 3: s->acc_owned |= 1u << ACC_SAFARI; break;
        default: add_coins(s, 100); break;
        }
    }
}

int pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s)
{
    if (pet_is_night(s, now_s) || s->stage == PET_EGG) return 0;
    if (n > 64) n = 64;
    uint32_t d = day_of(now_s);
    if (s->sig_day != d) { s->sig_day = d; s->sig_today = 0; }

    bool epoch = now_s - s->last_fam_epoch_s >= FAM_EPOCH_S;
    uint8_t seen_f[PET_FAM_MAX] = {0}, seen_r[PET_FAM_MAX] = {0};
    uint32_t newh[64];
    int new_n = 0, open_n = 0;
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
        if (f >= 0) { seen_f[f] = 1; continue; }
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

    if (new_n > 0) {
        s->signals += (uint32_t)new_n;
        int rw = new_n < SIGNAL_DAY_CAP - s->sig_today ? new_n : SIGNAL_DAY_CAP - s->sig_today;
        if (rw < 0) rw = 0;
        s->sig_today = (uint8_t)(s->sig_today + rw);
        add_coins(s, rw);
        add_xp(s, rw);
        int treats = 0;
        for (int i = 0; i < rw; i++) treats += rnd_below(s, TREAT_ONE_IN) == 0;
        if (treats) { add_inv(s, ITEM_TREAT, treats); s->events |= EVT_TREAT; }
        s->last_novel_s = now_s;
        s->novel_until_s = now_s + NOVEL_VIEW_S;
        s->events |= EVT_NEW_SIGNAL;
        milestones(s);
        wish_check(s, WISH_SIGNAL, 0, now_s);
    }
    if ((seen_n >= URBAN_NETS || open_n >= URBAN_OPEN) && now_s - s->last_urban_s >= URBAN_COOLDOWN_S) {
        add_inv(s, ITEM_TREAT, 1);
        s->last_urban_s = now_s;
        s->events |= EVT_TREAT;
    }
    s->last_scan_s = now_s;
    s->total_scans++;
    return new_n;
}

/* ---------------- paso activo ---------------- */
static void active_minute(pet_state_t *s, int64_t now)
{
    const bool night = pet_is_night(s, now), nap = pet_is_napping(s, now);
    const int64_t m = 60;

    if (s->stage == PET_EGG) {
        if (s->egg_active_s < 65535 - 60) s->egg_active_s += 60;
        evolve_check(s, now);
        return;
    }
    if (!night) {
        lose_floor(&s->satiety, rate(SAT_DECAY_H, m), 0);
        lose_floor(&s->fun, rate(nap ? FUN_DECAY_H / 2 : FUN_DECAY_H, m), 0);
        lose_floor(&s->hygiene, rate(HYG_DECAY_H + HYG_POOP_H * s->poops, m), 0);
        if (nap) s->energy = clampi((int64_t)s->energy + rate(ENERGY_NAP_H, m), 0, PET_MAX);
        else lose_floor(&s->energy, rate(s->sick ? ENERGY_DECAY_H * 3 / 2 : ENERGY_DECAY_H, m), 0);
        if (s->gut >= GUT_POOP && now - s->last_poop_s >= POOP_GAP_S && s->poops < PET_POOP_MAX) {
            s->poops++;
            s->gut -= GUT_POOP;
            s->last_poop_s = now;
            s->events |= EVT_POOP;
        }
        s->mood_sum += (uint32_t)pet_happiness(s);
        s->mood_n++;
    } else {
        lose_floor(&s->satiety, rate(SAT_DECAY_H * SAT_NIGHT_PCT / 100, m), 0);
        lose_floor(&s->hygiene, rate(HYG_NIGHT_H, m), 0);
        s->energy = clampi((int64_t)s->energy + rate(ENERGY_REST_H, m), 0, PET_MAX);
    }
    if (nap && s->energy >= PET_MAX) s->nap_until_s = now;   /* descansado: se despierta solo */

    /* malito */
    if (s->sick) {
        if (now - s->sick_since_s >= SICK_CURE_S) { s->sick = 0; s->dirty_s = 0; s->events |= EVT_CURED; }
    } else if (!night && (s->hygiene <= DIRTY_PTS * PET_ONE || s->poops >= DIRTY_POOPS)) {
        s->dirty_s = (uint16_t)(s->dirty_s + 60);
        if (s->dirty_s >= SICK_AFTER_S) {
            s->sick = 1;
            s->sick_since_s = now;
            s->dirty_s = 0;
            if (s->care_errors < 255) s->care_errors++;
            s->events |= EVT_SICK | EVT_ERROR;
        }
    } else {
        s->dirty_s = 0;
    }

    /* errores de cuidado */
    bool low = !night && (s->satiety <= NEED_LOW_PTS * PET_ONE || s->fun <= NEED_LOW_PTS * PET_ONE ||
                          s->hygiene <= NEED_LOW_PTS * PET_ONE);
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
    wish_tick(s, now);
    evolve_check(s, now);
}

void pet_step(pet_state_t *s, int64_t now_s)
{
    if (!s->started) { s->last_s = now_s; s->started = true; return; }
    int64_t dt = now_s - s->last_s;
    if (dt < 0) { s->last_s = now_s; return; }
    if (dt > STEP_MAX_GAP_S) { pet_catch_up(s, now_s); return; }
    int64_t total = s->step_rem_s + dt;
    int64_t mins = total / 60;
    s->step_rem_s = total % 60;
    /* cada minuto se evalua en su propio instante: la siesta y la noche cambian a mitad de paso */
    for (int64_t i = mins; i > 0; i--) active_minute(s, now_s - (i - 1) * 60 - s->step_rem_s);
    s->last_s = now_s;
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
        if (s->stage == PET_EGG) { t += chunk; continue; }
        bool nap = pet_is_napping(s, mid);
        if (awake) {
            lose_floor(&s->satiety, rate(SAT_DECAY_H, chunk) * ABSENCE_SAT_PCT / 100, ABSENCE_SAT_FLOOR);
            lose_floor(&s->fun, rate(FUN_DECAY_H, chunk) * ABSENCE_FUN_PCT / 100, ABSENCE_FUN_FLOOR);
            lose_floor(&s->hygiene, rate(HYG_DECAY_H + HYG_POOP_H * s->poops, chunk) * ABSENCE_HYG_PCT / 100, ABSENCE_HYG_FLOOR);
            if (nap) s->energy = clampi((int64_t)s->energy + rate(ENERGY_NAP_H, chunk), 0, PET_MAX);
            else lose_floor(&s->energy, rate(ENERGY_DECAY_H, chunk), 0);
            if (s->gut >= GUT_POOP && t - s->last_poop_s >= ABSENCE_POOP_GAP_S && s->poops < ABSENCE_POOP_MAX) {
                s->poops++;
                s->gut -= GUT_POOP;
                s->last_poop_s = t;
            }
            s->mood_sum += (uint32_t)pet_happiness(s) * (uint32_t)(chunk / 60);
            s->mood_n += (uint32_t)(chunk / 60);
            if (s->satiety < LOW_PTS * PET_ONE) {
                hungry_run += chunk;
                if (hungry_run >= ABSENCE_ERR_BLOCK) { blocks++; hungry_run = 0; }
            }
        } else {
            lose_floor(&s->satiety, rate(SAT_DECAY_H * SAT_NIGHT_PCT / 100, chunk) * ABSENCE_SAT_PCT / 100, ABSENCE_SAT_FLOOR);
            lose_floor(&s->hygiene, rate(HYG_NIGHT_H, chunk), ABSENCE_HYG_FLOOR);
            s->energy = clampi((int64_t)s->energy + rate(ENERGY_REST_H, chunk), 0, PET_MAX);
        }
        t += chunk;
    }
    if (s->stage >= PET_BABY) {
        int add = blocks < ABSENCE_ERR_MAX ? blocks : ABSENCE_ERR_MAX;
        s->care_errors = (uint8_t)(s->care_errors + add > 255 ? 255 : s->care_errors + add);
        if (s->sick && now_s - s->sick_since_s >= SICK_CURE_S) { s->sick = 0; s->events |= EVT_CURED; }
        if (s->nap_until_s && s->nap_until_s <= now_s) s->nap_until_s = 0;
        if (s->wish_kind != WISH_NONE && now_s >= s->wish_until_s) {
            s->wish_kind = WISH_NONE;
            s->wish_prog = 0;
            s->next_wish_s = now_s + WISH_FIRST_S;    /* un deseo poco despues de volver */
        }
    }
    s->dirty_s = 0;
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
    ring_push(s->touch_ms, &s->touch_cnt, mono_ms);
    if (pet_is_napping(s, now_s)) {           /* tocarla la despierta */
        pet_wake(s, now_s);
        react(s, REACT_SHAKE, mono_ms, 1500);
        return REACT_SHAKE;
    }
    visit(s, now_s);
    if (react_active(s, mono_ms, REACT_DIZZY)) return REACT_DIZZY;
    if (s->touch_cnt == 4 && (int32_t)(mono_ms - s->touch_ms[0]) <= 1500) {
        react(s, REACT_DIZZY, mono_ms, 3000);
        return REACT_DIZZY;
    }
    if (s->stage != PET_EGG && !pet_is_night(s, now_s)) {
        uint32_t hr = (uint32_t)(now_s / 3600);
        if (s->touch_hour != hr) { s->touch_hour = hr; s->touch_gain = 0; s->touch_xp = 0; }
        if (s->touch_gain < TOUCH_FUN_H) { add_pts(&s->fun, 3); s->touch_gain += 3; }
        if (s->touch_xp < TOUCH_XP_H) { add_xp(s, 1); s->touch_xp++; }
        wish_check(s, WISH_PET, 0, now_s);
    }
    if (!react_active(s, mono_ms, REACT_LOVE)) react(s, REACT_PET, mono_ms, 2500);
    evolve_check(s, now_s);
    return REACT_PET;
}

pet_reaction_t pet_shake(pet_state_t *s, int64_t now_s, uint32_t mono_ms)
{
    (void)now_s;
    ring_push(s->shake_ms, &s->shake_cnt, mono_ms);
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

void pet_take_gains(pet_state_t *s, int *coins, int *xp)
{
    if (coins) *coins = s->gain_coins;
    if (xp) *xp = s->gain_xp;
    s->gain_coins = s->gain_xp = 0;
}

/* ---------------- animo y decisiones ---------------- */
pet_want_t pet_want(const pet_state_t *s, int64_t now_s)
{
    if (s->stage == PET_EGG || pet_is_asleep(s, now_s)) return WANT_NONE;
    if (s->sick) return WANT_MEDICINE;
    if (s->satiety < LOW_PTS * PET_ONE) return WANT_FOOD;
    if (s->hygiene < LOW_PTS * PET_ONE || s->poops >= 2) return WANT_CLEAN;
    if (s->energy < TIRED_PTS * PET_ONE) return WANT_SLEEP;
    if (s->fun < LOW_PTS * PET_ONE) return WANT_PLAY;
    if (s->wish_kind != WISH_NONE) return WANT_WISH;
    return WANT_NONE;
}

pet_mood_t pet_mood(const pet_state_t *s, int64_t now_s)
{
    if (pet_is_asleep(s, now_s)) return MOOD_ASLEEP;
    if (s->sick) return MOOD_SICK;
    if (s->satiety < LOW_PTS * PET_ONE) return MOOD_HUNGRY;
    if (s->hygiene < LOW_PTS * PET_ONE || s->poops >= 2) return MOOD_DIRTY;
    if (s->energy < TIRED_PTS * PET_ONE) return MOOD_TIRED;
    if (s->fun < LOW_PTS * PET_ONE) return MOOD_BORED;
    if (pet_happiness(s) >= HAPPY_PTS) return MOOD_HAPPY;
    return MOOD_CALM;
}

pet_behavior_t pet_decide(pet_state_t *s, int64_t now_s)
{
    int en = s->energy / PET_ONE, hp = pet_happiness(s);
    int desire[BEH_COUNT];
    bool gate[BEH_COUNT];
    bool asleep = pet_is_asleep(s, now_s);

    for (int i = 0; i < BEH_COUNT; i++) { gate[i] = !asleep && s->stage != PET_EGG; desire[i] = 0; }
    gate[BEH_SLEEP] = asleep;
    desire[BEH_SLEEP] = 100;
    desire[BEH_REST] = 20 + (100 - en) / 2 + s->trait_calm * 2;
    desire[BEH_WALK] = 30 + s->trait_curiosity * 3 + (now_s < s->novel_until_s ? 20 : 0);
    desire[BEH_LOOK] = 22 + s->trait_curiosity * 2;
    gate[BEH_PLAY] = gate[BEH_PLAY] && en >= 30 && hp >= 50;
    desire[BEH_PLAY] = 15 + hp / 3 + en / 4 + s->trait_glutton;
    gate[BEH_ATTENTION] = gate[BEH_ATTENTION] && pet_want(s, now_s) != WANT_NONE;
    desire[BEH_ATTENTION] = 60;

    int score[BEH_COUNT], best = -1000;
    for (int i = 0; i < BEH_COUNT; i++) {
        if (!gate[i]) { score[i] = -1000; continue; }
        int pen = 0;
        for (int k = 0; k < 5; k++)
            if (s->hist[k] == i + 1) { pen = 50 - 10 * k; break; }   /* hist[0] = el mas reciente */
        score[i] = desire[i] + (int)rnd_below(s, 10) - pen;
        if (score[i] > best) best = score[i];
    }
    if (best == -1000) { s->behavior = BEH_REST; return BEH_REST; }   /* huevo */
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
    if (s->stage == PET_EGG) return PET_NEVER;
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

/* ---------------- init y generaciones ---------------- */
static void reset_creature(pet_state_t *s, int64_t now_s)
{
    uint64_t x = s->rng ^ (uint64_t)now_s;
    s->view_seed = (uint32_t)splitmix(&x);
    s->trait_curiosity = (uint8_t)rnd_below(s, 10);
    s->trait_calm = (uint8_t)rnd_below(s, 10);
    s->trait_glutton = (uint8_t)rnd_below(s, 10);
    s->stage = PET_EGG;
    s->branch = BRANCH_NONE;
    s->variant = VAR_NORMAL;
    s->egg_active_s = 0;
    s->satiety = 80 * PET_ONE;
    s->energy = 100 * PET_ONE;
    s->fun = 70 * PET_ONE;
    s->hygiene = 100 * PET_ONE;
    s->care_errors = s->juv_errors = 0;
    s->need_low_s = 0;
    s->mood_sum = s->mood_n = 0;
    s->born_s = s->last_s = s->last_novel_s = s->last_action_s = now_s;
    s->hatch_s = s->adult_s = 0;
    s->level = 1;
    s->xp = 0;
    memset(s->train, 0, sizeof s->train);
    s->poops = 0;
    s->gut = 0;
    s->last_poop_s = now_s;
    s->dirty_s = 0;
    s->sick = 0;
    s->sick_since_s = s->nap_until_s = 0;
    s->wish_kind = WISH_NONE;
    s->wish_arg = s->wish_prog = 0;
    s->wish_until_s = s->next_wish_s = 0;
    s->cake_day = 0;
    s->cake_today = 0;
    s->started = true;
}

void pet_init(pet_state_t *s, const uint8_t key[PET_KEY_LEN], int64_t now_s)
{
    memset(s, 0, sizeof(*s));
    memcpy(s->key, key, PET_KEY_LEN);
    uint64_t x = 0;
    for (int i = 0; i < PET_KEY_LEN; i++) x = (x << 5 | x >> 59) ^ key[i];
    x ^= 0x70657421ULL;
    s->rng = splitmix(&x);
    if (!s->rng) s->rng = 1;
    reset_creature(s, 0);
    s->view_seed = (uint32_t)splitmix(&x);   /* la semilla v1/v2 no dependia de la hora */
    s->born_s = s->last_s = s->last_novel_s = s->last_action_s = s->last_poop_s = now_s;
    s->last_fam_epoch_s = now_s - FAM_EPOCH_S;
    s->visit_day = UINT32_MAX;
    s->sig_day = day_of(now_s);
    s->night_start_h = 23;
    s->night_end_h = 8;
    s->volume = PET_VOL_DEFAULT;
    s->coins = 20;
}

bool pet_can_new_generation(const pet_state_t *s, int64_t now_s)
{
    return s->stage == PET_ADULT && now_s - s->adult_s >= NEWGEN_AGE_S;
}

bool pet_new_generation(pet_state_t *s, int64_t now_s)
{
    if (!pet_can_new_generation(s, now_s)) return false;
    pet_step(s, now_s);
    reset_creature(s, now_s);
    if (s->generation < 255) s->generation++;
    s->events = 0;
    return true;
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
