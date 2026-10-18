/* pet_render: C99 puro (sin ESP-IDF, LVGL ni SDL). Dibuja la criatura en un
 * lienzo indexado de PET_W x PET_H. Compila igual para PC y firmware. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PET_W 48
#define PET_H 48

/* Indices de la paleta (por criatura; ver pet_palette_rgb565). */
enum {
    PAL_BG = 0,      /* negro AMOLED, transparente */
    PAL_DARK,        /* ojos, sombra profunda */
    PAL_OUTLINE,     /* rampa: contorno */
    PAL_SHADOW,      /*        sombra */
    PAL_BASE,        /*        base */
    PAL_LIGHT,       /*        claro */
    PAL_WHITE,       /* brillos */
    PAL_ACCENT,      /* parte distintiva (antena, manchas) */
    PAL_BLUSH,       /* mejillas */
    PAL_GOLD,        /* fijos para accesorios */
    PAL_RED,
    PAL_INK,
    PAL_GREEN,
    PAL_KHAKI,
    PAL_COUNT
};

enum { VAR_NORMAL, VAR_POLISHED, VAR_CLUMSY, VAR_ODD };

/* Accesorios (uno a la vez, sobre la cabeza o los ojos). */
typedef enum {
    ACC_NONE, ACC_BOW, ACC_FLOWER, ACC_BEANIE, ACC_GLASSES, ACC_TOPHAT, ACC_CROWN,
    ACC_RADAR, ACC_SAFARI, ACC_COUNT
} pet_acc_t;

typedef enum { PET_EGG, PET_BABY, PET_JUVENILE, PET_ADULT, PET_STAGE_COUNT } pet_stage_t;

typedef enum {
    EXPR_NEUTRAL, EXPR_HAPPY, EXPR_SAD, EXPR_SLEEP,
    EXPR_HUNGRY, EXPR_SURPRISE, EXPR_ANGRY, EXPR_LOVE, EXPR_COUNT
} pet_expr_t;

/* Rasgos que fija la semilla y que se conservan toda la vida. */
typedef struct {
    uint8_t family;   /* 0 coral, 1 menta, 2 cielo, 3 niebla */
    uint8_t ear;      /* 0 puntiagudas, 1 redondas, 2 antena */
    uint8_t pattern;  /* 0 liso, 1 barriga, 2 manchas, 3 mejillas */
} pet_traits_t;

/* Lo que necesita el render (lo rellenara pet_core con pet_view()). */
typedef struct {
    uint32_t seed;
    uint8_t stage;      /* pet_stage_t */
    uint8_t shape;      /* silueta dentro de la etapa; adulto: 0 hogareno, 1 saltarin, 2 sabio, 3 forzudo */
    uint8_t expr;       /* pet_expr_t */
    bool    squash;     /* respiracion: fase de contraccion */
    int8_t  dy;         /* desplazamiento vertical (botar) */
    int8_t  look_x;     /* mirada, -1..1 */
    uint8_t variant;    /* 0 normal, 1 pulida, 2 torpe, 3 rara */
    uint8_t mouth_open; /* boca abierta (comer) */
    uint8_t food;       /* bocado en pantalla: 0 nada, 1..4 tamano restante */
    uint8_t zstep;      /* altura de la "z" al dormir (0..3) */
    bool    blink;      /* parpadeo: ojos cerrados sin cambiar la boca */
    uint8_t crack;      /* huevo a punto de eclosionar: 0 entero, 1 grieta, 2 grieta larga */
    uint8_t acc;        /* pet_acc_t */
} pet_view_t;

typedef struct { int w, h; } pet_size_t;

pet_traits_t pet_traits(uint32_t seed);
int          pet_shape_count(pet_stage_t stage);
const char  *pet_shape_name(pet_stage_t stage, int shape);

/* Pinta la criatura en idx (w*h, indexado). Devuelve false si w,h != PET_W,PET_H. */
bool pet_render_creature(const pet_view_t *v, uint8_t *idx, int w, int h);

/* Paleta RGB565 de PAL_COUNT entradas para esta semilla. */
void pet_palette_rgb565(uint32_t seed, uint16_t out[PAL_COUNT]);
void pet_palette_rgb888(uint32_t seed, uint32_t out[PAL_COUNT]);

/* Escala entera por vecino mas cercano a RGB565 (dst_stride en pixeles). */
void pet_blit_scaled_rgb565(const uint8_t *idx, int w, int h, int scale,
                            const uint16_t *pal, uint16_t *dst, int dst_stride);

/* Iconos de comida de 12x12 (tipo 0 casero, 1 lejano, 2 ciudad) a RGB565, ampliados por
 * scale en dst (12*scale)^2; el fondo se rellena con bg (el color del boton donde van). */
#define PET_FOOD_ICON 12
void pet_food_icon_rgb565(int type, int scale, uint16_t bg, uint16_t *dst);

/* Animaciones: transformaciones sobre la vista, cuantizadas a PET_FRAME_MS. */
#define PET_FRAME_MS 80
typedef enum { ANIM_IDLE, ANIM_BOUNCE, ANIM_EAT, ANIM_SLEEP, ANIM_COUNT } pet_anim_t;
uint32_t pet_anim_duration_ms(pet_anim_t a);   /* 0 = ciclica sin fin (idle) */
/* Rellena squash, dy, look_x, blink, mouth_open, food, zstep (y expr en dormir) segun t_ms. */
void pet_pose(pet_view_t *v, pet_anim_t a, uint32_t t_ms);
