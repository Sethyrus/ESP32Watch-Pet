/* pet_room: la habitación de la mascota en pixel art (estética «juguete de bolsillo», R26).
 * Se dibuja en coordenadas de pantalla (PET_ROOM_W x PET_ROOM_H) vista desde una ventana: (ox, oy)
 * es la esquina de pantalla que cae en el (0, 0) del búfer. Así el fondo fijo de la pantalla, la
 * escena y la tira del HUD salen de la misma habitación y casan al píxel. C99 puro. */
#pragma once
#include "pet_gfx.h"

#define PET_ROOM_W      410
#define PET_ROOM_H      502
#define PET_ROOM_FLOOR  330     /* y de pantalla donde empieza el suelo */
#define PET_ROOM_RUG_Y  404     /* centro de la alfombra (pies de la criatura) */

typedef enum { SKY_DAY, SKY_DUSK, SKY_NIGHT } pet_sky_t;

void pet_room_draw(pet_gfx_t *g, int ox, int oy, pet_sky_t sky);

/* Piezas de interfaz del mismo estilo, en coordenadas del búfer. */
void pet_room_button(pet_gfx_t *g, int x, int y, int w, int h);        /* botón de madera biselado */
void pet_room_pips(pet_gfx_t *g, int x, int y, int value, uint32_t rgb); /* 5 pips, value 0..100 */
void pet_room_box(pet_gfx_t *g, int x, int y, int w, int h);           /* ventana de menú de RPG */
void pet_room_dim(uint16_t *px, int n);                                  /* a la mitad de brillo */
void pet_room_night(uint16_t *px, int n);                                /* luz apagada: oscuro y azulado */
