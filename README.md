# ESP32Watch-Pet

Mascota virtual tipo Tamagotchi para la Waveshare **ESP32-S3-Touch-AMOLED-2.06** con `ESP-IDF 5.5.4` + `LVGL 9` + BSP oficial de Waveshare.

Diseno y mecanicas en [docs/PET_DESIGN.md](docs/PET_DESIGN.md) (TBD).

## Compilar y flashear

```sh
source "$HOME/.espressif/tools/activate_idf_v5.5.4.sh"   # EIM; otras instalaciones: core docs/SETUP.md
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor   # p. ej. /dev/tty.usbmodem1101; sin -p lo autodetecta
```

Para flashear dentro del launcher, usar `flash_all.sh` de ESP32Watch-Launcher (la app ocupa `ota_4`).

## Documentacion

La documentacion de hardware y entorno esta en [ESP32Watch-core/docs](https://github.com/Sethyrus/ESP32Watch-core/tree/main/docs): hardware y pines, setup de ESP-IDF, gotchas, arquitectura y bring-up.

## Licencia

MIT. Ver [LICENSE](LICENSE).
