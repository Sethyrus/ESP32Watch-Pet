# Decisiones

Registro de decisiones del proyecto. **Decididas** = confirmadas por el usuario o fijadas en el brief. **Pendientes** = llevan mi propuesta; se cierran antes de la fase 0 (las marcadas «antes de F0») o cuando haya datos.

## Decididas

| # | Decisión | Detalle |
| --- | --- | --- |
| D1 | Tono indulgente | La mascota nunca muere; el descuido solo ralentiza, empeora el ánimo y orienta a formas torpes o raras. |
| D2 | Crecimiento por entorno | La dieta de señales es el eje principal. |
| D3 | Uso mixto | Modos mesa y muñeca, detectados solos. |
| D4 | Arte 100 % por código | Sin assets de terceros en el producto. |
| D5 | Evolución visible | Cambia de forma entre etapas y según la rama. |
| D6 | **Despliegue** | Mismo repo instalable como **standalone** (única imagen) o como **app junto a las demás**; si convivir limitara el desarrollo, manda el standalone. Modo «Launcher/esfera»: futuro, solo si no limita otras funciones. |
| D7 | **Enfoque de arte** | Máscaras pixel art a mano en C + variación procedural por semilla + cara y animación por código. |
| D8 | **Ciclo de vida** | ≈ 1 semana hasta adulto (huevo: minutos; cría: horas; juvenil: ~2 días; adulto: ~6–7 días). |
| D9 | **Radios del MVP** | Solo WiFi pasivo. BLE y el eje WiFi/BLE, tras medir consumo. |

## Pendientes

| # | Decisión | Propuesta | Cuándo |
| --- | --- | --- | --- |
| P1 | **Interfaz**: render propio para todo, o escena propia + LVGL bajo demanda para menús y ajustes | **Escena propia + LVGL bajo demanda**, como `fluid_app/fluid_menu.c` (flush propio, asignador en PSRAM). Evita reinventar listas, desplazamiento y fuentes. | antes de F0 |
| P2 | **Rejilla y escala** | **Criatura en lienzo 48×48 a ×6** (288×288 px): deja ~70 px de HUD y ~120 px de bandeja. Alternativas: 64×64 a ×5; o pantalla completa 82×100 a ×5. | antes de F0 |
| P3 | **Slot y particiones** | Modo app: `ota_5`. Modo standalone: tabla propia solo si el binario no cabe en `factory` (decisión tras medir un esqueleto con WiFi); es una desviación deliberada de «no tocar offsets». | tras el primer build con WiFi |
| P4 | **Frecuencias de escaneo** | Provisional: mesa ~60 s; muñeca 10–15 min. Definitivo tras el plan de medición (`PET_TECH.md`, sección 8). | fase 2 |
| P5 | **Objetivo de consumo en muñeca** | **≤ +25 % sobre light sleep sin radio** (≈ 12 mA de referencia). ¿Aceptable? | antes de la medición |
| P6 | **Audio** | Pitidos de onda cuadrada/triangular generados en C, silenciables, apagados de noche; mínimo en MVP (señal nueva, caricia, llamada de hambre). | antes de F2 |
| P7 | **Navegación y gestos** | Toque = caricia; deslizar arriba = despensa; abajo = estado; BOOT = aceptar; PWR corto = atrás/salir (`PET_DESIGN.md`, sección 10). | antes de F2 |
| P8 | **Valores numéricos** (tasas, umbrales, topes) | Los de `PET_DESIGN.md` como punto de partida; se calibran con los tests de semanas (A1–A10) en F1. | fase 1 |
| P9 | **Nombre del proyecto y de la especie** | Proyecto: `ESP32Watch-Pet` (en el Launcher: «Pet»). Especie: decidir tras ver la primera criatura (C1). Candidatas: *Beacot* (beacon + mascot), *Sigmon*, *Aetherling*. | C1 |
| P10 | **Packs CC0 como referencia** | Mantenerlos **fuera del repo** (carpeta local ignorada por git); solo se citan en `RESEARCH.md`. Si algún día se incluye alguno, con su licencia y atribución en `docs/ASSETS.md`. | antes de F0 |
| P11 | **Noche por defecto** | 23:00–08:00, ajustable en Ajustes. | F1 |
| P12 | **Detección de mesa/muñeca** | USB conectado + quietud por IMU ≥ 60 s, con histéresis; umbral de quietud a medir en hardware. | fase 2 |
| P13 | **Despertar al levantar la muñeca** | Fuera del MVP (INT del QMI8658 en GPIO21 sin validar; el Launcher despierta solo por botones). Reabrir si core lo valida. | futuro |
| P14 | **GIF** | PNG en tira + herramienta externa; `gifenc` solo si su licencia lo permite. | antes de F0 |

## Cosas que necesitan tu confirmación expresa antes de empezar F0
P1, P2, P10 y P14 (afectan a lo primero que se construye), y P5 antes de medir.
