#pragma once

// Shared state and entry points between the pet_app modules (not a public API).

#include <stdbool.h>
#include <stdint.h>

#include "pet_core.h"

// The game state. Every access goes through pet_lock()/pet_unlock(). Lock order: the LVGL
// lock (bsp_display_lock) may be held when taking this one, never the other way round.
extern pet_state_t g_pet;
void pet_lock(void);
void pet_unlock(void);

int64_t pet_now_s(void);   // RTC local seconds
uint32_t pet_mono_ms(void);
void pet_mark_dirty(void); // ask for a save (rate-limited by system_task)
void pet_reset_game(void); // new pet, saved at once

// Battery, refreshed by system_task (percent < 0: unknown).
extern volatile int g_batt_pct;
extern volatile bool g_usb;

// pet_ui.c
void pet_ui_init(void);          // LVGL lock held
void pet_ui_tick(void);          // ui_task, LVGL lock NOT held: step, render, swap
bool pet_ui_back(void);          // LVGL lock held; true when a panel was closed
void pet_ui_boot_action(void);   // LVGL lock held; BOOT short press
