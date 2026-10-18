/* LVGL 9.3 config for the host UI simulator (tools/uisim).
 * Mirrors the firmware's sdkconfig (CONFIG_LV_*) for everything that changes rendering:
 * colour depth, DPI, default theme (light, grow, 80 ms transitions), default font,
 * draw buffers and caches. Differences on purpose: no OS, one draw unit, no LVGL drivers,
 * assert -> abort(), logs through uisim's callback, and Montserrat 36/48 enabled here
 * although the firmware does not enable them (enable them in sdkconfig.defaults before
 * using them in the UI, or the firmware will not link).
 * Any setting not listed takes the LVGL default (lv_conf_internal.h), as on the firmware. */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---- colour and memory ---- */
#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* ---- HAL ---- */
#define LV_DEF_REFR_PERIOD 15      /* CONFIG_LV_DEF_REFR_PERIOD */
#define LV_DPI_DEF 130             /* CONFIG_LV_DPI_DEF */

/* ---- OS / drawing ---- */
#define LV_USE_OS LV_OS_NONE
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_DRAW_LAYER_MAX_MEMORY 0
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE

/* ---- logging and asserts ---- */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0            /* uisim registers a print callback (stderr) */
#define LV_LOG_USE_TIMESTAMP 0
#define LV_LOG_USE_FILE_LINE 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();

/* ---- caches / misc ---- */
#define LV_CACHE_DEF_SIZE 0
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_COLOR_MIX_ROUND_OFS 128
#define LV_USE_OBSERVER 1
#define LV_USE_SYSMON 0
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

/* ---- fonts (firmware: 12 14 16 20 24 28, default 14) ---- */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1    /* not in the firmware (see header) */
#define LV_FONT_MONTSERRAT_48 1    /* not in the firmware (see header) */
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_FONT_COMPRESSED 1
#define LV_USE_FONT_PLACEHOLDER 1

/* ---- themes (default theme, light, as on the firmware) ---- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_SIMPLE 1
#define LV_USE_THEME_MONO 0

/* ---- layouts ---- */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* ---- drivers: none, uisim draws its own SDL window ---- */
#define LV_USE_SDL 0

#endif /* LV_CONF_H */
