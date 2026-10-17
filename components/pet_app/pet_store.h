#pragma once

#include "esp_err.h"
#include "pet_core.h"

// Persistence in NVS namespace "pet": two copies (save_a / save_b), the newest valid one
// wins. Writes alternate copies, so a power cut leaves the previous one intact.

// Loads the game. Returns true when a valid save was read; false means a new game.
bool pet_store_load(pet_state_t *s);
// Serializes s (caller holds the state lock) and writes it. Returns ESP_OK on success.
esp_err_t pet_store_save(const pet_state_t *s);
