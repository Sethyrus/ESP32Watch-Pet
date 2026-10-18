#pragma once

// Shared state and entry points between the pet_app modules (not a public API).
// tools/uisim implements the same functions on the PC, so pet_ui*.c runs unchanged there.

#include <stdbool.h>
#include <stdint.h>

#include "pet_core.h"

// The game state. Every access goes through pet_lock()/pet_unlock(). Lock order: the LVGL
// lock (bsp_display_lock) may be held when taking this one, never the other way round.
extern pet_state_t g_pet;
void pet_lock(void);
void pet_unlock(void);

int64_t pet_now_s(void);   // game clock: RTC local seconds, sped up in test mode
uint32_t pet_mono_ms(void);
void pet_mark_dirty(void); // ask for a save (rate-limited by system_task)
void pet_reset_game(void); // new pet, saved at once

// Test mode (Ajustes): the game clock runs `mul` times faster (1, 10 or 60) until reboot,
// and can jump forward. Scans keep their real-time pace.
int pet_time_speed(void);
void pet_set_time_speed(int mul);
void pet_skip_time(int64_t secs);

// True when the app was started from the launcher (PWR at the root returns there).
bool pet_in_launcher(void);

// Battery, refreshed by system_task (percent < 0: unknown).
extern volatile int g_batt_pct;
extern volatile bool g_usb;

// pet_ui.c
void pet_ui_init(void);          // LVGL lock held
void pet_ui_tick(void);          // ui_task, LVGL lock NOT held: step, render, swap
bool pet_ui_back(void);          // LVGL lock held; true when a panel was closed
void pet_ui_boot_action(void);   // LVGL lock held; BOOT short press
