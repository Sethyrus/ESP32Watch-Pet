/* pet_core: C99 puro (sin ESP-IDF, LVGL ni SDL). Reglas, tiempo y partida de la
 * mascota. Determinista: el azar vive en el estado, el tiempo entra por parametro.
 * Tiempo: segundos de hora local (RTC, sin zona); las funciones de evento que
 * necesitan subsegundos reciben ademas un reloj monotono en ms. Estadisticas en
 * punto fijo (PET_ONE = 1 punto). Sin float. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pet_render.h"

#define PET_ONE          10000          /* 1 punto de estadistica */
#define PET_MAX          (100 * PET_ONE)
#define PET_PANTRY_MAX   20
#define PET_FAM_MAX      32             /* familiares y recientes: 32 + 32 */
#define PET_KEY_LEN      16
#define PET_NEVER        INT64_MAX

typedef enum { FOOD_DOMESTIC, FOOD_EXOTIC, FOOD_URBAN, FOOD_COUNT } pet_food_t;
typedef enum { BRANCH_NONE, BRANCH_HOMEBODY, BRANCH_EXPLORER } pet_branch_t;

typedef enum {
    MOOD_ASLEEP, MOOD_HUNGRY, MOOD_TIRED, MOOD_SAD, MOOD_BORED,
    MOOD_CURIOUS, MOOD_HAPPY, MOOD_CALM
} pet_mood_t;

typedef enum {
    BEH_REST, BEH_CURIOUS, BEH_PLAY, BEH_ATTENTION, BEH_LOOK, BEH_SLEEP, BEH_COUNT
} pet_behavior_t;

typedef enum {
    REACT_NONE, REACT_PET, REACT_DIZZY, REACT_SHAKE, REACT_USB_IN, REACT_USB_OUT
} pet_reaction_t;

/* Eventos para la UI (mascara de bits; se leen y limpian con pet_take_events). */
enum {
    EVT_HATCHED = 1, EVT_EVOLVED = 2, EVT_PECKED = 4,
    EVT_NEW_SIGNAL = 8, EVT_ERROR = 16
};

/* Un AP tal como sale de la radio. El BSSID se hashea dentro y se descarta. */
typedef struct {
    uint8_t bssid[6];
    int8_t  rssi;
    bool    open;
    bool    hidden;
} pet_ap_t;

typedef struct { uint8_t type; int64_t born_s; } pet_bite_t;
typedef struct { uint32_t hash; uint8_t score4; } pet_fam_t;   /* puntos x4 */

typedef struct {
    /* ---- persistente ---- */
    uint8_t  key[PET_KEY_LEN];
    uint64_t rng;
    uint32_t view_seed;
    uint8_t  trait_curiosity, trait_calm, trait_glutton;     /* 0..9 */
    int64_t  born_s, hatch_s, last_s;
    uint8_t  stage, branch, variant;
    uint16_t egg_active_s;
    int32_t  satiety, energy, mood;                           /* PET_ONE */
    uint16_t gp, gp_food_today;
    uint32_t gp_day, visit_day;
    uint16_t exo_eaten, dom_eaten, manual_eaten, total_eaten;
    uint8_t  care_errors, juv_errors;
    uint16_t need_low_s;                                      /* segundos con necesidad <= 5 */
    uint32_t mood_sum, mood_n;                                /* media de animo (puntos x minuto) */
    int64_t  last_action_s, last_novel_s, last_urban_s, last_fam_epoch_s, last_scan_s;
    uint32_t exo_day;
    uint8_t  exo_today;
    uint8_t  pantry_n;
    pet_bite_t pantry[PET_PANTRY_MAX];
    pet_fam_t familiar[PET_FAM_MAX], recent[PET_FAM_MAX];
    uint8_t  familiar_n, recent_n;
    uint8_t  night_start_h, night_end_h;
    uint8_t  sound_on;
    uint32_t total_scans;                                     /* v2 */

    /* ---- solo en memoria (no se guarda) ---- */
    int64_t  step_rem_s;               /* segundos activos aun sin integrar */
    uint32_t events;
    int64_t  novel_until_s;
    int64_t  last_peck_s;
    uint32_t touch_ms[4];
    uint32_t shake_ms[4];
    uint8_t  touch_cnt, shake_cnt;
    uint32_t touch_hour, touch_gain;
    uint32_t react_until_ms;
    uint8_t  react_kind;
    uint8_t  hist[5];                  /* ultimos comportamientos */
    uint8_t  behavior;
    bool     started;                  /* hay marca de tiempo valida */
} pet_state_t;

/* ---- ciclo de vida ---- */
void pet_init(pet_state_t *s, const uint8_t key[PET_KEY_LEN], int64_t now_s);
/* Tiempo activo. Si el hueco desde la ultima llamada es > 5 min se trata como ausencia. */
void pet_step(pet_state_t *s, int64_t now_s);
/* Hueco ausente en un solo paso (tope 7 dias). Reloj que retrocede: no hace nada. */
void pet_catch_up(pet_state_t *s, int64_t now_s);
/* El RTC perdio la hora: reancla la marca sin simular. */
void pet_clock_lost(pet_state_t *s, int64_t now_s);

/* ---- comida ---- */
/* Devuelve los bocados anadidos a la despensa. Ignora el escaneo de noche. */
int  pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s);
int  pet_pantry_count(const pet_state_t *s, pet_food_t type);
bool pet_feed(pet_state_t *s, pet_food_t type, int64_t now_s);          /* manual */
/* Segundos hasta caducar el bocado mas proximo de ese tipo (-1 si no hay). */
int64_t pet_pantry_expires_in(const pet_state_t *s, pet_food_t type, int64_t now_s);

/* ---- interaccion ---- */
pet_reaction_t pet_touch(pet_state_t *s, int64_t now_s, uint32_t mono_ms);
pet_reaction_t pet_shake(pet_state_t *s, int64_t now_s, uint32_t mono_ms);
pet_reaction_t pet_usb_changed(pet_state_t *s, bool plugged, uint32_t mono_ms);
pet_behavior_t pet_decide(pet_state_t *s, int64_t now_s);              /* cada 8-15 s con la escena visible */
uint32_t       pet_take_events(pet_state_t *s);

/* ---- consulta ---- */
bool        pet_is_night(const pet_state_t *s, int64_t now_s);
pet_mood_t  pet_mood(const pet_state_t *s, int64_t now_s);
int64_t     pet_age_s(const pet_state_t *s, int64_t now_s);
int         pet_diet_axis_permille(const pet_state_t *s);              /* exoticos / (exoticos + domesticos) */
/* Planificador de escaneo. desk: modo mesa. battery_pct < 0: desconocida. */
int64_t     pet_next_scan_s(const pet_state_t *s, int64_t now_s, bool desk, int battery_pct);
/* Rellena la vista (etapa, forma, variante, expresion, semilla) y devuelve la
 * animacion recomendada; el llamante aplica pet_pose(view, anim, t_ms). */
pet_anim_t  pet_view(const pet_state_t *s, int64_t now_s, uint32_t mono_ms, pet_view_t *out);

/* ---- persistencia ---- */
#define PET_SAVE_VERSION 2
#define PET_SAVE_MAX     1024
typedef enum { PET_LOAD_OK, PET_LOAD_BAD, PET_LOAD_NEWER } pet_load_t;
/* Serializa en buf (cabecera + carga). version: PET_SAVE_VERSION (v1 solo para tests de migracion). */
size_t     pet_save(const pet_state_t *s, uint32_t seq, uint8_t *buf, size_t cap);
size_t     pet_save_version(const pet_state_t *s, uint32_t seq, int version, uint8_t *buf, size_t cap);
pet_load_t pet_load(pet_state_t *s, const uint8_t *buf, size_t len, uint32_t *seq_out);
/* Elige entre las copias A y B: la de seq mayor con CRC valido. Devuelve 0 (A), 1 (B) o -1. */
int        pet_load_ab(pet_state_t *s, const uint8_t *a, size_t la, const uint8_t *b, size_t lb);

/* ---- utilidades expuestas (tests) ---- */
uint64_t pet_siphash24(const uint8_t key[16], const uint8_t *in, size_t len);
uint32_t pet_crc32(const uint8_t *p, size_t n);
