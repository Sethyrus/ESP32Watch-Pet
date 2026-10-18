/* pet_scene: compone la escena principal en RGB565 (C99 puro): suelo, criatura,
 * cacas, burbuja de lo que pide y efectos (corazones, pompas, destellos, noche). */
#pragma once
#include "pet_gfx.h"

#define PET_SCENE_W     400
#define PET_SCENE_H     280
#define PET_SCENE_SCALE 5
#define PET_SCENE_FLOOR 266                       /* y del suelo */
#define PET_SCENE_XMAX  (PET_SCENE_W - PET_W * PET_SCENE_SCALE)   /* x maxima de la criatura */

/* Efectos: ráfagas de partículas con posición, velocidad, giro, tamaño y vida al azar. */
typedef enum {
    FX_NONE,
    FX_HEARTS,    /* caricia, plato favorito */
    FX_BUBBLES,   /* baño: pompas por todo el cuerpo que suben y explotan */
    FX_SPARKLE,   /* caca limpiada: chispas desde el punto tocado */
    FX_COINS,     /* premio: monedas que saltan, caen y rebotan */
    FX_DUST,      /* polvo al aterrizar */
    FX_CRUMBS,    /* migas al comer */
    FX_ZZZ,       /* una Z que sube (se emite cada poco mientras duerme) */
    FX_STARS,     /* nivel y evolución: estallido de estrellas */
    FX_DIZZY,     /* mareo: estrellas girando sobre la cabeza */
    FX_COUNT
} pet_fx_t;

/* Actividades por su cuenta, según a lo que más juega (su objeto se dibuja en la escena). */
typedef enum { ACT_NONE, ACT_BALL, ACT_LIFT, ACT_READ, ACT_DOZE, ACT_COUNT } pet_act_t;

#define PET_PARTS_MAX 64

typedef struct {
    float x, y, vx, vy, ay;      /* px, px/s, px/s² */
    int16_t age, life;           /* ms; age < 0: aún no ha salido */
    uint8_t kind, size, wob, ph; /* wob: vaivén lateral en px; ph: fase */
    uint16_t color;
} pet_part_t;

typedef struct {
    pet_part_t p[PET_PARTS_MAX];
    uint32_t rng;
} pet_parts_t;

void pet_parts_init(pet_parts_t *ps, uint32_t seed);
/* Lanza un efecto centrado en (x, y), en coordenadas de la escena. */
void pet_parts_emit(pet_parts_t *ps, pet_fx_t fx, int x, int y);
void pet_parts_step(pet_parts_t *ps, uint32_t dt_ms);
bool pet_parts_active(const pet_parts_t *ps);
void pet_parts_draw(const pet_parts_t *ps, pet_gfx_t *g);

typedef struct {
    pet_view_t view;
    int        x;               /* esquina izquierda del lienzo de la criatura (0..PET_SCENE_XMAX) */
    int        hop;             /* saltito al andar, px hacia arriba */
    uint8_t    poops;
    int        bubble;          /* pet_icon_t o -1 */
    bool       bubble_wish;     /* borde dorado (deseo) o rojo (necesidad) */
    bool       night;
    bool       dark;            /* luz apagada: todo atenuado */
    const pet_parts_t *parts;   /* partículas encima de todo (o NULL) */
    const uint16_t *bg;         /* fondo ya dibujado (la habitación) o NULL: negro con suelo */
    uint8_t    act;             /* pet_act_t: lo que está haciendo por su cuenta */
    uint32_t   act_ms;          /* tiempo desde que empezó la actividad */
    uint32_t   t_ms;            /* reloj para animar burbuja y estrellas */
} pet_scene_t;

void pet_scene_draw(const pet_scene_t *sc, uint16_t *dst);   /* PET_SCENE_W x PET_SCENE_H */
/* Centro aproximado de la cabeza de la criatura (origen de corazones, monedas, Z...). */
void pet_scene_head(const pet_scene_t *sc, int *x, int *y);

/* Posicion (esquina superior izquierda) de la caca i y su caja de toque. */
void pet_scene_poop_rect(int i, int *x, int *y, int *w, int *h);
int  pet_scene_poop_at(const pet_scene_t *sc, int x, int y);  /* indice o -1 */
bool pet_scene_bubble_at(const pet_scene_t *sc, int x, int y);
bool pet_scene_pet_at(const pet_scene_t *sc, int x, int y);
void pet_scene_bubble_rect(const pet_scene_t *sc, int *x, int *y, int *w, int *h);
