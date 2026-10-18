/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --bpp 1 --size 20 --no-compress --format lvgl --lv-include lvgl.h --font Jersey15.ttf -r 0x20-0x7E,0xA1,0xB0,0xB7,0xBF,0xC1,0xC9,0xCD,0xD1,0xD3,0xD7,0xDA,0xDC,0xE1,0xE9,0xED,0xF1,0xF3,0xFA,0xFC,0x2026 --font FontAwesome5.woff -r 0xF00C,0xF00D,0xF04B,0xF0E7 -o /Users/alejandro/Dev/ESP32Watch/ESP32Watch-Pet/components/pet_app/fonts/pet_font_s.c --lv-font-name pet_font_s
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PET_FONT_S
#define PET_FONT_S 1
#endif

#if PET_FONT_S

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf5, 0x3c,

    /* U+0022 "\"" */
    0xde, 0xf6, 0x0,

    /* U+0023 "#" */
    0xc, 0xc0, 0xcc, 0x3f, 0xf1, 0x98, 0x19, 0x83,
    0x30, 0x33, 0xf, 0xfc, 0x36, 0x3, 0x60,

    /* U+0024 "$" */
    0x8, 0x1f, 0x1f, 0xdd, 0x3c, 0x9f, 0x41, 0xfc,
    0x7f, 0xc9, 0xf4, 0xdf, 0xc7, 0xc0, 0x80, 0x40,

    /* U+0025 "%" */
    0x70, 0x4a, 0xa, 0x26, 0x6c, 0xc7, 0x20, 0x1c,
    0x3, 0x61, 0x9b, 0x72, 0x68, 0x2a, 0x6, 0x0,

    /* U+0026 "&" */
    0x3c, 0xf, 0xc1, 0x9c, 0x1f, 0x7, 0xc0, 0xf9,
    0xf3, 0xf6, 0x7e, 0xef, 0x9f, 0xfd, 0xf3, 0x80,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x6f, 0x6d, 0xb6, 0xdb, 0x66, 0xc0,

    /* U+0029 ")" */
    0xd9, 0xb6, 0xdb, 0x6d, 0xbd, 0x80,

    /* U+002A "*" */
    0x25, 0xbe, 0xe7, 0x0,

    /* U+002B "+" */
    0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18,

    /* U+002C "," */
    0xf6,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0xc, 0x30, 0x86, 0x18, 0xc3, 0xc, 0x31, 0x86,
    0x30, 0xc0,

    /* U+0030 "0" */
    0x7c, 0xfb, 0xbe, 0x3c, 0x78, 0xf1, 0xe3, 0xef,
    0xfd, 0xf0,

    /* U+0031 "1" */
    0xff, 0x33, 0x33, 0x33, 0x33, 0x30,

    /* U+0032 "2" */
    0x7c, 0xfb, 0xbe, 0x30, 0xe1, 0x8e, 0x38, 0x71,
    0xff, 0xf8,

    /* U+0033 "3" */
    0x7c, 0xfb, 0xbe, 0x30, 0xe7, 0x83, 0xe3, 0xee,
    0xf9, 0xf0,

    /* U+0034 "4" */
    0x6, 0x1c, 0xfb, 0xfe, 0x78, 0xf1, 0xff, 0xfe,
    0xc, 0x18,

    /* U+0035 "5" */
    0xff, 0xff, 0x6, 0xf, 0xff, 0xc3, 0xe3, 0xee,
    0xf9, 0xf0,

    /* U+0036 "6" */
    0x7c, 0xfb, 0xbe, 0x3f, 0xdf, 0xf1, 0xe3, 0xef,
    0xfd, 0xf0,

    /* U+0037 "7" */
    0xff, 0xf0, 0xc7, 0x1c, 0x73, 0x8e, 0x30, 0xc3,
    0x0,

    /* U+0038 "8" */
    0x7c, 0xff, 0xbe, 0x3e, 0xef, 0xfb, 0xe3, 0xee,
    0xfd, 0xf0,

    /* U+0039 "9" */
    0x7d, 0xff, 0xbe, 0x3c, 0x7f, 0xdf, 0xe3, 0xee,
    0xf9, 0xf0,

    /* U+003A ":" */
    0xf0, 0x3, 0xc0,

    /* U+003B ";" */
    0xf0, 0x3, 0xd8,

    /* U+003C "<" */
    0x3, 0xf, 0x1c, 0xf8, 0xf0, 0x7c, 0x1f, 0x3,

    /* U+003D "=" */
    0xff, 0xfc, 0x0, 0xf, 0xff, 0xc0,

    /* U+003E ">" */
    0xc0, 0xe0, 0x78, 0x1f, 0xf, 0x7c, 0xf8, 0xc0,

    /* U+003F "?" */
    0x78, 0xfb, 0xf8, 0xf1, 0xc7, 0x8, 0x0, 0x0,
    0x60, 0xc0,

    /* U+0040 "@" */
    0x3f, 0xc6, 0x6, 0xc0, 0x39, 0xf9, 0x90, 0x99,
    0x9, 0x90, 0x99, 0x8b, 0x8f, 0xee, 0x0, 0x7f,
    0xc0,

    /* U+0041 "A" */
    0x10, 0x71, 0xf7, 0x7c, 0x78, 0xff, 0xff, 0xc7,
    0x8f, 0x18,

    /* U+0042 "B" */
    0xfc, 0xfe, 0xcf, 0xc7, 0xcf, 0xfe, 0xff, 0xc7,
    0xcf, 0xfe, 0xfc,

    /* U+0043 "C" */
    0x7c, 0x7e, 0xef, 0xc7, 0xc0, 0xc0, 0xc0, 0xc7,
    0xef, 0x7e, 0x7c,

    /* U+0044 "D" */
    0xfd, 0xfb, 0x3e, 0x3c, 0x78, 0xf1, 0xe3, 0xcf,
    0xff, 0xf0,

    /* U+0045 "E" */
    0xff, 0xff, 0x6, 0xc, 0x1f, 0xbf, 0x60, 0xc1,
    0xff, 0xf8,

    /* U+0046 "F" */
    0xff, 0xff, 0x6, 0xc, 0x1f, 0xbf, 0x60, 0xc1,
    0x83, 0x0,

    /* U+0047 "G" */
    0x7c, 0xfb, 0x9e, 0xc, 0x1b, 0xf7, 0xe3, 0xef,
    0xfd, 0xf0,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x7f, 0xff, 0xe3, 0xc7,
    0x8f, 0x18,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0xf3, 0xfe,
    0xf8, 0xf0,

    /* U+004B "K" */
    0xc7, 0x8f, 0x1e, 0x3c, 0xff, 0xb3, 0xe3, 0xc7,
    0x8f, 0x18,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xff,
    0xc0,

    /* U+004D "M" */
    0xc0, 0x7c, 0x1f, 0xc7, 0xfd, 0xfd, 0xf7, 0x9c,
    0xf1, 0x1e, 0x3, 0xc0, 0x78, 0xf, 0x1, 0x80,

    /* U+004E "N" */
    0xe3, 0xe3, 0xf3, 0xf3, 0xfb, 0xdb, 0xdf, 0xcf,
    0xcf, 0xc7, 0xc7,

    /* U+004F "O" */
    0x7c, 0xfb, 0xbe, 0x3c, 0x78, 0xf1, 0xe3, 0xef,
    0xfd, 0xf0,

    /* U+0050 "P" */
    0xfd, 0xff, 0x3e, 0x3c, 0xff, 0xff, 0x60, 0xc1,
    0x83, 0x0,

    /* U+0051 "Q" */
    0x7c, 0xfb, 0xbe, 0x3c, 0x78, 0xf1, 0xe3, 0xef,
    0xfd, 0xf8, 0xf0,

    /* U+0052 "R" */
    0xfd, 0xff, 0x3e, 0x3c, 0x7f, 0xff, 0x66, 0xc7,
    0x8f, 0x18,

    /* U+0053 "S" */
    0x7e, 0x7e, 0xe7, 0xc3, 0xe0, 0x7e, 0x7f, 0xc7,
    0xe7, 0x7e, 0x7e,

    /* U+0054 "T" */
    0xff, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0x0,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xee,
    0xf9, 0xf0,

    /* U+0056 "V" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xf7, 0x7c,
    0x70, 0x40,

    /* U+0057 "W" */
    0xc6, 0x3c, 0x63, 0xc6, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3e, 0xf7, 0x7f, 0xe3, 0x9c, 0x10,
    0x80,

    /* U+0058 "X" */
    0xc7, 0x8f, 0x1b, 0x67, 0xcf, 0x9f, 0x36, 0xc7,
    0x8f, 0x18,

    /* U+0059 "Y" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0x7e, 0x3c, 0x18,
    0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0xff, 0x7, 0xf, 0x1c, 0x38, 0x78, 0xe0,
    0xc0, 0xff, 0xff,

    /* U+005B "[" */
    0xfc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf0,

    /* U+005C "\\" */
    0xc3, 0x4, 0x18, 0x60, 0xc3, 0xc, 0x30, 0x61,
    0x83, 0xc,

    /* U+005D "]" */
    0xf3, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf0,

    /* U+005E "^" */
    0x3c, 0x7e, 0x7e, 0xc3,

    /* U+005F "_" */
    0xff, 0xfc,

    /* U+0060 "`" */
    0xc9, 0x80,

    /* U+0061 "a" */
    0x7c, 0xfb, 0xfb, 0xfc, 0x78, 0xdf, 0xbb,

    /* U+0062 "b" */
    0xc1, 0x83, 0x6, 0xef, 0xfc, 0xf1, 0xe3, 0xc7,
    0xfb, 0xf0,

    /* U+0063 "c" */
    0x7c, 0x7e, 0xc7, 0xc0, 0xc0, 0xc7, 0x7e, 0x7c,

    /* U+0064 "d" */
    0x6, 0xc, 0x1b, 0xff, 0xf8, 0xf1, 0xe3, 0xce,
    0xfd, 0xd8,

    /* U+0065 "e" */
    0x7c, 0xfb, 0x1e, 0x3f, 0xf8, 0x3f, 0xbe,

    /* U+0066 "f" */
    0x1c, 0x63, 0xc, 0xfc, 0xc3, 0xc, 0x30, 0xc3,
    0x0,

    /* U+0067 "g" */
    0x7f, 0xff, 0x1e, 0x3c, 0x7f, 0xdd, 0x83, 0xc6,
    0xd9, 0xf0,

    /* U+0068 "h" */
    0xc1, 0x83, 0x6, 0xef, 0xdc, 0xf1, 0xe3, 0xc7,
    0x8f, 0x18,

    /* U+0069 "i" */
    0xf0, 0x3f, 0xfc,

    /* U+006A "j" */
    0x33, 0x0, 0x33, 0x33, 0x33, 0x33, 0x6e,

    /* U+006B "k" */
    0xc3, 0xc, 0x33, 0xcf, 0x7f, 0xbe, 0xcf, 0x3c,
    0xc0,

    /* U+006C "l" */
    0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xdc, 0xef, 0xde, 0xe7, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3c, 0x63,

    /* U+006E "n" */
    0xdd, 0xfb, 0x9e, 0x3c, 0x78, 0xf1, 0xe3,

    /* U+006F "o" */
    0x7d, 0xff, 0x1e, 0x3c, 0x78, 0xdf, 0x3e,

    /* U+0070 "p" */
    0xdd, 0xfb, 0x9e, 0x3c, 0x78, 0xff, 0xfe, 0xc1,
    0x83, 0x0,

    /* U+0071 "q" */
    0x7e, 0xff, 0x1e, 0x3c, 0x79, 0xff, 0xbb, 0x6,
    0xc, 0x18,

    /* U+0072 "r" */
    0xdf, 0xfe, 0xb0, 0xc3, 0xc, 0x30,

    /* U+0073 "s" */
    0x7c, 0x6e, 0xc0, 0x7c, 0xf, 0xc7, 0x6e, 0x7c,

    /* U+0074 "t" */
    0x10, 0xc3, 0x3f, 0x30, 0xc3, 0x4, 0x18, 0x70,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x79, 0xdf, 0xbb,

    /* U+0076 "v" */
    0xc7, 0x8f, 0x1e, 0x3e, 0xef, 0x8e, 0x8,

    /* U+0077 "w" */
    0xc6, 0x3c, 0x63, 0xc6, 0x3c, 0x63, 0xef, 0x77,
    0xfe, 0x7f, 0xe1, 0x8,

    /* U+0078 "x" */
    0xc7, 0xdd, 0xf3, 0xe7, 0xcd, 0xb1, 0xe3,

    /* U+0079 "y" */
    0xc7, 0x8f, 0x1e, 0x3e, 0xef, 0x8e, 0x38, 0x71,
    0x80,

    /* U+007A "z" */
    0xff, 0xf1, 0x86, 0x61, 0x8f, 0xff,

    /* U+007B "{" */
    0x33, 0x66, 0x62, 0x4c, 0x26, 0x66, 0x33,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xc0,

    /* U+007D "}" */
    0xcc, 0x66, 0x64, 0x23, 0x46, 0x66, 0xcc,

    /* U+007E "~" */
    0x79, 0x3c, 0xf3, 0xc9, 0xe0,

    /* U+00A1 "¡" */
    0xf1, 0x7f, 0xfc,

    /* U+00B0 "°" */
    0xf9, 0x9f,

    /* U+00B7 "·" */
    0xf0,

    /* U+00BF "¿" */
    0x30, 0x60, 0x0, 0x7, 0xc, 0x38, 0x67, 0xfe,
    0xf9, 0xf0,

    /* U+00C1 "Á" */
    0x18, 0x60, 0x0, 0x83, 0x8f, 0xbb, 0xe3, 0xc7,
    0xff, 0xfe, 0x3c, 0x78, 0xc0,

    /* U+00C9 "É" */
    0x18, 0x60, 0x7, 0xff, 0xf8, 0x30, 0x60, 0xfd,
    0xfb, 0x6, 0xf, 0xff, 0xc0,

    /* U+00CD "Í" */
    0x78, 0x6d, 0xb6, 0xdb, 0x6d, 0x80,

    /* U+00D1 "Ñ" */
    0x12, 0x3e, 0x24, 0x0, 0xe3, 0xe3, 0xf3, 0xf3,
    0xfb, 0xdb, 0xdf, 0xcf, 0xcf, 0xc7, 0xc7,

    /* U+00D3 "Ó" */
    0x18, 0x60, 0x3, 0xe7, 0xdd, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1f, 0x7f, 0xef, 0x80,

    /* U+00D7 "×" */
    0x42, 0xc7, 0x6e, 0x7c, 0x7c, 0x7e, 0xef, 0x42,

    /* U+00DA "Ú" */
    0x18, 0x60, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1f, 0x77, 0xcf, 0x80,

    /* U+00DC "Ü" */
    0x66, 0xcc, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1f, 0x37, 0xef, 0x80,

    /* U+00E1 "á" */
    0x18, 0x60, 0x0, 0x7, 0xcf, 0xbf, 0xbf, 0xc7,
    0x8d, 0xfb, 0xb0,

    /* U+00E9 "é" */
    0x18, 0x60, 0x0, 0x7, 0xcf, 0xb1, 0xe3, 0xff,
    0x83, 0xfb, 0xe0,

    /* U+00ED "í" */
    0x78, 0x0, 0x36, 0xdb, 0x60,

    /* U+00F1 "ñ" */
    0x20, 0xf8, 0x20, 0xd, 0xdf, 0xb9, 0xe3, 0xc7,
    0x8f, 0x1e, 0x30,

    /* U+00F3 "ó" */
    0x18, 0x60, 0x0, 0x7, 0xdf, 0xf1, 0xe3, 0xc7,
    0x8d, 0xf3, 0xe0,

    /* U+00FA "ú" */
    0x18, 0x60, 0x0, 0xc, 0x78, 0xf1, 0xe3, 0xc7,
    0x9d, 0xfb, 0xb0,

    /* U+00FC "ü" */
    0x66, 0xcc, 0x0, 0xc, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0xfb, 0xf0,

    /* U+2026 "…" */
    0xcc, 0xf3, 0x30,

    /* U+F00C "" */
    0x0, 0x0, 0x40, 0x0, 0xe, 0x0, 0x1, 0xf0,
    0x0, 0x3e, 0x0, 0x7, 0xc2, 0x0, 0xf8, 0x70,
    0x1f, 0xf, 0x83, 0xe0, 0x7c, 0x7c, 0x3, 0xef,
    0x80, 0x1f, 0xf0, 0x0, 0xfe, 0x0, 0x7, 0xc0,
    0x0, 0x38, 0x0, 0x1, 0x0, 0x0,

    /* U+F00D "" */
    0x60, 0x1b, 0xc0, 0xff, 0x87, 0xdf, 0x3e, 0x3f,
    0xf0, 0x7f, 0x80, 0xfc, 0x3, 0xf0, 0x1f, 0xe0,
    0xff, 0xc7, 0xcf, 0xbe, 0x1f, 0xf0, 0x3d, 0x80,
    0x60,

    /* U+F04B "" */
    0x0, 0x0, 0x3c, 0x0, 0xf, 0xc0, 0x3, 0xfc,
    0x0, 0xff, 0x80, 0x3f, 0xf8, 0xf, 0xff, 0x83,
    0xff, 0xf8, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0x8f,
    0xff, 0x83, 0xff, 0x80, 0xff, 0x80, 0x3f, 0xc0,
    0xf, 0xc0, 0x3, 0xc0, 0x0, 0x0, 0x0, 0x0,

    /* U+F0E7 "" */
    0x7f, 0x83, 0xfc, 0x1f, 0xc0, 0xfe, 0x7, 0xf0,
    0x3f, 0x3, 0xff, 0xdf, 0xfe, 0xff, 0xf7, 0xff,
    0x3f, 0xf8, 0xf, 0x80, 0x78, 0x3, 0xc0, 0x1c,
    0x0, 0xe0, 0xe, 0x0, 0x70, 0x3, 0x0, 0x10,
    0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 71, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 107, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 7, .adv_w = 201, .box_w = 12, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 166, .box_w = 9, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 38, .adv_w = 201, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 201, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 59, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 71, .adv_w = 83, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 77, .adv_w = 83, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 83, .adv_w = 119, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 87, .adv_w = 154, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 95, .adv_w = 59, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 96, .adv_w = 130, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 98, .adv_w = 59, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 99, .adv_w = 119, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 109, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 83, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 130, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 59, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 210, .adv_w = 142, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 218, .adv_w = 142, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 224, .adv_w = 142, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 232, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 213, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 142, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 142, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 301, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 119, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 119, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 201, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 400, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 431, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 130, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 481, .adv_w = 201, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 508, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 142, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 95, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 537, .adv_w = 119, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 547, .adv_w = 95, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 554, .adv_w = 154, .box_w = 8, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 558, .adv_w = 142, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 560, .adv_w = 47, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 562, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 569, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 579, .adv_w = 142, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 597, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 604, .adv_w = 107, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 623, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 71, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 643, .adv_w = 119, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 652, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 655, .adv_w = 201, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 667, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 674, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 681, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 691, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 701, .adv_w = 119, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 707, .adv_w = 142, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 715, .adv_w = 119, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 723, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 730, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 201, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 756, .adv_w = 142, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 765, .adv_w = 130, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 771, .adv_w = 95, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 778, .adv_w = 107, .box_w = 2, .box_h = 13, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 782, .adv_w = 95, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 789, .adv_w = 166, .box_w = 9, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 794, .adv_w = 59, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 797, .adv_w = 95, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 799, .adv_w = 59, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 800, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 142, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 823, .adv_w = 130, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 59, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 842, .adv_w = 154, .box_w = 8, .box_h = 15, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 857, .adv_w = 142, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 870, .adv_w = 142, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 878, .adv_w = 142, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 891, .adv_w = 142, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 904, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 915, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 926, .adv_w = 59, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 931, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 942, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 953, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 964, .adv_w = 142, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 975, .adv_w = 178, .box_w = 10, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 978, .adv_w = 320, .box_w = 20, .box_h = 15, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1016, .adv_w = 220, .box_w = 14, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1041, .adv_w = 280, .box_w = 18, .box_h = 21, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1089, .adv_w = 200, .box_w = 13, .box_h = 20, .ofs_x = 0, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0xf, 0x16, 0x1e, 0x20, 0x28, 0x2c, 0x30,
    0x32, 0x36, 0x39, 0x3b, 0x40, 0x48, 0x4c, 0x50,
    0x52, 0x59, 0x5b, 0x1f85, 0xef6b, 0xef6c, 0xefaa, 0xf046
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 161, .range_length = 61511, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 24, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t pet_font_s = {
#else
lv_font_t pet_font_s = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 21,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if PET_FONT_S*/

