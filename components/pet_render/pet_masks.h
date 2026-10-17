/* Datos de mascaras (internos de pet_render). Medias mascaras: se espejan.
 * Codigos: . vacio, B cuerpo, b cuerpo segun semilla, O borde forzado,
 * E ojo (esquina sup-izq del ojo izquierdo), M boca (fila; centrada),
 * F ancla de orejas (sobre el cuerpo), G ancla central (antena). */
#pragma once

typedef struct {
    const char *name;
    int w_half;                 /* ancho de la media mascara */
    int h;
    const char *const *rows;    /* h filas de w_half caracteres */
} pet_mask_t;

/* Pieza (media pieza izquierda); ancla = esquina inferior derecha.
 * B cuerpo, a acento. */
typedef struct { int w, h; const char *const *rows; } pet_piece_t;

extern const pet_mask_t *const PET_MASKS[4];
extern const int PET_MASK_COUNT[4];
extern const pet_piece_t PET_EARS[3];
extern const pet_piece_t PET_EARS_L[3];  /* juvenil y adulto */
