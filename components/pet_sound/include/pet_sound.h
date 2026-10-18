#pragma once

/* Sintetizador de efectos de la mascota: ondas cuadrada, triangular y ruido, al estilo de las
 * consolas de 8 bits, generadas por código (sin ficheros de audio). C99 puro y determinista:
 * el mismo código suena en el reloj (pet_app/pet_audio.c), en tools/uisim (SDL) y se exporta a
 * WAV con `tools/sim/build/sim sounds <dir>`. Mono, 16 bits, SND_RATE muestras por segundo. */

#include <stdbool.h>
#include <stdint.h>

#define SND_RATE   16000
#define SND_VOICES 4
#define SND_STEPS  16

typedef enum {
    /* voz de la criatura: depende de la especie (snd_set_voice) y varía un poco cada vez */
    SND_V_HELLO,   /* saludo al encender la pantalla o al nacer */
    SND_V_HAPPY,   /* caricia, plato rico */
    SND_V_ASK,     /* pide algo (burbuja nueva) */
    SND_V_SAD,     /* malito, triste */
    SND_V_LAUGH,   /* cosquillas */
    SND_V_LOVE,    /* plato favorito, carga USB */
    SND_V_YAWN,    /* sueño, siesta */
    SND_V_DIZZY,   /* mareado de tanto toque */
    /* acciones */
    SND_EAT, SND_POOP, SND_CLEAN, SND_WASH, SND_COIN, SND_BUY, SND_CURE, SND_DENY,
    SND_LEVEL, SND_EVOLVE, SND_CRACK, SND_WISH_DONE,
    /* minijuegos */
    SND_CATCH,     /* coge algo; snd_play_pitch sube con la racha */
    SND_STAR, SND_ZAP,
    SND_PAD0, SND_PAD1, SND_PAD2, SND_PAD3,   /* las cuatro teclas de Memoria */
    SND_GOOD, SND_FAIL,
    SND_TICK,      /* toque en Pesas; el tono sube con la barra */
    SND_LIFTED, SND_DROP,
    SND_END, SND_RECORD,
    SND_COUNT
} snd_id_t;

typedef struct {
    uint8_t wave, vol, duty, vib;   /* vib: profundidad del vibrato (0 = nada) */
    uint16_t f0, f1, ms;            /* glissando lineal de f0 a f1 Hz durante ms */
} snd_step_t;

typedef struct {
    snd_step_t st[SND_STEPS];
    uint8_t n, i;
    uint32_t pos, len, phase, lfo, age;
    uint16_t lfsr;
    int16_t noise;
} snd_chan_t;

typedef struct {
    snd_chan_t ch[SND_VOICES];
    uint32_t rng, age;
    uint16_t voice_hz;
    uint8_t voice_wave, voice_duty;
    int32_t lp;
} snd_t;

void snd_init(snd_t *s, uint32_t seed);
/* Voz de la criatura según etapa y rama de pet_core (PET_EGG.., BRANCH_..) y su semilla. */
void snd_set_species(snd_t *s, int stage, int branch, uint32_t seed);
void snd_play(snd_t *s, snd_id_t id);
void snd_play_pitch(snd_t *s, snd_id_t id, int semitones);   /* 0..12 */
void snd_stop(snd_t *s);
/* Mezcla n muestras en out. Devuelve las voces que siguen sonando. */
int snd_render(snd_t *s, int16_t *out, int n);
bool snd_busy(const snd_t *s);
const char *snd_name(snd_id_t id);
