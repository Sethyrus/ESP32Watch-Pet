/* Simulador PC de Pet: hojas de contacto y comprobaciones automaticas del arte.
 * Uso: sim sheets <dir> [escala] | sim check */
#include "pet_render.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEED(f, e, p, salt) ((uint32_t)((f) | ((e) << 2) | ((p) << 4) | ((salt) << 6)))

typedef struct { uint32_t *px; int w, h; } canvas_t;

static canvas_t canvas_new(int w, int h)
{
    canvas_t c = { calloc((size_t)w * h, 4), w, h };
    return c;
}
static void fill(canvas_t *c, int x, int y, int w, int h, uint32_t col)
{
    for (int j = y; j < y + h && j < c->h; j++)
        for (int i = x; i < x + w && i < c->w; i++) if (i >= 0 && j >= 0) c->px[j * c->w + i] = col;
}
/* Pinta el lienzo indexado con escala entera. sil: silueta blanca. */
static void blit(canvas_t *c, const uint8_t *idx, const uint32_t *pal, int ox, int oy, int sc, bool sil)
{
    for (int y = 0; y < PET_H; y++)
        for (int x = 0; x < PET_W; x++) {
            uint8_t p = idx[y * PET_W + x];
            if (!p) continue;
            fill(c, ox + x * sc, oy + y * sc, sc, sc, sil ? 0xf4f4f4 : pal[p]);
        }
}

/* Fuente 3x5 para etiquetas. */
static const struct { char c; const char *bits; } FONT[] = {
    {'A',"010101111101101"},{'B',"110101110101110"},{'C',"011100100100011"},{'D',"110101101101110"},
    {'E',"111100110100111"},{'F',"111100110100100"},{'G',"011100101101011"},{'H',"101101111101101"},
    {'I',"111010010010111"},{'J',"001001001101010"},{'K',"101101110101101"},{'L',"100100100100111"},
    {'M',"101111111101101"},{'N',"110101101101101"},{'O',"010101101101010"},{'P',"110101110100100"},
    {'Q',"010101101111011"},{'R',"110101110101101"},{'S',"011100010001110"},{'T',"111010010010010"},
    {'U',"101101101101111"},{'V',"101101101101010"},{'W',"101101111111101"},{'X',"101101010101101"},
    {'Y',"101101010010010"},{'Z',"111001010100111"},{'0',"111101101101111"},{'1',"010110010010111"},
    {'2',"110001010100111"},{'3',"110001010001110"},{'4',"101101111001001"},{'5',"111100110001110"},
    {'6',"011100111101111"},{'7',"111001010010010"},{'8',"111101111101111"},{'9',"111101111001110"},
    {'-',"000000111000000"},{'.',"000000000000010"},{':',"000010000010000"},{'/',"001001010100100"},
};
static void label(canvas_t *c, int x, int y, const char *s)
{
    for (; *s; s++, x += 4) {
        for (size_t k = 0; k < sizeof FONT / sizeof FONT[0]; k++)
            if (FONT[k].c == *s)
                for (int j = 0; j < 5; j++)
                    for (int i = 0; i < 3; i++)
                        if (FONT[k].bits[j * 3 + i] == '1') fill(c, x + i, y + j, 1, 1, 0x94b0c2);
    }
}
static bool draw(const pet_view_t *v, canvas_t *c, int ox, int oy, int sc, bool sil)
{
    uint8_t idx[PET_W * PET_H];
    uint32_t pal[PAL_COUNT];
    if (!pet_render_creature(v, idx, PET_W, PET_H)) return false;
    pet_palette_rgb888(v->seed, pal);
    blit(c, idx, pal, ox, oy, sc, sil);
    return true;
}

/* Linaje: huevo, cria, juvenil, adulto A (hogareno), adulto B (explorador); misma semilla por fila. */
static int sheet_lineage(const char *path, int sc, FILE *legend)
{
    const int cell = PET_W * sc, n = 4;
    canvas_t c = canvas_new(5 * cell + 8 + 5 * 48, n * cell);
    fprintf(legend, "Linaje: filas = 4 semillas (coral/puntiagudas/liso, menta/redondas/barriga, cielo/antena/manchas, niebla/puntiagudas/mejillas); columnas = huevo, cria, juvenil, adulto Hogareno, adulto Explorador; derecha = siluetas a 1x\n");
    for (int r = 0; r < n; r++)
        for (int s = 0; s < 5; s++) {
            pet_view_t v = { SEED(r, r % 3, r, 11 + r), (uint8_t)(s < 4 ? s : 3), (uint8_t)(s == 4), EXPR_NEUTRAL, false, 0, 0, 0 };
            draw(&v, &c, s * cell, r * cell, sc, false);
            { static const char *const SN[5] = {"HUEVO", "CRIA", "JUVENIL", "HOGARENO", "EXPLORADOR"}; label(&c, s * cell + 3, r * cell + 3, SN[s]); }
            draw(&v, &c, 5 * cell + 8 + s * 48, r * cell, 1, true);
        }
    int rc = png_write_rgb(path, c.px, c.w, c.h);
    free(c.px);
    return rc;
}

/* C2: adultos x variantes de cuidado. */
static int sheet_adults(const char *path, int sc, FILE *legend)
{
    const int cell = PET_W * sc;
    canvas_t c = canvas_new(4 * cell, 4 * cell);
    fprintf(legend, "Adultos: columnas = normal, pulida, torpe, rara; filas = Hogareno coral, Explorador coral, Hogareno cielo, Explorador cielo\n");
    for (int r = 0; r < 4; r++)
        for (int vr = 0; vr < 4; vr++) {
            int fam = r < 2 ? 0 : 2;
            pet_view_t v = { SEED(fam, fam, fam == 0 ? 1 : 2, 7), PET_ADULT, (uint8_t)(r & 1), EXPR_NEUTRAL, false, 0, 0, (uint8_t)vr };
            draw(&v, &c, vr * cell, r * cell, sc, false);
            static const char *const VN[4] = {"NORMAL", "PULIDA", "TORPE", "RARA"};
            char lb[48];
            snprintf(lb, sizeof lb, "%s %s %s", (r & 1) ? "EXPLORADOR" : "HOGARENO", fam ? "CIELO" : "CORAL", VN[vr]);
            label(&c, vr * cell + 3, r * cell + 3, lb);
        }
    int rc = png_write_rgb(path, c.px, c.w, c.h);
    free(c.px);
    return rc;
}

/* C1: caras de cria y juvenil (coral y cielo). */
static int sheet_faces(const char *path, int sc, FILE *legend)
{
    const int cell = PET_W * sc;
    canvas_t c = canvas_new(EXPR_COUNT * cell, 6 * cell);
    fprintf(legend, "Caras: columnas = neutral, feliz, triste, sueno, hambre, sorpresa, enfado, amor; filas = cria, juvenil, Hogareno, Explorador (coral) y juvenil, Hogareno (cielo)\n");
    for (int r = 0; r < 6; r++)
        for (int e = 0; e < EXPR_COUNT; e++) {
            static const uint8_t stg[6] = {1, 2, 3, 3, 2, 3}, shp[6] = {0, 0, 0, 1, 0, 0}, fm[6] = {0, 0, 0, 0, 2, 2};
            pet_view_t v = { SEED(fm[r], fm[r], fm[r], 7), stg[r], shp[r], (uint8_t)e, false, 0, 0, 0 };
            draw(&v, &c, e * cell, r * cell, sc, false);
            { static const char *const EN[EXPR_COUNT] = {"NEUTRAL","FELIZ","TRISTE","SUENO","HAMBRE","SORPRESA","ENFADO","AMOR"};
              static const char *const SN[6] = {"CRIA","JUVENIL","HOGARENO","EXPLORADOR","JUVENIL","HOGARENO"};
              char lb[40]; snprintf(lb, sizeof lb, "%s %s", SN[r], EN[e]); label(&c, e * cell + 3, r * cell + 3, lb); }
        }
    int rc = png_write_rgb(path, c.px, c.w, c.h);
    free(c.px);
    return rc;
}

/* C3: tiras de animacion. Filas = (criatura x animacion), columnas = frames sucesivos. */
static int sheet_anim(const char *path, FILE *legend)
{
    static const char *const AN[ANIM_COUNT] = {"RESPIRAR+PARPADEO+MIRAR", "BOTAR", "COMER", "DORMIR"};
    static const int step_ms[ANIM_COUNT] = {200, 80, 100, 300};   /* separacion entre frames de la tira */
    const int sc = 2, cell = PET_W * sc, cols = 12;
    struct { const char *name; uint8_t stage, shape, fam; } cr[2] = { {"CRIA CORAL", PET_BABY, 0, 0}, {"EXPLORADOR CIELO", PET_ADULT, 1, 2} };
    canvas_t c = canvas_new(cols * cell, 2 * ANIM_COUNT * cell);
    fprintf(legend, "Animaciones: filas = criatura x animacion; columnas = frames sucesivos separados 200 ms (respirar), 80 (botar), 100 (comer), 300 (dormir)\n");
    for (int k = 0; k < 2; k++)
        for (int a = 0; a < ANIM_COUNT; a++) {
            int row = k * ANIM_COUNT + a;
            for (int f = 0; f < cols; f++) {
                pet_view_t v = { SEED(cr[k].fam, cr[k].fam, cr[k].fam, 7), cr[k].stage, cr[k].shape, EXPR_NEUTRAL, false, 0, 0, 0, 0, 0, 0, false };
                pet_pose(&v, (pet_anim_t)a, (uint32_t)(f * step_ms[a]));
                draw(&v, &c, f * cell, row * cell, sc, false);
            }
            char lb[64];
            snprintf(lb, sizeof lb, "%s %s", cr[k].name, AN[a]);
            label(&c, 3, row * cell + 3, lb);
        }
    int rc = png_write_rgb(path, c.px, c.w, c.h);
    free(c.px);
    return rc;
}

/* ---- Comprobaciones automaticas ---- */
static double lum(uint32_t c)
{
    double ch[3] = { (c >> 16 & 255) / 255.0, (c >> 8 & 255) / 255.0, (c & 255) / 255.0 };
    for (int i = 0; i < 3; i++) ch[i] = ch[i] <= 0.03928 ? ch[i] / 12.92 : __builtin_pow((ch[i] + 0.055) / 1.055, 2.4);
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
}
static double ratio_vs_black(uint32_t c) { return (lum(c) + 0.05) / 0.05; }

static int check(void)
{
    int fails = 0;
    static const char *fam[4] = {"coral", "menta", "cielo", "niebla"};
    printf("== Contraste contra negro (contorno >= 1.8:1, sombra y base >= 3:1) ==\n");
    for (int f = 0; f < 4; f++) {
        uint32_t pal[PAL_COUNT];
        pet_palette_rgb888(SEED(f, 0, 0, 0), pal);
        double o = ratio_vs_black(pal[PAL_OUTLINE]), b = ratio_vs_black(pal[PAL_BASE]);
        double s = ratio_vs_black(pal[PAL_SHADOW]);
        printf("%-7s contorno %.2f%s  sombra %.2f%s  base %.2f%s\n", fam[f], o, o < 1.8 ? " FALLA" : "",
               s, s < 3 ? " FALLA" : "", b, b < 3 ? " FALLA" : "");
        fails += (o < 1.8) + (s < 3) + (b < 3);
    }
    printf("== Pixeles huerfanos, cara dentro del cuerpo, expresiones ==\n");
    for (int st = 0; st < PET_STAGE_COUNT; st++)
        for (int sh = 0; sh < pet_shape_count(st); sh++)
            for (int fm = 0; fm < 4; fm++)
                for (int e = 0; e < (st == PET_EGG ? 1 : EXPR_COUNT); e++)
                    for (int sq = 0; sq < 2; sq++)
                    for (int vr = 0; vr < (st == PET_ADULT ? 4 : 1); vr++) {
                        pet_view_t v = { SEED(fm, fm % 3, fm, 9 + sh), (uint8_t)st, (uint8_t)sh, (uint8_t)e, sq, 0, 0, (uint8_t)vr };
                        uint8_t idx[PET_W * PET_H];
                        if (!pet_render_creature(&v, idx, PET_W, PET_H)) { printf("render fallo %d/%d\n", st, sh); fails++; continue; }
                        for (int y = 0; y < PET_H; y++)
                            for (int x = 0; x < PET_W; x++) {
                                uint8_t p = idx[y * PET_W + x];
                                if (!p) continue;
                                int n = 0, bgn = 0;
                                static const int d[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};
                                for (int k = 0; k < 4; k++) {
                                    int xx = x + d[k][0], yy = y + d[k][1];
                                    uint8_t q = (xx < 0 || yy < 0 || xx >= PET_W || yy >= PET_H) ? 0 : idx[yy * PET_W + xx];
                                    n += q != 0; bgn += q == 0;
                                }
                                if (n == 0 && p != PAL_WHITE && e != EXPR_ANGRY && e != EXPR_LOVE) { printf("huerfano %d/%d f%d e%d (%d,%d)\n", st, sh, fm, e, x, y); fails++; }
                                if ((p == PAL_DARK || p == PAL_BLUSH) && bgn && e != EXPR_LOVE && e != EXPR_SLEEP && e != EXPR_ANGRY)
                                    { printf("cara fuera del cuerpo %d/%d f%d e%d (%d,%d)\n", st, sh, fm, e, x, y); fails++; }
                                if (x == 0 || y == 0 || x == PET_W - 1)
                                    { printf("pixel en el borde del lienzo %d/%d (%d,%d)\n", st, sh, x, y); fails++; }
                            }
                    }
    printf("== Animaciones: marcos 0..5 s por etapa ==\n");
    for (int st = 0; st < PET_STAGE_COUNT; st++)
        for (int sh = 0; sh < pet_shape_count(st); sh++)
            for (int a = 0; a < ANIM_COUNT; a++)
                for (uint32_t ms = 0; ms < 5000; ms += PET_FRAME_MS) {
                    pet_view_t v = { SEED(1, 1, 1, 9), (uint8_t)st, (uint8_t)sh, EXPR_NEUTRAL, false, 0, 0, 0, 0, 0, 0, false };
                    uint8_t idx[PET_W * PET_H];
                    pet_pose(&v, (pet_anim_t)a, ms);
                    if (!pet_render_creature(&v, idx, PET_W, PET_H)) { printf("anim fallo %d/%d a%d t%u\n", st, sh, a, ms); fails++; continue; }
                    int n = 0;
                    for (int i = 0; i < PET_W * PET_H; i++) n += idx[i] != 0;
                    if (n < 100) { printf("anim vacia %d/%d a%d t%u\n", st, sh, a, ms); fails++; }
                }
    int minDiff = 9999, a0 = 0, b0 = 0;
    for (int sh = 0; sh < 2; sh++)
        for (int a = 0; a < EXPR_COUNT; a++)
            for (int b = a + 1; b < EXPR_COUNT; b++) {
                uint8_t ia[PET_W * PET_H], ib[PET_W * PET_H];
                pet_view_t va = { SEED(0, 0, 0, 7), (uint8_t)(1 + sh), 0, (uint8_t)a, false, 0, 0, 0 }, vb = va;
                vb.expr = (uint8_t)b;
                pet_render_creature(&va, ia, PET_W, PET_H); pet_render_creature(&vb, ib, PET_W, PET_H);
                int d = 0;
                for (int i = 0; i < PET_W * PET_H; i++) d += ia[i] != ib[i];
                if (d < minDiff) { minDiff = d; a0 = a; b0 = b; }
            }
    printf("distincion minima entre expresiones: %d px (expr %d vs %d)\n", minDiff, a0, b0);
    printf("%s (%d fallos)\n", fails ? "REVISAR" : "OK", fails);
    return fails != 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "check")) return check();
    if (argc >= 3 && !strcmp(argv[1], "sheets")) {
        int sc = argc > 3 ? atoi(argv[3]) : 3;
        char p[512], q[512];
        snprintf(p, sizeof p, "%s/c2_lineage.png", argv[2]);
        snprintf(q, sizeof q, "%s/c2_faces.png", argv[2]);
        char lp[512];
        snprintf(lp, sizeof lp, "%s/c2_legend.txt", argv[2]);
        FILE *lg = fopen(lp, "w");
        if (!lg) { perror(lp); return 1; }
        char ap[512];
        snprintf(ap, sizeof ap, "%s/c2_adults.png", argv[2]);
        char np[512];
        snprintf(np, sizeof np, "%s/c3_anim.png", argv[2]);
        int r = sheet_lineage(p, sc, lg) | sheet_faces(q, sc, lg) | sheet_adults(ap, sc, lg) | sheet_anim(np, lg);
        fclose(lg);
        if (!r) printf("%s\n%s\n%s\n", p, q, lp);
        return r;
    }
    fprintf(stderr, "uso: sim sheets <dir> [escala] | sim check\n");
    return 2;
}
