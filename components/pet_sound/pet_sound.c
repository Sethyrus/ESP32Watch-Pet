/* Sintetizador de efectos (ver pet_sound.h). Cada efecto es una lista de pasos por voz:
 * onda, volumen, glissando y duración. Los de la criatura y las burbujas se generan al
 * reproducirlos con un poco de azar, para que no suenen siempre igual. */
#include "pet_sound.h"
#include <string.h>

enum { W_REST, W_SQ, W_TRI, W_NOISE };

#define AMP      7000      /* amplitud de una voz a volumen 255 */
#define ATTACK   40        /* muestras (2,5 ms): sin clics al empezar */
#define RELEASE  120       /* muestras (7,5 ms) */
#define LFO_INC  (uint32_t)(7.0 * 4294967296.0 / SND_RATE)   /* vibrato a 7 Hz */

/* notas (Hz) */
enum {
    E4 = 330, G4 = 392, A4 = 440, C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880,
    B5 = 988, C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568, A6 = 1760, C7 = 2093
};

static const uint16_t SEMI_Q8[13] = { 256, 271, 287, 304, 323, 342, 362, 384, 406, 431, 456, 483, 512 };

static uint32_t rnd(snd_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x ? x : 0x9e3779b9u;
}

/* entero en [lo, hi] */
static int rr(snd_t *s, int lo, int hi) { return lo + (int)(rnd(s) % (uint32_t)(hi - lo + 1)); }

typedef struct { snd_step_t st[SND_STEPS]; int n; } prog_t;

static void add(prog_t *p, int wave, int vol, int duty, int f0, int f1, int ms, int vib)
{
    if (p->n >= SND_STEPS) return;
    if (f0 < 20) f0 = 20;
    if (f1 < 20) f1 = 20;
    if (f0 > 7000) f0 = 7000;
    if (f1 > 7000) f1 = 7000;
    p->st[p->n++] = (snd_step_t){ (uint8_t)wave, (uint8_t)vol, (uint8_t)duty, (uint8_t)vib,
                                  (uint16_t)f0, (uint16_t)f1, (uint16_t)ms };
}

static void rest(prog_t *p, int ms) { add(p, W_REST, 0, 0, 100, 100, ms, 0); }
static void note(prog_t *p, int duty, int f, int ms, int vol) { add(p, W_SQ, vol, duty, f, f, ms, 0); }

/* ---------- voz ---------- */

/* sílaba de la criatura: de a a b (en milésimas de la voz base) */
static void syl(snd_t *s, prog_t *p, int a, int b, int ms, int vol, int vib)
{
    const int j = rr(s, 960, 1040);   /* ±4 % cada sílaba */
    const int f0 = (int)((uint32_t)s->voice_hz * (uint32_t)a / 1000u) * j / 1000;
    const int f1 = (int)((uint32_t)s->voice_hz * (uint32_t)b / 1000u) * j / 1000;
    add(p, s->voice_wave, vol, s->voice_duty, f0, f1, ms, vib);
}

static void voice(snd_t *s, snd_id_t id, prog_t *p)
{
    switch (id) {
    case SND_V_HELLO:
        syl(s, p, 1000, 1300, 70, 200, 0); rest(p, 35);
        syl(s, p, 1100, 1600, 120, 210, 0);
        break;
    case SND_V_HAPPY: {
        const int n = rr(s, 2, 3);
        for (int i = 0; i < n; i++) {
            const int a = rr(s, 1000, 1300);
            syl(s, p, a, a * rr(s, 1150, 1450) / 1000, rr(s, 45, 80), 200, 0);
            rest(p, rr(s, 25, 45));
        }
        break;
    }
    case SND_V_ASK:
        syl(s, p, 1000, 1000, 50, 180, 0); rest(p, 40);
        syl(s, p, 950, 1750, 170, 210, 0);
        break;
    case SND_V_SAD:
        syl(s, p, 1100, 750, 230, 170, 0); rest(p, 50);
        syl(s, p, 900, 600, 300, 140, 10);
        break;
    case SND_V_LAUGH:
        for (int i = 0; i < 5; i++) {
            const int a = 1450 - i * 50;
            syl(s, p, a, a - 200, rr(s, 30, 40), 190, 0);
            rest(p, rr(s, 25, 35));
        }
        break;
    case SND_V_LOVE:
        syl(s, p, 1000, 1400, 100, 200, 0); rest(p, 20);
        syl(s, p, 1400, 1250, 260, 190, 25);
        break;
    case SND_V_YAWN:
        add(p, W_TRI, 230, 0, s->voice_hz * 9 / 10, s->voice_hz * 13 / 10, 260, 0);
        add(p, W_TRI, 210, 0, s->voice_hz * 13 / 10, s->voice_hz * 6 / 10, 480, 8);
        break;
    case SND_V_DIZZY:
        for (int i = 0; i < 6; i++) syl(s, p, i & 1 ? 800 : 1250, i & 1 ? 1250 : 800, 70, 170, 30);
        break;
    default: break;
    }
}

/* ---------- efectos ---------- */

static void build(snd_t *s, snd_id_t id, prog_t *a, prog_t *b)
{
    if (id <= SND_V_DIZZY) { voice(s, id, a); return; }
    switch (id) {
    case SND_EAT:   /* tres mordiscos */
        for (int i = 0; i < 3; i++) {
            const int f = rr(s, 2600, 3600);
            add(a, W_NOISE, 170, 0, f, f / 2, rr(s, 40, 55), 0);
            rest(a, rr(s, 70, 100));
        }
        break;
    case SND_POOP:  /* plof */
        add(a, W_TRI, 240, 0, 320, 110, 170, 0);
        add(b, W_NOISE, 90, 0, 900, 300, 60, 0);
        break;
    case SND_CLEAN: /* destello */
        note(a, 32, E6, 40, 130); note(a, 32, G6, 40, 130); note(a, 32, C7, 100, 120);
        break;
    case SND_WASH:  /* burbujas: cada una a su tono y a su ritmo */
        for (int i = 0; i < 7; i++) {
            const int f = rr(s, 550, 1400);
            add(i & 1 ? b : a, W_TRI, rr(s, 150, 230), 0, f, f * 3 / 2, rr(s, 30, 50), 0);
            rest(i & 1 ? b : a, rr(s, 40, 110));
        }
        break;
    case SND_COIN:  note(a, 128, B5, 60, 190); note(a, 128, E6, 200, 170); break;
    case SND_BUY:
        add(a, W_NOISE, 120, 0, 5000, 4000, 30, 0);
        note(a, 128, C6, 50, 170); note(a, 128, E6, 60, 170); note(a, 128, G6, 160, 150);
        break;
    case SND_CURE:  add(a, W_TRI, 230, 0, 500, 1300, 320, 12); note(a, 64, C6, 120, 140); break;
    case SND_DENY:  note(a, 128, G4, 90, 170); rest(a, 35); note(a, 128, E4, 170, 170); break;
    case SND_LEVEL:
        note(a, 64, C5, 70, 190); note(a, 64, E5, 70, 190); note(a, 64, G5, 70, 190);
        add(a, W_SQ, 190, 64, C6, C6, 260, 10);
        rest(b, 210); add(b, W_TRI, 220, 0, G4, G4, 260, 0);
        break;
    case SND_EVOLVE:
        note(a, 64, G4, 100, 190); note(a, 64, C5, 100, 190); note(a, 64, E5, 100, 190);
        note(a, 64, G5, 100, 190); rest(a, 50); note(a, 64, E5, 80, 180); note(a, 64, G5, 80, 180);
        add(a, W_SQ, 190, 64, C6, C6, 450, 12);
        rest(b, 400); add(b, W_TRI, 230, 0, C5, C5, 160, 0); add(b, W_TRI, 230, 0, E5, E5, 160, 0);
        add(b, W_TRI, 230, 0, G5, G5, 450, 0);
        break;
    case SND_CRACK: add(a, W_NOISE, 200, 0, 3000, 1200, 45, 0); break;
    case SND_WISH_DONE:
        note(a, 64, C6, 60, 170); note(a, 64, G5, 60, 170); note(a, 64, C6, 60, 170);
        add(a, W_SQ, 170, 64, E6, E6, 220, 8);
        break;
    case SND_CATCH: note(a, 64, A5, 25, 150); note(a, 64, E6, 45, 150); break;
    case SND_STAR:
        note(a, 32, C6, 30, 150); note(a, 32, E6, 30, 150); note(a, 32, G6, 30, 150);
        note(a, 32, C7, 90, 140);
        break;
    case SND_ZAP:
        add(a, W_NOISE, 200, 0, 1800, 300, 200, 0);
        add(b, W_SQ, 160, 128, 220, 70, 200, 0);
        break;
    case SND_PAD0: add(a, W_SQ, 190, 64, C5, C5, 230, 0); add(b, W_TRI, 150, 0, C5, C5, 230, 0); break;
    case SND_PAD1: add(a, W_SQ, 190, 64, E5, E5, 230, 0); add(b, W_TRI, 150, 0, E5, E5, 230, 0); break;
    case SND_PAD2: add(a, W_SQ, 190, 64, G5, G5, 230, 0); add(b, W_TRI, 150, 0, G5, G5, 230, 0); break;
    case SND_PAD3: add(a, W_SQ, 190, 64, C6, C6, 230, 0); add(b, W_TRI, 150, 0, C6, C6, 230, 0); break;
    case SND_GOOD:  note(a, 64, G5, 60, 170); note(a, 64, C6, 130, 170); break;
    case SND_FAIL:  add(a, W_SQ, 200, 128, 330, 110, 420, 20); break;
    case SND_TICK:  note(a, 32, 900, 18, 110); break;
    case SND_LIFTED:
        note(a, 64, C5, 50, 180); note(a, 64, E5, 50, 180); note(a, 64, G5, 50, 180);
        note(a, 64, C6, 160, 170);
        break;
    case SND_DROP:  add(a, W_NOISE, 220, 0, 600, 100, 260, 0); add(b, W_TRI, 230, 0, 160, 60, 220, 0); break;
    case SND_END:   note(a, 64, E5, 90, 170); note(a, 64, C5, 90, 170); note(a, 64, G5, 220, 170); break;
    case SND_RECORD:
        note(a, 64, C6, 80, 180); note(a, 64, C6, 80, 180); note(a, 64, C6, 80, 180);
        add(a, W_SQ, 180, 64, G6, G6, 320, 12);
        add(b, W_TRI, 220, 0, C5, C5, 240, 0); add(b, W_TRI, 220, 0, E5, E5, 320, 0);
        break;
    default: break;
    }
}

/* ---------- mezcla ---------- */

void snd_init(snd_t *s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->rng = seed ? seed : 1;
    snd_set_species(s, 1, 0, seed);
}

void snd_set_species(snd_t *s, int stage, int branch, uint32_t seed)
{
    /* cría aguda y redonda; el adulto baja según su rama */
    static const uint16_t BASE[4] = { 1150, 1150, 880, 700 };
    static const uint16_t ADULT[5] = { 700, 700, 820, 640, 470 };   /* -, Hogareño, Saltarín, Sabio, Forzudo */
    static const uint8_t WAVE[5] = { W_SQ, W_SQ, W_SQ, W_TRI, W_SQ };
    static const uint8_t DUTY[5] = { 128, 128, 40, 0, 110 };
    if (stage < 0 || stage > 3) stage = 1;
    if (branch < 0 || branch > 4) branch = 0;
    int hz = stage == 3 ? ADULT[branch] : BASE[stage];
    hz = hz * (900 + (int)((seed >> 5) % 201u)) / 1000;   /* ±10 % por individuo */
    s->voice_hz = (uint16_t)hz;
    s->voice_wave = stage == 3 ? WAVE[branch] : W_SQ;
    s->voice_duty = stage == 3 ? DUTY[branch] : (stage == 2 ? 64 : 128);
}

static void start(snd_t *s, const prog_t *p, uint16_t q8)
{
    if (p->n == 0) return;
    snd_chan_t *c = NULL;
    for (int i = 0; i < SND_VOICES; i++) {
        if (s->ch[i].n == 0) { c = &s->ch[i]; break; }
    }
    if (!c) {   /* todas ocupadas: la más antigua */
        c = &s->ch[0];
        for (int i = 1; i < SND_VOICES; i++) if (s->ch[i].age < c->age) c = &s->ch[i];
    }
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < p->n; i++) {
        c->st[i] = p->st[i];
        c->st[i].f0 = (uint16_t)((uint32_t)p->st[i].f0 * q8 / 256u);
        c->st[i].f1 = (uint16_t)((uint32_t)p->st[i].f1 * q8 / 256u);
    }
    c->n = (uint8_t)p->n;
    c->len = (uint32_t)c->st[0].ms * SND_RATE / 1000u;
    if (c->len == 0) c->len = 1;
    c->lfsr = 0x4a3d;
    c->age = ++s->age;
}

void snd_play_pitch(snd_t *s, snd_id_t id, int semitones)
{
    if (id >= SND_COUNT) return;
    if (semitones < 0) semitones = 0;
    if (semitones > 12) semitones = 12;
    prog_t a = { .n = 0 }, b = { .n = 0 };
    build(s, id, &a, &b);
    start(s, &a, SEMI_Q8[semitones]);
    start(s, &b, SEMI_Q8[semitones]);
}

void snd_play(snd_t *s, snd_id_t id) { snd_play_pitch(s, id, 0); }

void snd_stop(snd_t *s)
{
    for (int i = 0; i < SND_VOICES; i++) s->ch[i].n = 0;
}

bool snd_busy(const snd_t *s)
{
    for (int i = 0; i < SND_VOICES; i++) if (s->ch[i].n) return true;
    return false;
}

static int32_t chan_sample(snd_chan_t *c)
{
    const snd_step_t *st = &c->st[c->i];
    int32_t v = 0;
    if (st->wave != W_REST && c->len) {
        int32_t f = st->f0 + (int32_t)(((int64_t)(st->f1 - st->f0) * (int64_t)c->pos) / (int64_t)c->len);
        if (st->vib) {
            /* LFO triangular: ±vib/1000 de la frecuencia */
            const int32_t l = (int32_t)(c->lfo >> 16);   /* 0..65535 */
            const int32_t tri = l < 32768 ? l * 2 - 32768 : 98303 - l * 2;   /* -32768..32767 */
            f += (int32_t)((int64_t)f * st->vib * tri / (1000LL * 32768));
            c->lfo += LFO_INC;
        }
        const uint32_t inc = (uint32_t)(((uint64_t)f << 32) / SND_RATE);
        const uint32_t prev = c->phase;
        c->phase += inc;
        int32_t w;
        switch (st->wave) {
        case W_SQ: w = (c->phase >> 24) < st->duty ? 32767 : -32767; break;
        case W_TRI: {
            const int32_t p = (int32_t)(c->phase >> 16);
            w = p < 32768 ? p * 2 - 32767 : 98303 - p * 2;
            break;
        }
        default:   /* ruido: un valor nuevo por ciclo */
            if (c->phase < prev) {
                const uint16_t bit = (uint16_t)((c->lfsr ^ (c->lfsr >> 1)) & 1u);
                c->lfsr = (uint16_t)((c->lfsr >> 1) | (bit << 14));
                c->noise = (c->lfsr & 1) ? 32767 : -32767;
            }
            w = c->noise;
            break;
        }
        int32_t env = 256;
        if (c->pos < ATTACK) env = (int32_t)(c->pos * 256 / ATTACK);
        const uint32_t left = c->len - c->pos;
        if (left < RELEASE && (int32_t)(left * 256 / RELEASE) < env) env = (int32_t)(left * 256 / RELEASE);
        v = (int32_t)((int64_t)w * AMP / 32767 * st->vol / 255 * env / 256);
    }
    if (++c->pos >= c->len) {
        c->pos = 0;
        if (++c->i >= c->n) {
            c->n = 0;
        } else {
            c->len = (uint32_t)c->st[c->i].ms * SND_RATE / 1000u;
            if (c->len == 0) c->len = 1;
        }
    }
    return v;
}

int snd_render(snd_t *s, int16_t *out, int n)
{
    for (int k = 0; k < n; k++) {
        int32_t mix = 0;
        for (int i = 0; i < SND_VOICES; i++) {
            if (s->ch[i].n) mix += chan_sample(&s->ch[i]);
        }
        /* paso bajo de un polo (~4 kHz): suaviza el aliasing de la cuadrada en un altavoz tan pequeño */
        s->lp += (mix - s->lp) * 3 / 4;
        int32_t v = s->lp;
        if (v > 32767) v = 32767;
        if (v < -32767) v = -32767;
        out[k] = (int16_t)v;
    }
    int busy = 0;
    for (int i = 0; i < SND_VOICES; i++) busy += s->ch[i].n != 0;
    return busy;
}

const char *snd_name(snd_id_t id)
{
    static const char *const N[SND_COUNT] = {
        "v_hello", "v_happy", "v_ask", "v_sad", "v_laugh", "v_love", "v_yawn", "v_dizzy",
        "eat", "poop", "clean", "wash", "coin", "buy", "cure", "deny", "level", "evolve", "crack", "wish_done",
        "catch", "star", "zap", "pad0", "pad1", "pad2", "pad3", "good", "fail", "tick", "lifted", "drop",
        "end", "record",
    };
    return id < SND_COUNT ? N[id] : "?";
}
