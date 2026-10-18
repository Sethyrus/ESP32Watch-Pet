/* pet_core: C99 puro (sin ESP-IDF, LVGL ni SDL). Reglas, tiempo y partida de la
 * mascota (diseno v2, docs/PET_DESIGN.md). Determinista: el azar vive en el estado,
 * el tiempo entra por parametro.
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
#define PET_EGG_HATCH_S  120            /* segundos activos hasta eclosionar */
#define PET_FAM_MAX      32             /* familiares y recientes: 32 + 32 */
#define PET_KEY_LEN      16
#define PET_NEVER        INT64_MAX
#define PET_POOP_MAX     4
#define PET_NAP_S        1800

/* Inventario: los cuatro primeros son la carta (se compran al comer). */
typedef enum { ITEM_BREAD, ITEM_FRUIT, ITEM_STEW, ITEM_CAKE, ITEM_TREAT, ITEM_MEDICINE, ITEM_COUNT } pet_item_t;
#define PET_MENU_COUNT 4

typedef enum { BRANCH_NONE, BRANCH_HOMEBODY, BRANCH_AGILE, BRANCH_WISE, BRANCH_STRONG, BRANCH_COUNT } pet_branch_t;
typedef enum { GAME_CATCH, GAME_MEMORY, GAME_LIFT, GAME_COUNT } pet_game_id_t;
typedef enum { NEED_FOOD, NEED_FUN, NEED_HYGIENE, NEED_ENERGY, NEED_COUNT } pet_need_t;

typedef enum {
    MOOD_ASLEEP, MOOD_SICK, MOOD_HUNGRY, MOOD_DIRTY, MOOD_TIRED, MOOD_BORED, MOOD_HAPPY, MOOD_CALM
} pet_mood_t;

/* Lo que pide la burbuja, por prioridad. */
typedef enum { WANT_NONE, WANT_MEDICINE, WANT_FOOD, WANT_CLEAN, WANT_SLEEP, WANT_PLAY, WANT_WISH } pet_want_t;
typedef enum { WISH_NONE, WISH_FOOD, WISH_GAME, WISH_PET, WISH_CLEAN, WISH_SIGNAL, WISH_COUNT } pet_wish_t;

typedef enum {
    BEH_REST, BEH_WALK, BEH_PLAY, BEH_ATTENTION, BEH_LOOK, BEH_SLEEP, BEH_COUNT
} pet_behavior_t;

typedef enum {
    REACT_NONE, REACT_PET, REACT_DIZZY, REACT_SHAKE, REACT_USB_IN, REACT_USB_OUT, REACT_LOVE
} pet_reaction_t;

/* Eventos para la UI (mascara de bits; se leen y limpian con pet_take_events). */
enum {
    EVT_HATCHED = 1, EVT_EVOLVED = 2, EVT_POOP = 4, EVT_NEW_SIGNAL = 8, EVT_ERROR = 16,
    EVT_SICK = 32, EVT_CURED = 64, EVT_LEVEL = 128, EVT_WISH_NEW = 256, EVT_WISH_DONE = 512,
    EVT_MILESTONE = 1024, EVT_DAILY = 2048, EVT_WOKE = 4096, EVT_TREAT = 8192
};

/* Un AP tal como sale de la radio. El BSSID se hashea dentro y se descarta. */
typedef struct {
    uint8_t bssid[6];
    int8_t  rssi;
    bool    open;
    bool    hidden;
} pet_ap_t;

typedef struct { uint32_t hash; uint8_t score4; } pet_fam_t;   /* puntos x4 */

typedef struct {
    /* ---- persistente ---- */
    uint8_t  key[PET_KEY_LEN];
    uint64_t rng;
    uint32_t view_seed;
    uint8_t  trait_curiosity, trait_calm, trait_glutton;     /* 0..9 */
    int64_t  born_s, hatch_s, last_s, adult_s;
    uint8_t  stage, branch, variant;
    uint16_t egg_active_s;
    int32_t  satiety, energy, fun, hygiene;                   /* PET_ONE */
    uint8_t  care_errors, juv_errors;
    uint16_t need_low_s;                                      /* segundos con una necesidad <= 5 */
    uint32_t mood_sum, mood_n;                                /* media de animo (puntos x minuto) */
    int64_t  last_action_s, last_novel_s, last_urban_s, last_fam_epoch_s, last_scan_s;
    uint32_t sig_day;
    uint8_t  sig_today;
    pet_fam_t familiar[PET_FAM_MAX], recent[PET_FAM_MAX];
    uint8_t  familiar_n, recent_n;
    uint8_t  night_start_h, night_end_h;
    uint8_t  volume;          /* 0 off, 1 low, 2 medium, 3 high */
    uint32_t total_scans;
    /* v3 */
    uint32_t coins, xp;
    uint8_t  level;
    uint16_t train[GAME_COUNT];
    uint8_t  inv[ITEM_COUNT];
    uint8_t  poops;
    int16_t  gut;
    int64_t  last_poop_s;
    uint16_t dirty_s;
    uint8_t  sick;
    int64_t  sick_since_s, nap_until_s;
    uint16_t acc_owned;                                       /* bit = pet_acc_t */
    uint8_t  acc_worn;
    uint8_t  wish_kind, wish_arg, wish_prog;
    int64_t  wish_until_s, next_wish_s;
    uint16_t wishes_done;
    uint32_t signals;
    uint8_t  sig_milestones;
    uint16_t streak;
    uint32_t visit_day, cake_day;
    uint8_t  cake_today;
    uint16_t games[GAME_COUNT], best[GAME_COUNT];
    uint8_t  generation;
    uint16_t album;                                           /* bit (rama-1)*4 + variante */
    uint8_t  tasted;                                          /* bit = pet_item_t */

    /* ---- solo en memoria (no se guarda) ---- */
    int64_t  step_rem_s;               /* segundos activos aun sin integrar */
    uint32_t events;
    int32_t  gain_coins, gain_xp;      /* para los rotulos "+8"; se leen con pet_take_gains */
    int64_t  novel_until_s;
    uint32_t touch_ms[4];
    uint32_t shake_ms[4];
    uint8_t  touch_cnt, shake_cnt;
    uint32_t touch_hour, touch_gain, touch_xp;
    uint32_t react_until_ms;
    uint8_t  react_kind;
    uint8_t  hist[5];                  /* ultimos comportamientos */
    uint8_t  behavior;
    uint8_t  last_milestone;           /* indice del ultimo hito alcanzado */
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
/* Adulto con >= 3 dias: huevo nuevo; conserva monedas, accesorios, senales y album. */
bool pet_can_new_generation(const pet_state_t *s, int64_t now_s);
bool pet_new_generation(pet_state_t *s, int64_t now_s);

/* ---- comida ---- */
typedef enum { FEED_OK, FEED_EGG, FEED_ASLEEP, FEED_FULL, FEED_NO_COINS, FEED_NONE, FEED_NOT_SICK } pet_feed_t;
int        pet_item_price(pet_item_t item);                  /* 0 = no se vende */
/* Efecto de un plato: comida, diversion, xp (sin favorito ni malito). */
void       pet_item_effect(pet_item_t item, int *food, int *fun, int *xp);
pet_item_t pet_favorite(const pet_state_t *s);
bool pet_bread_free(const pet_state_t *s); // no coins for the cheapest dish: bread is free
/* Usa el inventario o, si no hay, compra (si se vende y hay monedas). */
pet_feed_t pet_feed(pet_state_t *s, pet_item_t item, int64_t now_s);

/* ---- aseo y sueno ---- */
bool pet_clean_poop(pet_state_t *s, int64_t now_s);          /* una caca */
bool pet_wash(pet_state_t *s, int64_t now_s);                /* todas las cacas + bano */
bool pet_nap(pet_state_t *s, int64_t now_s);
bool pet_is_napping(const pet_state_t *s, int64_t now_s);
void pet_wake(pet_state_t *s, int64_t now_s);

/* ---- minijuegos ---- */
typedef enum { PLAY_OK, PLAY_EGG, PLAY_ASLEEP, PLAY_TIRED, PLAY_SICK, PLAY_LOCKED } pet_play_t;
typedef struct { int coins, xp, fun, train; bool record; } pet_reward_t;
int          pet_game_level(pet_game_id_t g);                /* nivel que lo desbloquea */
pet_play_t   pet_can_play(const pet_state_t *s, pet_game_id_t g, int64_t now_s);
/* perf 0..100; score es la puntuacion del juego (para el record). */
pet_reward_t pet_game_finish(pet_state_t *s, pet_game_id_t g, int perf, int score, int64_t now_s);

/* ---- tienda y armario ---- */
typedef enum { BUY_OK, BUY_OWNED, BUY_LOCKED, BUY_NO_COINS, BUY_NOT_SOLD } pet_buy_t;
int       pet_acc_price(pet_acc_t a);                        /* 0 = no se vende (hito de senales) */
int       pet_acc_level(pet_acc_t a);
bool      pet_acc_owned(const pet_state_t *s, pet_acc_t a);
pet_buy_t pet_buy_acc(pet_state_t *s, pet_acc_t a);
bool      pet_wear(pet_state_t *s, pet_acc_t a);              /* ACC_NONE quita */

/* ---- WiFi ---- */
/* Devuelve las senales nuevas del escaneo. Ignora el escaneo de noche y en el huevo. */
int  pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s);
int  pet_milestone_count(void);
int  pet_milestone_signals(int i);
/* Senales para el siguiente hito (-1 si ya estan todos). */
int  pet_next_milestone(const pet_state_t *s);

/* ---- interaccion ---- */
pet_reaction_t pet_touch(pet_state_t *s, int64_t now_s, uint32_t mono_ms);
pet_reaction_t pet_shake(pet_state_t *s, int64_t now_s, uint32_t mono_ms);
pet_reaction_t pet_usb_changed(pet_state_t *s, bool plugged, uint32_t mono_ms);
pet_behavior_t pet_decide(pet_state_t *s, int64_t now_s);              /* cada 6-10 s con la escena visible */
uint32_t       pet_take_events(pet_state_t *s);
void           pet_take_gains(pet_state_t *s, int *coins, int *xp);

/* ---- consulta ---- */
bool        pet_is_night(const pet_state_t *s, int64_t now_s);
bool        pet_is_asleep(const pet_state_t *s, int64_t now_s);       /* noche o siesta */
pet_mood_t  pet_mood(const pet_state_t *s, int64_t now_s);
pet_want_t  pet_want(const pet_state_t *s, int64_t now_s);
int         pet_need(const pet_state_t *s, pet_need_t n);              /* 0..100 */
int         pet_happiness(const pet_state_t *s);                       /* 0..100 */
int64_t     pet_age_s(const pet_state_t *s, int64_t now_s);
/* XP dentro del nivel actual y XP que hacen falta para el siguiente. */
void        pet_level_progress(const pet_state_t *s, int *have, int *need);
/* Requisitos de la siguiente etapa; false si ya es adulto. */
bool        pet_evo_target(const pet_state_t *s, int *level, int64_t *age_s);
/* Rama que saldria si evolucionara ahora (segun el entrenamiento). */
pet_branch_t pet_branch_lean(const pet_state_t *s);
/* Planificador de escaneo. desk: modo mesa. battery_pct < 0: desconocida. */
int64_t     pet_next_scan_s(const pet_state_t *s, int64_t now_s, bool desk, int battery_pct);
/* Rellena la vista (etapa, forma, variante, expresion, semilla, accesorio) y devuelve
 * la animacion recomendada; el llamante aplica pet_pose(view, anim, t_ms). */
pet_anim_t  pet_view(const pet_state_t *s, int64_t now_s, uint32_t mono_ms, pet_view_t *out);

/* ---- persistencia ---- */
#define PET_SAVE_VERSION 4
#define PET_SAVE_MAX     1024
#define PET_VOL_MAX      3
#define PET_VOL_DEFAULT  2
typedef enum { PET_LOAD_OK, PET_LOAD_BAD, PET_LOAD_NEWER } pet_load_t;
/* Serializa en buf (cabecera + carga). version: PET_SAVE_VERSION (anteriores solo para tests de migracion). */
size_t     pet_save(const pet_state_t *s, uint32_t seq, uint8_t *buf, size_t cap);
size_t     pet_save_version(const pet_state_t *s, uint32_t seq, int version, uint8_t *buf, size_t cap);
pet_load_t pet_load(pet_state_t *s, const uint8_t *buf, size_t len, uint32_t *seq_out);
/* Elige entre las copias A y B: la de seq mayor con CRC valido. Devuelve 0 (A), 1 (B) o -1. */
int        pet_load_ab(pet_state_t *s, const uint8_t *a, size_t la, const uint8_t *b, size_t lb);

/* ---- utilidades expuestas (tests) ---- */
uint64_t pet_siphash24(const uint8_t key[16], const uint8_t *in, size_t len);
uint32_t pet_crc32(const uint8_t *p, size_t n);
