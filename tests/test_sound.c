/* Tests del sintetizador (pet_sound): todos los efectos suenan, se oyen, no saturan, terminan
 * y son deterministas. `make test`. Para escucharlos: tools/sim/build/sim sounds <dir>. */
#include "pet_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define MAX_S (3 * SND_RATE)

/* Reproduce un efecto hasta que calla. Devuelve las muestras; pico y suma por puntero. */
static int run(snd_t *s, snd_id_t id, int semis, int *peak, uint32_t *hash)
{
    static int16_t buf[256];
    int n = 0, pk = 0;
    uint32_t h = 2166136261u;
    snd_play_pitch(s, id, semis);
    while (snd_busy(s) && n < MAX_S) {
        snd_render(s, buf, 256);
        for (int i = 0; i < 256; i++) {
            const int a = abs(buf[i]);
            if (a > pk) pk = a;
            h = (h ^ (uint16_t)buf[i]) * 16777619u;
        }
        n += 256;
    }
    if (peak) *peak = pk;
    if (hash) *hash = h;
    return n;
}

int main(void)
{
    printf("sonido: efectos\n");
    for (int stage = 0; stage < 4; stage++) {
        for (int branch = 0; branch < 5; branch++) {
            if (stage < 3 && branch) continue;
            snd_t s;
            snd_init(&s, 1234 + (uint32_t)branch);
            snd_set_species(&s, stage, branch, 77);
            for (int id = 0; id < SND_COUNT; id++) {
                int peak;
                const int n = run(&s, (snd_id_t)id, 0, &peak, NULL);
                CHECK(n > 0 && n < MAX_S, "%s termina (%d muestras)", snd_name((snd_id_t)id), n);
                CHECK(n <= 2 * SND_RATE, "%s dura <= 2 s (%d ms)", snd_name((snd_id_t)id), n * 1000 / SND_RATE);
                CHECK(peak > 1500, "%s se oye (pico %d)", snd_name((snd_id_t)id), peak);
                CHECK(peak < 32767, "%s no satura (pico %d)", snd_name((snd_id_t)id), peak);
                CHECK(!snd_busy(&s), "%s calla al terminar", snd_name((snd_id_t)id));
            }
        }
    }
    /* cuatro a la vez (el peor caso de mezcla) */
    snd_t s;
    snd_init(&s, 5);
    snd_play(&s, SND_EVOLVE); snd_play(&s, SND_ZAP); int pk;
    run(&s, SND_LEVEL, 0, &pk, NULL);
    CHECK(pk < 32767, "mezcla de 4 voces sin saturar (pico %d)", pk);
    /* determinista con la misma semilla; la voz varía entre reproducciones */
    snd_t a, b;
    snd_init(&a, 42); snd_init(&b, 42);
    uint32_t ha, hb, ha2;
    run(&a, SND_V_HAPPY, 0, NULL, &ha); run(&b, SND_V_HAPPY, 0, NULL, &hb);
    CHECK(ha == hb, "misma semilla, mismo sonido");
    run(&a, SND_V_HAPPY, 0, NULL, &ha2);
    CHECK(ha2 != ha, "la voz cambia un poco cada vez");
    /* el tono sube con snd_play_pitch */
    snd_init(&a, 1); snd_init(&b, 1);
    run(&a, SND_CATCH, 0, NULL, &ha); run(&b, SND_CATCH, 7, NULL, &hb);
    CHECK(ha != hb, "snd_play_pitch cambia el tono");
    /* especies con voces distintas */
    snd_init(&a, 1);
    snd_set_species(&a, 3, 4, 9);
    const uint16_t strong = a.voice_hz;
    snd_set_species(&a, 1, 0, 9);
    CHECK(a.voice_hz > strong, "la cría es más aguda que el Forzudo (%u > %u)", a.voice_hz, strong);
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
