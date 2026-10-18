# ESP32Watch-Pet

Mascota virtual tipo Tamagotchi para la Waveshare **ESP32-S3-Touch-AMOLED-2.06** con `ESP-IDF 5.5.4` + `LVGL 9` + BSP oficial de Waveshare.

Estado: **primera version jugable** (motor, escena, despensa, estado, ajustes, escaneo WiFi pasivo, guardado en NVS, noche y escaneo con la pantalla apagada). Falta medir el consumo y probarla horas en la muñeca y como firmware unico. La mascota se alimenta de las redes WiFi de su entorno (escaneo pasivo), evoluciona segun su dieta y los cuidados, y nunca muere.

| Documento | Contenido |
| --- | --- |
| [docs/PET_DESIGN.md](docs/PET_DESIGN.md) | Producto: mecanicas, estadisticas, evolucion, criterios de aceptacion |
| [docs/PET_TECH.md](docs/PET_TECH.md) | Arquitectura, render, radio, persistencia, medicion de consumo |
| [docs/STYLEGUIDE.md](docs/STYLEGUIDE.md) | Guia de estilo del arte generado por codigo |
| [docs/RESEARCH.md](docs/RESEARCH.md) | Analisis de referencias y de fuentes de assets |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Decisiones tomadas y pendientes |
| [docs/CAPABILITY-MAP.md](docs/CAPABILITY-MAP.md) | Modulos y fases |

Se podra instalar como app junto a las demas (slot OTA) o como unico firmware del reloj (standalone); ver `PET_TECH.md`.

## Compilar y flashear

```sh
source "$HOME/.espressif/tools/activate_idf_v5.5.4.sh"   # EIM; otras instalaciones: core docs/SETUP.md
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor   # p. ej. /dev/tty.usbmodem1101; sin -p lo autodetecta
```

Para flashear dentro del launcher, usar `flash_all.sh` de ESP32Watch-Launcher (esta en su `apps.conf`: `pet ota_5`; `ota_4` es el slot de pruebas de core). Sola, `idf.py flash` la graba en `factory` y sustituye al Launcher.

## Documentacion

La documentacion de hardware y entorno esta en [ESP32Watch-core/docs](https://github.com/Sethyrus/ESP32Watch-core/tree/main/docs): hardware y pines, setup de ESP-IDF, gotchas, arquitectura y bring-up.

## Licencia

MIT. Ver [LICENSE](LICENSE).
