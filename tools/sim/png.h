/* Escritor PNG minimo (RGB, deflate sin comprimir). Solo para el simulador. */
#pragma once
#include <stdint.h>
int png_write_rgb(const char *path, const uint32_t *rgb, int w, int h); /* rgb = 0xRRGGBB */
