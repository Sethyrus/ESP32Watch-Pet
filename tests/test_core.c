/* Tests de pet_core (diseno v2): unitarios y criterios de aceptacion B1-B9
 * (PET_DESIGN.md, 14). `make test` o `make asan`. */
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SECTION(n) printf("%s\n", n)
#define NOON (T0 + 12 * 3600)

static pet_state_t fresh(uint32_t seed)
{
    uint8_t key[PET_KEY_LEN];
    pet_state_t s;
    make_key(key, seed);
    pet_init(&s, key, T0);
    return s;
}

/* Cria recien nacida a mediodia, con la marca de tiempo en `now`. */
static pet_state_t baby(uint32_t seed, int64_t now)
{
    pet_state_t s = fresh(seed);
    s.stage = PET_BABY;
    s.born_s = now - 3600;
    s.hatch_s = now - 3600;
    s.last_s = now;
    s.next_wish_s = PET_NEVER;      /* sin deseos salvo que el test los quiera */
    return s;
}

static int coins_of(const pet_state_t *s) { return (int)s->coins; }

static void test_primitives(void)
{
    SECTION("primitivas");
    uint8_t key[16], in[16];
    for (int i = 0; i < 16; i++) key[i] = in[i] = (uint8_t)i;
    CHECK(pet_siphash24(key, in, 0) == 0x726fdb47dd0e0e31ULL, "siphash vacio");
    CHECK(pet_siphash24(key, in, 6) == 0xcbc9466e58fee3ceULL, "siphash 6 bytes");
    CHECK(pet_siphash24(key, in, 15) == 0xa129ca6149be45e5ULL, "siphash 15 bytes");
    CHECK(pet_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u, "crc32");
}

static void test_signals(void)
{
    SECTION("WiFi: senales, familiaridad, hitos");
    pet_state_t s = baby(1, NOON);
    pet_ap_t ap; mk_ap(&ap, 7, false);
    int64_t t = NOON;
    int c0 = coins_of(&s);
    CHECK(pet_ingest_scan(&s, &ap, 1, t) == 1 && s.signals == 1, "primer AP es senal nueva");
    CHECK(coins_of(&s) == c0 + 1, "senal nueva da 1 moneda");
    CHECK(s.recent_n == 1 && s.familiar_n == 0, "entra en recientes");
    CHECK(pet_ingest_scan(&s, &ap, 1, t + 60) == 0 && s.signals == 1, "visto de nuevo no es nuevo");
    for (int i = 1; i <= 4; i++) pet_ingest_scan(&s, &ap, 1, t + i * 1300);
    CHECK(s.familiar_n == 1 && s.recent_n == 0, "promovido a familiar tras 5 avistamientos");
    pet_ap_t other; mk_ap(&other, 99, false);
    uint64_t hh = pet_siphash24(s.key, ap.bssid, 6);
    for (int i = 0; i < 80; i++) pet_ingest_scan(&s, &other, 1, t + (6 + i) * 1300);
    bool still = false;
    for (int i = 0; i < s.familiar_n; i++) still |= s.familiar[i].hash == (uint32_t)(hh ^ (hh >> 32));
    CHECK(!still, "un AP que no se ve se olvida");

    /* tope diario de recompensas, pero el contador de senales sigue */
    pet_state_t d = baby(2, NOON);
    pet_ap_t many[64];
    for (int i = 0; i < 64; i++) mk_ap(&many[i], 1000 + (uint32_t)i, false);
    c0 = coins_of(&d);
    pet_ingest_scan(&d, many, 30, NOON);
    CHECK(d.signals == 30, "cuenta las 30 senales");
    CHECK(coins_of(&d) - c0 == 15 + 25, "monedas: tope de 15 al dia + hito de 10 senales (%d)", coins_of(&d) - c0);
    CHECK(pet_acc_owned(&d, ACC_RADAR), "hito de 30 senales: radar");
    CHECK(pet_next_milestone(&d) == 60, "siguiente hito 60");
    c0 = coins_of(&d);
    pet_ingest_scan(&d, many + 30, 10, NOON + 600);
    CHECK(coins_of(&d) == c0, "sin mas monedas el mismo dia");
    for (int i = 0; i < 20; i++) mk_ap(&many[i], 5000 + (uint32_t)i, false);
    c0 = coins_of(&d);
    pet_ingest_scan(&d, many, 20, NOON + DAY);
    CHECK(coins_of(&d) > c0, "al dia siguiente vuelve a dar");
    CHECK(d.inv[ITEM_TREAT] >= 3, "hito de 60: chuches (%d)", d.inv[ITEM_TREAT]);

    /* muchas redes: chuche con enfriamiento */
    pet_state_t u = baby(4, NOON);
    pet_ap_t net[16]; for (int i = 0; i < 16; i++) mk_ap(&net[i], 700 + (uint32_t)i, false);
    int before = u.inv[ITEM_TREAT];
    pet_ingest_scan(&u, net, 16, NOON);
    int after1 = u.inv[ITEM_TREAT];
    CHECK(after1 >= before + 1, "16 redes dan chuche urbana");
    pet_ingest_scan(&u, net, 16, NOON + 600);             /* mismas redes: sin senales nuevas */
    CHECK(u.inv[ITEM_TREAT] == after1, "urbana con enfriamiento de 30 min");
    /* noche y huevo: se ignora */
    pet_state_t e = fresh(5);
    CHECK(pet_ingest_scan(&e, &ap, 1, NOON) == 0 && e.signals == 0, "el huevo no olfatea");
    pet_state_t n = baby(6, T0 + 2 * 3600);
    CHECK(pet_ingest_scan(&n, &ap, 1, T0 + 2 * 3600) == 0, "de noche no olfatea");
}

static void test_food(void)
{
    SECTION("comer y monedas");
    pet_state_t s = baby(10, NOON);
    s.satiety = 30 * PET_ONE;
    s.coins = 10;
    pet_item_t fav = pet_favorite(&s);
    pet_item_t it = fav == ITEM_BREAD ? ITEM_FRUIT : ITEM_BREAD;
    int price = pet_item_price(it), food;
    pet_item_effect(it, &food, NULL, NULL);
    CHECK(pet_feed(&s, it, NOON) == FEED_OK, "comprar y comer");
    CHECK(coins_of(&s) == 10 - price + 5 && s.satiety == (30 + food) * PET_ONE, "cobra, sube comida (y +5 de la visita diaria): %d", coins_of(&s));
    CHECK(s.tasted >> it & 1, "queda probado");
    s.coins = 2;
    CHECK(pet_feed(&s, ITEM_STEW, NOON + 10) == FEED_NO_COINS, "sin monedas");
    s.inv[ITEM_STEW] = 1;
    CHECK(pet_feed(&s, ITEM_STEW, NOON + 20) == FEED_OK && s.inv[ITEM_STEW] == 0 && coins_of(&s) == 2, "del inventario no cobra");
    s.satiety = 40 * PET_ONE;
    CHECK(pet_bread_free(&s) && pet_feed(&s, ITEM_BREAD, NOON + 25) == FEED_OK && coins_of(&s) == 2, "sin monedas, el pan es gratis");
    s.coins = 3;
    CHECK(!pet_bread_free(&s), "con monedas, el pan se paga");
    s.satiety = 40 * PET_ONE;
    CHECK(pet_feed(&s, ITEM_TREAT, NOON + 30) == FEED_NONE, "la chuche no se compra");
    s.satiety = 96 * PET_ONE; s.coins = 50;
    CHECK(pet_feed(&s, ITEM_BREAD, NOON + 40) == FEED_FULL, "lleno: no quiere");
    CHECK(pet_feed(&s, ITEM_MEDICINE, NOON + 40) == FEED_NOT_SICK, "medicina sin estar malito");
    pet_state_t n = baby(11, T0 + 2 * 3600);
    n.satiety = 20 * PET_ONE;
    CHECK(pet_feed(&n, ITEM_BREAD, T0 + 2 * 3600) == FEED_ASLEEP, "de noche duerme");
    pet_state_t e = fresh(12);
    CHECK(pet_feed(&e, ITEM_BREAD, NOON) == FEED_EGG, "el huevo no come");
    /* favorito y pastel */
    pet_state_t f = baby(13, NOON);
    f.coins = 100; f.satiety = 20 * PET_ONE; f.fun = 20 * PET_ONE;
    pet_item_t fv = pet_favorite(&f);
    int ff;
    pet_item_effect(fv, NULL, &ff, NULL);
    pet_feed(&f, fv, NOON);
    CHECK(f.fun == (20 + ff + 10) * PET_ONE, "favorito: +10 de diversion");
    pet_state_t c = baby(14, NOON);
    c.coins = 100; c.satiety = 10 * PET_ONE; c.fun = 10 * PET_ONE;
    if (pet_favorite(&c) == ITEM_CAKE) c.view_seed ^= 1u << 12;
    pet_feed(&c, ITEM_CAKE, NOON);
    int32_t f1 = c.fun;
    pet_feed(&c, ITEM_CAKE, NOON + 10);
    CHECK(c.fun - f1 == 12 * PET_ONE, "segundo pastel del dia: la mitad (%d)", (int)((c.fun - f1) / PET_ONE));
}

static void test_hygiene_sleep(void)
{
    SECTION("cacas, malito, siesta");
    pet_state_t s = baby(20, NOON);
    s.last_poop_s = NOON - 2 * 3600;
    s.gut = 70;
    pet_step(&s, NOON + 60);
    CHECK(s.poops == 1 && (pet_take_events(&s) & EVT_POOP), "caca con la tripa llena");
    s.gut = 70;
    pet_step(&s, NOON + 120);
    CHECK(s.poops == 1, "no otra antes de 90 min");
    int c0 = coins_of(&s);
    CHECK(pet_clean_poop(&s, NOON + 130) && s.poops == 0 && coins_of(&s) >= c0 + 1, "limpiar una caca da moneda");
    CHECK(!pet_clean_poop(&s, NOON + 140), "sin cacas no hay nada que limpiar");
    /* malito por suciedad sostenida */
    pet_state_t d = baby(21, NOON);
    d.hygiene = 5 * PET_ONE;
    d.last_action_s = NOON;
    for (int m = 1; m <= 61; m++) pet_step(&d, NOON + m * 60);
    CHECK(d.sick == 1 && d.care_errors >= 1, "60 min sucio: malito y error");
    CHECK(pet_want(&d, NOON + 3700) == WANT_MEDICINE, "la burbuja pide medicina");
    CHECK(pet_can_play(&d, GAME_CATCH, NOON + 3700) == PLAY_SICK, "malito no juega");
    d.coins = 20;
    CHECK(pet_feed(&d, ITEM_MEDICINE, NOON + 3700) == FEED_OK && d.sick == 0, "la medicina cura");
    pet_state_t q = d;
    q.sick = 1; q.sick_since_s = NOON; q.hygiene = PET_MAX;
    pet_catch_up(&q, NOON + 13 * 3600);
    CHECK(q.sick == 0, "se cura solo a las 12 h");
    /* asear */
    pet_state_t w = baby(22, NOON);
    w.poops = 3; w.hygiene = 20 * PET_ONE;
    CHECK(pet_wash(&w, NOON) && w.poops == 0 && w.hygiene == 80 * PET_ONE, "asear quita cacas y sube 60");
    /* siesta */
    pet_state_t z = baby(23, NOON);
    z.energy = 10 * PET_ONE;
    CHECK(pet_want(&z, NOON) == WANT_SLEEP, "cansado: pide dormir");
    CHECK(pet_nap(&z, NOON) && pet_is_asleep(&z, NOON + 60), "siesta");
    for (int m = 1; m <= 30; m++) pet_step(&z, NOON + m * 60);
    CHECK(z.energy >= 45 * PET_ONE && !pet_is_napping(&z, NOON + PET_NAP_S), "30 min de siesta: +40 (%d)", (int)(z.energy / PET_ONE));
    pet_state_t y = baby(24, NOON);
    pet_nap(&y, NOON);
    pet_touch(&y, NOON + 120, 1000);
    CHECK(!pet_is_napping(&y, NOON + 121), "tocarla la despierta");
}

static void test_games_shop(void)
{
    SECTION("minijuegos, nivel y tienda");
    pet_state_t s = baby(30, NOON);
    s.visit_day = (uint32_t)(NOON / DAY);       /* sin bonus diario en las cuentas */
    CHECK(pet_can_play(&s, GAME_LIFT, NOON) == PLAY_LOCKED, "pesas desde el nivel 2");
    CHECK(pet_can_play(&s, GAME_CATCH, NOON) == PLAY_OK, "atrapa desde el nivel 1");
    s.fun = 10 * PET_ONE; s.energy = 50 * PET_ONE;
    int c0 = coins_of(&s);
    pet_reward_t r = pet_game_finish(&s, GAME_CATCH, 100, 42, NOON);
    CHECK(r.coins == 15 && r.xp == 20 && r.fun == 40 && r.train == 12 && r.record, "rendimiento 100");
    CHECK(s.fun == 50 * PET_ONE && s.energy == 42 * PET_ONE && s.train[GAME_CATCH] == 12 && s.best[GAME_CATCH] == 42, "aplica");
    CHECK(coins_of(&s) == c0 + 15, "monedas");
    r = pet_game_finish(&s, GAME_CATCH, 0, 3, NOON + 60);
    CHECK(r.coins == 2 && r.xp == 10 && !r.record, "rendimiento 0");
    CHECK(s.level == 1 && s.xp == 30, "xp acumulada");
    pet_game_finish(&s, GAME_MEMORY, 100, 9, NOON + 120);
    CHECK(s.level == 2 && (pet_take_events(&s) & EVT_LEVEL), "nivel 2 a los 40 XP");
    s.energy = 5 * PET_ONE;
    CHECK(pet_can_play(&s, GAME_LIFT, NOON) == PLAY_TIRED, "sin energia no juega");
    int have, need;
    pet_level_progress(&s, &have, &need);
    CHECK(need == 80 && have == (int)s.xp - 40, "progreso de nivel");
    /* tienda */
    s.coins = 100;
    CHECK(pet_buy_acc(&s, ACC_CROWN) == BUY_LOCKED, "corona bloqueada");
    CHECK(pet_buy_acc(&s, ACC_RADAR) == BUY_NOT_SOLD, "el radar no se vende");
    CHECK(pet_buy_acc(&s, ACC_BOW) == BUY_OK && s.coins == 40 && s.acc_worn == ACC_BOW, "comprar lazo y ponerlo");
    CHECK(pet_buy_acc(&s, ACC_BOW) == BUY_OWNED, "ya lo tiene");
    s.coins = 5; s.level = 3;
    CHECK(pet_buy_acc(&s, ACC_FLOWER) == BUY_NO_COINS, "sin monedas");
    CHECK(pet_wear(&s, ACC_NONE) && s.acc_worn == ACC_NONE, "quitar");
    CHECK(!pet_wear(&s, ACC_CROWN), "no se pone lo que no tiene");
}

static void test_wishes(void)
{
    SECTION("deseos y visita diaria");
    pet_state_t s = fresh(40);
    s.last_s = NOON;
    for (int m = 1; m <= 2; m++) pet_step(&s, NOON + m * 60);
    CHECK(s.stage == PET_BABY && (pet_take_events(&s) & EVT_HATCHED), "eclosiona a los 2 min activos");
    for (int m = 3; m <= 13; m++) pet_step(&s, NOON + m * 60);
    CHECK(s.wish_kind != WISH_NONE && (pet_take_events(&s) & EVT_WISH_NEW), "primer deseo a los 10 min");
    s.satiety = 50 * PET_ONE; s.coins = 100; s.energy = PET_MAX;
    int c0 = coins_of(&s), x0 = (int)s.xp, w0 = s.wishes_done;
    int64_t t = NOON + 14 * 60;
    switch (s.wish_kind) {
    case WISH_FOOD:   pet_feed(&s, (pet_item_t)s.wish_arg, t); break;
    case WISH_GAME:   pet_game_finish(&s, (pet_game_id_t)s.wish_arg, 50, 5, t); break;
    case WISH_PET:    for (int i = 0; i < 3; i++) pet_touch(&s, t, (uint32_t)(i * 2000)); break;
    case WISH_CLEAN:  pet_wash(&s, t); break;
    default: break;
    }
    CHECK(s.wishes_done == w0 + 1 && s.wish_kind == WISH_NONE, "deseo cumplido (tipo %d)", s.wish_kind);
    CHECK(coins_of(&s) > c0 && (int)s.xp >= x0 + 15, "recompensa");
    CHECK(s.next_wish_s >= t + 3600 && s.next_wish_s <= t + 9000, "el siguiente en 60-150 min");
    /* caduca sin castigo */
    pet_state_t e = baby(41, NOON);
    e.wish_kind = WISH_PET; e.wish_until_s = NOON + 60; e.next_wish_s = 0;
    pet_step(&e, NOON + 120);
    CHECK(e.wish_kind == WISH_NONE && e.care_errors == 0, "caduca");
    /* senal solo si ha visto alguna en 3 dias */
    int sig = 0;
    for (int i = 0; i < 200; i++) {
        pet_state_t g = baby(100 + (uint32_t)i, NOON);
        g.next_wish_s = NOON;
        pet_step(&g, NOON + 60);
        sig += g.wish_kind == WISH_SIGNAL;
    }
    CHECK(sig == 0, "sin senales recientes no pide senal");
    /* racha */
    pet_state_t r = baby(42, NOON);
    c0 = coins_of(&r);
    pet_touch(&r, NOON, 0);
    CHECK(coins_of(&r) == c0 + 5 && r.streak == 1, "primera visita: +5");
    c0 = coins_of(&r);
    pet_touch(&r, NOON + DAY, 5000);
    CHECK(coins_of(&r) == c0 + 7 && r.streak == 2, "segundo dia seguido: +7");
    pet_touch(&r, NOON + 3 * DAY, 9000);
    CHECK(r.streak == 1, "un dia sin visitar corta la racha");
}

static void test_evolution(void)
{
    SECTION("evolucion, ramas y generaciones");
    pet_state_t s = baby(50, NOON);
    s.level = 3;
    s.born_s = NOON - 11 * 3600;
    pet_step(&s, NOON + 60);
    CHECK(s.stage == PET_BABY, "con 11 h no pasa a juvenil");
    s.born_s = NOON - 12 * 3600;
    pet_step(&s, NOON + 120);
    CHECK(s.stage == PET_JUVENILE, "nivel 3 y 12 h: juvenil");
    pet_state_t f = baby(51, NOON);
    f.born_s = NOON - 3 * DAY;
    pet_step(&f, NOON + 60);
    CHECK(f.stage == PET_JUVENILE, "forzado a los 3 dias");
    /* adulto y rama */
    struct { uint16_t t[3]; int branch; } cases[] = {
        { {40, 5, 5}, BRANCH_AGILE }, { {5, 40, 5}, BRANCH_WISE }, { {5, 5, 40}, BRANCH_STRONG }, { {8, 8, 8}, BRANCH_HOMEBODY },
    };
    for (int i = 0; i < 4; i++) {
        pet_state_t a = baby(52 + (uint32_t)i, NOON);
        a.stage = PET_JUVENILE; a.level = 6; a.born_s = NOON - 3 * DAY;
        memcpy(a.train, cases[i].t, sizeof a.train);
        a.mood_sum = 60 * 100; a.mood_n = 100;
        pet_step(&a, NOON + 60);
        CHECK(a.stage == PET_ADULT && a.branch == cases[i].branch, "rama %d", cases[i].branch);
        CHECK(a.variant == VAR_POLISHED && (a.album >> ((a.branch - 1) * 4 + VAR_POLISHED) & 1), "pulido y en el album");
        pet_view_t v;
        pet_view(&a, NOON + 60, 0, &v);
        CHECK(v.shape == a.branch - 1, "forma de la rama");
    }
    pet_state_t v = baby(60, NOON);
    v.stage = PET_JUVENILE; v.level = 6; v.born_s = NOON - 3 * DAY; v.care_errors = 4;
    pet_step(&v, NOON + 60);
    CHECK(v.variant == VAR_CLUMSY, "4 errores: torpe");
    pet_state_t o = baby(61, NOON);
    o.stage = PET_JUVENILE; o.born_s = NOON - 10 * DAY; o.mood_sum = 20000; o.mood_n = 1000;
    pet_step(&o, NOON + 60);
    CHECK(o.stage == PET_ADULT && o.variant == VAR_ODD, "forzado a los 10 dias; animo bajo: raro");
    /* nueva generacion */
    o.coins = 77; o.acc_owned = 1u << ACC_BOW; o.signals = 42;
    uint16_t album = o.album;
    CHECK(!pet_can_new_generation(&o, NOON + DAY), "aun no");
    uint32_t seed = o.view_seed;
    CHECK(pet_new_generation(&o, NOON + 3 * DAY + 60), "nueva generacion");
    CHECK(o.stage == PET_EGG && o.generation == 1 && o.coins == 77 && pet_acc_owned(&o, ACC_BOW) && o.signals == 42 &&
          o.album == album && o.view_seed != seed && o.level == 1 && o.born_s == NOON + 3 * DAY + 60, "conserva lo acumulado");
}

static void test_absence(void)
{
    SECTION("B5 ausencia de 7 dias");
    for (int stage = PET_BABY; stage <= PET_ADULT; stage++) {
        pet_state_t s = baby(70 + (uint32_t)stage, NOON);
        s.stage = (uint8_t)stage;
        if (stage == PET_ADULT) { s.branch = BRANCH_HOMEBODY; s.adult_s = NOON; }
        s.born_s = NOON - (stage == PET_ADULT ? 11 * DAY : 3600);
        s.gut = 120;
        pet_catch_up(&s, NOON + 7 * DAY);
        CHECK(s.satiety >= 15 * PET_ONE && s.fun >= 20 * PET_ONE && s.hygiene >= 10 * PET_ONE, "suelos (etapa %d: %d %d %d)",
              stage, (int)(s.satiety / PET_ONE), (int)(s.fun / PET_ONE), (int)(s.hygiene / PET_ONE));
        CHECK(s.poops <= 3 && !s.sick && s.care_errors <= 2, "cacas %d, malito %d, errores %d", s.poops, s.sick, s.care_errors);
    }
    pet_state_t b = baby(80, NOON);
    pet_catch_up(&b, NOON - 100);
    CHECK(b.last_s == NOON - 100, "reloj hacia atras: solo reancla");
}

static void test_bots(void)
{
    SECTION("B1-B4 bots de jugador");
    const struct { const char *name; const profile_t *p; bool home; int jmin, jmax, amin, amax; } P[] = {
        { "activo",      &ACTIVE,  false, 1, 2, 4, 5 },
        { "activo casa", &ACTIVE,  true,  1, 2, 4, 5 },
        { "casual",      &CASUAL,  false, 1, 4, 7, 11 },
        { "abandono",    &NEGLECT, false, 1, 4, 10, 11 },
    };
    const int N = 200;
    for (size_t k = 0; k < sizeof P / sizeof P[0]; k++) {
        int jlo = 99, jhi = 0, alo = 99, ahi = 0, c3lo = 1 << 30;
        long jsum = 0, asum = 0, msum = 0, cend = 0;
        int br[BRANCH_COUNT] = {0}, var[4] = {0};
        for (int i = 0; i < N; i++) {
            pet_state_t s = fresh(1000 + (uint32_t)i);
            world_t w = { .rng = 5000 + (uint64_t)i, .stay_home = P[k].home, .outing_pct = P[k].home ? 0 : 40 };
            outcome_t o = run_days(&s, &w, P[k].p, 14);
            if (o.day_juv < jlo) jlo = o.day_juv;
            if (o.day_juv > jhi) jhi = o.day_juv;
            if (o.day_adult < alo) alo = o.day_adult;
            if (o.day_adult > ahi) ahi = o.day_adult;
            jsum += o.day_juv; asum += o.day_adult;
            br[o.branch]++; var[o.variant]++;
            if (o.coins_day3 < c3lo) c3lo = o.coins_day3;
            msum += o.mood_adult; cend += o.coins_end;
        }
        printf("    %-11s juvenil dia %d-%d (media %.1f), adulto dia %d-%d (media %.1f); ramas H%d S%d Sa%d F%d; variantes N%d P%d T%d R%d; monedas dia 3 >= %d, dia 14 media %ld; animo juvenil %ld\n",
               P[k].name, jlo, jhi, (double)jsum / N, alo, ahi, (double)asum / N, br[1], br[2], br[3], br[4],
               var[0], var[1], var[2], var[3], c3lo, cend / N, msum / N);
        CHECK(jlo >= P[k].jmin && jhi <= P[k].jmax, "%s: juvenil %d-%d fuera de %d-%d", P[k].name, jlo, jhi, P[k].jmin, P[k].jmax);
        CHECK(alo >= P[k].amin && ahi <= P[k].amax, "%s: adulto %d-%d fuera de %d-%d", P[k].name, alo, ahi, P[k].amin, P[k].amax);
        if (P[k].p == &ACTIVE) CHECK(c3lo >= 60, "B4: %s ahorra para un accesorio en 3 dias (%d)", P[k].name, c3lo);
    }
    SECTION("B3 la rama sigue al juego favorito");
    for (int g = -1; g < GAME_COUNT; g++) {
        profile_t p = ACTIVE;
        p.fav_game = g;
        if (g < 0) p = COUCH;
        int hit = 0;
        static const int want[GAME_COUNT] = { BRANCH_AGILE, BRANCH_WISE, BRANCH_STRONG };
        for (int i = 0; i < 100; i++) {
            pet_state_t s = fresh(3000 + (uint32_t)i);
            world_t w = { .rng = 9000 + (uint64_t)i, .outing_pct = 30 };
            outcome_t o = run_days(&s, &w, &p, 12);
            hit += o.branch == (g < 0 ? BRANCH_HOMEBODY : want[g]);
        }
        printf("    juego favorito %d: %d%% en su rama\n", g, hit);
        CHECK(hit >= 95, "rama del juego %d en %d%%", g, hit);
    }
}

static void test_determinism(void)
{
    SECTION("B7 determinismo");
    pet_state_t a = fresh(90), b = fresh(90);
    world_t wa = { .rng = 4, .outing_pct = 50 }, wb = { .rng = 4, .outing_pct = 50 };
    run_days(&a, &wa, &ACTIVE, 6);
    run_days(&b, &wb, &ACTIVE, 6);
    uint8_t x[PET_SAVE_MAX], y[PET_SAVE_MAX];
    size_t lx = pet_save(&a, 1, x, sizeof x), ly = pet_save(&b, 1, y, sizeof y);
    CHECK(lx == ly && memcmp(x, y, lx) == 0, "mismo guion, misma partida");
}

static size_t read_file(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

static void test_persistence(void)
{
    SECTION("B8 persistencia y migracion, B9 privacidad");
    pet_state_t s = fresh(50);
    world_t w = { .rng = 99, .outing_pct = 50 };
    run_days(&s, &w, &ACTIVE, 5);
    s.acc_owned = 0x1e; s.acc_worn = ACC_BEANIE; s.inv[ITEM_TREAT] = 4; s.poops = 2; s.sick = 1; s.sick_since_s = s.last_s;
    uint8_t a[PET_SAVE_MAX], b[PET_SAVE_MAX], c[PET_SAVE_MAX];
    size_t la = pet_save(&s, 5, a, sizeof(a));
    CHECK(la > 0 && la < 1024, "tamano de partida %zu B", la);
    pet_state_t r;
    uint32_t seq = 0;
    CHECK(pet_load(&r, a, la, &seq) == PET_LOAD_OK && seq == 5, "ida y vuelta");
    size_t lb = pet_save(&r, 5, b, sizeof(b));
    CHECK(lb == la && memcmp(a, b, la) == 0, "serializar tras cargar da los mismos bytes");
    CHECK(r.coins == s.coins && r.xp == s.xp && r.acc_worn == ACC_BEANIE && r.poops == 2 && r.sick == 1 &&
          r.signals == s.signals && r.train[0] == s.train[0] && r.hygiene == s.hygiene, "campos v3");
    /* B9 */
    world_t w2 = { .rng = 99, .outing_pct = 50 };
    bool leak = false;
    pet_ap_t aps[64];
    for (int i = 0; i < 300; i++) {
        size_t n = scan_at(&w2, T0 + 9 * 3600 + i * 900, aps);
        for (size_t k = 0; k < n; k++)
            for (size_t o = 0; o + 6 <= la; o++) if (memcmp(a + o, aps[k].bssid, 6) == 0) leak = true;
    }
    CHECK(!leak, "ningun BSSID en claro en la partida");
    CHECK(pet_load(&r, a, la - 1, NULL) == PET_LOAD_BAD, "truncado se rechaza");
    memcpy(c, a, la); c[la - 3] ^= 0x40;
    CHECK(pet_load(&r, c, la, NULL) == PET_LOAD_BAD, "CRC roto se rechaza");
    memcpy(c, a, la); c[4] = 99;
    CHECK(pet_load(&r, c, la, NULL) == PET_LOAD_NEWER, "version futura: se rechaza");
    pet_state_t s2 = s; pet_step(&s2, s.last_s + 100);
    size_t l2 = pet_save(&s2, 6, b, sizeof(b));
    pet_state_t g;
    CHECK(pet_load_ab(&g, a, la, b, l2) == 1, "elige la copia de seq mayor");
    CHECK(pet_load_ab(&g, a, la, b, l2 - 5) == 0, "B truncada: usa A");
    CHECK(pet_load_ab(&g, a, la - 5, b, l2 - 5) == -1, "ambas rotas: ninguna");

    /* migracion v2 sintetica: la despensa se guarda como pan y chuches */
    pet_state_t m0 = s;
    m0.inv[ITEM_BREAD] = 3; m0.inv[ITEM_TREAT] = 5; m0.branch = BRANCH_AGILE;
    uint8_t v2[PET_SAVE_MAX];
    size_t l2v = pet_save_version(&m0, 3, 2, v2, sizeof(v2));
    CHECK(l2v > 0 && l2v < la, "v2 es mas corta que v3");
    pet_state_t m;
    CHECK(pet_load(&m, v2, l2v, &seq) == PET_LOAD_OK && seq == 3, "se carga una partida v2");
    CHECK(m.stage == s.stage && m.born_s == s.born_s && m.satiety == s.satiety && m.fun == s.fun && m.branch == BRANCH_AGILE,
          "conserva etapa, edad, comida, animo->diversion y rama");
    CHECK(m.inv[ITEM_BREAD] == 3 && m.inv[ITEM_TREAT] == 5 && m.hygiene == PET_MAX && m.coins == 20, "despensa -> inventario; valores nuevos");
    CHECK(m.xp / 5 == s.xp / 5 && m.level >= 1, "PC -> XP");
    CHECK(pet_save(&m, 4, b, sizeof(b)) == la, "se reguarda como v4");
    CHECK(m.volume == PET_VOL_DEFAULT, "v2: volumen por defecto");

    /* v3 -> v4: solo añade el volumen */
    pet_state_t q0 = s;
    q0.volume = 3;
    uint8_t v3[PET_SAVE_MAX];
    size_t l3 = pet_save_version(&q0, 7, 3, v3, sizeof(v3));
    pet_state_t q;
    CHECK(l3 + 1 == la && pet_load(&q, v3, l3, NULL) == PET_LOAD_OK, "se carga una partida v3");
    CHECK(q.coins == s.coins && q.xp == s.xp && q.volume == PET_VOL_DEFAULT, "v3: conserva todo, volumen por defecto");
    q0.volume = 1;
    CHECK(pet_save(&q0, 8, c, sizeof(c)) == la && pet_load(&q, c, la, NULL) == PET_LOAD_OK && q.volume == 1, "v4 guarda el volumen");
    /* la partida real del reloj (v2, 2026-10-18) */
    uint8_t real[2][PET_SAVE_MAX];
    size_t lr[2] = { read_file("fixtures/watch_v2_a.bin", real[0], PET_SAVE_MAX),
                     read_file("fixtures/watch_v2_b.bin", real[1], PET_SAVE_MAX) };
    CHECK(lr[0] > 0 && lr[1] > 0, "fixtures presentes (ejecutar desde tests/)");
    pet_state_t wp;
    int which = pet_load_ab(&wp, real[0], lr[0], real[1], lr[1]);
    CHECK(which >= 0, "la partida del reloj carga");
    if (which >= 0) {
        printf("    reloj: etapa %d, edad %lld h, nivel %d (xp %u), pan %d, chuches %d, senales %u, escaneos %u\n", wp.stage,
               (long long)((wp.last_s - wp.born_s) / 3600), wp.level, (unsigned)wp.xp, wp.inv[ITEM_BREAD], wp.inv[ITEM_TREAT],
               (unsigned)wp.signals, (unsigned)wp.total_scans);
        CHECK(wp.stage == PET_BABY && wp.level >= 1 && wp.inv[ITEM_BREAD] + wp.inv[ITEM_TREAT] > 0, "cria con despensa convertida");
        pet_catch_up(&wp, wp.last_s + 3600);
        pet_view_t v;
        pet_view(&wp, wp.last_s, 0, &v);
        CHECK(v.stage == wp.stage && wp.stage >= PET_BABY, "se dibuja (etapa %d tras 1 h)", wp.stage);
    }

    /* robustez del cargador */
    uint32_t rng = 12345;
    int accepted = 0;
    for (int i = 0; i < 20000; i++) {
        memcpy(c, a, la);
        int flips = 1 + (int)(rng % 4);
        for (int k = 0; k < flips; k++) {
            rng = rng * 1664525u + 1013904223u;
            c[(rng >> 8) % la] ^= (uint8_t)(1u << (rng % 8));
        }
        pet_state_t f;
        if (pet_load(&f, c, la, NULL) == PET_LOAD_OK) {
            accepted++;
            pet_step(&f, f.last_s + 60);
            pet_catch_up(&f, f.last_s + 3 * DAY);
        }
    }
    CHECK(accepted < 200, "bytes corruptos casi nunca pasan el CRC (%d)", accepted);
    int bad_ok = 0;
    for (int i = 0; i < 5000; i++) {
        memcpy(c, a, la);
        rng = rng * 1664525u + 1013904223u; size_t off = 16 + (rng >> 8) % (la - 16);
        c[off] = (uint8_t)(rng >> 3);
        uint32_t crc = pet_crc32(c + 16, la - 16);
        c[12] = (uint8_t)crc; c[13] = (uint8_t)(crc >> 8); c[14] = (uint8_t)(crc >> 16); c[15] = (uint8_t)(crc >> 24);
        pet_state_t f;
        if (pet_load(&f, c, la, NULL) == PET_LOAD_OK) {
            bad_ok++;
            pet_step(&f, f.last_s + 60); pet_catch_up(&f, f.last_s + 2 * DAY);
            pet_view_t v; pet_view(&f, f.last_s, 0, &v);
            CHECK(f.satiety >= 0 && f.satiety <= PET_MAX && f.fun >= 0 && f.fun <= PET_MAX && f.hygiene >= 0 &&
                  f.hygiene <= PET_MAX && f.energy >= 0 && f.energy <= PET_MAX && v.shape < pet_shape_count(v.stage),
                  "estado en rango tras operar");
        }
    }
    printf("    (%d de 5000 cargas con un byte alterado y CRC valido aceptadas, todas en rango)\n", bad_ok);
}

static void test_fuzz_ops(void)
{
    SECTION("B6 operaciones aleatorias: no existe estado 'muerta'");
    uint32_t rng = 777;
    for (int run = 0; run < 200; run++) {
        pet_state_t s = fresh((uint32_t)run);
        int64_t t = T0 + 12 * 3600;
        pet_ap_t aps[64];
        world_t w = { .rng = (uint64_t)run + 1 };
        for (int i = 0; i < 500; i++) {
            rng = rng * 1664525u + 1013904223u;
            switch ((rng >> 12) % 14) {
            case 0: t += (int64_t)(rng % 90000); pet_catch_up(&s, t); break;
            case 1: t += (int64_t)(rng % 600); pet_step(&s, t); break;
            case 2: { size_t n = scan_at(&w, t, aps); pet_ingest_scan(&s, aps, n, t); break; }
            case 3: pet_feed(&s, (pet_item_t)(rng % ITEM_COUNT), t); break;
            case 4: pet_touch(&s, t, (uint32_t)(t * 1000)); break;
            case 5: pet_clean_poop(&s, t); break;
            case 6: pet_decide(&s, t); break;
            case 7: pet_wash(&s, t); break;
            case 8: pet_nap(&s, t); break;
            case 9: pet_game_finish(&s, (pet_game_id_t)(rng % GAME_COUNT), (int)(rng % 140) - 20, (int)(rng % 50), t); break;
            case 10: pet_buy_acc(&s, (pet_acc_t)(rng % ACC_COUNT)); pet_wear(&s, (pet_acc_t)(rng % ACC_COUNT)); break;
            case 11: pet_new_generation(&s, t); break;
            case 12: s.coins += rng % 50; break;
            default: t += 3600 * (int64_t)(rng % 30); pet_step(&s, t); break;
            }
            if (s.satiety < 0 || s.satiety > PET_MAX || s.fun < 0 || s.fun > PET_MAX || s.hygiene < 0 || s.hygiene > PET_MAX ||
                s.energy < 0 || s.energy > PET_MAX || s.poops > PET_POOP_MAX || s.level < 1 ||
                s.familiar_n > PET_FAM_MAX || s.recent_n > PET_FAM_MAX || s.acc_worn >= ACC_COUNT) {
                CHECK(false, "invariante rota (run %d, op %d)", run, i);
                return;
            }
            pet_view_t v; pet_view(&s, t, (uint32_t)(t * 1000), &v);
            if (v.expr >= EXPR_COUNT || v.stage >= PET_STAGE_COUNT || v.shape >= pet_shape_count(v.stage)) {
                CHECK(false, "vista invalida");
                return;
            }
            if (i % 50 == 0) {
                uint8_t buf[PET_SAVE_MAX];
                pet_state_t back;
                size_t l = pet_save(&s, 1, buf, sizeof buf);
                if (pet_load(&back, buf, l, NULL) != PET_LOAD_OK) { CHECK(false, "no recarga (run %d op %d)", run, i); return; }
            }
        }
    }
    CHECK(true, "ok");
}

static void test_view_decide(void)
{
    SECTION("vista, burbuja y decisiones");
    pet_state_t s = baby(95, NOON);
    s.satiety = 20 * PET_ONE;
    pet_view_t v;
    pet_view(&s, NOON, 0, &v);
    CHECK(v.expr == EXPR_HUNGRY && pet_want(&s, NOON) == WANT_FOOD, "hambre");
    s.satiety = 80 * PET_ONE; s.poops = 2;
    pet_view(&s, NOON, 0, &v);
    CHECK(v.expr == EXPR_ANGRY && pet_want(&s, NOON) == WANT_CLEAN, "sucio: grunon, pide aseo");
    s.poops = 0; s.fun = 10 * PET_ONE;
    CHECK(pet_want(&s, NOON) == WANT_PLAY && pet_mood(&s, NOON) == MOOD_BORED, "aburrido: pide jugar");
    s.fun = 90 * PET_ONE; s.wish_kind = WISH_PET; s.wish_until_s = NOON + 999;
    CHECK(pet_want(&s, NOON) == WANT_WISH, "deseo");
    s.acc_worn = ACC_CROWN;
    pet_view(&s, NOON, 0, &v);
    CHECK(v.acc == ACC_CROWN, "accesorio en la vista");
    CHECK(pet_view(&s, T0 + 2 * 3600, 0, &v) == ANIM_SLEEP, "de noche duerme");
    int cnt[BEH_COUNT] = {0};
    for (int i = 0; i < 300; i++) cnt[pet_decide(&s, NOON + i * 10)]++;
    CHECK(cnt[BEH_WALK] > 0 && cnt[BEH_REST] > 0 && cnt[BEH_SLEEP] == 0, "pasea y descansa de dia");
    pet_state_t e = fresh(96);
    CHECK(pet_decide(&e, NOON) == BEH_REST, "el huevo no decide");
    pet_state_t egg = fresh(97);
    egg.egg_active_s = 70;
    pet_view(&egg, NOON, 0, &v);
    CHECK(v.crack == 1, "huevo agrietado a mitad");
}

int main(void)
{
    test_primitives();
    test_signals();
    test_food();
    test_hygiene_sleep();
    test_games_shop();
    test_wishes();
    test_evolution();
    test_absence();
    test_view_decide();
    test_determinism();
    test_persistence();
    test_fuzz_ops();
    test_bots();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails != 0;
}
