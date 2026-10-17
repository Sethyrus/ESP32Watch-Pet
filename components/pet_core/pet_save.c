/* Partida versionada: cabecera {magic, version, len, seq, crc32} + carga.
 * La carga solo crece por el final; los campos de una version nueva van despues
 * de los de la anterior y una carga mas corta se completa con valores por defecto. */
#include "pet_core.h"
#include <string.h>

#define MAGIC 0x53544550u   /* "PETS" */
#define HDR   16

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

static void put_v1(wr_t *x, const pet_state_t *s)
{
    for (int i = 0; i < PET_KEY_LEN; i++) w(x, s->key[i], 1);
    w(x, s->rng, 8);
    w(x, s->view_seed, 4);
    w(x, s->trait_curiosity, 1); w(x, s->trait_calm, 1); w(x, s->trait_glutton, 1);
    w(x, (uint64_t)s->born_s, 4); w(x, (uint64_t)s->hatch_s, 4); w(x, (uint64_t)s->last_s, 4);
    w(x, s->stage, 1); w(x, s->branch, 1); w(x, s->variant, 1);
    w(x, s->egg_active_s, 2);
    w(x, (uint64_t)s->satiety, 4); w(x, (uint64_t)s->energy, 4); w(x, (uint64_t)s->mood, 4);
    w(x, s->gp, 2); w(x, s->gp_food_today, 2); w(x, s->gp_day, 4); w(x, s->visit_day, 4);
    w(x, s->exo_eaten, 2); w(x, s->dom_eaten, 2); w(x, s->manual_eaten, 2); w(x, s->total_eaten, 2);
    w(x, s->care_errors, 1); w(x, s->juv_errors, 1); w(x, s->need_low_s, 2);
    w(x, s->mood_sum, 4); w(x, s->mood_n, 4);
    w(x, (uint64_t)s->last_action_s, 4); w(x, (uint64_t)s->last_novel_s, 4);
    w(x, (uint64_t)s->last_urban_s, 4); w(x, (uint64_t)s->last_fam_epoch_s, 4);
    w(x, (uint64_t)s->last_scan_s, 4);
    w(x, s->exo_day, 4); w(x, s->exo_today, 1);
    w(x, s->pantry_n, 1);
    for (int i = 0; i < PET_PANTRY_MAX; i++) {
        w(x, s->pantry[i].type, 1); w(x, (uint64_t)s->pantry[i].born_s, 4);
    }
    w(x, s->familiar_n, 1); w(x, s->recent_n, 1);
    for (int i = 0; i < PET_FAM_MAX; i++) { w(x, s->familiar[i].hash, 4); w(x, s->familiar[i].score4, 1); }
    for (int i = 0; i < PET_FAM_MAX; i++) { w(x, s->recent[i].hash, 4); w(x, s->recent[i].score4, 1); }
}

static void put_v2(wr_t *x, const pet_state_t *s)
{
    w(x, s->night_start_h, 1); w(x, s->night_end_h, 1); w(x, s->sound_on, 1);
    w(x, s->total_scans, 4);
}

static uint32_t t32(rd_t *x) { return (uint32_t)r(x, 4); }

static void get_v1(rd_t *x, pet_state_t *s)
{
    for (int i = 0; i < PET_KEY_LEN; i++) s->key[i] = (uint8_t)r(x, 1);
    s->rng = r(x, 8);
    s->view_seed = t32(x);
    s->trait_curiosity = (uint8_t)r(x, 1); s->trait_calm = (uint8_t)r(x, 1); s->trait_glutton = (uint8_t)r(x, 1);
    s->born_s = t32(x); s->hatch_s = t32(x); s->last_s = t32(x);
    s->stage = (uint8_t)r(x, 1); s->branch = (uint8_t)r(x, 1); s->variant = (uint8_t)r(x, 1);
    s->egg_active_s = (uint16_t)r(x, 2);
    s->satiety = (int32_t)t32(x); s->energy = (int32_t)t32(x); s->mood = (int32_t)t32(x);
    s->gp = (uint16_t)r(x, 2); s->gp_food_today = (uint16_t)r(x, 2); s->gp_day = t32(x); s->visit_day = t32(x);
    s->exo_eaten = (uint16_t)r(x, 2); s->dom_eaten = (uint16_t)r(x, 2);
    s->manual_eaten = (uint16_t)r(x, 2); s->total_eaten = (uint16_t)r(x, 2);
    s->care_errors = (uint8_t)r(x, 1); s->juv_errors = (uint8_t)r(x, 1); s->need_low_s = (uint16_t)r(x, 2);
    s->mood_sum = t32(x); s->mood_n = t32(x);
    s->last_action_s = t32(x); s->last_novel_s = t32(x); s->last_urban_s = t32(x);
    s->last_fam_epoch_s = t32(x); s->last_scan_s = t32(x);
    s->exo_day = t32(x); s->exo_today = (uint8_t)r(x, 1);
    s->pantry_n = (uint8_t)r(x, 1);
    for (int i = 0; i < PET_PANTRY_MAX; i++) { s->pantry[i].type = (uint8_t)r(x, 1); s->pantry[i].born_s = t32(x); }
    s->familiar_n = (uint8_t)r(x, 1); s->recent_n = (uint8_t)r(x, 1);
    for (int i = 0; i < PET_FAM_MAX; i++) { s->familiar[i].hash = t32(x); s->familiar[i].score4 = (uint8_t)r(x, 1); }
    for (int i = 0; i < PET_FAM_MAX; i++) { s->recent[i].hash = t32(x); s->recent[i].score4 = (uint8_t)r(x, 1); }
}

static void get_v2(rd_t *x, pet_state_t *s)
{
    s->night_start_h = (uint8_t)r(x, 1); s->night_end_h = (uint8_t)r(x, 1); s->sound_on = (uint8_t)r(x, 1);
    s->total_scans = t32(x);
}

size_t pet_save_version(const pet_state_t *s, uint32_t seq, int version, uint8_t *buf, size_t cap)
{
    wr_t x = { buf, HDR, cap, cap >= HDR };
    if (!x.ok) return 0;
    put_v1(&x, s);
    if (version >= 2) put_v2(&x, s);
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

static bool valid(const pet_state_t *s)
{
    if (s->stage >= PET_STAGE_COUNT || s->branch > BRANCH_EXPLORER || s->variant > VAR_ODD) return false;
    if (s->satiety < 0 || s->satiety > PET_MAX || s->energy < 0 || s->energy > PET_MAX ||
        s->mood < 0 || s->mood > PET_MAX) return false;
    if (s->pantry_n > PET_PANTRY_MAX || s->familiar_n > PET_FAM_MAX || s->recent_n > PET_FAM_MAX) return false;
    for (int i = 0; i < s->pantry_n; i++) if (s->pantry[i].type >= FOOD_COUNT) return false;
    if (s->night_start_h > 23 || s->night_end_h > 23) return false;
    if (s->trait_curiosity > 9 || s->trait_calm > 9 || s->trait_glutton > 9) return false;
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
    memset(&t, 0, sizeof(t));
    t.night_start_h = 23; t.night_end_h = 8; t.sound_on = 1;       /* valores por defecto de campos nuevos */
    rd_t x = { buf + HDR, 0, plen, true };
    get_v1(&x, &t);
    if (ver >= 2) get_v2(&x, &t);
    if (!x.ok || !valid(&t)) return PET_LOAD_BAD;
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
