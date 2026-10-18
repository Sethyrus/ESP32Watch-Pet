# Simulador PC (sin SDL todavia). `make` -> build/sim ; `make check` ; `make sheets`
CC ?= cc
CFLAGS = -std=c99 -O1 -Wall -Wextra -Wno-missing-field-initializers -I../../components/pet_render/include -I../../components/pet_core/include -I../../components/pet_game/include -I../../components/pet_sound/include
SRC = sim.c png.c $(wildcard ../../components/pet_render/*.c) $(wildcard ../../components/pet_core/*.c) ../../components/pet_game/pet_game.c ../../components/pet_sound/pet_sound.c

build/sim: $(SRC) png.h $(wildcard ../../components/*/include/*.h) ../../components/pet_render/pet_masks.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

check: build/sim
	build/sim check

sheets: build/sim
	@mkdir -p ../../sheets
	build/sim sheets ../../sheets 3
	python3 -I pngpack.py ../../sheets/*.png
