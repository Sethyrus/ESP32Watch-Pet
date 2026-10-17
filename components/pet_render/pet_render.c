#include "pet_render.h"
#include "pet_masks.h"
#include <string.h>

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

pet_traits_t pet_traits(uint32_t seed)
{
    pet_traits_t t;
    t.family  = seed & 3;
    t.ear     = ((seed >> 2) & 3) % 3;
    t.pattern = (seed >> 4) & 3;
    return t;
}

int pet_shape_count(pet_stage_t stage) { return stage < PET_STAGE_COUNT ? PET_MASK_COUNT[stage] : 0; }

const char *pet_shape_name(pet_stage_t stage, int shape)
{
    if (stage >= PET_STAGE_COUNT || shape < 0 || shape >= PET_MASK_COUNT[stage]) return "?";
    return PET_MASKS[stage][shape].name;
}

/* ---- Paleta ---- */
typedef struct { uint32_t outline, shadow, base, light, accent, blush; } ramp_t;
static const ramp_t RAMPS[4] = {
    {0x5d275d, 0xb13e53, 0xef7d57, 0xffcd75, 0x73eff7, 0xffcd75}, /* coral */
    {0x257179, 0x38b764, 0xa7f070, 0xf4f4f4, 0xffcd75, 0xef7d57}, /* menta */
    {0x29366f, 0x3b5dc9, 0x41a6f6, 0x73eff7, 0xef7d57, 0xef7d57}, /* cielo */
    {0x333c57, 0x566c86, 0x94b0c2, 0xf4f4f4, 0xef7d57, 0xb13e53}, /* niebla */
};

void pet_palette_rgb888(uint32_t seed, uint32_t out[PAL_COUNT])
{
    const ramp_t *r = &RAMPS[pet_traits(seed).family];
    out[PAL_BG] = 0x000000;     out[PAL_DARK] = 0x1a1c2c;
    out[PAL_OUTLINE] = r->outline; out[PAL_SHADOW] = r->shadow;
    out[PAL_BASE] = r->base;    out[PAL_LIGHT] = r->light;
    out[PAL_WHITE] = 0xf4f4f4;  out[PAL_ACCENT] = r->accent;
    out[PAL_BLUSH] = r->blush;
}

void pet_palette_rgb565(uint32_t seed, uint16_t out[PAL_COUNT])
{
    uint32_t c[PAL_COUNT];
    pet_palette_rgb888(seed, c);
    for (int i = 0; i < PAL_COUNT; i++)
        out[i] = (uint16_t)(((c[i] >> 8) & 0xF800) | ((c[i] >> 5) & 0x07E0) | ((c[i] >> 3) & 0x001F));
}

void pet_blit_scaled_rgb565(const uint8_t *idx, int w, int h, int scale,
                            const uint16_t *pal, uint16_t *dst, int dst_stride)
{
    for (int y = 0; y < h; y++) {
        uint16_t *row = dst + (size_t)y * scale * dst_stride;
        for (int x = 0; x < w; x++) {
            uint16_t c = pal[idx[y * w + x]];
            for (int k = 0; k < scale; k++) row[x * scale + k] = c;
        }
        for (int k = 1; k < scale; k++) memcpy(row + k * dst_stride, row, (size_t)w * scale * 2);
    }
}

/* ---- Cara ---- */
typedef struct { int w, h; const char *r[5]; } bmp_t;

static const bmp_t EYES[EXPR_COUNT] = {
    [EXPR_NEUTRAL]  = {4, 4, {".XX.", "XHXX", "XXXX", ".XX."}},
    [EXPR_HAPPY]    = {4, 4, {".XX.", "X..X", "....", "...."}},
    [EXPR_SAD]      = {4, 4, {"..XX", ".XXX", "XHXX", ".XX."}},
    [EXPR_SLEEP]    = {4, 4, {"....", "....", "XXXX", "...."}},
    [EXPR_HUNGRY]   = {4, 4, {".XX.", "XHHX", "XHXX", ".XX."}},
    [EXPR_SURPRISE] = {4, 4, {".XX.", "XHHX", "XHHX", ".XX."}},
    [EXPR_ANGRY]    = {4, 4, {"XX..", "XXX.", "XHXX", ".XX."}},
    [EXPR_LOVE]     = {4, 4, {".XX.", "X..X", "....", "...."}},
};
static const bmp_t EYES5[EXPR_COUNT] = {
    [EXPR_NEUTRAL]  = {5, 5, {".XXX.", "XHXXX", "XXXXX", "XXXXX", ".XXX."}},
    [EXPR_HAPPY]    = {5, 5, {".XXX.", "X...X", ".....", ".....", "....."}},
    [EXPR_SAD]      = {5, 5, {"...XX", "..XXX", "XHXXX", "XXXXX", ".XXX."}},
    [EXPR_SLEEP]    = {5, 5, {".....", ".....", ".....", "XXXXX", "....."}},
    [EXPR_HUNGRY]   = {5, 5, {".XXX.", "XHHHX", "XHXXX", "XXXXX", ".XXX."}},
    [EXPR_SURPRISE] = {5, 5, {".XXX.", "XHHHX", "XHHHX", "XHHHX", ".XXX."}},
    [EXPR_ANGRY]    = {5, 5, {"XX...", "XXXX.", "XHXXX", "XXXXX", ".XXX."}},
    [EXPR_LOVE]     = {5, 5, {".XXX.", "X...X", ".....", ".....", "....."}},
};
static const bmp_t MOUTHS[EXPR_COUNT] = {
    [EXPR_NEUTRAL]  = {4, 1, {".XX."}},
    [EXPR_HAPPY]    = {4, 2, {"X..X", ".XX."}},
    [EXPR_SAD]      = {4, 2, {".XX.", "X..X"}},
    [EXPR_SLEEP]    = {2, 1, {"XX"}},
    [EXPR_HUNGRY]   = {4, 3, {".XX.", "XrrX", ".XX."}},
    [EXPR_SURPRISE] = {2, 2, {"XX", "XX"}},
    [EXPR_ANGRY]    = {4, 1, {"XXXX"}},
    [EXPR_LOVE]     = {4, 2, {"XXXX", ".XX."}},
};

static void put(uint8_t *idx, int x, int y, char c)
{
    uint8_t p;
    switch (c) {
    case 'X': p = PAL_DARK; break;
    case 'H': p = PAL_WHITE; break;
    case 'r': p = PAL_BLUSH; break;
    case 'a': p = PAL_ACCENT; break;
    default: return;
    }
    if (x >= 0 && x < PET_W && y >= 0 && y < PET_H) idx[y * PET_W + x] = p;
}

static void draw_bmp(uint8_t *idx, const bmp_t *b, int x, int y, bool mirror)
{
    for (int j = 0; j < b->h; j++)
        for (int i = 0; i < b->w; i++)
            put(idx, x + i, y + j, b->r[j][mirror ? b->w - 1 - i : i]);
}

/* Adorno de 1 px de ancho por columna: rows[j] es una cadena de 3 caracteres como mucho. */
static void adorn(uint8_t *idx, int x, int y, int h, const char *const *rows)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; rows[j][i]; i++) put(idx, x + i, y + j, rows[j][i]);
}

/* ---- Criatura ---- */
bool pet_render_creature(const pet_view_t *v, uint8_t *idx, int w, int h)
{
    if (w != PET_W || h != PET_H || v->stage >= PET_STAGE_COUNT) return false;
    if (v->shape >= PET_MASK_COUNT[v->stage]) return false;
    const pet_mask_t *m = &PET_MASKS[v->stage][v->shape];
    const pet_traits_t t = pet_traits(v->seed);
    const uint32_t hs = v->seed >> 6;
    const int wh = m->w_half, x0 = (PET_W - 2 * wh) / 2, y0 = PET_H - m->h;

    uint8_t L[PET_H][PET_W];
    memset(L, 0, sizeof L);
    int ex = -1, ey = -1, my = -1, fx = -1, fy = -1, gx = -1, gy = -1;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < wh; x++) {
            char c = m->rows[y][x];
            int Y = y0 + y, xl = x0 + x, xr = PET_W - 1 - xl;
            bool body = false;
            switch (c) {
            case 'B': body = true; break;
            case 'b': body = mix(hs ^ (uint32_t)(x * 64 + y)) & 1; break;
            case 'O': L[Y][xl] = L[Y][xr] = 3; break;
            case 'E': body = true; if (ey < 0) { ex = x; ey = y; } break;
            case 'M': body = true; my = y; break;
            case 'F': fx = x; fy = y; break;
            case 'G': gx = x; gy = y; break;
            default: break;
            }
            if (body) L[Y][xl] = L[Y][xr] = 1;
        }

    /* orejas / antena (no en el huevo) */
    if (v->stage != PET_EGG) {
        const bool big = v->stage >= PET_JUVENILE && !(v->stage == PET_ADULT && v->shape == 0);
        const pet_piece_t *p = (big ? PET_EARS_L : PET_EARS) + t.ear;
        const int rdrop = v->variant == VAR_CLUMSY ? 2 : 0;
        int ax = (t.ear == 2) ? gx : fx, ay = (t.ear == 2) ? gy : fy;
        if (ax >= 0)
            for (int j = 0; j < p->h; j++)
                for (int i = 0; i < p->w; i++) {
                    char c = p->rows[j][i];
                    int Y = y0 + ay - (p->h - 1) + j, xl = x0 + ax - (p->w - 1) + i;
                    if (c == '.' || Y < 0 || xl < 0) continue;
                    L[Y][xl] = (c == 'a') ? 2 : 1;
                    if (Y + rdrop < PET_H) L[Y + rdrop][PET_W - 1 - xl] = (c == 'a') ? 2 : 1;
                }
    }

    /* extension vertical, squash y desplazamiento */
    int top = -1, bot = -1;
    for (int y = 0; y < PET_H; y++)
        for (int x = 0; x < PET_W; x++)
            if (L[y][x]) { if (top < 0) top = y; bot = y; }
    if (top < 0) return false;
    const int dyc = (top + v->dy < 1) ? 1 - top : v->dy;   /* nunca sale del lienzo */
    const int s = top + (bot - top) / 3;
    const int span = bot > top ? bot - top : 1;
#define SHEAR(srcy) (v->variant == VAR_CLUMSY ? ((bot - (srcy)) * 3 / span) : 0)
    uint8_t T[PET_H][PET_W];
    memset(T, 0, sizeof T);
    for (int y = top; y <= bot; y++) {
        int oy;
        if (v->squash) {
            if (y == s + 1) continue;
            oy = (y <= s) ? y + 1 : y;
        } else {
            oy = y;
        }
        oy += dyc;
        if (oy >= 0 && oy < PET_H) {
            int sx = SHEAR(y);
            for (int x = 0; x < PET_W; x++)
                if (L[y][x] && x + sx < PET_W) T[oy][x + sx] = L[y][x];
        }
    }
#define MAPY(yy) ((yy) + ((v->squash && (yy) <= s) ? 1 : 0) + dyc)
    const int ctop = top + (v->squash ? 1 : 0) + dyc, cbot = bot + dyc;

    memset(idx, PAL_BG, PET_W * PET_H);
    /* relleno + sombreado */
    for (int y = 0; y < PET_H; y++)
        for (int x = 0; x < PET_W; x++) {
            uint8_t c = T[y][x];
            if (!c) continue;
            uint8_t p = PAL_BASE;
            if (c == 2) p = PAL_ACCENT;
            else if (c == 3) p = PAL_OUTLINE;
            else {
                bool up = y == 0 || !T[y - 1][x], dn = y == PET_H - 1 || !T[y + 1][x];
                bool rt = x == PET_W - 1 || !T[y][x + 1];
                if (up) p = PAL_LIGHT;
                else if (dn || (rt && x >= PET_W / 2)) p = PAL_SHADOW;
            }
            idx[y * PET_W + x] = p;
        }
    /* contorno de color de 1 px */
    for (int y = 0; y < PET_H; y++)
        for (int x = 0; x < PET_W; x++) {
            if (T[y][x]) continue;
            bool nb = (y > 0 && (T[y - 1][x] == 1 || T[y - 1][x] == 2)) ||
                      (y < PET_H - 1 && (T[y + 1][x] == 1 || T[y + 1][x] == 2)) ||
                      (x > 0 && (T[y][x - 1] == 1 || T[y][x - 1] == 2)) ||
                      (x < PET_W - 1 && (T[y][x + 1] == 1 || T[y][x + 1] == 2));
            if (nb) idx[y * PET_W + x] = PAL_OUTLINE;
        }

    /* patrones */
    const int ht = cbot - ctop;
    if (v->stage == PET_EGG) {
        for (int y = ctop; y <= cbot; y++)
            for (int x = 0; x < PET_W; x++) {
                if (T[y][x] != 1) continue;
                int r = y - ctop;
                bool in = false;
                if (t.pattern == 1) in = r >= ht * 5 / 9 && r < ht * 5 / 9 + 3;
                else if (t.pattern == 2) in = (mix(hs ^ (uint32_t)((x < 24 ? x : 47 - x) * 64 + y)) % 11) == 0;
                else if (t.pattern == 3) in = r < ht / 3 && y > ctop;
                if (in) idx[y * PET_W + x] = (t.pattern == 3) ? PAL_LIGHT : PAL_ACCENT;
            }
    } else if (t.pattern == 1 || t.pattern == 2 || v->variant == VAR_ODD) {
        int by = cbot - ht / 4, rx = 5, ry = ht / 4 + 1;
        for (int y = ctop; y <= cbot; y++)
            for (int x = 0; x < PET_W; x++) {
                if (T[y][x] != 1) continue;
                if (t.pattern == 1 && v->variant != VAR_ODD) {
                    int dx2 = 2 * x + 1 - PET_W, dy2 = 2 * (y - by);
                    if (dx2 * dx2 * ry * ry + dy2 * dy2 * rx * rx <= 4 * rx * rx * ry * ry)
                        idx[y * PET_W + x] = PAL_LIGHT;
                } else if (T[y - 1][x] == 1 && T[y + 1][x] == 1 && T[y][x - 1] == 1 && T[y][x + 1] == 1 &&
                           (mix(hs ^ (uint32_t)((x < 24 ? x : 47 - x) * 64 + y)) % (v->variant == VAR_ODD ? 17 : 29)) == 0) {
                    idx[y * PET_W + x] = v->variant == VAR_ODD ? PAL_LIGHT : PAL_ACCENT;
                }
            }
    }
    /* brillo en el hombro izquierdo */
    {
        int gy2 = ctop + 3;
        for (int x = 0; x < PET_W / 2; x++)
            if (T[gy2][x] == 1) {
                if (T[gy2 + 1][x + 2] == 1) { idx[gy2 * PET_W + x + 2] = PAL_WHITE; idx[(gy2 + 1) * PET_W + x + 2] = PAL_WHITE; }
                break;
            }
    }

    if (v->variant == VAR_POLISHED) {
        int mx = 0;
        for (int y = ctop; y < ctop + 8 && y < PET_H; y++)
            for (int x = 0; x < PET_W; x++) if (T[y][x] && x > mx) mx = x;
        static const char *const SP[3] = {".H.", "HHH", ".H."};
        adorn(idx, mx + 2 > PET_W - 3 ? PET_W - 3 : mx + 2, ctop + 1, 3, SP);
        for (int x = 0; x < PET_W / 2; x++)   /* segundo brillo, mas largo */
            if (T[ctop + 5][x] == 1) { idx[(ctop + 5) * PET_W + x + 2] = PAL_WHITE; idx[(ctop + 6) * PET_W + x + 2] = PAL_WHITE; break; }
    }

    if (v->food && v->stage != PET_EGG && my >= 0) {
        int d = v->food + 1, fy = MAPY(y0 + my) + 3, fx = PET_W / 2 - d / 2;
        for (int j = 0; j < d; j++)
            for (int i = 0; i < d; i++) {
                int dx = 2 * i - (d - 1), dy2 = 2 * j - (d - 1);
                if (dx * dx + dy2 * dy2 <= (d - 1) * (d - 1) + d && fy + j < PET_H) idx[(fy + j) * PET_W + fx + i] = (i + j == 0) ? PAL_WHITE : PAL_ACCENT;
            }
    }

    /* cara */
    if (v->stage != PET_EGG && ey >= 0) {
        const bmp_t *eye = (v->stage >= PET_JUVENILE ? EYES5 : EYES) + (v->blink ? EXPR_SLEEP : (v->expr < EXPR_COUNT ? v->expr : 0));
        const bmp_t *mouth = &MOUTHS[v->mouth_open ? EXPR_HUNGRY : (v->expr < EXPR_COUNT ? v->expr : 0)];
        const int look = v->variant == VAR_ODD ? 1 : v->look_x;
        int exl = x0 + ex + look + SHEAR(y0 + ey), eyy = MAPY(y0 + ey);
        const int ryo = v->variant == VAR_CLUMSY ? 1 : 0;
        int exr = PET_W - 1 - (x0 + ex) - (eye->w - 1) + look + SHEAR(y0 + ey);
        bool blush = t.pattern == 3 || v->expr == EXPR_HAPPY || v->expr == EXPR_LOVE;
        if (blush) {
            int by = eyy + eye->h;
            for (int i = 0; i < 2; i++) {
                int bl = exl + i, br = exr + eye->w - 1 - i;
                if (T[by][bl]) idx[by * PET_W + bl] = PAL_BLUSH;
                if (T[by][br]) idx[by * PET_W + br] = PAL_BLUSH;
            }
        }
        draw_bmp(idx, eye, exl, eyy, false);
        draw_bmp(idx, eye, exr, eyy + ryo, true);
        if (my >= 0) draw_bmp(idx, mouth, PET_W / 2 - mouth->w / 2 - ryo + SHEAR(y0 + my), MAPY(y0 + my), false);
        const int ay = ctop > 6 ? ctop - 5 : 1, ax = PET_W - 13;
        const int ty = eyy + eye->h;   /* bajo el ojo */
        switch (v->expr) {
        case EXPR_LOVE:     adorn(idx, ax, ay, 3, (const char *[]){"r.r", "rrr", ".r."}); break;
        case EXPR_SLEEP:    adorn(idx, ax, ay - v->zstep, 3, (const char *[]){"HHH", ".H.", "HHH"}); break;
        case EXPR_SURPRISE: adorn(idx, ax + 1, ay - 1, 5, (const char *[]){"H", "H", "H", ".", "H"}); break;
        case EXPR_ANGRY:    adorn(idx, ax, ay, 3, (const char *[]){"r.r", ".r.", "r.r"}); break;
        case EXPR_SAD:      adorn(idx, exl, ty, 2, (const char *[]){"H", "H"}); break;
        case EXPR_HUNGRY:   adorn(idx, PET_W / 2 + 1, MAPY(y0 + my) + 3, 2, (const char *[]){"H", "H"}); break;
        default: break;
        }
    }
    return true;
}
