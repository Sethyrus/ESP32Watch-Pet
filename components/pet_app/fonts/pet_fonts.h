#pragma once

// Pixel fonts for the "pocket toy" look (DECISIONS R26), all SIL OFL 1.1 (OFL.txt):
//   pet_font_s/m/l  Jersey 15 at 20/24/30 px, ASCII plus ¡¿°·×… and áéíóúüñ (and capitals), and the
//                   LVGL symbols the UI uses (OK, CLOSE, PLAY, CHARGE) from LVGL's Font Awesome 5 Free.
//   pet_font_clock  Jersey 10 at 64 px, digits, ':' and '-' only.
// Generated with lv_font_conv 1.5.3 at 1 bpp (crisp pixels); the exact command is at the top of each
// file. A new LV_SYMBOL_* in the UI must be added to the -r list of the Font Awesome part.

#include "lvgl.h"

LV_FONT_DECLARE(pet_font_s)
LV_FONT_DECLARE(pet_font_m)
LV_FONT_DECLARE(pet_font_l)
LV_FONT_DECLARE(pet_font_clock)
