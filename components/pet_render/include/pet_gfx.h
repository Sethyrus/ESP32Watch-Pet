/* pet_gfx: dibujo en RGB565 para la escena y los minijuegos (C99 puro).
 * Iconos de pixel art de 12x12, texto pixelado 3x5 y la criatura con transparencia. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pet_render.h"

typedef struct { uint16_t *px; int w, h; } pet_gfx_t;

typedef enum {
    ICON_BREAD, ICON_FRUIT, ICON_STEW, ICON_CAKE, ICON_TREAT, ICON_MEDICINE,   /* = pet_item_t */
    ICON_POOP, ICON_COIN, ICON_HEART, ICON_STAR, ICON_BOLT, ICON_SPIKY, ICON_BALL,
    ICON_SOAP, ICON_MOON, ICON_MENU, ICON_FOOD, ICON_LOCK, ICON_SIGNAL, ICON_DUMBBELL,
    ICON_GRID, ICON_BOOK, ICON_COUNT
} pet_icon_t;
#define PET_ICON 12

static inline uint16_t pet_rgb565(uint32_t c)
{
    return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

void pet_gfx_fill(pet_gfx_t *g, int x, int y, int w, int h, uint16_t c);
void pet_gfx_rrect(pet_gfx_t *g, int x, int y, int w, int h, int r, uint16_t c);   /* relleno, esquinas redondas */
void pet_gfx_icon(pet_gfx_t *g, pet_icon_t icon, int x, int y, int scale);         /* fondo transparente */
/* La criatura (lienzo de PET_W x PET_H) con su esquina superior izquierda en x,y. */
void pet_gfx_creature(pet_gfx_t *g, const pet_view_t *v, int x, int y, int scale);
/* Texto pixelado 3x5 (mayusculas, digitos y + - : . ! ? / %). Devuelve el ancho. */
int  pet_gfx_text(pet_gfx_t *g, const char *s, int x, int y, int scale, uint16_t c);
int  pet_gfx_text_width(const char *s, int scale);

/* Icono opaco sobre un color de fondo, para imagenes LVGL: dst de (12*scale)^2. */
void pet_icon_rgb565(pet_icon_t icon, int scale, uint16_t bg, uint16_t *dst);
/* Color de un pixel del icono (0 = transparente) en RGB888, para las hojas del simulador. */
uint32_t pet_icon_pixel(pet_icon_t icon, int x, int y);
