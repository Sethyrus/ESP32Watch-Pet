# Simulador PC (sin SDL todavia). `make` -> build/sim ; `make check` ; `make sheets`
CC ?= cc
CFLAGS = -std=c99 -O1 -Wall -Wextra -I../../components/pet_render/include
SRC = sim.c png.c ../../components/pet_render/pet_render.c ../../components/pet_render/pet_masks.c

build/sim: $(SRC) png.h ../../components/pet_render/include/pet_render.h ../../components/pet_render/pet_masks.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

check: build/sim
	build/sim check

sheets: build/sim
	@mkdir -p ../../sheets
	build/sim sheets ../../sheets 3
