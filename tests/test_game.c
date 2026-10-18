/* Tests de los minijuegos (B10): un bot perfecto, uno regular y uno al azar. `make test` */
#include "pet_game.h"
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef enum { BOT_PERFECT, BOT_SLOPPY, BOT_RANDOM } bot_t;

static uint32_t lcg(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return *r >> 8; }

/* Juega una partida a 30 fps. Devuelve el rendimiento; ms y score por puntero. */
static int play(pet_game_id_t id, uint32_t seed, bot_t bot, uint32_t *ms, int *score)
{
    pet_game_t g;
    pet_view_t v = { .seed = seed, .stage = PET_BABY };
    pet_game_start(&g, id, seed, &v);
    uint32_t r = seed * 7 + 1;
    pet_input_t in = {0};
    uint16_t fb[PET_GAME_W * PET_GAME_H];
    int frames = 0;
    while (!pet_game_over(&g) && frames < 3000) {
        pet_input_t want;
        pet_game_bot_input(&g, &want);
        switch (bot) {
        case BOT_PERFECT: in = want; break;
        case BOT_SLOPPY:                       /* reacciona tarde y se equivoca a veces */
            if (lcg(&r) % 100 < 70) in = want;
            if (id == GAME_MEMORY && want.down && lcg(&r) % 100 < 8) { in.x = (int16_t)(lcg(&r) % 400); in.y = (int16_t)(lcg(&r) % 280); }
            if (id == GAME_LIFT) in.down = (frames % 6) < 3;   /* 5 toques/s */
            break;
        default:                               /* toca en sitios al azar, 2 veces por segundo */
            in.down = (frames % 15) < 3;
            if (frames % 15 == 0) { in.x = (int16_t)(lcg(&r) % 400); in.y = (int16_t)(lcg(&r) % 280); }
            break;
        }
        pet_game_update(&g, 33, &in);
        if (frames % 97 == 0) pet_game_render(&g, fb);   /* el dibujo no debe romper nada */
        frames++;
    }
    *ms = g.t_ms;
    *score = pet_game_score(&g);
    return pet_game_perf(&g);
}

int main(void)
{
    static const char *const BN[3] = { "perfecto", "regular", "al azar" };
    for (int id = 0; id < GAME_COUNT; id++) {
        for (int b = 0; b < 3; b++) {
            int lo = 101, hi = -1, sum = 0;
            uint32_t maxms = 0;
            for (uint32_t s = 1; s <= 200; s++) {
                uint32_t ms; int sc;
                int p = play((pet_game_id_t)id, s, (bot_t)b, &ms, &sc);
                if (p < lo) lo = p;
                if (p > hi) hi = p;
                sum += p;
                if (ms > maxms) maxms = ms;
                CHECK(ms <= PET_GAME_MAX_MS, "%s dura %u ms", pet_game_name((pet_game_id_t)id), ms);
            }
            printf("%-8s bot %-8s rendimiento %3d-%3d (media %3d), maximo %u ms\n", pet_game_name((pet_game_id_t)id), BN[b], lo, hi, sum / 200, maxms);
            if (b == BOT_PERFECT) CHECK(sum / 200 >= 90 && lo >= 60, "%s: bot perfecto >= 90 de media (%d, min %d)", pet_game_name((pet_game_id_t)id), sum / 200, lo);
            if (b == BOT_RANDOM) CHECK(sum / 200 <= 30, "%s: bot al azar <= 30 de media (%d)", pet_game_name((pet_game_id_t)id), sum / 200);
        }
    }
    /* determinismo */
    uint32_t m1, m2; int s1, s2;
    play(GAME_CATCH, 42, BOT_SLOPPY, &m1, &s1);
    play(GAME_CATCH, 42, BOT_SLOPPY, &m2, &s2);
    CHECK(m1 == m2 && s1 == s2, "misma semilla, misma partida");
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails != 0;
}
