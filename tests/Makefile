# Tests de host de pet_core (C99 puro, sin ESP-IDF). `make test`
CC ?= cc
CORE = ../components/pet_core
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -I$(CORE)/include -I../components/pet_render/include
SRC = test_core.c $(CORE)/pet_core.c $(CORE)/pet_save.c $(CORE)/pet_view.c ../components/pet_render/pet_masks.c ../components/pet_render/pet_render.c

build/test_core: $(SRC) world.h $(CORE)/include/pet_core.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

test: build/test_core
	build/test_core

# Con sanitizadores: UB y desbordes en la serializacion y el fuzz.
asan:
	@mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(SRC) -o build/test_core_asan -lm
	build/test_core_asan
