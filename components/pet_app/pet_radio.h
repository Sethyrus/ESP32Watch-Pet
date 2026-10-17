#pragma once

#include <stddef.h>

#include "pet_core.h"

// One passive WiFi scan: radio on only for the call. Fills aps (BSSID, RSSI, open, hidden);
// the SSID never leaves this module. Returns the number of APs (0 = none heard) or -1 when
// the scan could not run (the caller must not treat that as "no networks").
// Blocks ~1.6 s; call from a task with >= 6 KB of stack. Not re-entrant.
int pet_radio_scan(pet_ap_t *aps, size_t max);
