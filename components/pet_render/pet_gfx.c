#include "pet_gfx.h"
#include <string.h>

/* ---- Iconos 12x12 ----
 * o contorno, B base, L claro, S sombra (colores del icono);
 * fijos: W blanco, K oscuro, G verde, Y oro, R rojo, U azul. */
typedef struct { uint32_t o, b, l, s; const char *r[PET_ICON]; } icon_t;

static const icon_t ICONS[ICON_COUNT] = {
    [ICON_BREAD] = {0x5a3a1e, 0xd9a066, 0xf4d29c, 0xa0663c, {
        "............", "...oooooo...", "..oLLLLLLo..", ".oLBBBBBBLo.", ".oBBLBBLBBo.", ".oBBBBBBBBo.",
        ".oBBBBBBBBo.", ".oSBBBBBBSo.", ".oSSSSSSSSo.", "..oooooooo..", "............", "............"}},
    [ICON_FRUIT] = {0x5d1f2e, 0xd83a3a, 0xff8a7a, 0x8a1f2e, {
        "......K.....", ".....KGG....", "...oo.Koo...", "..oBBooBBo..", ".oBWBBBBBBo.", ".oBWBBBBBBo.",
        ".oBBBBBBBBo.", ".oBBBBBBBSo.", "..oBBBBBSo..", "..oBBBBBSo..", "...oBooBo...", "....o..o...."}},
    [ICON_STEW] = {0x29366f, 0xef7d57, 0xffcd75, 0x3b5dc9, {
        "..W..W..W...", "...W..W..W..", "..W..W..W...", "............", "oooooooooooo", "oLLLBLLBLLLo",
        "oBBBBBBBBBBo", ".oSSSSSSSSo.", ".oSSSSSSSSo.", "..oSSSSSSo..", "...oooooo...", "............"}},
    [ICON_CAKE] = {0x5d275d, 0xf28ab6, 0xffcd75, 0xb13e53, {
        ".....R......", ".....o......", "..oooooooo..", ".oWWWWWWWWo.", ".oWSWWSWWWo.", ".oLLLLLLLLo.",
        ".oBBBBBBBBo.", ".oLLLLLLLLo.", ".oBBBBBBBBo.", ".oooooooooo.", "............", "............"}},
    [ICON_TREAT] = {0x29366f, 0x73eff7, 0x41a6f6, 0x29366f, {
        "............", "..o......o..", ".oLo.oo.oLo.", ".oLLoBBoLLo.", "..oLBBBBLo..", "...oBWBBo...",
        "...oBBBBo...", "..oLBBBBLo..", ".oLLoBBoLLo.", ".oLo.oo.oLo.", "..o......o..", "............"}},
    [ICON_MEDICINE] = {0x5d275d, 0xb13e53, 0xef7d57, 0x7a2a3a, {
        "............", "......oooo..", ".....oWWWWo.", "....oWWWWWo.", "...oBoWWWWo.", "..oBBBoWWo..",
        ".oBBBBBoo...", ".oBLBBBo....", ".oBBBBo.....", "..oBBo......", "...oo.......", "............"}},
    [ICON_POOP] = {0x3d2617, 0x8b5a2b, 0xc28a4a, 0x5e3a1c, {
        "............", ".....oo.....", "....oBBo....", "...oBLBBo...", "..ooBBBBoo..", ".oBBLBBBBBo.",
        ".oBBBBBBSBo.", "oBLBBBBBBBSo", "oBBBBBBBBBSo", ".oSSSSSSSSo.", "..oooooooo..", "............"}},
    [ICON_COIN] = {0x8a5a19, 0xffcd75, 0xfff4c2, 0xd99a2b, {
        "...oooooo...", "..oLLLLLLo..", ".oLLBBBBBBo.", "oLLBBLLBBBSo", "oLBBBLBBBBSo", "oLBBBLBBBBSo",
        "oLBBBLBBBBSo", "oLBBBLBBBBSo", "oBBBLLLBBSSo", ".oBBBBBBSSo.", "..oSSSSSSo..", "...oooooo..."}},
    [ICON_HEART] = {0x5d275d, 0xe43b44, 0xff9eb1, 0xa82a4a, {
        "............", ".ooo....ooo.", "oLLBo..oBBBo", "oLBBBooBBBBo", "oBBBBBBBBBBo", "oBBBBBBBBBSo",
        ".oBBBBBBBSo.", "..oBBBBBSo..", "...oBBBSo...", "....oBSo....", ".....oo.....", "............"}},
    [ICON_STAR] = {0x8a5a19, 0xffcd75, 0xfff4c2, 0xd99a2b, {
        ".....oo.....", ".....oLo....", "....oLBo....", "oooooLBBoooo", "oLLLLBBBBBSo", ".oLBBBBBBSo.",
        "..oBBBBBBo..", "..oBBBBBBo..", ".oBBBooBBBo.", ".oBSo..oSBo.", "oSoo....ooSo", "oo........oo"}},
    [ICON_BOLT] = {0x5a4a1a, 0xffe14d, 0xfff4c2, 0xd99a2b, {
        ".......ooo..", "......oLBo..", ".....oLBo...", "....oLBo....", "...oBBBooo..", "..oBBBBBBo..",
        "..oooBBBo...", "....oBBo....", "...oBSo.....", "...oSo......", "..oSo.......", "..oo........"}},
    [ICON_SPIKY] = {0x3b1f4f, 0x8b4fbf, 0xb983e0, 0x5e2f80, {
        "..o..o..o...", "...o.o.o....", ".o.oooooo.o.", "..oLBBBBBo..", "ooBWKBBWKBoo", "..oBBBBBBo..",
        "ooBBKKKKBBoo", "..oBBBBBSo..", ".o.oooooo.o.", "...o.o.o....", "..o..o..o...", "............"}},
    [ICON_BALL] = {0x5d275d, 0xe43b44, 0xff9eb1, 0xa82a4a, {
        "...oooooo...", "..oWWBBWWo..", ".oWWBBBBWWo.", "oWWWBBBBWWWo", "oBBBWWWWBBBo", "oBBWWWWWWBBo",
        "oBBWWWWWWBBo", "oBBBWWWWBBBo", "oWWWBBBBWWWo", ".oWWBBBBWWo.", "..oWWBBWWo..", "...oooooo..."}},
    [ICON_SOAP] = {0x257179, 0x41a6f6, 0x73eff7, 0x257179, {
        "......ooo...", ".....oWLLo..", ".....oLLLo..", "..ooo.ooo...", ".oWLLo......", ".oLLLo.oooo.",
        "..ooo.oWLLLo", "......oLLLLo", "..oo..oLLLLo", ".oWLo..oooo.", ".oLLo.......", "..oo........"}},
    [ICON_MOON] = {0x5a4a1a, 0xffcd75, 0xfff4c2, 0xd99a2b, {
        "....oooo....", "..ooLBBBo...", ".oLBBBoo....", ".oLBBo......", "oLBBo.......", "oBBBo.......",
        "oBBBo.......", "oBBBBo......", ".oBBBBoo...o", ".oSBBBBBoooo", "..ooSSSSSo..", "....oooo...."}},
    [ICON_MENU] = {0x566c86, 0xf4f4f4, 0xf4f4f4, 0x94b0c2, {
        "............", "............", "............", "............", ".oo..oo..oo.", "oBBooBBooBBo",
        "oSSooSSooSSo", ".oo..oo..oo.", "............", "............", "............", "............"}},
    [ICON_FOOD] = {0x3d2617, 0xc28a4a, 0xe8b878, 0x8b5a2b, {
        "......oooo..", ".....oLBBBo.", "....oLBBBBBo", "....oBBBBBBo", "....oBBBBBSo", "....oBBBBSo.",
        "...ooBSSSo..", "..oWooooo...", ".oWWo.......", "oWWWo.......", ".ooWo.......", "...o........"}},
    [ICON_LOCK] = {0x8a5a19, 0xffcd75, 0xfff4c2, 0xd99a2b, {
        "....oooo....", "...oWWWWo...", "..oWo..oWo..", "..oWo..oWo..", ".oooooooooo.", ".oLLLLLLLLo.",
        ".oBBBooBBBo.", ".oBBBooBBBo.", ".oBBBBBBBBo.", ".oSSSSSSSSo.", ".oooooooooo.", "............"}},
    [ICON_SIGNAL] = {0x73eff7, 0x73eff7, 0xf4f4f4, 0x41a6f6, {
        "............", "..oooooooo..", ".o........o.", "o..oooooo..o", "..o......o..", ".o..oooo..o.",
        "...o....o...", "..o..oo..o..", ".....LB.....", "....SBBS....", ".....SS.....", "............"}},
    [ICON_DUMBBELL] = {0x1a1c2c, 0x94b0c2, 0xf4f4f4, 0x566c86, {
        "............", "............", ".oo......oo.", "oLBo....oLBo", "oLBooooooLBo", "oBBLLLLLLBBo",
        "oBBSSSSSSBBo", "oSBooooooSBo", "oSBo....oSBo", ".oo......oo.", "............", "............"}},
    [ICON_GRID] = {0x1a1c2c, 0xf4f4f4, 0xf4f4f4, 0xf4f4f4, {
        "............", ".oooo..oooo.", ".oRRo..oGGo.", ".oRRo..oGGo.", ".oooo..oooo.", "............",
        "............", ".oooo..oooo.", ".oUUo..oYYo.", ".oUUo..oYYo.", ".oooo..oooo.", "............"}},
};

uint32_t pet_icon_pixel(pet_icon_t icon, int x, int y)
{
    if (icon >= ICON_COUNT || x < 0 || y < 0 || x >= PET_ICON || y >= PET_ICON) return 0;
    const icon_t *ic = &ICONS[icon];
    if (!ic->r[y]) return 0;
    switch (ic->r[y][x]) {
    case 'o': return ic->o | 0x01000000u;     /* bit 24: opaco (permite el negro) */
    case 'B': return ic->b | 0x01000000u;
    case 'L': return ic->l | 0x01000000u;
    case 'S': return ic->s | 0x01000000u;
    case 'W': return 0xf4f4f4 | 0x01000000u;
    case 'K': return 0x1a1c2c | 0x01000000u;
    case 'G': return 0x38b764 | 0x01000000u;
    case 'Y': return 0xffcd75 | 0x01000000u;
    case 'R': return 0xe43b44 | 0x01000000u;
    case 'U': return 0x41a6f6 | 0x01000000u;
    default:  return 0;
    }
}

void pet_icon_rgb565(pet_icon_t icon, int scale, uint16_t bg, uint16_t *dst)
{
    if (scale < 1) return;
    const int n = PET_ICON * scale;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            uint32_t c = pet_icon_pixel(icon, x / scale, y / scale);
            dst[y * n + x] = c ? pet_rgb565(c) : bg;
        }
}

/* ---- primitivas ---- */
void pet_gfx_fill(pet_gfx_t *g, int x, int y, int w, int h, uint16_t c)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > g->w ? g->w : x + w, y1 = y + h > g->h ? g->h : y + h;
    for (int j = y0; j < y1; j++) {
        uint16_t *row = g->px + (size_t)j * g->w;
        for (int i = x0; i < x1; i++) row[i] = c;
    }
}

void pet_gfx_rrect(pet_gfx_t *g, int x, int y, int w, int h, int r, uint16_t c)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int inset = 0;
        int dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
        if (dy) {
            while (inset < r && (r - inset) * (r - inset) + dy * dy > r * r + r) inset++;
        }
        pet_gfx_fill(g, x + inset, y + j, w - 2 * inset, 1, c);
    }
}

void pet_gfx_icon(pet_gfx_t *g, pet_icon_t icon, int x, int y, int scale)
{
    for (int j = 0; j < PET_ICON; j++)
        for (int i = 0; i < PET_ICON; i++) {
            uint32_t c = pet_icon_pixel(icon, i, j);
            if (c) pet_gfx_fill(g, x + i * scale, y + j * scale, scale, scale, pet_rgb565(c));
        }
}

void pet_gfx_creature(pet_gfx_t *g, const pet_view_t *v, int x, int y, int scale)
{
    uint8_t idx[PET_W * PET_H];
    uint16_t pal[PAL_COUNT];
    if (!pet_render_creature(v, idx, PET_W, PET_H)) return;
    pet_palette_rgb565(v->seed, pal);
    for (int j = 0; j < PET_H; j++) {
        int yy = y + j * scale;
        if (yy + scale <= 0 || yy >= g->h) continue;
        for (int i = 0; i < PET_W; i++) {
            uint8_t p = idx[j * PET_W + i];
            if (p) pet_gfx_fill(g, x + i * scale, yy, scale, scale, pal[p]);
        }
    }
}

/* ---- texto 3x5 ---- */
static const struct { char c; uint16_t bits; } FONT[] = {
#define F(c, s) {c, (uint16_t)(s)}
    F('A', 0x2BED), F('B', 0x6BAE), F('C', 0x3923), F('D', 0x6B6E), F('E', 0x79A7), F('F', 0x79A4),
    F('G', 0x396B), F('H', 0x5BED), F('I', 0x7497), F('J', 0x126A), F('K', 0x5BAD), F('L', 0x4927),
    F('M', 0x5FED), F('N', 0x6B6D), F('O', 0x2B6A), F('P', 0x6BA4), F('Q', 0x2B7B), F('R', 0x6BAD),
    F('S', 0x388E), F('T', 0x7492), F('U', 0x5B6F), F('V', 0x5B6A), F('W', 0x5BFD), F('X', 0x5AAD),
    F('Y', 0x5A92), F('Z', 0x72A7), F('0', 0x7B6F), F('1', 0x2C97), F('2', 0x62A7), F('3', 0x628E),
    F('4', 0x5BC9), F('5', 0x798E), F('6', 0x39EF), F('7', 0x7292), F('8', 0x7BEF), F('9', 0x7BCE),
    F('+', 0x05D0), F('-', 0x01C0), F(':', 0x0410), F('.', 0x0002), F('!', 0x2482), F('?', 0x6282),
    F('/', 0x12A4), F('%', 0x52A5),
#undef F
};

static uint16_t glyph(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    for (size_t k = 0; k < sizeof FONT / sizeof FONT[0]; k++) if (FONT[k].c == c) return FONT[k].bits;
    return 0;
}

int pet_gfx_text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * 4 - 1) * scale : 0;
}

int pet_gfx_text(pet_gfx_t *g, const char *s, int x, int y, int scale, uint16_t c)
{
    int x0 = x;
    for (; *s; s++, x += 4 * scale) {
        uint16_t b = glyph(*s);
        for (int j = 0; j < 5; j++)
            for (int i = 0; i < 3; i++)
                if (b >> (14 - (j * 3 + i)) & 1) pet_gfx_fill(g, x + i * scale, y + j * scale, scale, scale, c);
    }
    return x - x0 - scale;
}
