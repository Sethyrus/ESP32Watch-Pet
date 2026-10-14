#pragma once

#include "esp_err.h"

// Board init, pet UI and the system task (buttons, screen timeout, sleep).
// Call from app_main after watch_launcher_boot_once().
esp_err_t pet_app_start(void);
