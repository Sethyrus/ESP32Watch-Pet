# Pet Tech: especificación técnica

Estado: **borrador para revisión**. Producto en `PET_DESIGN.md`; decisiones pendientes en `DECISIONS.md`. Todo lo que dependa de hardware no medido o de código de core no revisado a fondo se marca **[verificar]** y se convierte en una tarea de la fase 2.

## 1. Estructura

```text
ESP32Watch-Pet/
├── components/
│   ├── pet_core/      C99 puro: estado, reglas, tiempo, serialización. Sin ESP-IDF.
│   ├── pet_render/    C99 puro: paleta, máscaras, cara, animación → búfer indexado.
│   └── pet_app/       ESP-IDF: HAL, radio, persistencia, UI, modos (usa watch_board).
├── tools/sim/         Simulador PC: SDL2 interactivo + modo headless (tests, hojas).
├── tests/             Tests headless de ciclo de vida (compilan con `cc`).
└── docs/
```

`pet_core` y `pet_render` no incluyen cabeceras de ESP-IDF ni de SDL: reciben el tiempo, el azar y las entradas como parámetros, de modo que **el mismo código compila para el simulador, los tests y el firmware** (patrón de pocket-tank y de `fluid_engine`/`tools/host_bench` de Fluid). El componente de lógica se llamaba `pet_engine` en `AGENTS.md`; se unifica en `pet_core` + `pet_render`.

### API de `pet_core` (boceto, a concretar en la fase 1)

```c
void   pet_init(pet_state_t *s, uint64_t seed, int64_t now_s);
void   pet_step(pet_state_t *s, int64_t now_s, const pet_inputs_t *in);   // tiempo activo
void   pet_catch_up(pet_state_t *s, int64_t now_s);                       // hueco ausente, un solo paso
void   pet_ingest_scan(pet_state_t *s, const pet_ap_t *aps, size_t n, int64_t now_s);
bool   pet_feed(pet_state_t *s, pet_food_t type, int64_t now_s);          // acción del jugador
void   pet_touch(pet_state_t *s, int64_t now_s);  /* y pet_shake, pet_usb_changed … */
size_t pet_save(const pet_state_t *s, uint8_t *buf, size_t cap);
pet_load_t pet_load(pet_state_t *s, const uint8_t *buf, size_t len);      // valida y migra
```

- Determinista: el azar es un xorshift64* con su estado dentro de la partida; sin `rand()` ni `time()`.
- `pet_ingest_scan` recibe BSSID en bruto, **calcula el hash con sal dentro** y descarta el BSSID: ninguna otra capa lo ve.
- El estado se mantiene con sus contadores como enteros de punto fijo (no `float` en la serialización) para que los tests sean reproducibles entre plataformas.

### API de `pet_render` (boceto)

```c
void pet_render_scene(const pet_view_t *v, uint8_t *fb_idx, int w, int h);  // búfer indexado
const uint16_t *pet_palette_rgb565(void);                                    // ya con byte-swap según panel
```

## 2. Dos formas de instalarla (requisito del usuario)

Un único repositorio y un único código, con un `Kconfig` `PET_STANDALONE` (por defecto `n`) que solo cambia `pet_boot.c`, la tabla de particiones y el modo de energía.

| | **Modo app** (`PET_STANDALONE=n`) | **Modo standalone** (`PET_STANDALONE=y`) |
| --- | --- | --- |
| Instalación | en un slot OTA junto a las demás apps (`flash_all.sh`) | única imagen del reloj |
| Arranque | `watch_launcher_boot_once()` primero; PWR en la raíz llama a `watch_launcher_exit()` | sin salida al Launcher |
| Vive mientras... | la app está abierta | siempre |
| Segundo plano | light sleep con despertar por timer para escanear **mientras la app está abierta**; con la app cerrada actúa `pet_catch_up()` al reabrir | igual, siempre |
| Tabla de particiones | la compartida del Launcher (no se toca) | **[verificar]** propia, con una imagen más grande si no cabe en `factory` (1,5 MB) |
| Pantalla de reposo | apagada; se despierta con botones | esfera mínima con la hora, como alternativa al reloj del Launcher |

Reglas y límites **[verificar]** antes de comprometerse:
- **Deep sleep no es viable en modo app:** `watch_launcher_boot_once()` devuelve el arranque a `factory`, así que al despertar arrancaría el Launcher (`ESP32Watch-core/components/watch_board/watch_launcher.c`). En standalone sí es técnicamente posible, pero `watch_board` solo ofrece light sleep y core no ha medido deep sleep ni sus efectos sobre los raíles del AXP2101 (el panel sigue alimentado por ALDO2, posible suelo de consumo). **Primero light sleep; deep sleep solo si la medición lo justifica.**
- El tamaño del binario con WiFi + render propio debe caber en 2 MB (slot) o decidir la tabla de standalone. Primera tarea de la fase 2: compilar un esqueleto con WiFi y medir (`idf.py size`).
- Cómo se comporta `watch_launcher_is_available()` cuando la app está sola en `factory` [verificar]; el modo standalone depende de que devuelva `false`.
- Cambiar offsets de particiones es una operación del Launcher (su `AGENTS.md`): un `partitions` de standalone es una desviación deliberada y debe aprobarse (`DECISIONS.md`).
- **Esfera/servicio dentro del Launcher** queda como futuro: `pet_core` sin dependencias permite integrarlo sin reescribir, y se evalúa solo si no limita otras funciones.

Slot en modo app: `ota_5` (libre; `ota_4` es el slot de pruebas de core, `PMU_SAFETY.md` "Herramientas"). Requiere una línea en `ESP32Watch-Launcher/apps.conf` y una entrada en `KNOWN[]` de `components/launcher/os_apps.c` (color, icono, descripción) al arrancar el desarrollo.

## 3. Render

- **Escena propia** (no LVGL) para la criatura y su entorno: búfer **indexado de 8 bit** → paleta RGB565 con byte-swap → escalado entero por vecino más cercano → bandas por DMA con `esp_lcd_panel_draw_bitmap`. Es exactamente el camino de `DG_DrawFrame` en `ESP32Watch-Doom/components/doom_app/doom_port.c` y de las bandas con suciedad de `ESP32Watch-Fluid/components/fluid_engine/fluid_render.c` (alineación de ventanas par/impar del panel incluida). Se reutiliza el patrón, no se importa código.
- **Resolución lógica y escala** (decisión abierta): criatura 48×48 a ×6 = 288×288 (propuesta), o rejilla de pantalla completa 82×100 a ×5 = 410×500. Con la rejilla completa todo es pixel art coherente (texto de fuente bitmap 5×7 → 25×35 px por carácter, ~13 caracteres por línea); con la criatura sola, la UI puede ir con otra resolución.
- **Menús y ajustes:** propuesta de **LVGL bajo demanda** como hace `ESP32Watch-Fluid/components/fluid_app/fluid_menu.c` (LVGL sin `esp_lvgl_port`, flush propio y asignador en PSRAM `lv_mem_core_psram.c`) frente a render propio para todo. Pendiente en `DECISIONS.md`.
- **Panel:** `bsp_display_new()` como Doom y Fluid, nunca `bsp_display_start()` (registra el panel QSPI como RGB: desbordamiento de heap). Confirmar con core `docs/ARCHITECTURE.md` ("los dos modos de pantalla") qué ofrece `watch_display_start()` sin LVGL **[verificar]**.
- **Esquinas redondeadas** (~100 px, medido en pocket-tank): HUD fuera de las esquinas.
- **Memoria:** el búfer lógico es de 2–10 KB; no existe un framebuffer completo (411 KB en RGB565). Las bandas de salida (16 líneas) van en memoria DMA.
- **Contra el quemado:** desplazamiento de píxeles de 1–2 px cada ~60 s y brillo reducido en modo mesa. AOD no existe en core; comportamiento del panel a muy bajo brillo **[verificar]**.
- Presupuesto: ≥ 25 fps cuando la pantalla está encendida (pocket-tank logra 30–32 fps con un render mucho más pesado).

## 4. Radio (WiFi pasivo)

- Solo WiFi en el MVP. BLE entra en una fase posterior, tras medir (core midió que **cualquier modo BLE cuesta 2,5–3,5× el reposo** y toma ~52 KB de RAM interna; `BRINGUP.md` "Bluetooth Y Consumo", `GOTCHAS.md` "BLE Y RAM Interna").
- Modo STA con **escaneo pasivo** (`wifi_scan_config_t.scan_type = WIFI_SCAN_TYPE_PASSIVE`, `show_hidden = true`): solo se escuchan balizas; no se transmiten sondas ni se conecta a nada. Tiempo por canal y canales por defecto **[verificar en el código de ESP-IDF 5.5.4]**.
- **Radio solo durante el escaneo:** `esp_wifi_init` → `start` → `scan` → `stop` → `deinit`, para liberar RAM interna y evitar consumo. Coste de arranque y pico de RAM a medir. Requiere `esp_netif` y el bucle de eventos por defecto.
- `esp_wifi_set_storage(WIFI_STORAGE_RAM)` para que WiFi no escriba en el NVS compartido [verificar].
- Resultado del escaneo → `pet_ap_t { uint8_t bssid[6]; int8_t rssi; bool open; bool hidden; }` → `pet_ingest_scan()`. **No se pasan ni almacenan SSID.** (La «abierta» sale de `authmode == WIFI_AUTH_OPEN`; «oculta» de SSID vacío.)
- Planificador: el siguiente escaneo se calcula en `pet_core` (modo, noche, batería) y el firmware solo lo ejecuta. Sin escaneo de noche ni con batería baja [umbral a fijar].
- **Hash:** SipHash-2-4 con clave de 16 bytes aleatoria generada en el primer arranque (`esp_random`) y guardada en la partida, truncado a 32 bits. Implementación propia en `pet_core` (unas 40 líneas; el algoritmo de referencia es de dominio público). La sal vive en el dispositivo y **nunca se exporta**.

## 5. Persistencia

- NVS, namespace **`pet`**, con `watch_nvs_init()` y sin borrar nunca (regla del workspace). Añadir `pet` a la tabla de namespaces de core `docs/ARCHITECTURE.md` al arrancar el desarrollo.
- **Doble copia A/B** (`save_a`, `save_b`): cabecera `{magic, version, seq, len, crc32}` + carga útil. Se escribe siempre la copia más antigua; al leer se elige la de `seq` más alto con CRC válido. Un corte de alimentación a mitad de escritura deja siempre una copia válida.
- **Versionado:** la carga útil **solo crece por el final** (pocket-tank); un cambio semántico sube `version` y pasa por una tabla de migraciones en `pet_core` probada con partidas dorado de cada versión.
- Tamaño estimado < 1 KB (estadísticas ~64 B; despensa 20 × 5 B; tablas de familiaridad 64 × 5 B; sal 16 B; contadores).
- **Cuándo se guarda:** por eventos (comer, evolucionar, cambio de modo, entrar en sueño) con un mínimo de 30 s entre escrituras, más un latido cada 10 min (pocket-tank). Siempre antes de `watch_power_sleep()` largo y antes de volver al Launcher.
- **Reloj:** se guarda el instante del RTC (hora local sin zona, `watch_rtc.h`). Si el RTC perdió la hora (`watch_rtc_time_was_lost()`) o la hora va hacia atrás, `dt = 0`. Un cambio de hora (DST) introduce a lo sumo ±1 h de error, irrelevante para estas mecánicas.

## 6. Tareas e integración en `watch_board`

Se sigue el patrón de la plantilla (`system_task` fuera del bloqueo de LVGL):

| Tarea | Función |
| --- | --- |
| `pet_task` | tick del juego (≈10 Hz con pantalla encendida, mucho menos en reposo) y render (≈25–30 fps solo con pantalla encendida) |
| `radio_task` | espera la hora del siguiente escaneo, ejecuta el ciclo de radio, entrega el resultado a `pet_task` por cola |
| `system_task` | botones (`watch_boot_debouncer_*`, `watch_pwr_key_take_short_press`), inactividad, `watch_power_sleep()`/`watch_power_screen_off()`, detección de modo |

Uso de `watch_board` (v0.5.1, anclado por etiqueta):
- `watch_battery_read()` → `usb`, `percent`, estado de carga: base de la detección de modo y del escaneo con batería baja.
- `imu_service_read()` (sondeo a ~20 ms, solo con pantalla encendida o en una ventana corta para detectar quietud) → toque/sacudida/levantar; **la quietud en mesa necesita una ventana de desviación de aceleración propia** (no existe en core). Despertar por IMU no está validado (INT GPIO21).
- `watch_rtc_*`, `watch_nvs_init()`, `watch_power_quiet_peripherals()` al arrancar, `watch_display_*` para brillo y sueño (el táctil se apaga en sleep, `GOTCHAS.md` "Tactil En Sleep").
- **No se escribe ningún registro del AXP2101** (`PMU_SAFETY.md`); este proyecto no toca el PMIC.
- Wake: **solo botones** en el MVP (coherente con la dirección «wake only by buttons» del Launcher).

## 7. Simulador y herramientas

`tools/sim/` compila el núcleo con `cc` para:
1. **Tests headless** (`tests/`): simulan semanas con dietas distintas y comprueban los criterios A1–A10 de `PET_DESIGN.md`. Se ejecutan sin ventana y sin ESP-IDF. Patrón reutilizable: `ESP32Watch-Fluid/tools/host_bench/run.sh` (build con `cc`, escenario guionizado, fallo ante estado inválido).
2. **Generador de hojas de contacto**: PNG a escala real (410×502) con etapas × ramas × variantes × expresiones × frames, más tira de frames o GIF. PNG con `stb_image_write` (una cabecera, vendorizada con su aviso); GIF mediante tira de PNG + herramienta externa, o `gifenc` si su licencia lo permite [verificar].
3. **Ventana SDL2 interactiva**: ratón como táctil, teclado como botones y como sensores (sacudida, USB, avance rápido del tiempo). Aviso: no hay ningún simulador SDL en el workspace; es trabajo nuevo.
4. **Avance rápido del tiempo** (×1, ×60, ×3600) para ver una vida entera en minutos.

## 8. Plan de medición de consumo (primer hito de la fase 2)

Antes de fijar ninguna frecuencia de escaneo. Se usa el método de core (tendencia de tensión de batería, mV/h; línea base del Launcher ≈ 24 mV/h ≈ 12 mA en reposo sin BLE, `launcher-baseline-test`):

| Prueba | Descripción |
| --- | --- |
| E0 | Referencia: light sleep sin radio |
| E1 | Escaneo pasivo cada 5 min, radio init/deinit en cada escaneo |
| E2 | Igual cada 15 min |
| E3 | WiFi iniciada y ociosa (descartable si E1/E2 ya son bajos) |
| E4 | Mesa: pantalla atenuada con USB (solo funcional, no de consumo) |

Cada prueba ≥ 3 h en muñeca real. Salidas: coste incremental del escaneo, tiempo de escaneo, pico y suelo de RAM interna. **Objetivo propuesto: ≤ +25 % sobre E0** en modo muñeca (a confirmar con el usuario). Se compila como firmware de prueba en `ota_4` (slot de pruebas): **no se deja flasheado sin avisar**, y se reflashea el normal al terminar.

## 9. Trabajo entre repositorios (al arrancar el desarrollo, no ahora)

- Launcher: línea en `apps.conf` y entrada en `KNOWN[]` (`os_apps.c`).
- Core: fila en la tabla de repos de `workspace/AGENTS.md`; namespace `pet` en `docs/ARCHITECTURE.md`; si aparece un hallazgo de hardware (WiFi pasivo, consumo), a `docs/` de core y, si cambia cómo se escriben apps, a la plantilla.
- Este repo: `.github/workflows/build.yml`, `dependencies.lock`, `.devcontainer`, `.vscode`, `.clangd` y `skills-lock.json` que `check_apps.sh` espera. Ejecutar `ESP32Watch-core/tools/check_apps.sh` y dejarlo sin avisos.
- `main/idf_component.yml`: el BSP está fijado como `^1.0.6` (no 1.0.7, como decía el spec anterior).

## 10. Riesgos técnicos

| Riesgo | Mitigación |
| --- | --- |
| RAM interna con WiFi + render + LVGL | radio solo durante el escaneo; búfer lógico pequeño; medir pico en E1 |
| Binario > 2 MB en slot | medir esqueleto con WiFi primero; ajustar `sdkconfig.defaults` |
| Consumo del escaneo en muñeca | plan de medición del apartado 8 antes de fijar frecuencias |
| Standalone: particiones y `launcher_is_available()` | verificar en el primer build; decidir tabla propia |
| Sin simulador previo | construirlo en fase 0 junto al render |
| Calidad del arte procedural | hojas de contacto + guía de estilo + puntos de control de aprobación (`STYLEGUIDE.md`) |
