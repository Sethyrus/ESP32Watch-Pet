/* pet_scene: compone la escena principal en RGB565 (C99 puro): suelo, criatura,
 * cacas, burbuja de lo que pide y efectos (corazones, pompas, destellos, noche). */
#pragma once
#include "pet_gfx.h"

#define PET_SCENE_W     400
#define PET_SCENE_H     280
#define PET_SCENE_SCALE 5
#define PET_SCENE_FLOOR 266                       /* y del suelo */
#define PET_SCENE_XMAX  (PET_SCENE_W - PET_W * PET_SCENE_SCALE)   /* x maxima de la criatura */

typedef enum { FX_NONE, FX_HEARTS, FX_BUBBLES, FX_SPARKLE, FX_COINS } pet_fx_t;

typedef struct {
    pet_view_t view;
    int        x;               /* esquina izquierda del lienzo de la criatura (0..PET_SCENE_XMAX) */
    int        hop;             /* saltito al andar, px hacia arriba */
    uint8_t    poops;
    int        bubble;          /* pet_icon_t o -1 */
    bool       bubble_wish;     /* borde dorado (deseo) o rojo (necesidad) */
    bool       night;
    bool       dark;            /* luz apagada: todo atenuado */
    pet_fx_t   fx;
    uint32_t   fx_ms;           /* tiempo desde que empezo el efecto */
    int        fx_x, fx_y;      /* punto del efecto (destello) */
    uint32_t   t_ms;            /* reloj para animar burbuja y estrellas */
} pet_scene_t;

void pet_scene_draw(const pet_scene_t *sc, uint16_t *dst);   /* PET_SCENE_W x PET_SCENE_H */
uint32_t pet_scene_fx_ms(pet_fx_t fx);                       /* duracion del efecto */

/* Posicion (esquina superior izquierda) de la caca i y su caja de toque. */
void pet_scene_poop_rect(int i, int *x, int *y, int *w, int *h);
int  pet_scene_poop_at(const pet_scene_t *sc, int x, int y);  /* indice o -1 */
bool pet_scene_bubble_at(const pet_scene_t *sc, int x, int y);
bool pet_scene_pet_at(const pet_scene_t *sc, int x, int y);
void pet_scene_bubble_rect(const pet_scene_t *sc, int *x, int *y, int *w, int *h);
