#pragma once

#include <stdbool.h>
#include <stdint.h>

// Hourly diary of battery and scans, kept in NVS (namespace "pet", key "tlm") so power use
// can be measured with the normal app and read back later (tools/read_log.py). A ring of
// the last PET_LOG_MAX hours; nothing in it identifies a network.
#define PET_LOG_MAX 120

enum { PET_LOG_USB = 1, PET_LOG_SCREEN_ON = 2, PET_LOG_BOOT = 4 };

typedef struct __attribute__((packed)) {
    uint32_t t;      // local seconds (RTC)
    uint16_t mv;     // battery millivolts (0 unknown)
    uint16_t scans;  // total_scans, low 16 bits
    int8_t pct;      // fuel gauge, -1 unknown
    uint8_t flags;   // PET_LOG_*
} pet_log_entry_t;

void pet_log_load(void);
// Appends an entry and writes the ring to NVS.
void pet_log_add(int64_t t, int mv, int pct, uint8_t flags, uint32_t scans);
