# Pet Tech: especificación técnica

Estado: **juego v2** (2026-10-18; ver `DECISIONS.md`, R16–R21). Producto en `PET_DESIGN.md`; decisiones en `DECISIONS.md`. Lo verificado el 2026-10-17 (código de core, compilaciones de prueba) se marca **[verificado]**; lo que solo puede medirse en el reloj, **[medir en F2]**.

## 1. Estructura

```text
ESP32Watch-Pet/
├── components/
│   ├── pet_core/      C99 puro: estado, reglas, tiempo, serialización. Sin ESP-IDF.
│   ├── pet_render/    C99 puro: paleta, máscaras, cara, animación, iconos (pet_gfx), escena (pet_scene).
│   ├── pet_game/      C99 puro: minijuegos deterministas (lógica + dibujo en RGB565).
│   └── pet_app/       ESP-IDF: UI (LVGL), radio, persistencia, energía (usa watch_board).
├── tools/
│   ├── sim/           Hojas de contacto, escenas y fotogramas de juego en PNG.
│   ├── uisim/         La UI LVGL real en el PC (SDL2 o por guion con capturas PNG).
│   └── probes/        Firmwares de medición (no son la app; ver sección 8).
├── tests/             Tests headless: motor, bots de jugador y de minijuegos (compilan con `cc`).
└── docs/
```

**Escena y juegos (v2):** `pet_scene` y `pet_game` dibujan en un búfer RGB565 de 400×280 en PSRAM (doble búfer) que la UI enseña con un `lv_image`; la escena se redibuja solo cuando cambia algo. `tools/uisim` compila `pet_ui*.c` sin tocarlo con unos shims (`bsp/esp-bsp.h`, `esp_heap_caps.h`, `esp_log.h`) e implementa `pet_priv.h`. Imagen del firmware: 1,14 MiB (cabe en `factory`).

`pet_core`, `pet_render` y `pet_game` no incluyen cabeceras de ESP-IDF, LVGL ni SDL: reciben el tiempo, el azar y las entradas como parámetros, de modo que **el mismo código compila para el simulador, los tests y el firmware** (patrón de pocket-tank y de `ESP32Watch-Fluid/components/fluid_engine` + `tools/host_bench`).

### API de `pet_core` (real: `components/pet_core/include/pet_core.h`)

Resumen; la cabecera es la referencia. Tiempo en segundos de hora local; los eventos con subsegundos reciben además un reloj monotónico en ms. `pet_view()` rellena el `pet_view_t` de `pet_render` y devuelve la animación recomendada (`pet_core/pet_view.c` es el único punto que incluye `pet_render.h`). `pet_step` trata un hueco > 5 min como ausencia (`pet_catch_up`); la app **solo** llama a `pet_step` con la pantalla encendida y a `pet_ingest_scan` en segundo plano. Partida v3, 653 B: `pet_save`/`pet_load`/`pet_load_ab` (A/B con CRC; migración v1/v2→v3 probada, también con la partida real del reloj).

Boceto original:

```c
void   pet_init(pet_state_t *s, uint64_t seed, int64_t now_s);
void   pet_step(pet_state_t *s, int64_t now_s, const pet_inputs_t *in);   // tiempo activo; tolera dt grandes
void   pet_catch_up(pet_state_t *s, int64_t now_s);                       // hueco ausente, un solo paso
void   pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s);
bool   pet_feed(pet_state_t *s, pet_food_t type, int64_t now_s);
void   pet_touch(pet_state_t *s, int64_t now_s);  /* y pet_shake, pet_usb_changed … */
int64_t pet_next_scan_s(const pet_state_t *s, const pet_env_t *env);      // planificador
size_t pet_save(const pet_state_t *s, uint8_t *buf, size_t cap);
pet_load_t pet_load(pet_state_t *s, const uint8_t *buf, size_t len);      // valida CRC y migra
void   pet_view(const pet_state_t *s, int64_t now_s, pet_view_t *out);    // lo que necesita el render
```

- Determinista: xorshift64* con su estado dentro de la partida; nada de `rand()` ni `time()`.
- `pet_ingest_scan` recibe el BSSID en bruto, **calcula el hash con sal dentro** y lo descarta: ninguna otra capa lo guarda.
- Serialización en enteros de punto fijo (sin `float`), para que tests y firmware den lo mismo.

### API de `pet_render` (boceto)

```c
void pet_render_creature(const pet_view_t *v, uint32_t t_ms, uint8_t *idx, int w, int h); // 48×48 indexado
void pet_blit_scaled_rgb565(const uint8_t *idx, int w, int h, int scale, const uint16_t *pal,
                            uint16_t *dst, int dst_stride);                               // ×6 vecino más cercano
const uint16_t *pet_palette_rgb565(void);
```

## 2. Instalación: app o standalone, mismo binario

**[verificado]** en `ESP32Watch-core/components/watch_board/watch_launcher.c`: `watch_launcher_is_available()` es verdadero solo si la imagen en ejecución **no** está en `factory` y existe `factory`; `watch_launcher_boot_once()` no hace nada en otro caso. Por tanto **no hace falta ningún Kconfig**: el mismo binario se comporta como app o como standalone según dónde se grabe.

| | **App** (junto a las demás) | **Standalone** (único firmware) |
| --- | --- | --- |
| Cómo se graba | `flash_all.sh` del Launcher, slot **`ota_5`** (2 MB) | `idf.py flash` desde este repo: escribe la app en `factory` (0x20000, 1,5 MB) y un `otadata` en blanco; **sustituye al Launcher** |
| `watch_launcher_is_available()` | `true` | `false` |
| PWR en la raíz | vuelve al Launcher (`watch_launcher_exit()`) | no hace nada (convención de core, `ARCHITECTURE.md`) |
| Vive mientras... | la app está abierta; al reabrir se ejecuta `pet_catch_up()` | siempre |
| Tabla de particiones | la compartida (sin cambios) | la misma |
| Volver al estado anterior | — | `flash_all.sh` del Launcher reinstala todo |

**Presupuesto de flash (restricción de diseño): ≤ 1,5 MiB**, para que el standalone quepa en `factory`. Medido con una compilación de prueba [verificado]:

| Imagen | Binario | Libre en `factory` |
| --- | --- | --- |
| Plantilla (sin WiFi) | 0,72 MiB | — |
| Pet + WiFi por defecto (con `esp_netif`/lwip) | 1,16 MiB | 22 % |
| **Pet + WiFi solo-escaneo** (config de la sección 4) | **0,99 MiB** | **34 % (~530 KB)** |

El juego, el render y los iconos tienen ~530 KB de margen. Si algún día no cupiera, el modo app seguiría funcionando (slot de 2 MB) y el standalone necesitaría una tabla propia, decisión que pasaría por el Launcher.

**Deep sleep: fuera del MVP en ambos modos.** En modo app es imposible (al despertar arrancaría el Launcher, porque `boot_once` apunta a `factory`). En standalone sería posible, pero `watch_board` solo ofrece light sleep y core no ha medido deep sleep ni el suelo de consumo con el panel alimentado (ALDO2). Usar el mismo camino en los dos modos evita dos comportamientos que probar. Se reconsidera solo si la medición de F2 lo justifica.

**Esfera/servicio dentro del Launcher:** futuro. `pet_core` sin dependencias permite integrarlo sin reescribir.

## 3. Pantalla y render

**Decisión: LVGL por el camino soportado de core (`watch_display_start()`, «UI con LVGL» en `ARCHITECTURE.md`).** Es el modo con el que `watch_power` gestiona sueño, táctil apagado en reposo y brillo [verificado en `watch_power.h`]; las apps con framebuffer propio (Fluid) tienen que copiar ese manejo a mano (`rev.md`: Fluid no tiene sueño por inactividad). La escena de una mascota no necesita 30 fps: la animación avanza a pasos de 80 ms (`STYLEGUIDE.md`).

- **Escena:** `pet_render` dibuja la criatura en 48×48 indexado y `pet_blit_scaled_rgb565` la escala ×6 a un búfer RGB565 de 288×288 (166 KB) en **PSRAM**, mostrado con un `lv_image` (o `lv_canvas`). Doble búfer: se dibuja en el oculto sin el bloqueo de LVGL y se intercambia + `lv_obj_invalidate()` con el bloqueo tomado. Solo se invalida cuando el frame cambia.
- **Menús, despensa, ajustes, HUD:** widgets LVGL.
- **Búfer de dibujo de LVGL: `buffer_lines = 20`** (2 × 410 × 20 × 2 B ≈ 33 KB de RAM interna DMA, frente a ~82 KB por defecto) para dejar sitio a WiFi (sección 5).
- **Prueba D1 [medido 2026-10-17, `tools/probes/scene_probe.c`]:** render de la criatura 4,4 ms + `lv_refr_now` con flush de la imagen 288×288 en PSRAM **14 ms de media (máx. 17)** con `buffer_lines = 20`: ≈ 18 ms por frame, el doble de holgura que el objetivo (≤ 40 ms). **Plan B** si no llega: el segundo modo de core (`bsp_display_new()` + bandas DMA como `ESP32Watch-Fluid/components/fluid_engine/fluid_render.c` / `ESP32Watch-Doom/components/doom_app/doom_port.c`), con LVGL solo para menús. `pet_render` no cambia, porque no sabe por qué camino sale el frame.
- Nunca `bsp_display_start()` (`GOTCHAS.md`, «BSP Registra El Panel Como RGB»).
- **Esquinas redondeadas** (~100 px de radio, medido en pocket-tank): nada importante en ellas.
- **Contra el quemado:** desplazamiento de la escena de 1–3 px cada ~60 s; brillo reducido en modo mesa. AOD no existe en core; comportamiento del panel a brillo muy bajo **[medir en F2]**.

## 4. Radio: WiFi pasivo, solo escaneo

**Configuración [verificado compilando]:** sin `esp_netif` ni lwip (no hace falta IP para escanear), solo el bucle de eventos por defecto.

```c
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
cfg.nvs_enable = 0;                               // WiFi no toca el NVS compartido
esp_wifi_init(&cfg);
esp_wifi_set_storage(WIFI_STORAGE_RAM);
esp_wifi_set_mode(WIFI_MODE_STA);
esp_wifi_set_country_code("ES", false);           // canales 1–13
esp_wifi_start();
wifi_scan_config_t sc = { .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_PASSIVE,
                          .scan_time.passive = PASSIVE_MS };
esp_wifi_scan_start(&sc, true);                   // bloqueante; solo escucha balizas
esp_wifi_scan_get_ap_records(&n, recs);
esp_wifi_stop(); esp_wifi_deinit();               // radio y RAM liberadas hasta el siguiente
```

`sdkconfig.defaults` cuando entre la radio (F2):
```
CONFIG_ESP_WIFI_IRAM_OPT=n
CONFIG_ESP_WIFI_EXTRA_IRAM_OPT=n
CONFIG_ESP_WIFI_RX_IRAM_OPT=n
CONFIG_LWIP_IRAM_OPTIMIZATION=n
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n
CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=n
```

- **Pasivo de verdad:** `WIFI_SCAN_TYPE_PASSIVE` no transmite sondas; nunca se llama a `esp_wifi_connect()`.
- **Tiempo por canal:** el valor por defecto de IDF es 360 ms (`WIFI_PASSIVE_SCAN_DEFAULT_TIME`), unos 4,7 s de radio para 13 canales [verificado en la cabecera]. Las balizas salen cada ~102 ms, así que **120–200 ms** debería bastar. Se fija en F2 comparando APs detectados a 120 y 360 ms con la sonda.
- **Datos que cruzan a `pet_core`:** `pet_ap_t { uint8_t bssid[6]; int8_t rssi; bool open; bool hidden; }` (abierta = `authmode == WIFI_AUTH_OPEN`; oculta = `ssid[0] == 0`). El SSID no sale del módulo de radio.
- **Planificador:** `pet_next_scan_s()` decide según modo, noche y batería; el firmware solo ejecuta. Sin escaneo de noche ni con batería < 15 %.
- **Hash:** SipHash-2-4 con clave de 16 bytes de `esp_random()` generada en el primer arranque, guardada en la partida y truncada a 32 bits. Implementación propia en `pet_core` (~40 líneas; el algoritmo de referencia es de dominio público). La clave nunca se exporta ni se registra en logs.

## 5. RAM interna

RAM interna estática (`idf.py size`, DIRAM de 341.760 B) [verificado]:

| Imagen | DIRAM usada | Libre |
| --- | --- | --- |
| Plantilla | 150,7 KB | 191 KB |
| Pet + WiFi por defecto | 197,8 KB | 144 KB |
| **Pet + WiFi solo-escaneo** | **177,2 KB** | **164,6 KB** |

**Medido en el reloj [2026-10-17]:** con LVGL (`buffer_lines = 20`) y la escena arrancadas quedan **165,6 KB** libres en RAM interna. Con WiFi iniciado bajan a ~89 KB y el **mínimo durante el escaneo fue 86 KB** (criterio ≥ 30 KB: sobrado); al hacer `deinit` se recuperan los 64 KB. **Pilas:** `pet_render_creature` usa ~5,5 KB de pila (búferes de capas); las tareas que dibujan necesitan **≥ 8 KB** y `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` si se dibuja desde `main` (con los 3,5 KB por defecto hay desbordamiento de pila).

En tiempo de ejecución esos ~165 KB se reparten entre los búferes de LVGL (~33 KB con `buffer_lines = 20`), las pilas de las tareas y el heap de WiFi mientras escanea **[medir en F2]**. Core ya vio con BLE que quedarse en ~12 KB congela la app sin aviso (`GOTCHAS.md`, «BLE Y RAM Interna»). Reglas:
- Comprobar siempre el retorno de `xTaskCreate` y de `esp_wifi_init`; si falla, saltar el escaneo, nunca bloquearse.
- Registrar `heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)` en cada escaneo; **criterio de F2: mínimo ≥ 30 KB** durante un escaneo con la UI activa.
- Búferes grandes (escena, LVGL heap) en PSRAM.

## 6. Energía, sueño y tareas

**Semántica de core [verificado en `watch_power.h`]:** `watch_power_sleep(timeout)` **vuelve a encender la pantalla** al despertar, también por timeout. Por eso **no sirve para escanear en segundo plano**. Se usa `watch_power_screen_off(timeout, stay_awake)`: panel apagado y LVGL en pausa; mientras `stay_awake()` devuelve `true` el chip no duerme; cuando devuelve `false` entra en light sleep (despertando cada ~200 ms para mirar PWR) **sin encender la pantalla**. Con USB el chip no duerme (USB-Serial-JTAG), cosa que encaja con el modo mesa. **Con USB `stay_awake()` no se llama nunca** (verificado en el reloj, R12): `radio_task` mira también el calendario cada 5 s mientras la pantalla está apagada y guarda la partida al terminar el escaneo; en batería los tics se paran en light sleep y el calendario lo lleva `stay_awake()`.

Patrón de escaneo con pantalla apagada:
1. `system_task` llama a `watch_power_screen_off(0, pet_stay_awake)` al expirar la inactividad.
2. `pet_stay_awake()` (llamado en cada despertar de ~200 ms) compara `esp_timer_get_time()` con la hora del siguiente escaneo; si toca, avisa a `radio_task` y devuelve `true` hasta que el escaneo termina y se guarda la partida. Se usa `esp_timer` porque avanza durante el light sleep; no se confía en `vTaskDelay` a través del sueño.
3. BOOT o PWR despiertan la pantalla; el táctil no (decisión de core, coherente con el Launcher).

| Tarea | Función | LVGL |
| --- | --- | --- |
| `pet_task` | dueña de `pet_state_t`: `pet_step`, ingesta de escaneos, guardado | nunca toma el bloqueo |
| `ui_task` | `pet_view` → render → intercambio de búfer; widgets | `bsp_display_lock(timeout)` **con timeout**, porque `watch_power_*` retiene el bloqueo durante el sueño |
| `radio_task` | ciclo de radio bajo demanda | no |
| `system_task` | botones (`watch_boot_debouncer_*`, `watch_pwr_key_take_short_press`), inactividad, `watch_power_screen_off`, detección de modo | no (requisito de `watch_power`) |

Otras piezas de `watch_board` (v0.5.1):
- `watch_battery_read()` → `usb`, `percent`: detección de mesa y batería baja (como mucho una lectura por segundo).
- `imu_service_read()` (sondeo a ~20 ms) solo con pantalla encendida o en ventanas cortas: toque/sacudida/levantar y **quietud propia** (desviación de la aceleración < umbral durante 60 s; no existe en core).
- `watch_rtc_*`, `watch_nvs_init()`, `watch_power_quiet_peripherals()` al arrancar, `watch_display_set_brightness()`.
- **No se escribe ningún registro del AXP2101** (`PMU_SAFETY.md`).

## 7. Persistencia

- NVS, namespace **`pet`**, `watch_nvs_init()`, nunca se borra.
- **Doble copia A/B** (`save_a`, `save_b`): cabecera `{magic, version, seq, len, crc32}` + carga útil. Se escribe la copia más antigua; al leer se toma la de `seq` mayor con CRC válido.
- **Versionado:** la carga útil solo crece por el final (pocket-tank); cambios semánticos suben `version` y pasan por migraciones en `pet_core`, probadas con partidas dorado.
- Tamaño < 1 KB (estadísticas ~64 B; despensa 20 × 5 B; familiaridad 64 × 5 B; clave 16 B).
- **Cuándo:** por eventos (comer, evolucionar, cambio de modo, tras escanear, antes de dormir y antes de `watch_launcher_exit()`) con ≥ 30 s entre escrituras, más latido cada 10 min.
- **Reloj:** instante del RTC (hora local sin zona). Si `watch_rtc_time_was_lost()` o la hora retrocede, `dt = 0`.

## 8. Medición en el reloj (primer hito de F2)

La sonda de radio está en `tools/probes/radio_probe.c` (compilada y validada en una copia de Pet; no forma parte de la app). Mide tiempo de init/escaneo/deinit, APs detectados y heap interno con la UI arrancada.

| Prueba | Qué mide |
| --- | --- |
| R1 | Sonda a 120 y 360 ms por canal: APs detectados, tiempos, heap interno mínimo (criterio ≥ 30 KB). **Hecha:** 120 ms → 3-4 APs en 1,6 s; 360 ms → 4 APs en 4,7 s (init 150 ms, deinit 13-60 ms); heap mínimo 86 KB. Solo había 3-4 APs en el sitio de la prueba: repetir en una zona densa antes de dar 120 ms por definitivo |
| E0 | Referencia: `watch_power_screen_off` en batería, sin radio |
| E1 | Escaneo pasivo cada 5 min con init/deinit |
| E2 | Escaneo cada 15 min |
| D1 | LVGL: tiempo de flush de la escena 288×288 (objetivo ≤ 40 ms). **Hecha:** 14 ms (máx. 17) |

**Diario de consumo (R14):** la app escribe en NVS una fila por hora (mV, %, escaneos, USB/pantalla/arranque); `python tools/read_log.py` la lee del reloj y calcula los mV/h entre horas consecutivas en batería, sin firmware de prueba. Es la vía normal para E1/E2: se lleva el reloj varios días y se lee después.

Método de consumo de core: tendencia de tensión (mV/h), ≥ 3 h por prueba, en la muñeca. **Objetivo: el escaneo en muñeca añade ≤ 25 % sobre E0.** Con eso se fija la frecuencia; si no se alcanza a 15 min, se espacia o se condiciona al movimiento.

Las pruebas se graban en **`ota_4`** (slot de pruebas de core) y se lanzan desde el Launcher. **Nunca se dejan grabadas sin avisar**; al terminar se borra el slot o se reinstala lo normal.

## 9. Trabajo entre repositorios (cuando haya firmware jugable)

- Launcher: `pet ota_5 ../ESP32Watch-Pet` en `apps.conf` y entrada en `KNOWN[]` (`components/launcher/os_apps.c`).
- Core: fila en la tabla de repos de `workspace/AGENTS.md`; namespace `pet` en `docs/ARCHITECTURE.md`; los hallazgos de WiFi y consumo a `docs/` de core (WiFi nunca se había usado en el reloj).
- `ESP32Watch-core/tools/check_apps.sh` sin avisos.

## 10. Riesgos

| Riesgo | Estado | Mitigación |
| --- | --- | --- |
| Binario > 1,5 MiB | medido: 0,99 MiB con WiFi | config solo-escaneo; vigilar `idf.py size` en cada fase |
| RAM interna con WiFi + LVGL | estática medida; runtime en R1 | `buffer_lines = 20`, WiFi en PSRAM, init/deinit por escaneo, comprobar retornos |
| Escanear sin encender la pantalla | resuelto en diseño | `watch_power_screen_off` + `stay_awake` |
| Consumo del escaneo | sin medir | E0–E2 antes de fijar frecuencias |
| Fluidez de la escena con LVGL | sin medir | D1; plan B con framebuffer propio sin tocar `pet_render` |
| Calidad del arte | — | hojas de contacto + guía + puntos de control (`STYLEGUIDE.md`) |
