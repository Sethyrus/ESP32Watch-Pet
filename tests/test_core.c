/* Tests de pet_core: unitarios y criterios de aceptacion A1-A10 (PET_DESIGN.md, 13). */
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SECTION(n) printf("%s\n", n)

static pet_state_t fresh(uint32_t seed)
{
    uint8_t key[PET_KEY_LEN];
    pet_state_t s;
    make_key(key, seed);
    pet_init(&s, key, T0);
    return s;
}

static void to_stage(pet_state_t *s, int stage, int64_t now)
{
    /* Deja la mascota en una etapa con la edad y PC justos para evolucionar. */
    s->stage = (uint8_t)stage;
    if (stage >= PET_BABY) s->born_s = now - 20 * DAY;
}

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

static void test_familiarity(void)
{
    SECTION("familiaridad y despensa");
    pet_state_t s = fresh(1);
    s.stage = PET_BABY; s.born_s = T0;
    pet_ap_t ap; mk_ap(&ap, 7, false);
    int64_t t = T0 + 9 * 3600;
    int added = pet_ingest_scan(&s, &ap, 1, t);
    CHECK(added == 1 && pet_pantry_count(&s, FOOD_EXOTIC) == 1, "primer AP es exotico");
    CHECK(s.recent_n == 1 && s.familiar_n == 0, "entra en recientes");
    /* escaneos pegados (< 20 min) no suben la puntuacion ni dan bocados */
    CHECK(pet_ingest_scan(&s, &ap, 1, t + 60) == 0, "visto de nuevo no es exotico ni domestico");
    CHECK(s.recent[0].score4 == 4, "sin epoca no cambia la puntuacion");
    /* una epoca por cada 20 min: promueve al llegar a 5 */
    for (int i = 1; i <= 4; i++) pet_ingest_scan(&s, &ap, 1, t + i * 1300);
    CHECK(s.familiar_n == 1 && s.recent_n == 0, "promovido a familiar tras 5 avistamientos");
    int before = pet_pantry_count(&s, FOOD_DOMESTIC);
    pet_ingest_scan(&s, &ap, 1, t + 5 * 1300);
    CHECK(pet_pantry_count(&s, FOOD_DOMESTIC) == before + 1, "familiar visible da domestico");
    /* desaparece: -0,25 por epoca y se elimina al llegar a 0 */
    pet_ap_t other; mk_ap(&other, 99, false);
    for (int i = 0; i < 80 && s.familiar_n; i++) pet_ingest_scan(&s, &other, 1, t + (6 + i) * 1300);
    uint64_t hh = pet_siphash24(s.key, ap.bssid, 6);
    bool still = false;
    for (int i = 0; i < s.familiar_n; i++) still |= s.familiar[i].hash == (uint32_t)(hh ^ (hh >> 32));
    CHECK(!still, "un AP que no se ve se olvida");
    /* caducidad */
    pet_state_t p = fresh(2);
    p.stage = PET_BABY;
    pet_ingest_scan(&p, &ap, 1, T0 + 9 * 3600);
    CHECK(pet_pantry_expires_in(&p, FOOD_EXOTIC, T0 + 9 * 3600) == 48 * 3600, "exotico caduca en 48 h");
    pet_ingest_scan(&p, &other, 1, T0 + 9 * 3600 + 49 * 3600 - 86400 * 2 + 86400 * 2 - 0 + 0 + 86400); /* otro dia, de dia */
    CHECK(pet_pantry_count(&p, FOOD_EXOTIC) == 1, "el viejo caduco; solo queda el nuevo");
    /* despensa llena: descarta domesticos primero */
    pet_state_t f = fresh(3);
    f.stage = PET_BABY;
    for (int i = 0; i < 12; i++) f.pantry[f.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, T0 + 9 * 3600 };
    for (int i = 0; i < 8; i++) f.pantry[f.pantry_n++] = (pet_bite_t){ FOOD_EXOTIC, T0 + 9 * 3600 };
    pet_ap_t many[3]; for (int i = 0; i < 3; i++) mk_ap(&many[i], 500 + (uint32_t)i, false);
    pet_ingest_scan(&f, many, 3, T0 + 9 * 3600 + 10);
    CHECK(f.pantry_n == PET_PANTRY_MAX, "la despensa no se desborda");
    CHECK(pet_pantry_count(&f, FOOD_EXOTIC) == 11 && pet_pantry_count(&f, FOOD_DOMESTIC) == 9, "salen domesticos");
    /* urbano */
    pet_state_t u = fresh(4);
    u.stage = PET_BABY;
    pet_ap_t net[16]; for (int i = 0; i < 16; i++) mk_ap(&net[i], 700 + (uint32_t)i, false);
    pet_ingest_scan(&u, net, 16, T0 + 9 * 3600);
    CHECK(pet_pantry_count(&u, FOOD_URBAN) == 1, "16 redes dan urbano");
    for (int i = 0; i < 16; i++) mk_ap(&net[i], 800 + (uint32_t)i, false);
    pet_ingest_scan(&u, net, 16, T0 + 9 * 3600 + 600);
    CHECK(pet_pantry_count(&u, FOOD_URBAN) == 1, "urbano con enfriamiento de 30 min");
}

static void test_night_and_mood(void)
{
    SECTION("noche, animo y planificador");
    pet_state_t s = fresh(5);
    s.stage = PET_BABY;
    pet_ap_t ap; mk_ap(&ap, 1, false);
    CHECK(pet_is_night(&s, T0 + 23 * 3600) && pet_is_night(&s, T0 + 3 * 3600) && !pet_is_night(&s, T0 + 8 * 3600), "ventana de noche");
    CHECK(pet_ingest_scan(&s, &ap, 1, T0 + 2 * 3600) == 0, "de noche no se escanea");
    s.pantry[s.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, T0 };
    CHECK(!pet_feed(&s, FOOD_DOMESTIC, T0 + 2 * 3600), "de noche no se come");
    CHECK(pet_mood(&s, T0 + 2 * 3600) == MOOD_ASLEEP, "dormida de noche");
    int64_t d = T0 + 12 * 3600;
    s.last_novel_s = d; s.novel_until_s = 0; s.mood = 80 * PET_ONE; s.satiety = 80 * PET_ONE; s.energy = 80 * PET_ONE;
    CHECK(pet_mood(&s, d) == MOOD_HAPPY, "feliz");
    s.mood = 50 * PET_ONE; CHECK(pet_mood(&s, d) == MOOD_CALM, "calma");
    s.mood = 30 * PET_ONE; CHECK(pet_mood(&s, d) == MOOD_SAD, "triste");
    s.energy = 10 * PET_ONE; CHECK(pet_mood(&s, d) == MOOD_TIRED, "cansada");
    s.satiety = 10 * PET_ONE; CHECK(pet_mood(&s, d) == MOOD_HUNGRY, "hambrienta gana a todo");
    s.satiety = 80 * PET_ONE; s.energy = 80 * PET_ONE; s.mood = 80 * PET_ONE;
    CHECK(pet_mood(&s, d + 3 * 3600) == MOOD_BORED, "aburrida tras 2 h sin novedad");
    s.novel_until_s = d + 100; s.last_novel_s = d;
    CHECK(pet_mood(&s, d + 50) == MOOD_HAPPY || pet_mood(&s, d + 50) == MOOD_CURIOUS, "curiosa con senal nueva");
    CHECK(pet_mood(&s, d + 50) == MOOD_CURIOUS, "curiosa gana a feliz");
    /* planificador */
    s.last_scan_s = T0 + 9 * 3600;
    CHECK(pet_next_scan_s(&s, T0 + 9 * 3600 + 10, false, 80) == T0 + 9 * 3600 + 900, "muneca: 15 min");
    CHECK(pet_next_scan_s(&s, T0 + 9 * 3600 + 10, true, 80) == T0 + 9 * 3600 + 60, "mesa: 60 s");
    CHECK(pet_next_scan_s(&s, T0 + 9 * 3600 + 10, true, 10) == PET_NEVER, "bateria baja: nunca");
    s.last_scan_s = T0 + 22 * 3600 + 3000;
    CHECK(pet_next_scan_s(&s, T0 + 22 * 3600 + 3100, false, 80) == T0 + DAY + 8 * 3600, "de noche espera a la manana");
    s.last_scan_s = T0 + 1 * 3600;
    CHECK(pet_next_scan_s(&s, T0 + 2 * 3600, false, 80) == T0 + 8 * 3600, "madrugada: a las 08:00 de hoy");
}

static void test_stats(void)
{
    SECTION("estadisticas y reacciones");
    pet_state_t s = fresh(6);
    s.stage = PET_BABY; s.born_s = T0;
    int64_t t = T0 + 12 * 3600;
    s.last_s = t; s.last_action_s = t; s.satiety = s.energy = s.mood = 100 * PET_ONE;
    for (int i = 1; i <= 60; i++) pet_step(&s, t + i * 60);
    int sat_h = (100 * PET_ONE - s.satiety) / (PET_ONE / 10);       /* decimas de punto/h */
    CHECK(sat_h >= 78 && sat_h <= 82, "saciedad baja ~8/h (decimas=%d)", sat_h);
    CHECK(s.mood < 100 * PET_ONE - PET_ONE && s.mood > 100 * PET_ONE - 3 * PET_ONE, "animo baja ~2/h");
    /* amortiguacion: la ganancia cerca de 100 casi no cuenta, cerca de 0 se triplica */
    pet_state_t a = fresh(7); a.stage = PET_BABY;
    a.pantry[a.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, T0 };
    a.satiety = 95 * PET_ONE; a.last_s = T0 + 12 * 3600;
    pet_feed(&a, FOOD_DOMESTIC, T0 + 12 * 3600);
    int gain_hi = a.satiety - 95 * PET_ONE;
    pet_state_t b = fresh(7); b.stage = PET_BABY;
    b.pantry[b.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, T0 };
    b.satiety = 5 * PET_ONE; b.last_s = T0 + 12 * 3600;
    pet_feed(&b, FOOD_DOMESTIC, T0 + 12 * 3600);
    int gain_lo = b.satiety - 5 * PET_ONE;
    CHECK(gain_lo > 5 * gain_hi, "recuperarse es mas facil que sobrealimentar (%d vs %d)", gain_lo, gain_hi);
    /* tabla de efectos del exotico: PC +4, topes diarios */
    pet_state_t g = fresh(8); g.stage = PET_BABY; g.last_s = T0 + 12 * 3600;
    for (int i = 0; i < 15; i++) g.pantry[g.pantry_n++] = (pet_bite_t){ FOOD_EXOTIC, T0 + 12 * 3600 };
    for (int i = 0; i < 15; i++) { g.satiety = 20 * PET_ONE; pet_feed(&g, FOOD_EXOTIC, T0 + 12 * 3600); }
    CHECK(g.gp_food_today <= 50 && g.gp >= 50, "tope de 50 PC/dia por comida (food=%d gp=%d)", g.gp_food_today, g.gp);
    /* picoteo autonomo: con hambre y 5 min sin actuar */
    pet_state_t p = fresh(9); p.stage = PET_BABY; p.born_s = T0;
    p.last_s = p.last_action_s = T0 + 12 * 3600; p.satiety = 20 * PET_ONE;
    p.pantry[p.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, T0 + 12 * 3600 };
    for (int i = 1; i <= 4; i++) pet_step(&p, T0 + 12 * 3600 + i * 60);
    CHECK(p.pantry_n == 1, "no pica antes de 5 min");
    for (int i = 5; i <= 7; i++) pet_step(&p, T0 + 12 * 3600 + i * 60);
    CHECK(p.pantry_n == 0 && (pet_take_events(&p) & EVT_PECKED), "pica con hambre pasados 5 min");
    CHECK(p.manual_eaten == 0 && p.total_eaten == 1, "el picoteo no cuenta como manual");
    /* toques */
    pet_state_t k = fresh(10); k.stage = PET_BABY; k.last_s = T0 + 12 * 3600; k.mood = 50 * PET_ONE;
    uint32_t m = 1000000;
    CHECK(pet_touch(&k, T0 + 12 * 3600, m) == REACT_PET, "caricia");
    CHECK(k.mood > 50 * PET_ONE, "la caricia sube el animo");
    pet_touch(&k, T0 + 12 * 3600, m + 300); pet_touch(&k, T0 + 12 * 3600, m + 600);
    int32_t mood_before = k.mood;
    CHECK(pet_touch(&k, T0 + 12 * 3600, m + 900) == REACT_DIZZY, "4 toques en 1,5 s marean");
    CHECK(k.mood == mood_before, "mareada: sin ganancia");
    CHECK(pet_touch(&k, T0 + 12 * 3600, m + 2000) == REACT_DIZZY, "sigue mareada 3 s");
    CHECK(pet_touch(&k, T0 + 12 * 3600, m + 6000) == REACT_PET, "se le pasa");
    /* presupuesto de caricias: max +10 por hora */
    pet_state_t q = fresh(11); q.stage = PET_BABY; q.last_s = T0 + 12 * 3600; q.mood = 10 * PET_ONE;
    for (int i = 0; i < 40; i++) pet_touch(&q, T0 + 12 * 3600, 2000000 + (uint32_t)i * 5000);
    CHECK(q.mood <= 10 * PET_ONE + 14 * PET_ONE, "presupuesto horario de caricias (animo=%d)", q.mood / PET_ONE);
    /* sacudidas: tras > 3 en un minuto el animo baja */
    pet_state_t z = fresh(12); z.stage = PET_BABY; z.mood = 60 * PET_ONE;
    for (int i = 0; i < 6; i++) pet_shake(&z, T0, 3000000 + (uint32_t)i * 1000);
    CHECK(z.mood < 60 * PET_ONE, "sacudir mucho molesta");
}

static void test_evolution(void)
{
    SECTION("evolucion");
    pet_state_t s = fresh(20);
    int64_t t = T0 + 12 * 3600;
    s.last_s = t;
    for (int i = 1; i <= 11; i++) pet_step(&s, t + i * 60);
    CHECK(s.stage == PET_BABY && (pet_take_events(&s) & EVT_HATCHED), "eclosiona tras 10 min activos");
    /* juvenil: edad y PC */
    s.born_s = t - 40 * 3600; s.gp = 39; s.last_s = t;
    pet_step(&s, t + 60);
    CHECK(s.stage == PET_BABY, "faltan PC para juvenil");
    s.gp = 40; pet_step(&s, t + 120);
    CHECK(s.stage == PET_JUVENILE, "juvenil con edad >= 36 h y PC >= 40");
    /* plazo maximo */
    pet_state_t f = fresh(21); f.stage = PET_BABY; f.born_s = t - 8 * DAY; f.last_s = t;
    pet_step(&f, t + 60);
    CHECK(f.stage == PET_JUVENILE, "plazo: juvenil a los 7 dias sin PC");
    f.born_s = t - 15 * DAY; pet_step(&f, t + 120);
    CHECK(f.stage == PET_ADULT, "plazo: adulto a los 14 dias sin PC");
    /* A4: los errores nunca bajan de 0 ni se acumulan entre etapas */
    SECTION("A4 errores de cuidado");
    pet_state_t e = fresh(22); e.stage = PET_JUVENILE; e.born_s = t - 2 * DAY;
    e.last_s = t; e.last_action_s = t - 3600; e.satiety = 0; e.mood = 80 * PET_ONE; e.pantry_n = 0;
    for (int i = 1; i <= 60; i++) pet_step(&e, t + i * 60);
    CHECK(e.care_errors == 4, "15 min con necesidad a 0 = 1 error (errores=%d)", e.care_errors);
    e.born_s = t - 15 * DAY; pet_step(&e, t + 61 * 60);
    CHECK(e.stage == PET_ADULT && e.care_errors == 0 && e.juv_errors == 4, "se reinician al evolucionar (juv=%d)", e.juv_errors);
    CHECK(e.variant == VAR_CLUMSY, ">= 4 errores en juvenil: torpe");
    /* ausencia con hambre: max 1 por 12 h y 2 por ausencia */
    SECTION("A3 ausencia de 7 dias");
    for (int stage = PET_BABY; stage <= PET_ADULT; stage++) {
        pet_state_t a = fresh(23 + (uint32_t)stage);
        to_stage(&a, stage, t); a.born_s = t - 2 * DAY; a.gp = 0;
        a.last_s = t; a.satiety = 100 * PET_ONE; a.mood = 100 * PET_ONE; a.pantry_n = 0; a.care_errors = 1;
        pet_catch_up(&a, t + 7 * DAY);
        CHECK(a.satiety >= 15 * PET_ONE, "saciedad >= 15 (etapa %d: %d)", stage, a.satiety / PET_ONE);
        CHECK(a.mood >= 35 * PET_ONE, "animo >= 35 (etapa %d: %d)", stage, a.mood / PET_ONE);
        CHECK(a.care_errors <= 1 + 2 || a.stage != stage, "errores anadidos <= 2 (etapa %d: %d)", stage, a.care_errors);
    }
    pet_state_t big = fresh(30); to_stage(&big, PET_BABY, t); big.born_s = t - DAY; big.last_s = t;
    pet_catch_up(&big, t + 400 * DAY);
    CHECK(big.satiety >= 15 * PET_ONE && big.mood >= 35 * PET_ONE, "tope de 7 dias aunque pasen 400");
    pet_state_t back = fresh(31); back.stage = PET_BABY; back.last_s = t + 10000;
    int32_t s0 = back.satiety;
    pet_catch_up(&back, t);
    CHECK(back.satiety == s0 && back.last_s == t, "reloj hacia atras: dt = 0");
    /* perdida de hora del RTC */
    pet_state_t lost = fresh(32); lost.stage = PET_BABY; lost.last_s = t; s0 = lost.satiety;
    pet_clock_lost(&lost, t + 5 * DAY);
    CHECK(lost.satiety == s0 && lost.last_s == t + 5 * DAY, "RTC perdido: sin simulacion");
}

static void test_a10(void)
{
    SECTION("A10 la rama no depende del cuidado");
    int64_t t = T0 + 12 * 3600;
    struct { int errors; uint32_t mean; int want; } care[] = {
        { 0, 70, VAR_POLISHED }, { 2, 60, VAR_POLISHED }, { 3, 70, VAR_NORMAL },
        { 1, 50, VAR_NORMAL }, { 0, 30, VAR_ODD }, { 6, 80, VAR_CLUMSY }, { 4, 20, VAR_CLUMSY },
    };
    for (int diet = 0; diet <= 10; diet++) {
        int branch0 = -1;
        for (unsigned c = 0; c < sizeof(care) / sizeof(care[0]); c++) {
            pet_state_t s = fresh(40);
            to_stage(&s, PET_JUVENILE, t);
            s.last_s = t;
            s.exo_eaten = (uint16_t)diet; s.dom_eaten = (uint16_t)(10 - diet);
            s.care_errors = (uint8_t)care[c].errors; s.mood_n = 1000; s.mood_sum = 1000 * care[c].mean;
            s.satiety = s.mood = 70 * PET_ONE;
            pet_step(&s, t + 30);
            if (s.stage != PET_ADULT) { s.gp = 200; pet_step(&s, t + 60); }
            CHECK(s.stage == PET_ADULT, "llega a adulto");
            if (branch0 < 0) branch0 = s.branch;
            CHECK(s.branch == branch0, "dieta %d/10: la rama no cambia con el cuidado", diet);
            CHECK(s.variant == care[c].want, "variante errores=%d animo=%u: %d (esperaba %d)", care[c].errors, care[c].mean, s.variant, care[c].want);
        }
        int want = diet * 100 >= 300 ? BRANCH_EXPLORER : BRANCH_HOMEBODY;
        CHECK(branch0 == want, "dieta %d/10 -> rama %d", diet, branch0);
    }
}

static void test_clumsy_reachable(void)
{
    SECTION("Torpe es alcanzable con abandono sostenido");
    int64_t t = T0 + 12 * 3600;
    pet_state_t s = fresh(90);
    to_stage(&s, PET_JUVENILE, t); s.born_s = t - 3 * DAY; s.last_s = t; s.gp = 0;
    s.satiety = 100 * PET_ONE; s.pantry_n = 0;
    for (int k = 0; k < 3; k++) {
        pet_catch_up(&s, t + 3 * DAY);
        t += 3 * DAY;
        pet_touch(&s, t, (uint32_t)(t * 1000));
        s.pantry_n = 0;
    }
    CHECK(s.care_errors >= 4, "tres ausencias de 3 dias: %d errores", s.care_errors);
    s.born_s = t - 15 * DAY; pet_step(&s, t + 60);
    CHECK(s.stage == PET_ADULT && s.variant == VAR_CLUMSY, "adulto torpe");
}

static void test_persistence(void)
{
    SECTION("A8 persistencia, A9 privacidad");
    pet_state_t s = fresh(50);
    world_t w = { .rng = 99 };
    run_days(&s, &w, &ACTIVE, 3);
    uint8_t a[PET_SAVE_MAX], b[PET_SAVE_MAX], c[PET_SAVE_MAX];
    size_t la = pet_save(&s, 5, a, sizeof(a));
    CHECK(la > 0 && la < 1024, "tamano de partida %zu B", la);
    pet_state_t r;
    uint32_t seq = 0;
    CHECK(pet_load(&r, a, la, &seq) == PET_LOAD_OK && seq == 5, "ida y vuelta");
    size_t lb = pet_save(&r, 5, b, sizeof(b));
    CHECK(lb == la && memcmp(a, b, la) == 0, "serializar tras cargar da los mismos bytes");
    /* A9: ningun BSSID en claro */
    world_t w2 = { .rng = 99 };
    bool leak = false;
    pet_ap_t aps[64];
    for (int i = 0; i < 200; i++) {
        size_t n = scan_at(&w2, T0 + 9 * 3600 + i * 900, aps);
        for (size_t k = 0; k < n; k++)
            for (size_t o = 0; o + 6 <= la; o++) if (memcmp(a + o, aps[k].bssid, 6) == 0) leak = true;
    }
    CHECK(!leak, "ningun BSSID en claro en la partida");
    /* truncado, CRC, version futura, basura */
    CHECK(pet_load(&r, a, la - 1, NULL) == PET_LOAD_BAD, "truncado se rechaza");
    memcpy(c, a, la); c[la - 3] ^= 0x40;
    CHECK(pet_load(&r, c, la, NULL) == PET_LOAD_BAD, "CRC roto se rechaza");
    memcpy(c, a, la); c[4] = 99;
    CHECK(pet_load(&r, c, la, NULL) == PET_LOAD_NEWER, "version futura: se rechaza sin tocar el estado");
    /* copia A/B */
    pet_state_t s2 = s; pet_step(&s2, s.last_s + 100);
    size_t l2 = pet_save(&s2, 6, b, sizeof(b));
    pet_state_t g;
    CHECK(pet_load_ab(&g, a, la, b, l2) == 1, "elige la copia de seq mayor");
    CHECK(pet_load_ab(&g, a, la, b, l2 - 5) == 0, "B truncada: usa A");
    CHECK(pet_load_ab(&g, a, la - 5, b, l2) == 1, "A truncada: usa B");
    CHECK(pet_load_ab(&g, a, la - 5, b, l2 - 5) == -1, "ambas rotas: ninguna");
    CHECK(pet_load_ab(&g, NULL, 0, b, l2) == 1, "A vacia: usa B");
    /* A8: migracion desde la version 1 */
    uint8_t v1[PET_SAVE_MAX];
    size_t l1 = pet_save_version(&s, 3, 1, v1, sizeof(v1));
    CHECK(l1 > 0 && l1 < la, "v1 es mas corta que v2");
    pet_state_t m;
    CHECK(pet_load(&m, v1, l1, &seq) == PET_LOAD_OK && seq == 3, "se carga una partida v1");
    CHECK(m.night_start_h == 23 && m.night_end_h == 8 && m.sound_on == 1 && m.total_scans == 0, "campos nuevos con valores por defecto");
    CHECK(m.stage == s.stage && m.gp == s.gp && m.satiety == s.satiety && m.pantry_n == s.pantry_n, "los campos de v1 se conservan");
    size_t lm = pet_save(&m, 4, b, sizeof(b));
    CHECK(lm == la, "se reguarda como v2");
    pet_step(&m, m.last_s + 120);
    /* A5: fuzz del cargador */
    SECTION("A5 robustez");
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
            pet_step(&f, f.last_s + 60);                       /* no debe romperse */
            pet_catch_up(&f, f.last_s + 3 * DAY);
        }
    }
    CHECK(accepted < 200, "bytes corruptos casi nunca pasan el CRC (%d)", accepted);
    /* con CRC rehecho: valores absurdos se rechazan */
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
            CHECK(f.stage < PET_STAGE_COUNT && f.satiety >= 0 && f.satiety <= PET_MAX && f.pantry_n <= PET_PANTRY_MAX, "estado cargado valido");
            pet_step(&f, f.last_s + 60); pet_catch_up(&f, f.last_s + 2 * DAY);
            CHECK(f.satiety >= 0 && f.satiety <= PET_MAX && f.mood >= 0 && f.mood <= PET_MAX && f.energy >= 0 && f.energy <= PET_MAX, "estadisticas en rango tras operar");
        }
    }
    printf("    (%d de 5000 cargas con valor alterado y CRC valido fueron aceptadas, todas con estado valido)\n", bad_ok);
}

static void test_fuzz_ops(void)
{
    SECTION("A5 operaciones aleatorias: no existe estado 'muerta'");
    uint32_t rng = 777;
    for (int run = 0; run < 200; run++) {
        pet_state_t s = fresh((uint32_t)run);
        int64_t t = T0 + 12 * 3600;
        pet_ap_t aps[64];
        world_t w = { .rng = (uint64_t)run + 1 };
        for (int i = 0; i < 400; i++) {
            rng = rng * 1664525u + 1013904223u;
            switch ((rng >> 12) % 8) {
            case 0: t += (int64_t)(rng % 90000); pet_catch_up(&s, t); break;
            case 1: t += (int64_t)(rng % 600); pet_step(&s, t); break;
            case 2: { size_t n = scan_at(&w, t, aps); pet_ingest_scan(&s, aps, n, t); break; }
            case 3: pet_feed(&s, (pet_food_t)(rng % 3), t); break;
            case 4: pet_touch(&s, t, (uint32_t)(t * 1000)); break;
            case 5: pet_shake(&s, t, (uint32_t)(t * 1000)); break;
            case 6: pet_decide(&s, t); break;
            default: t += 3600 * (int64_t)(rng % 30); pet_step(&s, t); break;
            }
            if (s.satiety < 0 || s.satiety > PET_MAX || s.mood < 0 || s.mood > PET_MAX ||
                s.energy < 0 || s.energy > PET_MAX || s.pantry_n > PET_PANTRY_MAX ||
                s.familiar_n > PET_FAM_MAX || s.recent_n > PET_FAM_MAX) {
                CHECK(false, "invariante rota (run %d, op %d)", run, i);
                return;
            }
            pet_view_t v; pet_view(&s, t, (uint32_t)(t * 1000), &v);
            if (v.expr >= EXPR_COUNT || v.stage >= PET_STAGE_COUNT) { CHECK(false, "vista invalida"); return; }
        }
    }
    CHECK(true, "ok");
}

static void test_a6(void)
{
    SECTION("A6 tope diario de novedad");
    pet_state_t s = fresh(60);
    s.stage = PET_BABY; s.born_s = T0;
    int exo = 0;
    uint32_t id = 1000;
    for (int day = 0; day < 3; day++) {
        int day_exo = 0;
        for (int scan = 0; scan < 20; scan++) {
            pet_ap_t aps[64];
            for (int i = 0; i < 64; i++) mk_ap(&aps[i], id++, false);
            int64_t t = T0 + day * DAY + 8 * 3600 + scan * 1200;
            int before = pet_pantry_count(&s, FOOD_EXOTIC);
            pet_ingest_scan(&s, aps, 64, t);
            day_exo += pet_pantry_count(&s, FOOD_EXOTIC) - before;
            s.pantry_n = 0;                      /* vaciamos para contar sin el tope de despensa */
        }
        CHECK(day_exo <= 12 && day_exo == 12, "dia %d: %d exoticos (esperado 12)", day, day_exo);
        exo += day_exo;
    }
    CHECK(exo == 36, "3 dias: 36 exoticos");
}

typedef struct { int n, juv_ok, adult_ok, branch_ok; int min_adult, max_adult; } agg_t;

static void test_a1_a2(void)
{
    SECTION("A1 rama por dieta (1000 semillas)");
    /* La dieta es lo que se COME (manual y picoteo). Tres escenarios:
     * (a) en casa y solo domesticos: dieta 0 % exotica -> Hogareno siempre.
     * (b) con salidas y 70 % exoticos manuales: dieta >= 60 % -> Explorador siempre.
     * (c) con salidas y solo domesticos manuales: el picoteo come lo mas antiguo,
     *     asi que la dieta depende del entorno; la rama debe seguir al eje exactamente. */
    int bad_a = 0, bad_b = 0, bad_c = 0, miss = 0, c_exp = 0, minb = 1000, maxa = 0;
    for (uint32_t seed = 0; seed < 1000; seed++) {
        for (int sc = 0; sc < 3; sc++) {
            pet_state_t s = fresh(seed);
            world_t w = { .rng = seed * 2654435761u + 1, .stay_home = sc == 0 };
            profile_t p = ACTIVE;
            p.exotic_pct = sc == 1 ? 70 : 0;
            outcome_t o = run_days(&s, &w, &p, 16);
            if (o.day_adult < 0) { miss++; continue; }
            int tot = o.exo_eaten + o.dom_eaten, axis = tot ? o.exo_eaten * 1000 / tot : 0;
            if (sc == 0) { if (axis > maxa) maxa = axis; if (o.branch != BRANCH_HOMEBODY) bad_a++; }
            if (sc == 1) { if (axis < minb) minb = axis; if (axis >= 600 && o.branch != BRANCH_EXPLORER) bad_b++; }
            if (sc == 2) { if ((axis >= 300) != (o.branch == BRANCH_EXPLORER)) bad_c++; c_exp += o.branch == BRANCH_EXPLORER; }
        }
    }
    printf("    (a) eje maximo en casa: %d permil; (b) eje minimo con 70%% manual: %d permil; (c) solo domesticos manuales con salidas: %d de 1000 Exploradores\n", maxa, minb, c_exp);
    CHECK(bad_a == 0, "(a) dieta 0%% exotica -> Hogareno: %d fallos", bad_a);
    CHECK(bad_b == 0 && minb >= 600, "(b) dieta >= 60%% -> Explorador: %d fallos, eje minimo %d", bad_b, minb);
    CHECK(bad_c == 0, "(c) la rama sigue al eje: %d fallos", bad_c);
    CHECK(miss == 0, "todas llegan a adulto en 16 dias (%d no)", miss);

    SECTION("A2 duracion del ciclo");
    struct { const char *name; profile_t p; bool home; int lo, hi; } sc[] = {
        { "activo (3 visitas, con salidas)", ACTIVE, false, 6, 8 },
        { "una visita al dia, con salidas", CASUAL, false, 1, 14 },
        { "una visita al dia, siempre en casa", CASUAL, true, 1, 14 },
        { "activo siempre en casa", ACTIVE, true, 1, 14 },
    };
    for (unsigned i = 0; i < sizeof(sc) / sizeof(sc[0]); i++) {
        int mn = 99, mx = 0, miss = 0, juv_mn = 99, juv_mx = 0, br[3] = {0};
        for (uint32_t seed = 0; seed < 200; seed++) {
            pet_state_t s = fresh(seed);
            world_t w = { .rng = seed + 5, .stay_home = sc[i].home };
            outcome_t o = run_days(&s, &w, &sc[i].p, 20);
            if (o.day_adult < 0) { miss++; continue; }
            if (o.day_adult < mn) mn = o.day_adult;
            if (o.day_adult > mx) mx = o.day_adult;
            if (o.day_juv < juv_mn) juv_mn = o.day_juv;
            if (o.day_juv > juv_mx) juv_mx = o.day_juv;
            br[o.branch]++;
        }
        printf("    %-38s adulto dia %d-%d (juvenil %d-%d), sin llegar: %d, A/B: %d/%d\n",
               sc[i].name, mn, mx, juv_mn, juv_mx, miss, br[BRANCH_HOMEBODY], br[BRANCH_EXPLORER]);
        CHECK(miss == 0, "%s: todas llegan a adulto", sc[i].name);
        CHECK(mn >= sc[i].lo && mx <= sc[i].hi, "%s: dia %d-%d fuera de [%d,%d]", sc[i].name, mn, mx, sc[i].lo, sc[i].hi);
    }
}

static void test_lifestyle_report(void)
{
    SECTION("calibracion: estilo de vida (informe)");
    struct { const char *name; profile_t p; } pr[] = { { "activo", ACTIVE }, { "casual", CASUAL } };
    int pcts[] = { 0, 20, 40, 60, 80, 100 };
    double prev_axis = -1;
    for (int k = 0; k < 2; k++) {
        prev_axis = -1;
        for (unsigned i = 0; i < sizeof(pcts) / sizeof(pcts[0]); i++) {
            long axis_sum = 0, errs = 0, sat = 0; int br[3] = {0}, var[4] = {0}, n = 0, dmax = 0;
            for (uint32_t seed = 0; seed < 300; seed++) {
                pet_state_t s = fresh(seed);
                world_t w = { .rng = seed + 11, .outing_pct = pcts[i] ? pcts[i] : 0, .stay_home = pcts[i] == 0 };
                outcome_t o = run_days(&s, &w, &pr[k].p, 14);
                int tot = o.exo_eaten + o.dom_eaten;
                axis_sum += tot ? o.exo_eaten * 1000 / tot : 0;
                errs += o.juv_errors; sat += s.satiety / PET_ONE;
                br[o.branch]++; var[o.variant]++; n++;
                if (o.day_adult > dmax) dmax = o.day_adult;
            }
            double axis = (double)axis_sum / n;
            printf("    %-6s salidas %3d%%: eje medio %4.0f permil, A/B %3d/%3d, variantes N/P/T/R %3d/%3d/%3d/%3d, errores juv medios %.1f, adulto dia<=%d\n",
                   pr[k].name, pcts[i], axis, br[1], br[2], var[0], var[1], var[2], var[3], (double)errs / n, dmax);
            CHECK(axis >= prev_axis, "mas salidas, mas dieta exotica");
            prev_axis = axis;
        }
    }
}

static void test_a7(void)
{
    SECTION("A7 determinismo");
    uint8_t a[PET_SAVE_MAX], b[PET_SAVE_MAX], c[PET_SAVE_MAX];
    pet_state_t s1 = fresh(70), s2 = fresh(70), s3 = fresh(71);
    world_t w1 = { .rng = 5 }, w2 = { .rng = 5 }, w3 = { .rng = 5 };
    run_days(&s1, &w1, &ACTIVE, 9);
    run_days(&s2, &w2, &ACTIVE, 9);
    run_days(&s3, &w3, &ACTIVE, 9);
    size_t la = pet_save(&s1, 1, a, sizeof(a)), lb = pet_save(&s2, 1, b, sizeof(b)), lc = pet_save(&s3, 1, c, sizeof(c));
    CHECK(la == lb && memcmp(a, b, la) == 0, "misma semilla y escenario: mismos bytes");
    CHECK(lc != la || memcmp(a, c, la) != 0, "otra semilla: otra partida");
    pet_state_t d1 = fresh(72), d2 = fresh(72);
    for (int i = 0; i < 500; i++) { pet_decide(&d1, T0 + 12 * 3600 + i * 10); pet_decide(&d2, T0 + 12 * 3600 + i * 10); }
    CHECK(memcmp(d1.hist, d2.hist, sizeof(d1.hist)) == 0 && d1.rng == d2.rng, "decisiones deterministas");
}

static void test_decisions_and_view(void)
{
    SECTION("decisiones y vista");
    pet_state_t s = fresh(80);
    s.stage = PET_JUVENILE; s.born_s = T0;
    int count[BEH_COUNT] = {0};
    int64_t t = T0 + 12 * 3600;
    s.satiety = s.mood = s.energy = 70 * PET_ONE;
    pet_behavior_t last = BEH_COUNT; int repeats = 0;
    for (int i = 0; i < 600; i++) {
        pet_behavior_t b = pet_decide(&s, t + i * 10);
        count[b]++;
        if (b == last) repeats++;
        last = b;
    }
    int variety = 0;
    for (int i = 0; i < BEH_COUNT; i++) variety += count[i] > 0;
    CHECK(variety >= 4, "variedad de comportamientos (%d)", variety);
    CHECK(repeats < 600 / 3, "la penalizacion por recencia evita repetir (%d repeticiones)", repeats);
    CHECK(count[BEH_SLEEP] == 0, "de dia no duerme");
    for (int i = 0; i < 20; i++) CHECK(pet_decide(&s, T0 + 2 * 3600 + i) == BEH_SLEEP, "de noche solo duerme");

    pet_view_t v;
    pet_state_t e = fresh(81);
    pet_view(&e, T0 + 12 * 3600, 0, &v);
    CHECK(v.stage == PET_EGG && v.shape < pet_shape_count(PET_EGG), "huevo");
    pet_state_t ad = fresh(82); ad.stage = PET_ADULT; ad.branch = BRANCH_EXPLORER; ad.variant = VAR_ODD;
    ad.satiety = ad.mood = ad.energy = 90 * PET_ONE; ad.last_novel_s = T0 + 12 * 3600;
    pet_anim_t an = pet_view(&ad, T0 + 12 * 3600, 0, &v);
    CHECK(v.shape == 1 && v.variant == VAR_ODD && v.expr == EXPR_HAPPY && an == ANIM_IDLE, "adulto explorador raro y feliz");
    an = pet_view(&ad, T0 + 2 * 3600, 0, &v);
    CHECK(v.expr == EXPR_SLEEP && an == ANIM_SLEEP, "de noche duerme");
    pet_touch(&ad, T0 + 12 * 3600, 500);
    pet_view(&ad, T0 + 12 * 3600, 600, &v);
    CHECK(v.expr == EXPR_HAPPY, "reaccion de caricia");
    pet_usb_changed(&ad, true, 10000);
    pet_view(&ad, T0 + 12 * 3600, 10100, &v);
    CHECK(v.expr == EXPR_LOVE, "USB: ojos de corazon");
    pet_view(&ad, T0 + 12 * 3600, 20000, &v);
    CHECK(v.expr != EXPR_LOVE, "la reaccion caduca");
}

static void test_boredom_and_crack(void)
{
    SECTION("aburrimiento y grietas del huevo");
    pet_state_t s = fresh(20);
    s.stage = PET_BABY;
    const int64_t d = T0 + 12 * 3600, t = d + 3 * 3600;
    s.satiety = s.energy = s.mood = 80 * PET_ONE;
    s.novel_until_s = 0; s.last_novel_s = d;
    s.last_s = t - 1;                                   /* sin hueco de ausencia */
    CHECK(pet_mood(&s, t) == MOOD_BORED, "aburrida tras 3 h sin novedad");
    s.pantry[s.pantry_n++] = (pet_bite_t){ FOOD_DOMESTIC, t - 60 };
    CHECK(pet_feed(&s, FOOD_DOMESTIC, t), "come");
    CHECK(s.last_novel_s == t && pet_mood(&s, t) != MOOD_BORED, "comer reinicia el aburrimiento");
    s.last_novel_s = d; s.last_s = t - 1;
    CHECK(pet_touch(&s, t, 1000) == REACT_PET && s.last_novel_s == t, "una caricia reinicia el aburrimiento");
    pet_state_t n = fresh(21);
    n.stage = PET_BABY; n.last_s = t - 1; n.last_novel_s = d;
    n.satiety = n.energy = n.mood = 80 * PET_ONE;
    pet_ap_t ap; mk_ap(&ap, 7, false);
    pet_ingest_scan(&n, &ap, 1, t);
    CHECK(pet_mood(&n, t) == MOOD_CURIOUS, "una senal nueva sigue dando curiosidad");

    pet_state_t e = fresh(22);
    pet_view_t v0, v1, v2;
    int cr[3] = { 0, PET_EGG_HATCH_S * 6 / 10, PET_EGG_HATCH_S * 9 / 10 };
    pet_view_t *vv[3] = { &v0, &v1, &v2 };
    for (int i = 0; i < 3; i++) { e.egg_active_s = (uint16_t)cr[i]; pet_view(&e, T0, 0, vv[i]); }
    CHECK(v0.crack == 0 && v1.crack == 1 && v2.crack == 2, "grietas 0/1/2 segun el progreso: %d %d %d", v0.crack, v1.crack, v2.crack);
    static uint8_t a[PET_W * PET_H], b[PET_W * PET_H], c[PET_W * PET_H];
    pet_render_creature(&v0, a, PET_W, PET_H); pet_render_creature(&v1, b, PET_W, PET_H); pet_render_creature(&v2, c, PET_W, PET_H);
    int d01 = 0, d12 = 0;
    for (int i = 0; i < PET_W * PET_H; i++) { d01 += a[i] != b[i]; d12 += b[i] != c[i]; }
    CHECK(d01 >= 4 && d12 >= 4, "las grietas se ven (%d y %d px)", d01, d12);
    uint16_t px[PET_FOOD_ICON * PET_FOOD_ICON * 4], px2[PET_FOOD_ICON * PET_FOOD_ICON * 4];
    pet_food_icon_rgb565(0, 2, 0x1234, px); pet_food_icon_rgb565(1, 2, 0x1234, px2);
    CHECK(px[0] == 0x1234 && memcmp(px, px2, sizeof(px)) != 0, "iconos de comida distintos y con fondo");
}

int main(void)
{
    test_primitives();
    test_familiarity();
    test_night_and_mood();
    test_stats();
    test_evolution();
    test_a10();
    test_clumsy_reachable();
    test_persistence();
    test_fuzz_ops();
    test_a6();
    test_a7();
    test_decisions_and_view();
    test_boredom_and_crack();
    test_a1_a2();
    test_lifestyle_report();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
