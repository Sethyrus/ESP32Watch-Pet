/* pet_game: minijuegos (C99 puro, deterministas). La logica avanza con
 * pet_game_update(dt, entrada) y se dibuja en RGB565 con pet_game_render.
 * El resultado (rendimiento 0..100 y puntuacion) se entrega a pet_core con
 * pet_game_finish(). Diseno: docs/PET_DESIGN.md, seccion 5. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pet_core.h"
#include "pet_scene.h"

#define PET_GAME_W 400
#define PET_GAME_H 280
#define PET_GAME_MAX_MS 45000          /* ninguna partida dura mas (B10) */

typedef struct {
    bool    down;                      /* dedo apoyado */
    int16_t x, y;                      /* coordenadas en el lienzo del juego */
} pet_input_t;

typedef struct {
    int16_t x, y;                      /* centro */
    uint8_t kind;                      /* 0 libre; ver pet_game.c */
    uint8_t icon;
} pet_drop_t;

typedef struct {
    uint8_t  id;                       /* pet_game_id_t */
    uint32_t rng;
    uint32_t t_ms;
    bool     over, prev_down;
    int      score;
    pet_view_t pet;
    /* atrapa */
    int      px;                       /* centro de la criatura */
    pet_drop_t drops[16];
    uint32_t next_spawn, stun_until, flash_until;
    int8_t   flash;                    /* +1 bien, -1 mal */
    /* memoria */
    uint8_t  seq[12], len, pos, phase;
    uint32_t phase_t;
    int8_t   lit;
    uint32_t lit_until;
    /* pesas: una flecha va y viene por el medidor; hay que tocar con ella en la zona */
    int      fill;                     /* posicion de la flecha 0..1000 */
    int8_t   dir;
    int16_t  zone, zone_w;             /* centro y media anchura de la zona buena */
    uint8_t  weight, lives, perfects;
    bool     last_perfect;
    uint32_t lift_t;
    /* comun */
    uint8_t  level;                    /* nivel de la criatura (lo pone la UI tras start): sube la dificultad */
    pet_parts_t parts;                 /* chispas y estrellas del juego */
    /* sucesos para sonidos y efectos (pet_game_take_events) */
    uint16_t events;
    int8_t   ev_pad;                   /* tecla de Memoria que acaba de encenderse */
    uint8_t  combo;                    /* Atrapa: cosas seguidas sin fallar */
} pet_game_t;

enum {
    GEV_CATCH = 1 << 0, GEV_STAR = 1 << 1, GEV_ZAP = 1 << 2, GEV_PAD = 1 << 3, GEV_GOOD = 1 << 4,
    GEV_FAIL = 1 << 5, GEV_TICK = 1 << 6, GEV_LIFTED = 1 << 7, GEV_DROP = 1 << 8, GEV_WIN = 1 << 9,
    GEV_PERFECT = 1 << 10
};

void pet_game_start(pet_game_t *g, pet_game_id_t id, uint32_t seed, const pet_view_t *pet);
void pet_game_update(pet_game_t *g, uint32_t dt_ms, const pet_input_t *in);
bool pet_game_over(const pet_game_t *g);
int  pet_game_score(const pet_game_t *g);
int  pet_game_perf(const pet_game_t *g);             /* 0..100 */
int  pet_game_seconds_left(const pet_game_t *g);     /* -1 si no hay reloj */
const char *pet_game_hint(const pet_game_t *g);      /* texto corto para la UI */
void pet_game_render(const pet_game_t *g, uint16_t *dst);   /* PET_GAME_W x PET_GAME_H */
const char *pet_game_name(pet_game_id_t id);
uint16_t pet_game_take_events(pet_game_t *g);          /* GEV_* desde la última llamada */

/* Para los bots de los tests: lo que tocaria un jugador perfecto ahora. */
bool pet_game_bot_input(const pet_game_t *g, pet_input_t *out);
