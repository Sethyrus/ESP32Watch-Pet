# uisim: PC simulator of the real UI

Builds the firmware's LVGL UI (`components/pet_app/pet_ui*.c` and its fonts, unchanged) plus `pet_core`,
`pet_render`, `pet_game` and `pet_sound` for macOS. It talks to the UI only through `pet_app/pet_priv.h`, and covers the ESP-IDF
headers with `shim/` (`bsp/esp-bsp.h`, `esp_heap_caps.h`, `esp_log.h`, `esp_err.h`, `esp_timer.h`).
You can check screens with headless screenshots, or play in a window, without flashing the watch.

Requirements: SDL2 (`brew install sdl2`), and `managed_components/lvgl__lvgl` already there
(run `idf.py build` or `idf.py reconfigure` once).

```sh
make -C tools/uisim          # build/uisim; LVGL compiles once into build/liblvgl.a (~5 s)
make -C tools/uisim shots    # scripts/smoke.txt -> tools/uisim/out/*.png
make -C tools/uisim run      # interactive window
```

`lv_conf.h` copies the firmware's `CONFIG_LV_*` settings (16-bit colour, DPI 130, light default theme,
Montserrat 14 as the default font). Differences: no OS, assert calls `abort()`, and Montserrat 36/48 are
enabled here but not in the firmware. Before you use those two fonts in the UI, enable them in
`sdkconfig.defaults`.

## Simulated world
- Day 20000 at 10:00:00 (local seconds), seed 1, battery 80 %, no USB. `ESP_LOGx` goes to stderr
  only with `UISIM_LOG=1`; LVGL warnings always go to stderr.
- One frame = 33 ms: monotonic clock +33 ms, game clock +33 ms × speed, then `pet_ui_tick()`, then
  `lv_timer_handler()`.

## Script (`uisim script <file> <outdir>`)
One command per line; `#` starts a comment. On an error it reports `file:line` and exits with 2.

| Command | Effect |
| --- | --- |
| `wait <ms>` | runs frames |
| `tap <x> <y>` | presses for 100 ms, releases, then runs 4 frames |
| `swipe <x1> <y1> <x2> <y2> [ms]` | drag over ≥10 pointer reads (default 200 ms); fires LVGL gestures |
| `press <x> <y>` / `move <x> <y> [ms]` / `release` | manual drag (`move` with ms interpolates) |
| `boot` / `pwr` | `pet_ui_boot_action()` / `pet_ui_back()`, then 1 frame |
| `advance <s>` | jumps the game clock with no frames (time away) |
| `run <s>` | runs time without sleeping. Up to 60 s it uses normal frames. Above 60 s: one `pet_ui_tick` per simulated second, LVGL every 10 s, then a few normal frames (1 day ≈ 2 s) |
| `clock HH:MM` | moves forward to the next HH:MM (never back) |
| `speed <n>` | game seconds per real second (default 1) |
| `batt <pct>` / `usb 0\|1` | `g_batt_pct` / `g_usb` (pct < 0 = unknown) |
| `seed <n>` | new game (`pet_init`) with a key derived from n |
| `scan <n> [base] [open]` | `pet_ingest_scan` with n APs, ids base..base+n-1 (fresh ids if omitted), the first `open` ones open |
| `set <field> <value>` | sets a `pet_state_t` field; stats (satiety, energy, mood) in points. The field table is at the top of `uisim.c` |
| `shot <name>` | writes `<outdir>/<name>.png` and prints its path |
| `expect_dirty` | fails unless `pet_mark_dirty()` was called since the last check |
| `expect_sound <name>` | fails unless that sound (`snd_name()`, e.g. `eat`, `v_hello`) played since the last check; clears the list |
| `wake` | `pet_ui_wake()`, as when the screen turns on |

Screenshots paint the area outside a 100 px rounded rectangle grey (#303030), so you can see content
that the screen corners would clip. `UISIM_CORNERS=0` turns this off.

## Interactive (`uisim run [-z 2] [seed]`)
The mouse is the touch screen, and the sounds play through the computer's speakers. `b` = BOOT,
`p`/Esc/Backspace = PWR, `w` = screen wake (greeting), `h` = +1 h, `d` = +1 day,
`1`/`2`/`3` = speed x1/x60/x600, `s` = screenshot to `tools/uisim/out/shot-N.png`, `q` = quit.

## Visual mockups (`make -C tools/uisim mock`)
`mock/mock.c` draws the candidate visual directions at real size with the same pieces as the
firmware (LVGL, `pet_render`, fonts converted with `lv_font_conv`), into `out/mock/*.png` plus
`out/mock/comparativa.png`. The candidate fonts in `mock/fonts` are Google Fonts under the SIL OFL 1.1:
Pixelify Sans, Jersey 10 and 15, Silkscreen, Barlow Condensed and Fredoka.
