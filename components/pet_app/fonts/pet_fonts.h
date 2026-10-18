#pragma once

// Barlow (SIL OFL 1.1, OFL.txt) with the Spanish letters: ASCII plus ¡¿°·×… and áéíóúüñ (and capitals),
// plus the LVGL symbols the UI uses (OK, CLOSE, PLAY, CHARGE) from LVGL's Font Awesome 5 Free (SIL OFL 1.1).
// Generated with lv_font_conv 1.5.3, 4 bpp; the exact command is at the top of each file. A new LV_SYMBOL_*
// in the UI must be added to the -r list of the Font Awesome part.
// Medium for 12-16, SemiBold for 20-28.

#include "lvgl.h"

LV_FONT_DECLARE(pet_font_12)
LV_FONT_DECLARE(pet_font_14)
LV_FONT_DECLARE(pet_font_16)
LV_FONT_DECLARE(pet_font_20)
LV_FONT_DECLARE(pet_font_24)
LV_FONT_DECLARE(pet_font_28)
