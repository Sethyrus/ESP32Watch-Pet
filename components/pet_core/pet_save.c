/* Partida versionada: cabecera {magic, version, len, seq, crc32} + carga.
 * La carga solo crece por el final; los campos de una version nueva van despues
 * de los de la anterior y una carga mas corta se completa con valores por defecto.
 * v1/v2 son el diseno v1 (despensa WiFi); al cargarlas se migran a v3 (R16):
 * animo -> diversion, PC -> XP, despensa -> inventario. v3 sigue escribiendo los
 * huecos de v1 (con la despensa sacada del inventario) para no romper el formato.
 * v4 añade el volumen (0-3); el byte de sonido sí/no de v2 queda como hueco. */
#include "pet_core.h"
#include <string.h>

#define MAGIC 0x53544550u   /* "PETS" */
#define HDR   16
#define V1_PANTRY 20        /* bocados de la despensa v1 */
#define V1_DOMESTIC 0
#define V1_EXOTIC   1
#define V1_URBAN    2
#define V1_GP_XP    5       /* XP por cada PC de v1 */

uint32_t pet_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

typedef struct { uint8_t *p; size_t pos, cap; bool ok; } wr_t;
typedef struct { const uint8_t *p; size_t pos, len; bool ok; } rd_t;

static void w(wr_t *x, uint64_t v, int bytes)
{
    if (x->pos + (size_t)bytes > x->cap) { x->ok = false; return; }
    for (int i = 0; i < bytes; i++) x->p[x->pos++] = (uint8_t)(v >> (8 * i));
}
static uint64_t r(rd_t *x, int bytes)
{
    uint64_t v = 0;
    if (x->pos + (size_t)bytes > x->len) { x->ok = false; return 0; }
    for (int i = bytes - 1; i >= 0; i--) v = v << 8 | x->p[x->pos + (size_t)i];
    x->pos += (size_t)bytes;
    return v;
}

/* Campos de v1 que v3 ya no usa: se leen para migrar. */
typedef struct {
    uint16_t gp;
    uint16_t exo_eaten;
    uint8_t  pantry_n;
    uint8_t  pantry[V1_PANTRY];
} legacy_t;

static void put_v1(wr_t *x, const pet_state_t *s, int version)
{
    for (int i = 0; i < PET_KEY_LEN; i++) w(x, s->key[i], 1);
    w(x, s->rng, 8);
    w(x, s->view_seed, 4);
    w(x, s->trait_curiosity, 1); w(x, s->trait_calm, 1); w(x, s->trait_glutton, 1);
    w(x, (uint64_t)s->born_s, 4); w(x, (uint64_t)s->hatch_s, 4); w(x, (uint64_t)s->last_s, 4);
    w(x, s->stage, 1);
    /* v1/v2 no conocen las ramas sabio y forzudo */
    w(x, version < 3 && s->branch > BRANCH_AGILE ? BRANCH_AGILE : s->branch, 1);
    w(x, s->variant, 1);
    w(x, s->egg_active_s, 2);
    w(x, (uint64_t)s->satiety, 4); w(x, (uint64_t)s->energy, 4); w(x, (uint64_t)s->fun, 4);
    w(x, s->xp / V1_GP_XP > 65535 ? 65535 : s->xp / V1_GP_XP, 2);   /* PC */
    w(x, 0, 2); w(x, 0, 4); w(x, s->visit_day, 4);                    /* PC del dia, dia de PC, visita */
    w(x, 0, 2); w(x, 0, 2); w(x, 0, 2); w(x, 0, 2);                   /* contadores de dieta */
    w(x, s->care_errors, 1); w(x, s->juv_errors, 1); w(x, s->need_low_s, 2);
    w(x, s->mood_sum, 4); w(x, s->mood_n, 4);
    w(x, (uint64_t)s->last_action_s, 4); w(x, (uint64_t)s->last_novel_s, 4);
    w(x, (uint64_t)s->last_urban_s, 4); w(x, (uint64_t)s->last_fam_epoch_s, 4);
    w(x, (uint64_t)s->last_scan_s, 4);
    w(x, s->sig_day, 4); w(x, s->sig_today, 1);
    /* despensa v1: pan como domesticos y chuches como exoticos (solo la leen v1/v2) */
    int nb = s->inv[ITEM_BREAD], nt = s->inv[ITEM_TREAT];
    if (version >= 3) nb = nt = 0;
    if (nb > V1_PANTRY) nb = V1_PANTRY;
    if (nt > V1_PANTRY - nb) nt = V1_PANTRY - nb;
    w(x, (uint64_t)(nb + nt), 1);
    for (int i = 0; i < V1_PANTRY; i++) {
        w(x, i < nb ? V1_DOMESTIC : (i < nb + nt ? V1_EXOTIC : 0), 1);
        w(x, i < nb + nt ? (uint64_t)s->last_s : 0, 4);
    }
    w(x, s->familiar_n, 1); w(x, s->recent_n, 1);
    for (int i = 0; i < PET_FAM_MAX; i++) { w(x, s->familiar[i].hash, 4); w(x, s->familiar[i].score4, 1); }
    for (int i = 0; i < PET_FAM_MAX; i++) { w(x, s->recent[i].hash, 4); w(x, s->recent[i].score4, 1); }
}

static void put_v2(wr_t *x, const pet_state_t *s)
{
    w(x, s->night_start_h, 1); w(x, s->night_end_h, 1); w(x, s->volume > 0, 1);   /* v2: sonido sí/no */
    w(x, s->total_scans, 4);
}

static void put_v3(wr_t *x, const pet_state_t *s)
{
    w(x, (uint64_t)s->adult_s, 4);
    w(x, (uint64_t)s->hygiene, 4);
    w(x, s->coins, 4); w(x, s->xp, 4); w(x, s->level, 1);
    for (int i = 0; i < GAME_COUNT; i++) w(x, s->train[i], 2);
    for (int i = 0; i < ITEM_COUNT; i++) w(x, s->inv[i], 1);
    w(x, s->poops, 1); w(x, (uint16_t)s->gut, 2); w(x, (uint64_t)s->last_poop_s, 4); w(x, s->dirty_s, 2);
    w(x, s->sick, 1); w(x, (uint64_t)s->sick_since_s, 4); w(x, (uint64_t)s->nap_until_s, 4);
    w(x, s->acc_owned, 2); w(x, s->acc_worn, 1);
    w(x, s->wish_kind, 1); w(x, s->wish_arg, 1); w(x, s->wish_prog, 1);
    w(x, (uint64_t)s->wish_until_s, 4); w(x, (uint64_t)s->next_wish_s, 4); w(x, s->wishes_done, 2);
    w(x, s->signals, 4); w(x, s->sig_milestones, 1);
    w(x, s->streak, 2); w(x, s->cake_day, 4); w(x, s->cake_today, 1);
    for (int i = 0; i < GAME_COUNT; i++) { w(x, s->games[i], 2); w(x, s->best[i], 2); }
    w(x, s->generation, 1); w(x, s->album, 2); w(x, s->tasted, 1);
}

static void put_v4(wr_t *x, const pet_state_t *s) { w(x, s->volume, 1); }

static uint32_t t32(rd_t *x) { return (uint32_t)r(x, 4); }

static void get_v1(rd_t *x, pet_state_t *s, legacy_t *lg)
{
    for (int i = 0; i < PET_KEY_LEN; i++) s->key[i] = (uint8_t)r(x, 1);
    s->rng = r(x, 8);
    s->view_seed = t32(x);
    s->trait_curiosity = (uint8_t)r(x, 1); s->trait_calm = (uint8_t)r(x, 1); s->trait_glutton = (uint8_t)r(x, 1);
    s->born_s = t32(x); s->hatch_s = t32(x); s->last_s = t32(x);
    s->stage = (uint8_t)r(x, 1); s->branch = (uint8_t)r(x, 1); s->variant = (uint8_t)r(x, 1);
    s->egg_active_s = (uint16_t)r(x, 2);
    s->satiety = (int32_t)t32(x); s->energy = (int32_t)t32(x); s->fun = (int32_t)t32(x);
    lg->gp = (uint16_t)r(x, 2); r(x, 2); r(x, 4); s->visit_day = t32(x);
    lg->exo_eaten = (uint16_t)r(x, 2); r(x, 2); r(x, 2); r(x, 2);
    s->care_errors = (uint8_t)r(x, 1); s->juv_errors = (uint8_t)r(x, 1); s->need_low_s = (uint16_t)r(x, 2);
    s->mood_sum = t32(x); s->mood_n = t32(x);
    s->last_action_s = t32(x); s->last_novel_s = t32(x); s->last_urban_s = t32(x);
    s->last_fam_epoch_s = t32(x); s->last_scan_s = t32(x);
    s->sig_day = t32(x); s->sig_today = (uint8_t)r(x, 1);
    lg->pantry_n = (uint8_t)r(x, 1);
    for (int i = 0; i < V1_PANTRY; i++) { lg->pantry[i] = (uint8_t)r(x, 1); t32(x); }
    s->familiar_n = (uint8_t)r(x, 1); s->recent_n = (uint8_t)r(x, 1);
    for (int i = 0; i < PET_FAM_MAX; i++) { s->familiar[i].hash = t32(x); s->familiar[i].score4 = (uint8_t)r(x, 1); }
    for (int i = 0; i < PET_FAM_MAX; i++) { s->recent[i].hash = t32(x); s->recent[i].score4 = (uint8_t)r(x, 1); }
}

static void get_v2(rd_t *x, pet_state_t *s)
{
    s->night_start_h = (uint8_t)r(x, 1); s->night_end_h = (uint8_t)r(x, 1); r(x, 1);   /* sonido sí/no: v4 lo sustituye por volume */
    s->total_scans = t32(x);
}

static void get_v3(rd_t *x, pet_state_t *s)
{
    s->adult_s = t32(x);
    s->hygiene = (int32_t)t32(x);
    s->coins = t32(x); s->xp = t32(x); s->level = (uint8_t)r(x, 1);
    for (int i = 0; i < GAME_COUNT; i++) s->train[i] = (uint16_t)r(x, 2);
    for (int i = 0; i < ITEM_COUNT; i++) s->inv[i] = (uint8_t)r(x, 1);
    s->poops = (uint8_t)r(x, 1); s->gut = (int16_t)r(x, 2); s->last_poop_s = t32(x); s->dirty_s = (uint16_t)r(x, 2);
    s->sick = (uint8_t)r(x, 1); s->sick_since_s = t32(x); s->nap_until_s = t32(x);
    s->acc_owned = (uint16_t)r(x, 2); s->acc_worn = (uint8_t)r(x, 1);
    s->wish_kind = (uint8_t)r(x, 1); s->wish_arg = (uint8_t)r(x, 1); s->wish_prog = (uint8_t)r(x, 1);
    s->wish_until_s = t32(x); s->next_wish_s = t32(x); s->wishes_done = (uint16_t)r(x, 2);
    s->signals = t32(x); s->sig_milestones = (uint8_t)r(x, 1);
    s->streak = (uint16_t)r(x, 2); s->cake_day = t32(x); s->cake_today = (uint8_t)r(x, 1);
    for (int i = 0; i < GAME_COUNT; i++) { s->games[i] = (uint16_t)r(x, 2); s->best[i] = (uint16_t)r(x, 2); }
    s->generation = (uint8_t)r(x, 1); s->album = (uint16_t)r(x, 2); s->tasted = (uint8_t)r(x, 1);
}

static void get_v4(rd_t *x, pet_state_t *s) { s->volume = (uint8_t)r(x, 1); }

static uint8_t level_for(uint32_t xp)
{
    uint8_t l = 1;
    while (l < 99 && xp >= 20u * (uint32_t)(l + 1) * l) l++;
    return l;
}

/* v1/v2 -> v3 */
static void migrate(pet_state_t *s, const legacy_t *lg)
{
    s->hygiene = PET_MAX;
    s->coins = 20;
    s->xp = (uint32_t)lg->gp * V1_GP_XP;
    s->level = level_for(s->xp);
    s->signals = lg->exo_eaten;
    s->last_poop_s = s->last_s;
    for (int i = 0; i < lg->pantry_n && i < V1_PANTRY; i++) {
        pet_item_t it = lg->pantry[i] == V1_DOMESTIC ? ITEM_BREAD : ITEM_TREAT;
        if (s->inv[it] < 99) s->inv[it]++;
    }
    if (s->stage == PET_ADULT) {
        s->adult_s = s->last_s;
        if (s->branch >= BRANCH_HOMEBODY && s->branch <= BRANCH_AGILE)
            s->album |= (uint16_t)(1u << ((s->branch - 1) * 4 + s->variant));
    }
}

size_t pet_save_version(const pet_state_t *s, uint32_t seq, int version, uint8_t *buf, size_t cap)
{
    wr_t x = { buf, HDR, cap, cap >= HDR };
    if (!x.ok || version < 1 || version > PET_SAVE_VERSION) return 0;
    put_v1(&x, s, version);
    if (version >= 2) put_v2(&x, s);
    if (version >= 3) put_v3(&x, s);
    if (version >= 4) put_v4(&x, s);
    if (!x.ok) return 0;
    size_t len = x.pos - HDR;
    wr_t h = { buf, 0, HDR, true };
    w(&h, MAGIC, 4); w(&h, (uint64_t)version, 2); w(&h, len, 2); w(&h, seq, 4);
    w(&h, pet_crc32(buf + HDR, len), 4);
    return x.pos;
}

size_t pet_save(const pet_state_t *s, uint32_t seq, uint8_t *buf, size_t cap)
{
    return pet_save_version(s, seq, PET_SAVE_VERSION, buf, cap);
}

static bool stat_ok(int32_t v) { return v >= 0 && v <= PET_MAX; }

static bool valid(const pet_state_t *s)
{
    if (s->stage >= PET_STAGE_COUNT || s->branch >= BRANCH_COUNT || s->variant > VAR_ODD) return false;
    if (!stat_ok(s->satiety) || !stat_ok(s->energy) || !stat_ok(s->fun) || !stat_ok(s->hygiene)) return false;
    if (s->familiar_n > PET_FAM_MAX || s->recent_n > PET_FAM_MAX) return false;
    if (s->night_start_h > 23 || s->night_end_h > 23) return false;
    if (s->trait_curiosity > 9 || s->trait_calm > 9 || s->trait_glutton > 9) return false;
    if (s->level < 1 || s->level > 99 || s->poops > PET_POOP_MAX || s->acc_worn >= ACC_COUNT) return false;
    if (s->wish_kind >= WISH_COUNT || s->sick > 1 || s->volume > PET_VOL_MAX) return false;
    return s->rng != 0;
}

pet_load_t pet_load(pet_state_t *s, const uint8_t *buf, size_t len, uint32_t *seq_out)
{
    if (len < HDR) return PET_LOAD_BAD;
    rd_t h = { buf, 0, HDR, true };
    uint32_t magic = (uint32_t)r(&h, 4);
    unsigned ver = (unsigned)r(&h, 2), plen = (unsigned)r(&h, 2);
    uint32_t seq = (uint32_t)r(&h, 4), crc = (uint32_t)r(&h, 4);
    if (magic != MAGIC || ver < 1 || len < HDR + plen) return PET_LOAD_BAD;
    if (pet_crc32(buf + HDR, plen) != crc) return PET_LOAD_BAD;
    if (ver > PET_SAVE_VERSION) return PET_LOAD_NEWER;

    pet_state_t t;
    legacy_t lg;
    memset(&t, 0, sizeof(t));
    memset(&lg, 0, sizeof(lg));
    t.night_start_h = 23; t.night_end_h = 8; t.volume = PET_VOL_DEFAULT;       /* valores por defecto de campos nuevos */
    t.level = 1;
    rd_t x = { buf + HDR, 0, plen, true };
    get_v1(&x, &t, &lg);
    if (ver >= 2) get_v2(&x, &t);
    if (ver >= 3) get_v3(&x, &t);
    else migrate(&t, &lg);
    if (ver >= 4) get_v4(&x, &t);
    if (!x.ok || lg.pantry_n > V1_PANTRY || !valid(&t)) return PET_LOAD_BAD;
    t.started = true;
    *s = t;
    if (seq_out) *seq_out = seq;
    return PET_LOAD_OK;
}

static bool peek_seq(const uint8_t *b, size_t l, uint32_t *seq)
{
    pet_state_t tmp;
    return b && pet_load(&tmp, b, l, seq) == PET_LOAD_OK;
}

int pet_load_ab(pet_state_t *s, const uint8_t *a, size_t la, const uint8_t *b, size_t lb)
{
    uint32_t sa = 0, sb = 0;
    bool oka = peek_seq(a, la, &sa), okb = peek_seq(b, lb, &sb);
    if (!oka && !okb) return -1;
    bool use_b = okb && (!oka || (int32_t)(sb - sa) > 0);
    pet_load(s, use_b ? b : a, use_b ? lb : la, NULL);
    return use_b ? 1 : 0;
}
