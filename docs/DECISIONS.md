# Decisiones

Registro de decisiones del proyecto. Criterio general (usuario, 2026-10-17): **prima lo funcional**; se descarta cualquier vía que pueda dar problemas o funcionar mal, aunque sea más vistosa. Las decisiones técnicas se tomaron con datos verificados (código de core y compilaciones de prueba; ver `PET_TECH.md`).

## Producto (usuario / brief)

| # | Decisión | Detalle |
| --- | --- | --- |
| D1 | Tono indulgente | Nunca muere; el descuido solo ralentiza, empeora el ánimo y orienta a formas torpes o raras. |
| D2 | Crecimiento por entorno | La dieta de señales es el eje principal. |
| D3 | Uso mixto | Modos mesa y muñeca, detectados solos. |
| D4 | Arte 100 % por código | Sin assets de terceros en el producto. |
| D5 | Evolución visible | Cambia de forma entre etapas y según la rama. |
| D6 | Despliegue | App junto a las demás **y** standalone. El modo Launcher/esfera queda para el futuro. |
| D7 | Enfoque de arte | Máscaras pixel art a mano en C + variación por semilla + cara y animación por código. |
| D8 | Ciclo de vida | ≈ 1 semana hasta adulto. |
| D9 | Radios del MVP | Solo WiFi pasivo; BLE tras medir. |

## Técnicas (cerradas con criterio funcional)

| # | Decisión | Por qué |
| --- | --- | --- |
| T1 | **Un solo binario para app y standalone**, sin Kconfig; el modo se detecta con `watch_launcher_is_available()` | Verificado en `watch_launcher.c`: es falso cuando la app corre desde `factory`. Un único camino que probar. |
| T2 | **Presupuesto de flash ≤ 1,5 MiB** | Es lo que cabe en `factory` para el standalone. Medido: 0,99 MiB con WiFi solo-escaneo (~530 KB de margen). |
| T3 | **Sin deep sleep en el MVP** (ninguno de los dos modos) | En modo app arrancaría el Launcher; en standalone no está medido ni soportado por `watch_board`. Light sleep para todo. |
| T4 | **Escaneo con pantalla apagada mediante `watch_power_screen_off(timeout, stay_awake)`** | `watch_power_sleep()` vuelve a encender la pantalla al despertar por timeout (verificado en `watch_power.h`). |
| T5 | **UI con LVGL por `watch_display_start()`**; escena de la criatura como imagen RGB565 en PSRAM rellenada por `pet_render` | Es el modo en que core gestiona sueño, táctil y brillo; la escena solo necesita 12,5 fps. Plan B (framebuffer propio) sin tocar `pet_render`, solo si la prueba D1 falla. |
| T6 | **`buffer_lines = 20`** en `watch_display` | Libera ~49 KB de RAM interna para WiFi (el caso de BLE en `GOTCHAS.md`). |
| T7 | **Lienzo de criatura 48×48 a ×6** (288×288 px) | Deja ~70 px de HUD y ~120 px de bandeja; un único tamaño de píxel. |
| T8 | **WiFi solo-escaneo**: sin `esp_netif`/lwip, sin optimizaciones IRAM, búferes en PSRAM, `nvs_enable = 0`, init/deinit por escaneo, país `ES` | Medido: ≈ −180 KB de flash y ≈ −20 KB de RAM interna frente a la config por defecto; no toca el NVS compartido. |
| T9 | **Slot `ota_5`** en modo app | Libre; `ota_4` sigue siendo el slot de pruebas de core. |
| T10 | **Objetivo de consumo: el escaneo en muñeca añade ≤ 25 % sobre light sleep sin radio** | Es la referencia para fijar la frecuencia en F2. |
| T11 | **Frecuencias**: provisional mesa ~60 s, muñeca 15 min; definitivas tras E0–E2 | No se fija nada sin medir. |
| T12 | **Despertar solo con botones**; levantar la muñeca solo reacciona con la pantalla encendida | Core no ha validado la interrupción del IMU; coherente con el Launcher. |
| T13 | **Persistencia A/B con CRC en NVS `pet`**, carga útil que solo crece, ≥ 30 s entre escrituras + latido de 10 min | Resistente a cortes y migrable. |
| T14 | **Hash SipHash-2-4 con clave por dispositivo**, 32 bits | Privacidad sin coste; colisiones irrelevantes con 64 entradas. |
| T15 | **Hojas de contacto:** PNG con `stb_image_write` (vendorizado con su aviso); GIF con `ffmpeg` (instalado) a partir de los PNG, opcional | Sin dependencias raras; en CI solo PNG. |
| T16 | **Simulador interactivo con SDL2** (instalado vía Homebrew); los tests headless no dependen de SDL | Los tests corren en cualquier máquina y en CI. |
| T17 | **Tests de host en un workflow aparte** (`host-tests.yml`) | `check_apps.sh` exige que `build.yml` sea idéntico al de la plantilla. |
| T18 | **Packs CC0 de referencia fuera del repo** | Ni licencias ni atribuciones que mantener. |

## Producto (cerradas por mí, revisables)

| # | Decisión |
| --- | --- |
| R1 | Navegación: toque = caricia; deslizar arriba = despensa; abajo = estado; BOOT = aceptar; PWR corto = atrás (en la raíz: Launcher en modo app, nada en standalone). |
| R2 | Noche por defecto 23:00–08:00, ajustable. |
| R3 | Audio mínimo (señal nueva, caricia, hambre) con ondas generadas en C, silenciable y mudo de noche; es lo último de F2 y se recorta si complica el consumo. |
| R4 | Valores numéricos de `PET_DESIGN.md` como punto de partida; se calibran con los tests A1–A10 en F1. |

## Abiertas

| # | Decisión | Cuándo |
| --- | --- | --- |
| A1 | Nombre de la especie (candidatas: *Beacot*, *Sigmon*, *Aetherling*); el proyecto sigue siendo `ESP32Watch-Pet` | al ver la primera criatura (C1) |
| A2 | Tiempo pasivo por canal (120–200 ms) | prueba R1 en F2 |
| A3 | Umbral de quietud para el modo mesa | F2, en el reloj |
