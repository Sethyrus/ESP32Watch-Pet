# Capability Map: ESP32Watch-Pet

Estado: **fase 3 (juego v2) en curso.** Módulos, orden de construcción y puertas de salida de cada fase. Detalle en `PET_DESIGN.md` (producto), `PET_TECH.md` (técnica) y `STYLEGUIDE.md` (arte).

## Módulos

| Módulo | Responsabilidad | Depende de |
| --- | --- | --- |
| `pet_core` | Estado, reglas, despensa, decisiones, ánimo, evolución, tiempo ausente, serialización versionada. C99 puro; sin hardware; determinista. | — |
| `pet_render` | Paleta, máscaras, cara, animación, iconos → búfer indexado. C99 puro. | contrato de datos de `pet_core` (`pet_view_t`) |
| `tools/sim` | Tests headless, hojas de contacto PNG/tira, ventana SDL2 interactiva, avance rápido del tiempo. | `pet_core`, `pet_render` |
| `pet_app` (HAL/UI) | UI LVGL, táctil, botones, IMU, RTC, batería, NVS, sueño sobre `watch_board`. | `pet_core`, `pet_render` |
| `pet_app` (radio) | Escaneo WiFi pasivo, hash con sal, planificador. | `pet_core` |
| `pet_app` (modos) | Modo app y standalone (detectado con `watch_launcher_is_available()`), detección mesa/muñeca. | HAL, radio |

## Fases

### Fase 0: arte (mayor riesgo, va primero) — **hecha (C0–C4 aprobados el 2026-10-17)**
1. Simulador mínimo + generador de hojas de contacto con la primera criatura de prueba.
2. **C0:** guía de estilo + moodboard + paleta aprobados.
3. **C1:** cría y juvenil con la cara completa. **C2:** los dos adultos y variantes. **C3:** animaciones en tira.
4. **C4 (salida de la fase):** hoja de referencia aprobada con 4 etapas, ≥ 2 adultos y animaciones básicas; hojas aprobadas guardadas.

Resultado: `components/pet_render` (huevo, cría blob, juvenil pera, adultos Hogareño y Explorador, variantes pulida/torpe/rara, 8 expresiones, animaciones idle/botar/comer/dormir), `tools/sim` (`make check`, `make sheets`) y las hojas de referencia en `docs/art/approved/`. Pendiente para más adelante: iconos de comida, huevo agrietado al eclosionar, ventana SDL2 interactiva.

### Fase 1: motor en PC — **hecha** (A1–A10 en verde)
1. `pet_core`: estado, despensa, familiaridad por BSSID, comida, ánimo, decisiones, evolución, errores de cuidado.
2. Tiempo ausente (`pet_catch_up`), noche, serialización A/B con migración.
3. Tests de semanas con distintas dietas.
- **Salida:** criterios A1–A10 de `PET_DESIGN.md` en verde; valores calibrados (P8).

Resultado: `components/pet_core` (~16 KB de código, 0 B de RAM estática, compilado para xtensa sin avisos), `tests/` (`make -C tests test` y `make -C tests asan`; 9.728 comprobaciones) y la calibración de `PET_DESIGN.md`, sección 14. Decisiones R5–R9 en `DECISIONS.md`.

Las fases 0 y 1 pueden solaparse una vez fijado `pet_view_t` (qué necesita el render del estado).

### Fase 2: firmware
Orden:
1. **Pruebas en el reloj** R1 (sonda de radio) y D1 (flush LVGL de la escena) en `ota_4`: **hechas el 2026-10-17, ambas superadas** (heap mínimo 86 KB frente a ≥ 30; flush 14 ms frente a ≤ 40). `ota_4` borrado y `otadata` en blanco. Tamaño con WiFi ya medido: 0,99 MiB.
2. **Consumo** E0–E2 (`PET_TECH.md`, sección 8): fija la frecuencia de escaneo (T10, T11).
3. Bring-up: escena por `lv_image`, táctil, gestos, botones, RTC. **Hecho (2026-10-17, probado en `ota_4`).**
4. Radio real → `pet_ingest_scan`; persistencia en NVS A/B. **Hecho**; con USB y pantalla apagada escanea cada 60 s (R12).
5. Detección de modo (USB = mesa), sueño, noche. **Hecho** salvo saludo e IMU (R11). **Sin probar:** el camino en batería con la pantalla apagada (`stay_awake` en light sleep) durante horas.
6. Comprobar los dos modos de instalación con el mismo binario (`ota_5` y `factory`). **App en `ota_5`: hecho** (2026-10-18; el Launcher la lista como Pet, arranca con la partida guardada y PWR vuelve al Launcher). **Standalone en `factory`: pendiente.**
7. Alta en el Launcher y trabajo entre repositorios (`PET_TECH.md`, sección 9). **Hecho:** `apps.conf`, `KNOWN[]`, tablas y GOTCHAS de core, grabada con `flash_all.sh pet` y `launcher`; `ota_4` borrado; `check_apps.sh` sin avisos.
- **Salida:** MVP jugable en el reloj; medidas de consumo documentadas en core `docs/`.

### Fase 3: juego v2 (R16–R21) — en curso desde el 2026-10-18

| # | Paso | Estado |
| --- | --- | --- |
| 1 | Motor v3 (`pet_core`): necesidades, monedas, cacas, malito, deseos, nivel, ramas por entrenamiento, generaciones; partida v3 con migración | **Hecho.** B1–B9 en verde con bots. |
| 2 | Minijuegos (`pet_game`): Atrapa, Memoria, Pesas | **Hecho.** B10 en verde. |
| 3 | Arte: adultos Sabio y Forzudo, accesorios, iconos, escena (`pet_gfx`, `pet_scene`) | **Hecho.** Hojas en `make -C tools/sim sheets` y `build/sim scenes`. |
| 4 | Interfaz v2 y simulador `tools/uisim` | **Hecho.** Recorrido completo en `make -C tools/uisim shots`. |
| 5 | En el reloj | **Instalado en `ota_5`** el 2026-10-18. Arranca, migra la partida real (pasa a juvenil, nivel 4), escanea y guarda. **Pendiente:** prueba de sensación del usuario: tacto de los juegos, fluidez y claridad. |
| 6 | Siguiente tanda | Sonido (R3), fondos de escena, más accesorios y juegos, IMU en Atrapa (pide apagar `imu_service` en core), standalone en `factory`. |

## Fuera del MVP

BLE y eje WiFi/BLE, voz, social, esfera del Launcher, despertar al levantar la muñeca.
