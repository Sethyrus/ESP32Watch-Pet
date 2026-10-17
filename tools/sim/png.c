#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_tab[256];
static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320U ^ (c >> 1) : c >> 1;
        crc_tab[n] = c;
    }
}
static uint32_t crc_upd(uint32_t c, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ b[i]) & 0xff] ^ (c >> 8);
    return c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8];
    be32(h, n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    uint32_t c = crc_upd(0xffffffffU, (const uint8_t *)type, 4);
    if (n) c = crc_upd(c, d, n);
    be32(h, ~c);
    fwrite(h, 1, 4, f);
}

int png_write_rgb(const char *path, const uint32_t *rgb, int w, int h)
{
    crc_init();
    size_t rowb = (size_t)w * 3 + 1, raw = rowb * h;
    uint8_t *buf = malloc(raw);
    if (!buf) return -1;
    for (int y = 0; y < h; y++) {
        uint8_t *r = buf + rowb * y;
        *r++ = 0;
        for (int x = 0; x < w; x++) {
            uint32_t c = rgb[(size_t)y * w + x];
            *r++ = c >> 16; *r++ = c >> 8; *r++ = c;
        }
    }
    size_t nblk = (raw + 65534) / 65535, zlen = 2 + raw + nblk * 5 + 4;
    uint8_t *z = malloc(zlen), *p = z;
    if (!z) { free(buf); return -1; }
    *p++ = 0x78; *p++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + buf[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off > 65535 ? 65535 : raw - off;
        *p++ = (off + n >= raw); *p++ = n; *p++ = n >> 8; *p++ = ~n; *p++ = (~n) >> 8;
        memcpy(p, buf + off, n); p += n;
    }
    be32(p, (b << 16) | a); p += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); free(z); return -1; }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    uint8_t ih[13];
    fwrite(sig, 1, 8, f);
    be32(ih, w); be32(ih + 4, h); ih[8] = 8; ih[9] = 2; ih[10] = ih[11] = ih[12] = 0;
    chunk(f, "IHDR", ih, 13);
    chunk(f, "IDAT", z, (uint32_t)(p - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(buf); free(z);
    return 0;
}
